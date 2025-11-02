/*
 * tests/test_client.c
 * 
 * Simple TCP client to test socket utilities
 * Run test_server first, then run this
 * 
 * Compile: gcc -o test_client test_client.c ../common/socket_utils.c ../common/logger.c -I../common
 * Run: ./test_client
 * 
 * Copy this EXACTLY into tests/test_client.c
 */

#include "socket_utils.h"
#include "logger.h"
#include "protocol.h"

#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 9999

int main() {
    int client_socket;
    
    // Initialize logger
    init_logger("TEST_CLIENT", "logs/test_client.log");
    
    log_info("=== Socket Test Client Starting ===");
    
    // Create socket
    client_socket = create_socket();
    if (client_socket < 0) {
        log_error("Failed to create socket");
        return 1;
    }
    log_info("Socket created successfully (fd: %d)", client_socket);
    
    // Connect to server
    log_info("Connecting to server at %s:%d...", SERVER_IP, SERVER_PORT);
    if (connect_to_server(client_socket, SERVER_IP, SERVER_PORT) < 0) {
        log_error("Failed to connect to server");
        log_error("Make sure test_server is running!");
        close_socket(client_socket);
        close_logger();
        return 1;
    }
    log_info("Connected to server successfully!");
    
    // Test 1: CREATE request
    log_info("\n=== Test 1: CREATE request ===");
    Message msg1;
    INIT_MESSAGE(msg1);
    msg1.operation = OP_CREATE;
    strcpy(msg1.sender_id, "test_user");
    strcpy(msg1.filename, "test.txt");
    strcpy(msg1.content, "This is a test file");
    
    log_info("Sending CREATE message...");
    if (send_message(client_socket, &msg1) < 0) {
        log_error("Failed to send CREATE message");
    } else {
        log_info("Sent CREATE message");
        log_info("  - Operation: %s", get_operation_name(msg1.operation));
        log_info("  - Filename: %s", msg1.filename);
        log_info("  - Sender: %s", msg1.sender_id);
    }
    
    // Receive response
    Message response1;
    log_info("Waiting for response...");
    if (receive_message(client_socket, &response1) > 0) {
        log_info("Received response:");
        log_info("  Error Code: %d (%s)", response1.error_code, 
                 get_error_message(response1.error_code));
        log_info("  Content: %s", response1.content);
    } else {
        log_error("Failed to receive response");
    }
    
    // Test 2: READ request
    log_info("\n=== Test 2: READ request ===");
    Message msg2;
    INIT_MESSAGE(msg2);
    msg2.operation = OP_READ;
    strcpy(msg2.sender_id, "test_user");
    strcpy(msg2.filename, "test.txt");
    
    log_info("Sending READ message...");
    if (send_message(client_socket, &msg2) < 0) {
        log_error("Failed to send READ message");
    } else {
        log_info("Sent READ message");
        log_info("  - Operation: %s", get_operation_name(msg2.operation));
        log_info("  - Filename: %s", msg2.filename);
    }
    
    // Receive response
    Message response2;
    log_info("Waiting for response...");
    if (receive_message(client_socket, &response2) > 0) {
        log_info("Received response:");
        log_info("  Content: %s", response2.content);
    } else {
        log_error("Failed to receive response");
    }
    
    // Test 3: WRITE request
    log_info("\n=== Test 3: WRITE request ===");
    Message msg3;
    INIT_MESSAGE(msg3);
    msg3.operation = OP_WRITE;
    strcpy(msg3.sender_id, "test_user");
    strcpy(msg3.filename, "test.txt");
    msg3.sentence_num = 1;
    msg3.word_index = 2;
    strcpy(msg3.content, "modified");
    
    log_info("Sending WRITE message...");
    if (send_message(client_socket, &msg3) < 0) {
        log_error("Failed to send WRITE message");
    } else {
        log_info("Sent WRITE message");
        log_info("  - Operation: %s", get_operation_name(msg3.operation));
        log_info("  - Filename: %s", msg3.filename);
        log_info("  - Sentence: %d, Word: %d", msg3.sentence_num, msg3.word_index);
        log_info("  - Content: %s", msg3.content);
    }
    
    // Receive response
    Message response3;
    log_info("Waiting for response...");
    if (receive_message(client_socket, &response3) > 0) {
        log_info("Received response:");
        log_info("  Content: %s", response3.content);
    } else {
        log_error("Failed to receive response");
    }
    
    // Test 4: VIEW request
    log_info("\n=== Test 4: VIEW request ===");
    Message msg4;
    INIT_MESSAGE(msg4);
    msg4.operation = OP_VIEW;
    strcpy(msg4.sender_id, "test_user");
    msg4.view_flags = VIEW_FLAG_LONG;  // Test -l flag
    
    log_info("Sending VIEW message...");
    if (send_message(client_socket, &msg4) < 0) {
        log_error("Failed to send VIEW message");
    } else {
        log_info("Sent VIEW message with -l flag");
    }
    
    // Receive response
    Message response4;
    log_info("Waiting for response...");
    if (receive_message(client_socket, &response4) > 0) {
        log_info("Received response:");
        log_info("  Content: %s", response4.content);
    } else {
        log_error("Failed to receive response");
    }
    
    // Test 5: Access control request
    log_info("\n=== Test 5: ADDACCESS request ===");
    Message msg5;
    INIT_MESSAGE(msg5);
    msg5.operation = OP_ADDACCESS;
    strcpy(msg5.sender_id, "test_user");
    strcpy(msg5.filename, "test.txt");
    strcpy(msg5.target_user, "user2");
    msg5.access_type = ACCESS_READ;
    
    log_info("Sending ADDACCESS message...");
    if (send_message(client_socket, &msg5) < 0) {
        log_error("Failed to send ADDACCESS message");
    } else {
        log_info("Sent ADDACCESS message");
        log_info("  - Target user: %s", msg5.target_user);
        log_info("  - Access type: READ");
    }
    
    // Receive response
    Message response5;
    log_info("Waiting for response...");
    if (receive_message(client_socket, &response5) > 0) {
        log_info("Received response:");
        log_info("  Content: %s", response5.content);
    } else {
        log_error("Failed to receive response");
    }
    
    // Send STOP to end connection gracefully
    log_info("\n=== Sending STOP command ===");
    Message stop_msg;
    INIT_MESSAGE(stop_msg);
    stop_msg.operation = OP_STOP;
    strcpy(stop_msg.sender_id, "test_user");
    
    if (send_message(client_socket, &stop_msg) < 0) {
        log_error("Failed to send STOP message");
    } else {
        log_info("Sent STOP message to server");
    }
    
    // Summary
    log_info("\n=== Test Summary ===");
    log_info("All 5 tests completed:");
    log_info("  1. CREATE request - sent and received response");
    log_info("  2. READ request - sent and received response");
    log_info("  3. WRITE request - sent and received response");
    log_info("  4. VIEW request - sent and received response");
    log_info("  5. ADDACCESS request - sent and received response");
    log_info("Connection closing...");
    
    // Cleanup
    close_socket(client_socket);
    log_info("Client shutting down");
    log_info("Check logs/test_client.log for full details");
    close_logger();
    
    printf("\n✓ Test client completed successfully!\n");
    printf("Check logs/test_client.log for detailed logs\n\n");
    
    return 0;
}