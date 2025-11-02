/*
 * tests/test_server.c
 * 
 * Simple TCP echo server to test socket utilities
 * Run this first, then run test_client.c
 * 
 * Compile: gcc -o test_server test_server.c ../common/socket_utils.c ../common/logger.c -I../common
 * Run: ./test_server
 * 
 * Copy this EXACTLY into tests/test_server.c
 */

#include "socket_utils.h"
#include "logger.h"
#include "protocol.h"

#define PORT 9999
#define MAX_CLIENTS 5

int main() {
    int server_socket, client_socket;
    char client_ip[MAX_IP_LEN];
    int client_port;
    
    // Initialize logger
    init_logger("TEST_SERVER", "logs/test_server.log");
    
    log_info("=== Socket Test Server Starting ===");
    
    // Create socket
    server_socket = create_socket();
    if (server_socket < 0) {
        log_error("Failed to create socket");
        close_logger();
        return 1;
    }
    log_info("Socket created successfully (fd: %d)", server_socket);
    
    // Bind to port
    if (bind_socket(server_socket, PORT) < 0) {
        log_error("Failed to bind to port %d", PORT);
        close_socket(server_socket);
        close_logger();
        return 1;
    }
    log_info("Socket bound to port %d", PORT);
    
    // Listen for connections
    if (listen_socket(server_socket, MAX_CLIENTS) < 0) {
        log_error("Failed to listen on socket");
        close_socket(server_socket);
        close_logger();
        return 1;
    }
    log_info("Server listening on port %d...", PORT);
    log_info("Waiting for client connections...");
    
    // Accept one client connection
    client_socket = accept_connection(server_socket, client_ip, &client_port);
    if (client_socket < 0) {
        log_error("Failed to accept connection");
        close_socket(server_socket);
        close_logger();
        return 1;
    }
    log_info("Client connected from %s:%d", client_ip, client_port);
    
    // Receive and echo messages
    log_info("Entering echo loop...");
    log_info("Server will echo back all received messages");
    
    int message_count = 0;
    while (1) {
        Message msg;
        int bytes = receive_message(client_socket, &msg);
        
        if (bytes <= 0) {
            if (bytes == 0) {
                log_info("Client disconnected");
            } else {
                log_error("Failed to receive message");
            }
            break;
        }
        
        message_count++;
        log_info("\n--- Message %d Received ---", message_count);
        log_info("Operation: %s (%d)", get_operation_name(msg.operation), msg.operation);
        log_info("Sender: %s", msg.sender_id);
        log_info("Timestamp: %ld", msg.timestamp);
        
        // Log details based on operation type
        if (msg.operation == OP_CREATE || msg.operation == OP_READ || 
            msg.operation == OP_DELETE || msg.operation == OP_STREAM) {
            log_info("Filename: %s", msg.filename);
        }
        
        if (msg.operation == OP_WRITE) {
            log_info("Filename: %s", msg.filename);
            log_info("Sentence: %d, Word Index: %d", msg.sentence_num, msg.word_index);
            log_info("Content: %s", msg.content);
        }
        
        if (msg.operation == OP_VIEW) {
            log_info("View flags: %d", msg.view_flags);
        }
        
        if (msg.operation == OP_ADDACCESS || msg.operation == OP_REMACCESS) {
            log_info("Filename: %s", msg.filename);
            log_info("Target user: %s", msg.target_user);
            log_info("Access type: %d", msg.access_type);
        }
        
        if (strlen(msg.content) > 0 && msg.operation != OP_WRITE) {
            log_info("Content: %s", msg.content);
        }
        
        // Prepare echo response
        Message response;
        INIT_MESSAGE(response);
        response.operation = OP_ACK;
        response.error_code = ERR_SUCCESS;
        response.timestamp = time(NULL);
        
        // Create appropriate response based on operation
        switch (msg.operation) {
            case OP_CREATE:
                snprintf(response.content, sizeof(response.content), 
                         "Echo: Received CREATE request from %s for file '%s'", 
                         msg.sender_id, msg.filename);
                break;
                
            case OP_READ:
                snprintf(response.content, sizeof(response.content), 
                         "Echo: Received READ request from %s for file '%s'", 
                         msg.sender_id, msg.filename);
                break;
                
            case OP_WRITE:
                snprintf(response.content, sizeof(response.content), 
                         "Echo: Received WRITE request from %s for file '%s' sentence %d word %d", 
                         msg.sender_id, msg.filename, msg.sentence_num, msg.word_index);
                break;
                
            case OP_DELETE:
                snprintf(response.content, sizeof(response.content), 
                         "Echo: Received DELETE request from %s for file '%s'", 
                         msg.sender_id, msg.filename);
                break;
                
            case OP_VIEW:
                snprintf(response.content, sizeof(response.content), 
                         "Echo: Received VIEW request from %s with flags %d", 
                         msg.sender_id, msg.view_flags);
                break;
                
            case OP_ADDACCESS:
                snprintf(response.content, sizeof(response.content), 
                         "Echo: Received ADDACCESS request from %s for file '%s' to user '%s'", 
                         msg.sender_id, msg.filename, msg.target_user);
                break;
                
            case OP_REMACCESS:
                snprintf(response.content, sizeof(response.content), 
                         "Echo: Received REMACCESS request from %s for file '%s' from user '%s'", 
                         msg.sender_id, msg.filename, msg.target_user);
                break;
                
            case OP_STOP:
                snprintf(response.content, sizeof(response.content), 
                         "Echo: Received STOP command from %s", 
                         msg.sender_id);
                break;
                
            default:
                snprintf(response.content, sizeof(response.content), 
                         "Echo: Received unknown operation %d from %s", 
                         msg.operation, msg.sender_id);
                break;
        }
        
        // Send response
        log_info("Sending echo response...");
        if (send_message(client_socket, &response) < 0) {
            log_error("Failed to send response");
            break;
        }
        log_info("Sent echo response: %s", response.content);
        
        // If client sends STOP command, break
        if (msg.operation == OP_STOP) {
            log_info("Client sent STOP command, closing connection");
            break;
        }
    }
    
    // Summary
    log_info("\n=== Connection Summary ===");
    log_info("Total messages received: %d", message_count);
    log_info("Client IP: %s", client_ip);
    log_info("Client Port: %d", client_port);
    
    // Cleanup
    close_socket(client_socket);
    log_info("Client socket closed");
    
    close_socket(server_socket);
    log_info("Server socket closed");
    
    log_info("Server shutting down");
    log_info("Check logs/test_server.log for full details");
    close_logger();
    
    printf("\n✓ Test server completed successfully!\n");
    printf("Check logs/test_server.log for detailed logs\n\n");
    
    return 0;
}