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
#define HASH_TABLE_SIZE 1009  // Prime number for better distribution

/* ============================================================================
 * EXTENDED STORAGE SERVER INFO (add files list)
 * ============================================================================ */

typedef struct {
    StorageServerInfo base;  // Use the one from protocol.h
    int file_count;
    char files[MAX_FILES][MAX_FILENAME_LEN];
} ExtendedSSInfo;

/* ============================================================================
 * HASH MAP FOR FILE->SS MAPPING (O(1) lookup)
 * ============================================================================ */

typedef struct HashNode {
    char filename[MAX_FILENAME_LEN];
    int ss_index;  // Which storage server has this file
    struct HashNode* next;  // For collision chaining
} HashNode;

typedef struct {
    HashNode* buckets[HASH_TABLE_SIZE];
    int size;  // Total number of entries
} HashMap;

HashMap file_map;

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
    
    // Initialize HashMap
    memset(&file_map, 0, sizeof(HashMap));
    for (int i = 0; i < HASH_TABLE_SIZE; i++) {
        file_map.buckets[i] = NULL;
    }
    file_map.size = 0;
}

/* ============================================================================
 * HASH MAP IMPLEMENTATION FOR O(1) FILE LOOKUP
 * ============================================================================ */

// Simple hash function (djb2 algorithm)
unsigned int hash_string(const char* str) {
    unsigned long hash = 5381;
    int c;
    
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c; // hash * 33 + c
    }
    
    return hash % HASH_TABLE_SIZE;
}

// Add file to HashMap
void hashmap_add(const char* filename, int ss_index) {
    unsigned int index = hash_string(filename);
    
    // Check if file already exists (update if so)
    HashNode* current = file_map.buckets[index];
    while (current != NULL) {
        if (strcmp(current->filename, filename) == 0) {
            current->ss_index = ss_index;  // Update existing entry
            log_debug("Updated HashMap: %s -> SS[%d]", filename, ss_index);
            return;
        }
        current = current->next;
    }
    
    // Create new node
    HashNode* new_node = (HashNode*)malloc(sizeof(HashNode));
    if (new_node == NULL) {
        log_error("Failed to allocate memory for HashMap node");
        return;
    }
    
    strncpy(new_node->filename, filename, MAX_FILENAME_LEN - 1);
    new_node->filename[MAX_FILENAME_LEN - 1] = '\0';
    new_node->ss_index = ss_index;
    
    // Insert at head of bucket (most efficient)
    new_node->next = file_map.buckets[index];
    file_map.buckets[index] = new_node;
    file_map.size++;
    
    log_debug("Added to HashMap: %s -> SS[%d] (bucket %u, total: %d)", 
             filename, ss_index, index, file_map.size);
}

// Find which SS has a file - O(1) average case
int hashmap_find(const char* filename) {
    unsigned int index = hash_string(filename);
    
    HashNode* current = file_map.buckets[index];
    while (current != NULL) {
        if (strcmp(current->filename, filename) == 0) {
            log_debug("HashMap lookup: %s found at SS[%d]", filename, current->ss_index);
            return current->ss_index;
        }
        current = current->next;
    }
    
    log_debug("HashMap lookup: %s not found", filename);
    return -1;  // Not found
}

// Remove file from HashMap
void hashmap_remove(const char* filename) {
    unsigned int index = hash_string(filename);
    
    HashNode* current = file_map.buckets[index];
    HashNode* prev = NULL;
    
    while (current != NULL) {
        if (strcmp(current->filename, filename) == 0) {
            // Found it - remove from linked list
            if (prev == NULL) {
                // First node in bucket
                file_map.buckets[index] = current->next;
            } else {
                prev->next = current->next;
            }
            
            log_debug("Removed from HashMap: %s (was at SS[%d])", 
                     filename, current->ss_index);
            free(current);
            file_map.size--;
            return;
        }
        prev = current;
        current = current->next;
    }
}

// Get HashMap statistics (for monitoring/debugging)
void hashmap_stats() {
    int used_buckets = 0;
    int max_chain = 0;
    
    for (int i = 0; i < HASH_TABLE_SIZE; i++) {
        if (file_map.buckets[i] != NULL) {
            used_buckets++;
            
            // Count chain length
            int chain_len = 0;
            HashNode* current = file_map.buckets[i];
            while (current != NULL) {
                chain_len++;
                current = current->next;
            }
            
            if (chain_len > max_chain) {
                max_chain = chain_len;
            }
        }
    }
    
    double load_factor = (double)file_map.size / HASH_TABLE_SIZE;
    log_info("HashMap Stats: %d files, %d/%d buckets used (%.2f%%), max chain: %d, load: %.3f",
             file_map.size, used_buckets, HASH_TABLE_SIZE, 
             (used_buckets * 100.0 / HASH_TABLE_SIZE), max_chain, load_factor);
}

/* ============================================================================
 * FILE LOOKUP FUNCTIONS (Updated to use HashMap)
 * ============================================================================ */

int find_ss_for_file(const char* filename) {
    // Use O(1) HashMap lookup instead of O(N) linear search
    return hashmap_find(filename);
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
        
        // Add to HashMap for O(1) lookup
        hashmap_add(filename, ss_index);
        
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
            
            // Add to HashMap for O(1) lookup
            hashmap_add(token, ss_index);
            
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
    
    // Log HashMap statistics
    hashmap_stats();
    
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
        // Remove from HashMap
        hashmap_remove(msg->filename);
        
        // Remove from SS's file list
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
 * REQUEST HANDLER - Process client/SS requests
 * ============================================================================ */

void handle_connection(int conn_socket, char* client_ip, int client_port) {
    log_info("New connection from %s:%d on socket %d", client_ip, client_port, conn_socket);
    
    Message msg;
    int bytes = receive_message(conn_socket, &msg);
    
    if (bytes <= 0) {
        log_error("Failed to receive initial message from %s:%d", client_ip, client_port);
        close_socket(conn_socket);
        return;
    }
    
    switch (msg.operation) {
        case OP_REGISTER_SS:
            log_info("Storage Server registration request");
            handle_ss_registration(conn_socket, &msg);
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
            log_warning("Unknown operation: %d from %s:%d", msg.operation, client_ip, client_port);
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

/* ============================================================================
 * MAIN SERVER LOOP - Using select() for concurrent connections
 * ============================================================================ */

int main() {
    int server_socket;
    int opt = 1;
    fd_set read_fds, active_fds;
    int max_fd;
    struct timeval timeout;
    
    init_logger("NS", "logs/nameserver.log");
    
    log_info("==========================================================");
    log_info("Name Server Starting - Docs++ Distributed Document System");
    log_info("==========================================================");
    
    init_registries();
    log_info("Registries initialized with HashMap (O(1) lookup)");
    
    // Create server socket
    server_socket = create_socket();
    if (server_socket < 0) {
        log_critical("Failed to create server socket");
        return 1;
    }
    log_info("Server socket created (fd: %d)", server_socket);
    
    // Set SO_REUSEADDR to prevent "Address already in use"
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
    log_info("Server listening on port %d with select() multiplexing", NS_PORT);
    log_info("Ready to handle concurrent SS and Client connections");
    log_info("==========================================================");
    
    // Initialize file descriptor sets for select()
    FD_ZERO(&active_fds);
    FD_SET(server_socket, &active_fds);
    max_fd = server_socket;
    
    // Main server loop with select()
    while (1) {
        // Copy active_fds to read_fds (select modifies the set)
        read_fds = active_fds;
        
        // Set timeout for select (1 second - allows periodic checks)
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;
        
        // Wait for activity on any socket
        int activity = select(max_fd + 1, &read_fds, NULL, NULL, &timeout);
        
        if (activity < 0) {
            if (errno == EINTR) {
                // Interrupted by signal, continue
                continue;
            }
            log_error("select() error: %s", strerror(errno));
            continue;
        }
        
        if (activity == 0) {
            // Timeout - no activity, just continue
            continue;
        }
        
        // Check all file descriptors for activity
        for (int fd = 0; fd <= max_fd; fd++) {
            if (!FD_ISSET(fd, &read_fds)) {
                continue;  // No activity on this fd
            }
            
            if (fd == server_socket) {
                // Activity on server socket = new connection
                char client_ip[MAX_IP_LEN];
                int client_port;
                
                int new_socket = accept_connection(server_socket, client_ip, &client_port);
                if (new_socket < 0) {
                    log_error("Failed to accept connection");
                    continue;
                }
                
                log_info("New connection accepted on socket %d from %s:%d", 
                        new_socket, client_ip, client_port);
                
                // Add new socket to active set
                FD_SET(new_socket, &active_fds);
                if (new_socket > max_fd) {
                    max_fd = new_socket;
                }
                
                log_debug("Added socket %d to active set (max_fd: %d)", new_socket, max_fd);
                
            } else {
                // Activity on existing connection = data ready to read
                char client_ip[MAX_IP_LEN] = "unknown";
                int client_port = 0;
                
                // Get peer info for logging
                struct sockaddr_in addr;
                socklen_t addr_len = sizeof(addr);
                if (getpeername(fd, (struct sockaddr*)&addr, &addr_len) == 0) {
                    inet_ntop(AF_INET, &addr.sin_addr, client_ip, MAX_IP_LEN);
                    client_port = ntohs(addr.sin_port);
                }
                
                                // Handle the request (this will close the socket when done)
                handle_connection(fd, client_ip, client_port);
                
                // Remove from active set (socket was closed in handle_connection)
                FD_CLR(fd, &active_fds);
                log_debug("Removed socket %d from active set", fd);
            }
        }
    }
    
    close_socket(server_socket);
    close_logger();
    
    return 0;
}
