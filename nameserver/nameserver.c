/*
 * nameserver/nameserver.c
 * * Name Server - Central coordinator for Docs++ system
 * Handles registrations and routes requests between clients and storage servers
 * * Compile: gcc -o nameserver nameserver.c ../common/socket_utils.c ../common/logger.c -I../common -pthread
 * Run: ./nameserver
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/select.h>
#include <sys/socket.h> // Required for setsockopt
#include <errno.h>
#include "socket_utils.h"
#include "logger.h"
#include "protocol.h"

#define NS_PORT 8080
#define MAX_CONNECTIONS 50
#define MAX_FILES 1000

/* ============================================================================
 * EXTENDED STORAGE SERVER INFO (add files list)
 * ============================================================================ */

typedef struct {
    StorageServerInfo base;  // Use the one from protocol.h
    int file_count;
    char files[MAX_FILES][MAX_FILENAME_LEN];
} ExtendedSSInfo;

/* ============================================================================
 * GLOBAL REGISTRY (Original code retained)
 * ============================================================================ */

ExtendedSSInfo storage_servers[MAX_STORAGE_SERVERS];
int ss_count = 0;

ClientInfo clients[MAX_CONCURRENT_CLIENTS];
int client_count = 0;

/* ============================================================================
 * HELPER FUNCTIONS (Original code retained)
 * ============================================================================ */

void init_registries() {
    memset(storage_servers, 0, sizeof(storage_servers));
    memset(clients, 0, sizeof(clients));
    ss_count = 0;
    client_count = 0;
}

int find_ss_for_file(const char* filename) {
    for (int i = 0; i < ss_count; i++) {
        if (!storage_servers[i].base.is_alive) continue;
        
        for (int j = 0; j < storage_servers[i].file_count; j++) {
            if (strcmp(storage_servers[i].files[j], filename) == 0) {
                return i;
            }
        }
    }
    return -1;
}

int find_available_ss() {
    for (int i = 0; i < ss_count; i++) {
        if (storage_servers[i].base.is_alive) {
            return i;
        }
    }
    return -1;
}

void add_file_to_ss(int ss_index, const char* filename) {
    if (ss_index < 0 || ss_index >= ss_count) return;
    
    int file_idx = storage_servers[ss_index].file_count;
    if (file_idx < MAX_FILES) {
        strncpy(storage_servers[ss_index].files[file_idx], filename, MAX_FILENAME_LEN - 1);
        storage_servers[ss_index].files[file_idx][MAX_FILENAME_LEN - 1] = '\0';
        storage_servers[ss_index].file_count++;
        log_info("Added file '%s' to SS[%d], total files: %d", 
                 filename, ss_index, storage_servers[ss_index].file_count);
    }
}

int file_exists(const char* filename) {
    return find_ss_for_file(filename) >= 0;
}

/* ============================================================================
 * REGISTRATION HANDLERS (Modified for file sync)
 * ============================================================================ */

void handle_ss_registration(int ss_socket, Message* msg) {
    if (ss_count >= MAX_STORAGE_SERVERS) {
        log_error("Maximum storage servers reached, rejecting registration");
        Message response;
        INIT_MESSAGE(response);
        response.operation = OP_ACK;
        response.error_code = ERR_SERVER_ERROR;
        strcpy(response.content, "Maximum storage servers reached");
        send_message(ss_socket, &response);
        return;
    }
    
    // Store SS info
    int ss_index = ss_count;
    strncpy(storage_servers[ss_index].base.ip, msg->ip_address, MAX_IP_LEN - 1);
    storage_servers[ss_index].base.nm_port = msg->nm_port;
    storage_servers[ss_index].base.client_port = msg->client_port;
    storage_servers[ss_index].base.is_alive = 1;
    storage_servers[ss_index].base.last_heartbeat = time(NULL);
    // Initialize file count before deserializing
    storage_servers[ss_index].file_count = 0;
    
    // FIX: Process the file list sent in the content field
    char temp_content[MAX_CONTENT_LEN];
    strncpy(temp_content, msg->content, MAX_CONTENT_LEN - 1);
    temp_content[MAX_CONTENT_LEN - 1] = '\0';
    
    char* token = strtok(temp_content, "|");
    int files_added = 0;
    
    // Parse the file names separated by '|'
    while (token != NULL) {
        if (files_added < MAX_FILES) {
            strncpy(storage_servers[ss_index].files[files_added], token, MAX_FILENAME_LEN - 1);
            storage_servers[ss_index].files[files_added][MAX_FILENAME_LEN - 1] = '\0';
            files_added++;
        }
        token = strtok(NULL, "|");
    }
    
    // Store the count of existing files
    storage_servers[ss_index].file_count = files_added;
    
    ss_count++;
    
    log_info("Storage Server registered: SS[%d]", ss_index);
    log_info("  IP: %s", storage_servers[ss_index].base.ip);
    log_info("  NM Port: %d", storage_servers[ss_index].base.nm_port);
    log_info("  Client Port: %d", storage_servers[ss_index].base.client_port);
    log_info("  Loaded %d existing files.", files_added);
    
    // Send ACK
    Message response;
    INIT_MESSAGE(response);
    response.operation = OP_ACK;
    response.error_code = ERR_SUCCESS;
    snprintf(response.content, sizeof(response.content), 
             "Storage Server registered successfully as SS[%d] with %d files", 
             ss_index, files_added);
    send_message(ss_socket, &response);
    
    log_info("Total active storage servers: %d", ss_count);
}

void handle_client_registration(int client_socket, Message* msg) {
    if (client_count >= MAX_CONCURRENT_CLIENTS) {
        log_error("Maximum clients reached, rejecting registration");
        Message response;
        INIT_MESSAGE(response);
        response.operation = OP_ACK;
        response.error_code = ERR_SERVER_ERROR;
        strcpy(response.content, "Maximum clients reached");
        send_message(client_socket, &response);
        return;
    }
    
    // Store client info
    int client_index = client_count;
    strncpy(clients[client_index].username, msg->sender_id, MAX_USERNAME_LEN - 1);
    strncpy(clients[client_index].ip, msg->ip_address, MAX_IP_LEN - 1);
    clients[client_index].port = msg->nm_port;
    clients[client_index].is_active = 1;
    clients[client_index].connected_time = time(NULL);
    
    client_count++;
    
    log_info("Client registered: %s from %s:%d", 
             clients[client_index].username,
             clients[client_index].ip,
             clients[client_index].port);
    
    // Send ACK
    Message response;
    INIT_MESSAGE(response);
    response.operation = OP_ACK;
    response.error_code = ERR_SUCCESS;
    snprintf(response.content, sizeof(response.content), 
             "Welcome %s! Connected to Name Server", msg->sender_id);
    send_message(client_socket, &response);
    
    log_info("Total active clients: %d", client_count);
}

/* ============================================================================
 * REQUEST HANDLERS (Original code retained)
 * ============================================================================ */

void handle_create_request(int client_socket, Message* msg) {
    log_info("CREATE request from %s for file '%s'", msg->sender_id, msg->filename);
    
    if (file_exists(msg->filename)) {
        log_warning("File '%s' already exists", msg->filename);
        Message response;
        INIT_MESSAGE(response);
        response.operation = OP_ACK;
        response.error_code = ERR_CONFLICT;
        snprintf(response.content, sizeof(response.content), 
                 "File '%s' already exists", msg->filename);
        send_message(client_socket, &response);
        return;
    }
    
    int ss_index = find_available_ss();
    if (ss_index < 0) {
        log_error("No storage servers available");
        Message response;
        INIT_MESSAGE(response);
        response.operation = OP_ACK;
        response.error_code = ERR_SS_UNAVAILABLE;
        strcpy(response.content, "No storage servers available");
        send_message(client_socket, &response);
        return;
    }
    
    log_info("Forwarding CREATE to SS[%d]", ss_index);
    
    int ss_socket = create_socket();
    if (ss_socket < 0) {
        log_error("Failed to create socket for SS connection");
        Message response;
        INIT_MESSAGE(response);
        response.operation = OP_ACK;
        response.error_code = ERR_SERVER_ERROR;
        strcpy(response.content, "Failed to contact storage server");
        send_message(client_socket, &response);
        return;
    }
    
    // Connect to SS on its NM_PORT (8081) for forwarding
    if (connect_to_server(ss_socket, storage_servers[ss_index].base.ip, 
                         storage_servers[ss_index].base.nm_port) < 0) {
        log_error("Failed to connect to SS[%d]", ss_index);
        Message response;
        INIT_MESSAGE(response);
        response.operation = OP_ACK;
        response.error_code = ERR_SS_UNAVAILABLE;
        strcpy(response.content, "Failed to contact storage server");
        send_message(client_socket, &response);
        close_socket(ss_socket);
        return;
    }
    
    Message ss_request;
    memcpy(&ss_request, msg, sizeof(Message));
    
    if (send_message(ss_socket, &ss_request) < 0) {
        log_error("Failed to forward CREATE to SS[%d]", ss_index);
        Message response;
        INIT_MESSAGE(response);
        response.operation = OP_ACK;
        response.error_code = ERR_SERVER_ERROR;
        strcpy(response.content, "Failed to contact storage server");
        send_message(client_socket, &response);
        close_socket(ss_socket);
        return;
    }
    
    Message ss_response;
    if (receive_message(ss_socket, &ss_response) <= 0) {
        log_error("Failed to receive response from SS[%d]", ss_index);
        Message response;
        INIT_MESSAGE(response);
        response.operation = OP_ACK;
        response.error_code = ERR_SERVER_ERROR;
        strcpy(response.content, "Storage server did not respond");
        send_message(client_socket, &response);
        close_socket(ss_socket);
        return;
    }
    
    close_socket(ss_socket);
    
    if (ss_response.error_code == ERR_SUCCESS || ss_response.error_code == ERR_CREATED) {
        add_file_to_ss(ss_index, msg->filename);
        log_info("File '%s' created successfully on SS[%d]", msg->filename, ss_index);
    }
    
    send_message(client_socket, &ss_response);
}

void handle_read_request(int client_socket, Message* msg) {
    log_info("READ request from %s for file '%s'", msg->sender_id, msg->filename);
    
    int ss_index = find_ss_for_file(msg->filename);
    if (ss_index < 0) {
        log_warning("File '%s' not found", msg->filename);
        Message response;
        INIT_MESSAGE(response);
        response.operation = OP_ACK;
        response.error_code = ERR_NOT_FOUND;
        snprintf(response.content, sizeof(response.content), 
                 "File '%s' not found", msg->filename);
        send_message(client_socket, &response);
        return;
    }
    
    log_info("File '%s' found on SS[%d]", msg->filename, ss_index);
    
    Message response;
    INIT_MESSAGE(response);
    response.operation = OP_ROUTE_INFO;
    response.error_code = ERR_SUCCESS;
    strncpy(response.ss_ip, storage_servers[ss_index].base.ip, MAX_IP_LEN - 1);
    response.ss_port = storage_servers[ss_index].base.client_port;
    snprintf(response.content, sizeof(response.content),
             "Connect to SS at %s:%d", response.ss_ip, response.ss_port);
    
    log_info("Routing client to SS[%d] at %s:%d", 
             ss_index, response.ss_ip, response.ss_port);
    
    send_message(client_socket, &response);
}

void handle_delete_request(int client_socket, Message* msg) {
    log_info("DELETE request from %s for file '%s'", msg->sender_id, msg->filename);
    
    int ss_index = find_ss_for_file(msg->filename);
    if (ss_index < 0) {
        log_warning("File '%s' not found", msg->filename);
        Message response;
        INIT_MESSAGE(response);
        response.operation = OP_ACK;
        response.error_code = ERR_NOT_FOUND;
        snprintf(response.content, sizeof(response.content), 
                 "File '%s' not found", msg->filename);
        send_message(client_socket, &response);
        return;
    }
    
    log_info("Forwarding DELETE to SS[%d]", ss_index);
    
    int ss_socket = create_socket();
    if (ss_socket < 0 || 
        connect_to_server(ss_socket, storage_servers[ss_index].base.ip, 
                         storage_servers[ss_index].base.nm_port) < 0) {
        log_error("Failed to connect to SS[%d]", ss_index);
        Message response;
        INIT_MESSAGE(response);
        response.operation = OP_ACK;
        response.error_code = ERR_SERVER_ERROR;
        strcpy(response.content, "Failed to contact storage server");
        send_message(client_socket, &response);
        if (ss_socket >= 0) close_socket(ss_socket);
        return;
    }
    
    if (send_message(ss_socket, msg) < 0) {
        log_error("Failed to forward DELETE to SS");
        Message response;
        INIT_MESSAGE(response);
        response.operation = OP_ACK;
        response.error_code = ERR_SERVER_ERROR;
        strcpy(response.content, "Failed to contact storage server");
        send_message(client_socket, &response);
        close_socket(ss_socket);
        return;
    }
    
    Message ss_response;
    if (receive_message(ss_socket, &ss_response) <= 0) {
        log_error("Failed to receive DELETE response from SS");
        Message response;
        INIT_MESSAGE(response);
        response.operation = OP_ACK;
        response.error_code = ERR_SERVER_ERROR;
        strcpy(response.content, "Storage server did not respond");
        send_message(client_socket, &response);
        close_socket(ss_socket);
        return;
    }
    
    close_socket(ss_socket);
    
    if (ss_response.error_code == ERR_SUCCESS) {
        for (int i = 0; i < storage_servers[ss_index].file_count; i++) {
            if (strcmp(storage_servers[ss_index].files[i], msg->filename) == 0) {
                for (int j = i; j < storage_servers[ss_index].file_count - 1; j++) {
                    strcpy(storage_servers[ss_index].files[j], 
                           storage_servers[ss_index].files[j + 1]);
                }
                storage_servers[ss_index].file_count--;
                log_info("Removed file '%s' from SS[%d] registry", msg->filename, ss_index);
                break;
            }
        }
    }
    
    send_message(client_socket, &ss_response);
}

/* ============================================================================
 * MAIN SERVER LOOP (Modified for SO_REUSEADDR)
 * ============================================================================ */

int main() {
    int server_socket;
    int opt = 1; // Used for SO_REUSEADDR option
    
    init_logger("NS", "logs/nameserver.log");
    
    log_info("==========================================================");
    log_info("Name Server Starting - Docs++ Distributed Document System");
    log_info("==========================================================");
    
    init_registries();
    log_info("Registries initialized");
    
    // Create server socket
    server_socket = create_socket();
    if (server_socket < 0) {
        log_critical("Failed to create server socket");
        return 1;
    }
    log_info("Server socket created (fd: %d)", server_socket);
    
    // FIX: Set SO_REUSEADDR to prevent "Address already in use"
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        log_critical("setsockopt(SO_REUSEADDR) failed for NS listener");
    }
    
    // Bind to port
    if (bind_socket(server_socket, NS_PORT) < 0) {
        log_critical("Failed to bind to port %d", NS_PORT);
        close_socket(server_socket);
        return 1;
    }
    log_info("Socket bound to port %d", NS_PORT);
    
    // Listen
    if (listen_socket(server_socket, MAX_CONNECTIONS) < 0) {
        log_critical("Failed to listen on socket");
        close_socket(server_socket);
        return 1;
    }
    log_info("Server listening on port %d", NS_PORT);
    log_info("Waiting for Storage Servers and Clients to connect...");
    log_info("==========================================================");
    
    // Main server loop
    while (1) {
        char client_ip[MAX_IP_LEN];
        int client_port;
        
        int conn_socket = accept_connection(server_socket, client_ip, &client_port);
        if (conn_socket < 0) {
            log_error("Failed to accept connection");
            continue;
        }
        
        log_info("New connection from %s:%d", client_ip, client_port);
        
        Message msg;
        int bytes = receive_message(conn_socket, &msg);
        
        if (bytes <= 0) {
            log_error("Failed to receive initial message");
            close_socket(conn_socket);
            continue;
        }
        
        switch (msg.operation) {
            case OP_REGISTER_SS:
                log_info("Storage Server registration request");
                handle_ss_registration(conn_socket, &msg);
                // SS registration connection is closed inside handle_ss_registration 
                // after sending ACK (or stays open for heartbeats, which is a choice).
                // If it's a transient connection (like SS->NS->ACK), close here.
                // Based on SS code, it's transient:
                close_socket(conn_socket); 
                break;
                
            case OP_REGISTER_CLIENT:
                log_info("Client registration request from %s", msg.sender_id);
                handle_client_registration(conn_socket, &msg);
                close_socket(conn_socket);
                break;
                
            case OP_CREATE:
                handle_create_request(conn_socket, &msg);
                close_socket(conn_socket);
                break;
                
            case OP_READ:
                handle_read_request(conn_socket, &msg);
                close_socket(conn_socket);
                break;
                
            case OP_DELETE:
                handle_delete_request(conn_socket, &msg);
                close_socket(conn_socket);
                break;
                
            default:
                log_warning("Unknown operation: %d", msg.operation);
                Message response;
                INIT_MESSAGE(response);
                response.operation = OP_ACK;
                response.error_code = ERR_NOT_IMPLEMENTED;
                strcpy(response.content, "Operation not implemented");
                send_message(conn_socket, &response);
                close_socket(conn_socket);
                break;
        }
    }
    
    close_socket(server_socket);
    close_logger();
    
    return 0;
}