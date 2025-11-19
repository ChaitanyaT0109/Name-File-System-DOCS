/*
 * client/client.c
 * 
 * Client - Interactive terminal interface for Docs++ system
 * Connects to Name Server and performs file operations
 * 
 * Implemented Features:
 * - Days 3-5: CREATE, READ, DELETE operations
 * - Days 6-7: VIEW, INFO, LIST, ADDACCESS, REMACCESS commands
 * 
 * Compile: gcc -o client client.c ../common/socket_utils.c ../common/logger.c -I../common
 * Run: ./client
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <signal.h>
#include "socket_utils.h"
#include "logger.h"
#include "protocol.h"

#define NS_IP "192.168.1.187"  // Friend's IP (where Name Server is)
#define NS_PORT 8080
#define MAX_INPUT 1024

// Global state
char current_username[MAX_USERNAME_LEN];
char local_ip[MAX_IP_LEN];
volatile sig_atomic_t should_exit = 0;  // Flag for graceful shutdown

/* ============================================================================
 * UTILITY FUNCTIONS
 * ============================================================================ */

// Trim whitespace from string
void trim(char* str) {
    char* start = str;
    char* end;
    
    // Trim leading space
    while (isspace((unsigned char)*start)) start++;
    
    // All spaces?
    if (*start == 0) {
        *str = 0;
        return;
    }
    
    // Trim trailing space
    end = start + strlen(start) - 1;
    while (end > start && isspace((unsigned char)*end)) end--;
    
    // Write new null terminator
    end[1] = '\0';
    
    // Move trimmed string to start
    memmove(str, start, end - start + 2);
}

// Parse command line input (now supports up to 3 arguments for ADDACCESS)
int parse_input(char* input, char* command, char* arg1, char* arg2, char* arg3) {
    command[0] = '\0';
    arg1[0] = '\0';
    arg2[0] = '\0';
    arg3[0] = '\0';
    
    trim(input);
    
    // Get command
    char* token = strtok(input, " ");
    if (token == NULL) return 0;
    
    strncpy(command, token, MAX_FILENAME_LEN - 1);
    command[MAX_FILENAME_LEN - 1] = '\0';
    
    // Convert to uppercase
    for (int i = 0; command[i]; i++) {
        command[i] = toupper(command[i]);
    }
    
    // Get first argument
    token = strtok(NULL, " ");
    if (token != NULL) {
        strncpy(arg1, token, MAX_FILENAME_LEN - 1);
        arg1[MAX_FILENAME_LEN - 1] = '\0';
    }
    
    // Get second argument
    token = strtok(NULL, " ");
    if (token != NULL) {
        strncpy(arg2, token, MAX_FILENAME_LEN - 1);
        arg2[MAX_FILENAME_LEN - 1] = '\0';
    }
    
    // Get third argument (for ADDACCESS)
    token = strtok(NULL, " ");
    if (token != NULL) {
        strncpy(arg3, token, MAX_FILENAME_LEN - 1);
        arg3[MAX_FILENAME_LEN - 1] = '\0';
    }
    
    return 1;
}

/* ============================================================================
 * REGISTRATION
 * ============================================================================ */

int register_with_nameserver() {
    log_info("Registering with Name Server at %s:%d", NS_IP, NS_PORT);
    
    // Create socket
    int ns_socket = create_socket();
    if (ns_socket < 0) {
        printf("Error: Failed to create socket\n");
        return -1;
    }
    
    // Connect to NS
    if (connect_to_server(ns_socket, NS_IP, NS_PORT) < 0) {
        printf("Error: Failed to connect to Name Server at %s:%d\n", NS_IP, NS_PORT);
        printf("Make sure Name Server is running!\n");
        close_socket(ns_socket);
        return -1;
    }
    
    log_info("Connected to Name Server");
    
    // Send registration
    Message reg_msg;
    INIT_MESSAGE(reg_msg);
    reg_msg.operation = OP_REGISTER_CLIENT;
    strncpy(reg_msg.sender_id, current_username, MAX_USERNAME_LEN - 1);
    strncpy(reg_msg.ip_address, local_ip, MAX_IP_LEN - 1);
    reg_msg.nm_port = 0;  // Client doesn't listen
    
    if (send_message(ns_socket, &reg_msg) < 0) {
        printf("Error: Failed to send registration\n");
        close_socket(ns_socket);
        return -1;
    }
    
    // Wait for ACK
    Message ack;
    if (receive_message(ns_socket, &ack) <= 0) {
        printf("Error: Failed to receive registration response\n");
        close_socket(ns_socket);
        return -1;
    }
    
    if (ack.error_code != ERR_SUCCESS) {
        printf("Error: Registration failed: %s\n", ack.content);
        close_socket(ns_socket);
        return -1;
    }
    
    printf("%s\n", ack.content);
    log_info("Registration successful");
    
    close_socket(ns_socket);
    return 0;
}

void unregister_from_nameserver() {
    log_info("Unregistering from Name Server");
    
    // Create socket
    int ns_socket = create_socket();
    if (ns_socket < 0) {
        return;  // Silently fail during cleanup
    }
    
    // Connect to NS
    if (connect_to_server(ns_socket, NS_IP, NS_PORT) < 0) {
        close_socket(ns_socket);
        return;  // Silently fail during cleanup
    }
    
    // Send unregister message
    Message unreg_msg;
    INIT_MESSAGE(unreg_msg);
    unreg_msg.operation = OP_UNREGISTER_CLIENT;
    strncpy(unreg_msg.sender_id, current_username, MAX_USERNAME_LEN - 1);
    strncpy(unreg_msg.ip_address, local_ip, MAX_IP_LEN - 1);
    
    send_message(ns_socket, &unreg_msg);
    close_socket(ns_socket);
    
    log_info("Unregistered from Name Server");
}

void signal_handler(int signum) {
    if (signum == SIGINT) {
        should_exit = 1;
        printf("\n\nReceived Ctrl+C. Disconnecting gracefully...\n");
        unregister_from_nameserver();
        close_logger();
        exit(0);
    }
}

/* ============================================================================
 * COMMAND HANDLERS
 * ============================================================================ */

void cmd_create(const char* filename) {
    if (strlen(filename) == 0) {
        printf("Usage: CREATE <filename>\n");
        return;
    }
    
    log_info("CREATE command: %s", filename);
    
    // Connect to NS
    int ns_socket = create_socket();
    if (ns_socket < 0 || connect_to_server(ns_socket, NS_IP, NS_PORT) < 0) {
        printf("Error: Cannot connect to Name Server\n");
        if (ns_socket >= 0) close_socket(ns_socket);
        return;
    }
    
    // Send CREATE request
    Message msg;
    INIT_MESSAGE(msg);
    msg.operation = OP_CREATE;
    strncpy(msg.sender_id, current_username, MAX_USERNAME_LEN - 1);
    strncpy(msg.filename, filename, MAX_FILENAME_LEN - 1);
    
    if (send_message(ns_socket, &msg) < 0) {
        printf("Error: Failed to send CREATE request\n");
        close_socket(ns_socket);
        return;
    }
    
    log_info("Sent CREATE request to NS");
    
    // Wait for response
    Message response;
    if (receive_message(ns_socket, &response) <= 0) {
        printf("Error: Failed to receive response\n");
        close_socket(ns_socket);
        return;
    }
    
    // Display result
    if (response.error_code == ERR_SUCCESS || response.error_code == ERR_CREATED) {
        printf("✓ File '%s' created successfully!\n", filename);
        log_info("CREATE successful");
    } else {
        printf("✗ Error: %s\n", response.content);
        log_error("CREATE failed: %s", get_error_message(response.error_code));
    }
    
    close_socket(ns_socket);
}

void cmd_read(const char* filename) {
    if (strlen(filename) == 0) {
        printf("Usage: READ <filename>\n");
        return;
    }
    
    log_info("READ command: %s", filename);
    
    // Connect to NS
    int ns_socket = create_socket();
    if (ns_socket < 0 || connect_to_server(ns_socket, NS_IP, NS_PORT) < 0) {
        printf("Error: Cannot connect to Name Server\n");
        if (ns_socket >= 0) close_socket(ns_socket);
        return;
    }
    
    // Send READ request
    Message msg;
    INIT_MESSAGE(msg);
    msg.operation = OP_READ;
    strncpy(msg.sender_id, current_username, MAX_USERNAME_LEN - 1);
    strncpy(msg.filename, filename, MAX_FILENAME_LEN - 1);
    
    if (send_message(ns_socket, &msg) < 0) {
        printf("Error: Failed to send READ request\n");
        close_socket(ns_socket);
        return;
    }
    
    log_info("Sent READ request to NS");
    
    // Wait for routing info from NS
    Message route_response;
    if (receive_message(ns_socket, &route_response) <= 0) {
        printf("Error: Failed to receive routing info\n");
        close_socket(ns_socket);
        return;
    }
    
    close_socket(ns_socket);
    
    // Check if NS returned error
    if (route_response.operation != OP_ROUTE_INFO) {
        printf("✗ Error: %s\n", route_response.content);
        log_error("READ failed: %s", get_error_message(route_response.error_code));
        return;
    }
    
    // Extract SS info
    char ss_ip[MAX_IP_LEN];
    int ss_port;
    strncpy(ss_ip, route_response.ss_ip, MAX_IP_LEN - 1);
    ss_ip[MAX_IP_LEN - 1] = '\0';
    ss_port = route_response.ss_port;
    
    log_info("Connecting to Storage Server at %s:%d", ss_ip, ss_port);
    
    // Connect directly to SS
    int ss_socket = create_socket();
    if (ss_socket < 0 || connect_to_server(ss_socket, ss_ip, ss_port) < 0) {
        printf("Error: Cannot connect to Storage Server\n");
        if (ss_socket >= 0) close_socket(ss_socket);
        return;
    }
    
    log_info("Connected to Storage Server");
    
    // Send READ request to SS
    Message ss_msg;
    INIT_MESSAGE(ss_msg);
    ss_msg.operation = OP_READ;
    strncpy(ss_msg.sender_id, current_username, MAX_USERNAME_LEN - 1);
    strncpy(ss_msg.filename, filename, MAX_FILENAME_LEN - 1);
    
    if (send_message(ss_socket, &ss_msg) < 0) {
        printf("Error: Failed to send READ request to Storage Server\n");
        close_socket(ss_socket);
        return;
    }
    
    // Receive file content from SS
    Message ss_response;
    if (receive_message(ss_socket, &ss_response) <= 0) {
        printf("Error: Failed to receive file content\n");
        close_socket(ss_socket);
        return;
    }
    
    // Check for errors
    if (ss_response.error_code != ERR_SUCCESS) {
        printf("✗ Error: %s\n", ss_response.content);
        log_error("READ failed: %s", get_error_message(ss_response.error_code));
        close_socket(ss_socket);
        return;
    }
    
    // Display file content
    printf("\n");
    printf("─────────────────────────────────────────────────────────────\n");
    printf("File: %s\n", filename);
    printf("─────────────────────────────────────────────────────────────\n");
    
    if (strlen(ss_response.content) == 0) {
        printf("(empty file)\n");
    } else {
        printf("%s\n", ss_response.content);
    }
    
    printf("─────────────────────────────────────────────────────────────\n");
    printf("\n");
    
    log_info("READ successful");
    
    // Wait for STOP packet
    Message stop;
    receive_message(ss_socket, &stop);
    
    close_socket(ss_socket);
}

void cmd_delete(const char* filename) {
    if (strlen(filename) == 0) {
        printf("Usage: DELETE <filename>\n");
        return;
    }
    
    log_info("DELETE command: %s", filename);
    
    // Connect to NS
    int ns_socket = create_socket();
    if (ns_socket < 0 || connect_to_server(ns_socket, NS_IP, NS_PORT) < 0) {
        printf("Error: Cannot connect to Name Server\n");
        if (ns_socket >= 0) close_socket(ns_socket);
        return;
    }
    
    // Send DELETE request
    Message msg;
    INIT_MESSAGE(msg);
    msg.operation = OP_DELETE;
    strncpy(msg.sender_id, current_username, MAX_USERNAME_LEN - 1);
    strncpy(msg.filename, filename, MAX_FILENAME_LEN - 1);
    
    if (send_message(ns_socket, &msg) < 0) {
        printf("Error: Failed to send DELETE request\n");
        close_socket(ns_socket);
        return;
    }
    
    log_info("Sent DELETE request to NS");
    
    // Wait for response
    Message response;
    if (receive_message(ns_socket, &response) <= 0) {
        printf("Error: Failed to receive response\n");
        close_socket(ns_socket);
        return;
    }
    
    // Display result
    if (response.error_code == ERR_SUCCESS) {
        printf("✓ File '%s' deleted successfully!\n", filename);
        log_info("DELETE successful");
    } else {
        printf("✗ Error: %s\n", response.content);
        log_error("DELETE failed: %s", get_error_message(response.error_code));
    }
    
    close_socket(ns_socket);
}

void cmd_list() {
    log_info("LIST command");
    
    // Connect to NS
    int ns_socket = create_socket();
    if (ns_socket < 0 || connect_to_server(ns_socket, NS_IP, NS_PORT) < 0) {
        printf("Error: Cannot connect to Name Server\n");
        if (ns_socket >= 0) close_socket(ns_socket);
        return;
    }
    
    // Send LIST request
    Message msg;
    INIT_MESSAGE(msg);
    msg.operation = OP_LIST;
    strncpy(msg.sender_id, current_username, MAX_USERNAME_LEN - 1);
    
    if (send_message(ns_socket, &msg) < 0) {
        printf("Error: Failed to send LIST request\n");
        close_socket(ns_socket);
        return;
    }
    
    // Wait for response
    Message response;
    if (receive_message(ns_socket, &response) <= 0) {
        printf("Error: Failed to receive response\n");
        close_socket(ns_socket);
        return;
    }
    
    // Display result
    if (response.error_code == ERR_SUCCESS) {
        printf("\n%s\n", response.content);
        log_info("LIST successful");
    } else {
        printf("✗ Error: %s\n", response.content);
        log_error("LIST failed: %s", get_error_message(response.error_code));
    }
    
    close_socket(ns_socket);
}

void cmd_view(const char* flags) {
    log_info("VIEW command with flags: %s", flags);
    
    // Connect to NS
    int ns_socket = create_socket();
    if (ns_socket < 0 || connect_to_server(ns_socket, NS_IP, NS_PORT) < 0) {
        printf("Error: Cannot connect to Name Server\n");
        if (ns_socket >= 0) close_socket(ns_socket);
        return;
    }
    
    // Send VIEW request
    Message msg;
    INIT_MESSAGE(msg);
    msg.operation = OP_VIEW;
    strncpy(msg.sender_id, current_username, MAX_USERNAME_LEN - 1);
    
    // Parse flags
    msg.view_flags = VIEW_FLAG_NONE;
    if (strlen(flags) > 0) {
        if (strcmp(flags, "-a") == 0) {
            msg.view_flags = VIEW_FLAG_ALL;
        } else if (strcmp(flags, "-l") == 0) {
            msg.view_flags = VIEW_FLAG_LONG;
        } else if (strcmp(flags, "-al") == 0 || strcmp(flags, "-la") == 0) {
            msg.view_flags = VIEW_FLAG_ALL_LONG;
        } else {
            // Invalid flag provided
            printf("Error: Invalid flag '%s'. Valid flags: -a, -l, -al, -la\n", flags);
            close_socket(ns_socket);
            return;
        }
    }
    
    if (send_message(ns_socket, &msg) < 0) {
        printf("Error: Failed to send VIEW request\n");
        close_socket(ns_socket);
        return;
    }
    
    // Wait for response
    Message response;
    if (receive_message(ns_socket, &response) <= 0) {
        printf("Error: Failed to receive response\n");
        close_socket(ns_socket);
        return;
    }
    
    // Display result
    if (response.error_code == ERR_SUCCESS) {
        printf("\n%s\n", response.content);
        log_info("VIEW successful");
    } else {
        printf("✗ Error: %s\n", response.content);
        log_error("VIEW failed: %s", get_error_message(response.error_code));
    }
    
    close_socket(ns_socket);
}

void cmd_info(const char* filename) {
    if (strlen(filename) == 0) {
        printf("Usage: INFO <filename>\n");
        return;
    }
    
    log_info("INFO command: %s", filename);
    
    // Connect to NS
    int ns_socket = create_socket();
    if (ns_socket < 0 || connect_to_server(ns_socket, NS_IP, NS_PORT) < 0) {
        printf("Error: Cannot connect to Name Server\n");
        if (ns_socket >= 0) close_socket(ns_socket);
        return;
    }
    
    // Send INFO request
    Message msg;
    INIT_MESSAGE(msg);
    msg.operation = OP_INFO;
    strncpy(msg.sender_id, current_username, MAX_USERNAME_LEN - 1);
    strncpy(msg.filename, filename, MAX_FILENAME_LEN - 1);
    
    if (send_message(ns_socket, &msg) < 0) {
        printf("Error: Failed to send INFO request\n");
        close_socket(ns_socket);
        return;
    }
    
    // Wait for response
    Message response;
    if (receive_message(ns_socket, &response) <= 0) {
        printf("Error: Failed to receive response\n");
        close_socket(ns_socket);
        return;
    }
    
    // Display result
    if (response.error_code == ERR_SUCCESS) {
        printf("\n%s\n", response.content);
        log_info("INFO successful");
    } else {
        printf("✗ Error: %s\n", response.content);
        log_error("INFO failed: %s", get_error_message(response.error_code));
    }
    
    close_socket(ns_socket);
}

void cmd_addaccess(const char* flag, const char* filename, const char* username) {
    if (strlen(flag) == 0 || strlen(filename) == 0 || strlen(username) == 0) {
        printf("Usage: ADDACCESS -R|-W <filename> <username>\n");
        printf("  -R : Grant read access\n");
        printf("  -W : Grant write access\n");
        return;
    }
    
    // Parse access type
    int access_type;
    if (strcmp(flag, "-R") == 0 || strcmp(flag, "-r") == 0) {
        access_type = ACCESS_READ;
    } else if (strcmp(flag, "-W") == 0 || strcmp(flag, "-w") == 0) {
        access_type = ACCESS_WRITE;
    } else {
        printf("Error: Invalid flag '%s'. Use -R for read or -W for write\n", flag);
        return;
    }
    
    log_info("ADDACCESS command: %s %s to %s", flag, filename, username);
    
    // Connect to NS
    int ns_socket = create_socket();
    if (ns_socket < 0 || connect_to_server(ns_socket, NS_IP, NS_PORT) < 0) {
        printf("Error: Cannot connect to Name Server\n");
        if (ns_socket >= 0) close_socket(ns_socket);
        return;
    }
    
    // Send ADDACCESS request
    Message msg;
    INIT_MESSAGE(msg);
    msg.operation = OP_ADDACCESS;
    strncpy(msg.sender_id, current_username, MAX_USERNAME_LEN - 1);
    strncpy(msg.filename, filename, MAX_FILENAME_LEN - 1);
    strncpy(msg.target_user, username, MAX_USERNAME_LEN - 1);
    msg.access_type = access_type;
    
    if (send_message(ns_socket, &msg) < 0) {
        printf("Error: Failed to send ADDACCESS request\n");
        close_socket(ns_socket);
        return;
    }
    
    // Wait for response
    Message response;
    if (receive_message(ns_socket, &response) <= 0) {
        printf("Error: Failed to receive response\n");
        close_socket(ns_socket);
        return;
    }
    
    // Display result
    if (response.error_code == ERR_SUCCESS) {
        printf("✓ %s\n", response.content);
        log_info("ADDACCESS successful");
    } else {
        printf("✗ Error: %s\n", response.content);
        log_error("ADDACCESS failed: %s", get_error_message(response.error_code));
    }
    
    close_socket(ns_socket);
}

void cmd_remaccess(const char* filename, const char* username) {
    if (strlen(filename) == 0 || strlen(username) == 0) {
        printf("Usage: REMACCESS <filename> <username>\n");
        return;
    }
    
    log_info("REMACCESS command: %s for %s", filename, username);
    
    // Connect to NS
    int ns_socket = create_socket();
    if (ns_socket < 0 || connect_to_server(ns_socket, NS_IP, NS_PORT) < 0) {
        printf("Error: Cannot connect to Name Server\n");
        if (ns_socket >= 0) close_socket(ns_socket);
        return;
    }
    
    // Send REMACCESS request
    Message msg;
    INIT_MESSAGE(msg);
    msg.operation = OP_REMACCESS;
    strncpy(msg.sender_id, current_username, MAX_USERNAME_LEN - 1);
    strncpy(msg.filename, filename, MAX_FILENAME_LEN - 1);
    strncpy(msg.target_user, username, MAX_USERNAME_LEN - 1);
    
    if (send_message(ns_socket, &msg) < 0) {
        printf("Error: Failed to send REMACCESS request\n");
        close_socket(ns_socket);
        return;
    }
    
    // Wait for response
    Message response;
    if (receive_message(ns_socket, &response) <= 0) {
        printf("Error: Failed to receive response\n");
        close_socket(ns_socket);
        return;
    }
    
    // Display result
    if (response.error_code == ERR_SUCCESS) {
        printf("✓ %s\n", response.content);
        log_info("REMACCESS successful");
    } else {
        printf("✗ Error: %s\n", response.content);
        log_error("REMACCESS failed: %s", get_error_message(response.error_code));
    }
    
    close_socket(ns_socket);
}

void cmd_write(const char* filename, const char* sentence_num_str) {
    if (!filename || strlen(filename) == 0) {
        printf("Error: Filename required\n");
        printf("Usage: WRITE <filename> <sentence_num>\n");
        return;
    }
    
    if (!sentence_num_str || strlen(sentence_num_str) == 0) {
        printf("Error: Sentence number required\n");
        printf("Usage: WRITE <filename> <sentence_num>\n");
        return;
    }
    
    int sentence_num = atoi(sentence_num_str);
    if (sentence_num < 0) {
        printf("Error: Invalid sentence number\n");
        return;
    }
    
    log_info("WRITE command: file=%s, sentence=%d", filename, sentence_num);
    
    // Step 1: Contact NS to get SS routing info
    int ns_socket = create_socket();
    if (ns_socket < 0 || connect_to_server(ns_socket, NS_IP, NS_PORT) < 0) {
        printf("Error: Failed to connect to Name Server\n");
        if (ns_socket >= 0) close_socket(ns_socket);
        return;
    }
    
    Message msg;
    INIT_MESSAGE(msg);
    msg.operation = OP_WRITE;
    strncpy(msg.sender_id, current_username, MAX_USERNAME_LEN - 1);
    strncpy(msg.filename, filename, MAX_FILENAME_LEN - 1);
    msg.sentence_num = sentence_num;
    
    send_message(ns_socket, &msg);
    
    Message response;
    if (receive_message(ns_socket, &response) <= 0) {
        printf("Error: No response from Name Server\n");
        close_socket(ns_socket);
        return;
    }
    
    close_socket(ns_socket);
    
    if (response.error_code != ERR_SUCCESS) {
        printf("Error: %s\n", response.content);
        return;
    }
    
    // Step 2: Connect to SS
    char ss_ip[MAX_IP_LEN];
    int ss_port = response.ss_port;
    strncpy(ss_ip, response.ss_ip, MAX_IP_LEN - 1);
    
    printf("Connecting to Storage Server at %s:%d...\n", ss_ip, ss_port);
    
    int ss_socket = create_socket();
    if (ss_socket < 0 || connect_to_server(ss_socket, ss_ip, ss_port) < 0) {
        printf("Error: Failed to connect to Storage Server\n");
        if (ss_socket >= 0) close_socket(ss_socket);
        return;
    }
    
    // Step 3: Start WRITE session (acquire lock)
    Message write_start;
    INIT_MESSAGE(write_start);
    write_start.operation = OP_WRITE_START;
    strncpy(write_start.sender_id, current_username, MAX_USERNAME_LEN - 1);
    strncpy(write_start.filename, filename, MAX_FILENAME_LEN - 1);
    write_start.sentence_num = sentence_num;
    
    send_message(ss_socket, &write_start);
    
    Message start_response;
    if (receive_message(ss_socket, &start_response) <= 0) {
        printf("Error: No response from Storage Server\n");
        close_socket(ss_socket);
        return;
    }
    
    if (start_response.error_code != ERR_SUCCESS) {
        printf("Error: %s\n", start_response.content);
        close_socket(ss_socket);
        return;
    }
    
    printf("Lock acquired! %s\n", start_response.content);
    printf("\nWRITE Session Active\n");
    printf("═══════════════════════════════════════════════════════════════\n");
    printf("Enter word updates in format: <word_index> <word_content>\n");
    printf("Type 'ETIRW' to commit changes, or 'CANCEL' to abort\n");
    printf("═══════════════════════════════════════════════════════════════\n");
    
    // Step 4: Interactive loop for word updates
    char input_line[MAX_INPUT];
    int updates_made = 0;
    
    while (1) {
        printf("\nwrite> ");
        fflush(stdout);
        
        if (fgets(input_line, sizeof(input_line), stdin) == NULL) {
            break;
        }
        
        trim(input_line);
        
        if (strlen(input_line) == 0) {
            continue;
        }
        
        // Check for commit
        if (strcmp(input_line, "ETIRW") == 0 || strcmp(input_line, "etirw") == 0) {
            // Commit changes
            Message commit_msg;
            INIT_MESSAGE(commit_msg);
            commit_msg.operation = OP_WRITE_COMMIT;
            strncpy(commit_msg.sender_id, current_username, MAX_USERNAME_LEN - 1);
            strncpy(commit_msg.filename, filename, MAX_FILENAME_LEN - 1);
            commit_msg.sentence_num = sentence_num;
            
            send_message(ss_socket, &commit_msg);
            
            Message commit_response;
            if (receive_message(ss_socket, &commit_response) <= 0) {
                printf("Error: Failed to commit changes\n");
            } else if (commit_response.error_code == ERR_SUCCESS) {
                printf("✓ Changes committed successfully!\n");
                printf("  Total updates made: %d\n", updates_made);
            } else {
                printf("Error: %s\n", commit_response.content);
            }
            
            close_socket(ss_socket);
            return;
        }
        
        // Check for cancel
        if (strcmp(input_line, "CANCEL") == 0 || strcmp(input_line, "cancel") == 0) {
            printf("Write session cancelled (lock will timeout)\n");
            close_socket(ss_socket);
            return;
        }
        
        // Parse word update: <word_idx> <content>
        char* space_pos = strchr(input_line, ' ');
        if (!space_pos) {
            printf("Error: Invalid format. Use: <word_index> <word_content>\n");
            continue;
        }
        
        *space_pos = '\0';
        int word_idx = atoi(input_line);
        const char* word_content = space_pos + 1;
        
        if (word_idx < 0) {
            printf("Error: Invalid word index\n");
            continue;
        }
        
        if (strlen(word_content) == 0) {
            printf("Error: Word content cannot be empty\n");
            continue;
        }
        
        // Send update to SS
        Message update_msg;
        INIT_MESSAGE(update_msg);
        update_msg.operation = OP_WRITE_UPDATE;
        strncpy(update_msg.sender_id, current_username, MAX_USERNAME_LEN - 1);
        strncpy(update_msg.filename, filename, MAX_FILENAME_LEN - 1);
        update_msg.sentence_num = sentence_num;
        update_msg.word_index = word_idx;
        strncpy(update_msg.content, word_content, MAX_CONTENT_LEN - 1);
        
        send_message(ss_socket, &update_msg);
        
        Message update_response;
        if (receive_message(ss_socket, &update_response) <= 0) {
            printf("Error: No response from Storage Server\n");
            close_socket(ss_socket);
            return;
        }
        
        if (update_response.error_code == ERR_SUCCESS) {
            printf("✓ %s\n", update_response.content);
            updates_made++;
        } else {
            printf("✗ Error: %s\n", update_response.content);
        }
    }
    
    close_socket(ss_socket);
}

void cmd_undo(const char* filename) {
    if (!filename || strlen(filename) == 0) {
        printf("Error: Filename required\n");
        printf("Usage: UNDO <filename>\n");
        return;
    }
    
    log_info("UNDO command: file=%s", filename);
    
    // Step 1: Contact NS to get SS routing info
    int ns_socket = create_socket();
    if (ns_socket < 0 || connect_to_server(ns_socket, NS_IP, NS_PORT) < 0) {
        printf("Error: Failed to connect to Name Server\n");
        if (ns_socket >= 0) close_socket(ns_socket);
        return;
    }
    
    Message msg;
    INIT_MESSAGE(msg);
    msg.operation = OP_UNDO;
    strncpy(msg.sender_id, current_username, MAX_USERNAME_LEN - 1);
    strncpy(msg.filename, filename, MAX_FILENAME_LEN - 1);
    
    send_message(ns_socket, &msg);
    
    Message response;
    if (receive_message(ns_socket, &response) <= 0) {
        printf("Error: No response from Name Server\n");
        close_socket(ns_socket);
        return;
    }
    
    close_socket(ns_socket);
    
    if (response.error_code != ERR_SUCCESS) {
        printf("Error: %s\n", response.content);
        return;
    }
    
    // Step 2: Connect to SS
    char ss_ip[MAX_IP_LEN];
    int ss_port = response.ss_port;
    strncpy(ss_ip, response.ss_ip, MAX_IP_LEN - 1);
    
    int ss_socket = create_socket();
    if (ss_socket < 0 || connect_to_server(ss_socket, ss_ip, ss_port) < 0) {
        printf("Error: Failed to connect to Storage Server\n");
        if (ss_socket >= 0) close_socket(ss_socket);
        return;
    }
    
    // Step 3: Send UNDO request
    Message undo_msg;
    INIT_MESSAGE(undo_msg);
    undo_msg.operation = OP_UNDO;
    strncpy(undo_msg.sender_id, current_username, MAX_USERNAME_LEN - 1);
    strncpy(undo_msg.filename, filename, MAX_FILENAME_LEN - 1);
    
    send_message(ss_socket, &undo_msg);
    
    Message undo_response;
    if (receive_message(ss_socket, &undo_response) <= 0) {
        printf("Error: No response from Storage Server\n");
        close_socket(ss_socket);
        return;
    }
    
    if (undo_response.error_code == ERR_SUCCESS) {
        printf("✓ File restored: %s\n", undo_response.content);
    } else {
        printf("Error: %s\n", undo_response.content);
    }
    
    close_socket(ss_socket);
    log_info("UNDO completed");
}

/* ============================================================================
 * STREAM Command Implementation (Days 12-13)
 * ============================================================================ */
void cmd_stream(const char* filename) {
    if (!filename || strlen(filename) == 0) {
        printf("Error: Filename required\n");
        printf("Usage: STREAM <filename>\n");
        return;
    }
    
    log_info("STREAM command: file=%s", filename);
    
    // Step 1: Contact NS to get SS routing info
    int ns_socket = create_socket();
    if (ns_socket < 0 || connect_to_server(ns_socket, NS_IP, NS_PORT) < 0) {
        printf("Error: Failed to connect to Name Server\n");
        if (ns_socket >= 0) close_socket(ns_socket);
        return;
    }
    
    Message msg;
    INIT_MESSAGE(msg);
    msg.operation = OP_STREAM;
    strncpy(msg.sender_id, current_username, MAX_USERNAME_LEN - 1);
    strncpy(msg.filename, filename, MAX_FILENAME_LEN - 1);
    
    send_message(ns_socket, &msg);
    
    Message response;
    if (receive_message(ns_socket, &response) <= 0) {
        printf("Error: No response from Name Server\n");
        close_socket(ns_socket);
        return;
    }
    
    close_socket(ns_socket);
    
    // Check if NS returned error
    if (response.operation != OP_ROUTE_INFO) {
        printf("✗ Error: %s\n", response.content);
        log_error("STREAM failed: %s", get_error_message(response.error_code));
        return;
    }
    
    // Step 2: Connect to SS
    char ss_ip[MAX_IP_LEN];
    int ss_port = response.ss_port;
    strncpy(ss_ip, response.ss_ip, MAX_IP_LEN - 1);
    ss_ip[MAX_IP_LEN - 1] = '\0';
    
    log_info("Connecting to Storage Server at %s:%d for streaming", ss_ip, ss_port);
    
    int ss_socket = create_socket();
    if (ss_socket < 0 || connect_to_server(ss_socket, ss_ip, ss_port) < 0) {
        printf("Error: Failed to connect to Storage Server\n");
        if (ss_socket >= 0) close_socket(ss_socket);
        return;
    }
    
    // Step 3: Send STREAM request to SS
    Message stream_msg;
    INIT_MESSAGE(stream_msg);
    stream_msg.operation = OP_STREAM;
    strncpy(stream_msg.sender_id, current_username, MAX_USERNAME_LEN - 1);
    strncpy(stream_msg.filename, filename, MAX_FILENAME_LEN - 1);
    
    if (send_message(ss_socket, &stream_msg) < 0) {
        printf("Error: Failed to send STREAM request to Storage Server\n");
        close_socket(ss_socket);
        return;
    }
    
    // Step 4: Receive and display streaming words
    printf("\n");
    printf("─────────────────────────────────────────────────────────────\n");
    printf("Streaming: %s\n", filename);
    printf("─────────────────────────────────────────────────────────────\n");
    
    int word_count = 0;
    int words_per_line = 10;  // Display 10 words per line
    
    while (1) {
        Message word_msg;
        int bytes = receive_message(ss_socket, &word_msg);
        
        if (bytes <= 0) {
            printf("\nError: Connection lost during streaming\n");
            break;
        }
        
        // Check for stream end
        if (word_msg.operation == OP_STOP) {
            printf("\n");
            printf("─────────────────────────────────────────────────────────────\n");
            printf("✓ Stream complete: %d words received\n", word_count);
            break;
        }
        
        // Check for errors
        if (word_msg.error_code != ERR_SUCCESS) {
            printf("\nError: %s\n", word_msg.content);
            break;
        }
        
        // Display the word
        printf("%s ", word_msg.content);
        fflush(stdout);  // Ensure immediate display
        
        word_count++;
        
        // Add newline every 10 words for readability
        if (word_count % words_per_line == 0) {
            printf("\n");
        }
    }
    
    close_socket(ss_socket);
    log_info("STREAM completed: %d words", word_count);
}

/* ============================================================================
 * EXEC Command Implementation (Days 12-13)
 * ============================================================================ */
void cmd_exec(const char* filename) {
    if (!filename || strlen(filename) == 0) {
        printf("Error: Filename required\n");
        printf("Usage: EXEC <filename>\n");
        return;
    }
    
    log_info("EXEC command: file=%s", filename);
    
    // Contact NS to execute the file
    int ns_socket = create_socket();
    if (ns_socket < 0 || connect_to_server(ns_socket, NS_IP, NS_PORT) < 0) {
        printf("Error: Failed to connect to Name Server\n");
        if (ns_socket >= 0) close_socket(ns_socket);
        return;
    }
    
    Message msg;
    INIT_MESSAGE(msg);
    msg.operation = OP_EXEC;
    strncpy(msg.sender_id, current_username, MAX_USERNAME_LEN - 1);
    strncpy(msg.filename, filename, MAX_FILENAME_LEN - 1);
    
    if (send_message(ns_socket, &msg) < 0) {
        printf("Error: Failed to send EXEC request\n");
        close_socket(ns_socket);
        return;
    }
    
    log_info("Sent EXEC request to NS");
    
    // Receive execution output
    Message response;
    if (receive_message(ns_socket, &response) <= 0) {
        printf("Error: No response from Name Server\n");
        close_socket(ns_socket);
        return;
    }
    
    close_socket(ns_socket);
    
    // Display results
    if (response.error_code == ERR_SUCCESS) {
        printf("\n");
        printf("─────────────────────────────────────────────────────────────\n");
        printf("Execution Output: %s\n", filename);
        printf("─────────────────────────────────────────────────────────────\n");
        printf("%s", response.content);
        if (strlen(response.content) > 0 && response.content[strlen(response.content)-1] != '\n') {
            printf("\n");
        }
        printf("─────────────────────────────────────────────────────────────\n");
        printf("✓ Command executed successfully\n");
    } else {
        printf("✗ Error: %s\n", response.content);
        log_error("EXEC failed: %s", get_error_message(response.error_code));
    }
    
    log_info("EXEC completed");
}

/* ============================================================================
 * BONUS FEATURES - Command Implementations
 * ============================================================================ */

// CREATEFOLDER - Create a new folder (Bonus: Hierarchical folders)
void cmd_createfolder(const char* foldername) {
    if (strlen(foldername) == 0) {
        printf("Error: Folder name cannot be empty\n");
        printf("Usage: CREATEFOLDER <foldername>\n");
        return;
    }
    
    log_info("CREATEFOLDER: '%s'", foldername);
    
    int ns_socket = create_socket();
    if (ns_socket < 0 || connect_to_server(ns_socket, NS_IP, NS_PORT) < 0) {
        printf("Error: Failed to connect to Name Server\n");
        if (ns_socket >= 0) close_socket(ns_socket);
        return;
    }
    
    Message msg;
    INIT_MESSAGE(msg);
    msg.operation = OP_CREATEFOLDER;
    strncpy(msg.sender_id, current_username, MAX_USERNAME_LEN - 1);
    strncpy(msg.folder_name, foldername, MAX_FILENAME_LEN - 1);
    
    send_message(ns_socket, &msg);
    
    Message response;
    if (receive_message(ns_socket, &response) <= 0) {
        printf("Error: Failed to receive response from server\n");
        close_socket(ns_socket);
        return;
    }
    
    close_socket(ns_socket);
    
    if (response.error_code == ERR_SUCCESS) {
        printf("Folder '%s' created successfully!\n", foldername);
        log_info("CREATEFOLDER successful: %s", foldername);
    } else {
        printf("Error: %s\n", response.content);
        log_error("CREATEFOLDER failed: %s", get_error_message(response.error_code));
    }
}

// MOVE - Move file to folder (Bonus: Hierarchical folders)
void cmd_move(const char* filename, const char* foldername) {
    if (strlen(filename) == 0 || strlen(foldername) == 0) {
        printf("Error: Filename and folder name cannot be empty\n");
        printf("Usage: MOVE <filename> <foldername>\n");
        return;
    }
    
    log_info("MOVE: '%s' -> '%s'", filename, foldername);
    
    int ns_socket = create_socket();
    if (ns_socket < 0 || connect_to_server(ns_socket, NS_IP, NS_PORT) < 0) {
        printf("Error: Failed to connect to Name Server\n");
        if (ns_socket >= 0) close_socket(ns_socket);
        return;
    }
    
    Message msg;
    INIT_MESSAGE(msg);
    msg.operation = OP_MOVE;
    strncpy(msg.sender_id, current_username, MAX_USERNAME_LEN - 1);
    strncpy(msg.filename, filename, MAX_FILENAME_LEN - 1);
    strncpy(msg.folder_name, foldername, MAX_FILENAME_LEN - 1);
    
    send_message(ns_socket, &msg);
    
    Message response;
    if (receive_message(ns_socket, &response) <= 0) {
        printf("Error: Failed to receive response from server\n");
        close_socket(ns_socket);
        return;
    }
    
    close_socket(ns_socket);
    
    if (response.error_code == ERR_SUCCESS) {
        printf("File '%s' moved to '%s' successfully!\n", filename, foldername);
        log_info("MOVE successful");
    } else {
        printf("Error: %s\n", response.content);
        log_error("MOVE failed: %s", get_error_message(response.error_code));
    }
}

// VIEWFOLDER - List folder contents (Bonus: Hierarchical folders)
void cmd_viewfolder(const char* foldername) {
    if (strlen(foldername) == 0) {
        printf("Error: Folder name cannot be empty\n");
        printf("Usage: VIEWFOLDER <foldername>\n");
        return;
    }
    
    log_info("VIEWFOLDER: '%s'", foldername);
    
    int ns_socket = create_socket();
    if (ns_socket < 0 || connect_to_server(ns_socket, NS_IP, NS_PORT) < 0) {
        printf("Error: Failed to connect to Name Server\n");
        if (ns_socket >= 0) close_socket(ns_socket);
        return;
    }
    
    Message msg;
    INIT_MESSAGE(msg);
    msg.operation = OP_VIEWFOLDER;
    strncpy(msg.sender_id, current_username, MAX_USERNAME_LEN - 1);
    strncpy(msg.folder_name, foldername, MAX_FILENAME_LEN - 1);
    
    send_message(ns_socket, &msg);
    
    Message response;
    if (receive_message(ns_socket, &response) <= 0) {
        printf("Error: Failed to receive response from server\n");
        close_socket(ns_socket);
        return;
    }
    
    close_socket(ns_socket);
    
    if (response.error_code == ERR_SUCCESS) {
        printf("Contents of folder '%s':\n", foldername);
        printf("%s\n", response.content);
        log_info("VIEWFOLDER successful");
    } else {
        printf("Error: %s\n", response.content);
        log_error("VIEWFOLDER failed: %s", get_error_message(response.error_code));
    }
}

// CHECKPOINT - Create a checkpoint (Bonus: Checkpoints)
void cmd_checkpoint(const char* filename, const char* tag) {
    if (strlen(filename) == 0 || strlen(tag) == 0) {
        printf("Error: Filename and tag cannot be empty\n");
        printf("Usage: CHECKPOINT <filename> <tag>\n");
        return;
    }
    
    log_info("CHECKPOINT: '%s' tag '%s'", filename, tag);
    
    // First get SS info from NS
    int ns_socket = create_socket();
    if (ns_socket < 0 || connect_to_server(ns_socket, NS_IP, NS_PORT) < 0) {
        printf("Error: Failed to connect to Name Server\n");
        if (ns_socket >= 0) close_socket(ns_socket);
        return;
    }
    
    Message msg;
    INIT_MESSAGE(msg);
    msg.operation = OP_READ;  // Use READ to get SS info
    strncpy(msg.sender_id, current_username, MAX_USERNAME_LEN - 1);
    strncpy(msg.filename, filename, MAX_FILENAME_LEN - 1);
    
    send_message(ns_socket, &msg);
    
    Message response;
    if (receive_message(ns_socket, &response) <= 0) {
        printf("Error: Failed to get file location\n");
        close_socket(ns_socket);
        return;
    }
    
    close_socket(ns_socket);
    
    if (response.error_code != ERR_SUCCESS) {
        printf("Error: %s\n", response.content);
        return;
    }
    
    // Connect to SS directly
    int ss_socket = create_socket();
    if (ss_socket < 0 || connect_to_server(ss_socket, response.ip_address, response.nm_port) < 0) {
        printf("Error: Failed to connect to Storage Server\n");
        if (ss_socket >= 0) close_socket(ss_socket);
        return;
    }
    
    // Send checkpoint request
    Message checkpoint_msg;
    INIT_MESSAGE(checkpoint_msg);
    checkpoint_msg.operation = OP_CHECKPOINT;
    strncpy(checkpoint_msg.sender_id, current_username, MAX_USERNAME_LEN - 1);
    strncpy(checkpoint_msg.filename, filename, MAX_FILENAME_LEN - 1);
    strncpy(checkpoint_msg.checkpoint_tag, tag, MAX_FILENAME_LEN - 1);
    
    send_message(ss_socket, &checkpoint_msg);
    
    if (receive_message(ss_socket, &response) <= 0) {
        printf("Error: Failed to receive response\n");
        close_socket(ss_socket);
        return;
    }
    
    close_socket(ss_socket);
    
    if (response.error_code == ERR_SUCCESS) {
        printf("Checkpoint '%s' created for file '%s'\n", tag, filename);
        log_info("CHECKPOINT successful");
    } else {
        printf("Error: %s\n", response.content);
        log_error("CHECKPOINT failed: %s", get_error_message(response.error_code));
    }
}

// VIEWCHECKPOINT - View checkpoint content (Bonus: Checkpoints)
void cmd_viewcheckpoint(const char* filename, const char* tag) {
    if (strlen(filename) == 0 || strlen(tag) == 0) {
        printf("Error: Filename and tag cannot be empty\n");
        printf("Usage: VIEWCHECKPOINT <filename> <tag>\n");
        return;
    }
    
    log_info("VIEWCHECKPOINT: '%s' tag '%s'", filename, tag);
    
    // Get SS info from NS
    int ns_socket = create_socket();
    if (ns_socket < 0 || connect_to_server(ns_socket, NS_IP, NS_PORT) < 0) {
        printf("Error: Failed to connect to Name Server\n");
        if (ns_socket >= 0) close_socket(ns_socket);
        return;
    }
    
    Message msg;
    INIT_MESSAGE(msg);
    msg.operation = OP_READ;
    strncpy(msg.sender_id, current_username, MAX_USERNAME_LEN - 1);
    strncpy(msg.filename, filename, MAX_FILENAME_LEN - 1);
    
    send_message(ns_socket, &msg);
    
    Message response;
    if (receive_message(ns_socket, &response) <= 0) {
        printf("Error: Failed to get file location\n");
        close_socket(ns_socket);
        return;
    }
    
    close_socket(ns_socket);
    
    if (response.error_code != ERR_SUCCESS) {
        printf("Error: %s\n", response.content);
        return;
    }
    
    // Connect to SS
    int ss_socket = create_socket();
    if (ss_socket < 0 || connect_to_server(ss_socket, response.ip_address, response.nm_port) < 0) {
        printf("Error: Failed to connect to Storage Server\n");
        if (ss_socket >= 0) close_socket(ss_socket);
        return;
    }
    
    Message view_msg;
    INIT_MESSAGE(view_msg);
    view_msg.operation = OP_VIEWCHECKPOINT;
    strncpy(view_msg.sender_id, current_username, MAX_USERNAME_LEN - 1);
    strncpy(view_msg.filename, filename, MAX_FILENAME_LEN - 1);
    strncpy(view_msg.checkpoint_tag, tag, MAX_FILENAME_LEN - 1);
    
    send_message(ss_socket, &view_msg);
    
    if (receive_message(ss_socket, &response) <= 0) {
        printf("Error: Failed to receive response\n");
        close_socket(ss_socket);
        return;
    }
    
    close_socket(ss_socket);
    
    if (response.error_code == ERR_SUCCESS) {
        printf("\n=== Checkpoint '%s' of '%s' ===\n", tag, filename);
        printf("%s\n", response.content);
        log_info("VIEWCHECKPOINT successful");
    } else {
        printf("Error: %s\n", response.content);
        log_error("VIEWCHECKPOINT failed: %s", get_error_message(response.error_code));
    }
}

// REVERT - Revert to checkpoint (Bonus: Checkpoints)
void cmd_revert(const char* filename, const char* tag) {
    if (strlen(filename) == 0 || strlen(tag) == 0) {
        printf("Error: Filename and tag cannot be empty\n");
        printf("Usage: REVERT <filename> <tag>\n");
        return;
    }
    
    log_info("REVERT: '%s' to tag '%s'", filename, tag);
    
    // Get SS info
    int ns_socket = create_socket();
    if (ns_socket < 0 || connect_to_server(ns_socket, NS_IP, NS_PORT) < 0) {
        printf("Error: Failed to connect to Name Server\n");
        if (ns_socket >= 0) close_socket(ns_socket);
        return;
    }
    
    Message msg;
    INIT_MESSAGE(msg);
    msg.operation = OP_READ;
    strncpy(msg.sender_id, current_username, MAX_USERNAME_LEN - 1);
    strncpy(msg.filename, filename, MAX_FILENAME_LEN - 1);
    
    send_message(ns_socket, &msg);
    
    Message response;
    if (receive_message(ns_socket, &response) <= 0) {
        printf("Error: Failed to get file location\n");
        close_socket(ns_socket);
        return;
    }
    
    close_socket(ns_socket);
    
    if (response.error_code != ERR_SUCCESS) {
        printf("Error: %s\n", response.content);
        return;
    }
    
    // Connect to SS
    int ss_socket = create_socket();
    if (ss_socket < 0 || connect_to_server(ss_socket, response.ip_address, response.nm_port) < 0) {
        printf("Error: Failed to connect to Storage Server\n");
        if (ss_socket >= 0) close_socket(ss_socket);
        return;
    }
    
    Message revert_msg;
    INIT_MESSAGE(revert_msg);
    revert_msg.operation = OP_REVERT;
    strncpy(revert_msg.sender_id, current_username, MAX_USERNAME_LEN - 1);
    strncpy(revert_msg.filename, filename, MAX_FILENAME_LEN - 1);
    strncpy(revert_msg.checkpoint_tag, tag, MAX_FILENAME_LEN - 1);
    
    send_message(ss_socket, &revert_msg);
    
    if (receive_message(ss_socket, &response) <= 0) {
        printf("Error: Failed to receive response\n");
        close_socket(ss_socket);
        return;
    }
    
    close_socket(ss_socket);
    
    if (response.error_code == ERR_SUCCESS) {
        printf("File '%s' reverted to checkpoint '%s'\n", filename, tag);
        log_info("REVERT successful");
    } else {
        printf("Error: %s\n", response.content);
        log_error("REVERT failed: %s", get_error_message(response.error_code));
    }
}

// LISTCHECKPOINTS - List all checkpoints (Bonus: Checkpoints)
void cmd_listcheckpoints(const char* filename) {
    if (strlen(filename) == 0) {
        printf("Error: Filename cannot be empty\n");
        printf("Usage: LISTCHECKPOINTS <filename>\n");
        return;
    }
    
    log_info("LISTCHECKPOINTS: '%s'", filename);
    
    // Get SS info
    int ns_socket = create_socket();
    if (ns_socket < 0 || connect_to_server(ns_socket, NS_IP, NS_PORT) < 0) {
        printf("Error: Failed to connect to Name Server\n");
        if (ns_socket >= 0) close_socket(ns_socket);
        return;
    }
    
    Message msg;
    INIT_MESSAGE(msg);
    msg.operation = OP_READ;
    strncpy(msg.sender_id, current_username, MAX_USERNAME_LEN - 1);
    strncpy(msg.filename, filename, MAX_FILENAME_LEN - 1);
    
    send_message(ns_socket, &msg);
    
    Message response;
    if (receive_message(ns_socket, &response) <= 0) {
        printf("Error: Failed to get file location\n");
        close_socket(ns_socket);
        return;
    }
    
    close_socket(ns_socket);
    
    if (response.error_code != ERR_SUCCESS) {
        printf("Error: %s\n", response.content);
        return;
    }
    
    // Connect to SS
    int ss_socket = create_socket();
    if (ss_socket < 0 || connect_to_server(ss_socket, response.ip_address, response.nm_port) < 0) {
        printf("Error: Failed to connect to Storage Server\n");
        if (ss_socket >= 0) close_socket(ss_socket);
        return;
    }
    
    Message list_msg;
    INIT_MESSAGE(list_msg);
    list_msg.operation = OP_LISTCHECKPOINTS;
    strncpy(list_msg.sender_id, current_username, MAX_USERNAME_LEN - 1);
    strncpy(list_msg.filename, filename, MAX_FILENAME_LEN - 1);
    
    send_message(ss_socket, &list_msg);
    
    if (receive_message(ss_socket, &response) <= 0) {
        printf("Error: Failed to receive response\n");
        close_socket(ss_socket);
        return;
    }
    
    close_socket(ss_socket);
    
    if (response.error_code == ERR_SUCCESS) {
        printf("Checkpoints for '%s':\n", filename);
        printf("%s\n", response.content);
        log_info("LISTCHECKPOINTS successful");
    } else {
        printf("Error: %s\n", response.content);
        log_error("LISTCHECKPOINTS failed: %s", get_error_message(response.error_code));
    }
}

// REQUESTACCESS - Request access to file (Bonus: Access requests)
void cmd_requestaccess(const char* filename) {
    if (strlen(filename) == 0) {
        printf("Error: Filename cannot be empty\n");
        printf("Usage: REQUESTACCESS <filename>\n");
        return;
    }
    
    log_info("REQUESTACCESS: '%s'", filename);
    
    int ns_socket = create_socket();
    if (ns_socket < 0 || connect_to_server(ns_socket, NS_IP, NS_PORT) < 0) {
        printf("Error: Failed to connect to Name Server\n");
        if (ns_socket >= 0) close_socket(ns_socket);
        return;
    }
    
    Message msg;
    INIT_MESSAGE(msg);
    msg.operation = OP_REQUESTACCESS;
    strncpy(msg.sender_id, current_username, MAX_USERNAME_LEN - 1);
    strncpy(msg.filename, filename, MAX_FILENAME_LEN - 1);
    
    send_message(ns_socket, &msg);
    
    Message response;
    if (receive_message(ns_socket, &response) <= 0) {
        printf("Error: Failed to receive response\n");
        close_socket(ns_socket);
        return;
    }
    
    close_socket(ns_socket);
    
    if (response.error_code == ERR_SUCCESS) {
        printf("%s\n", response.content);
        log_info("REQUESTACCESS successful");
    } else {
        printf("Error: %s\n", response.content);
        log_error("REQUESTACCESS failed: %s", get_error_message(response.error_code));
    }
}

// VIEWREQUESTS - View pending requests (Bonus: Access requests)
void cmd_viewrequests(const char* filename) {
    if (strlen(filename) == 0) {
        printf("Error: Filename cannot be empty\n");
        printf("Usage: VIEWREQUESTS <filename>\n");
        return;
    }
    
    log_info("VIEWREQUESTS: '%s'", filename);
    
    int ns_socket = create_socket();
    if (ns_socket < 0 || connect_to_server(ns_socket, NS_IP, NS_PORT) < 0) {
        printf("Error: Failed to connect to Name Server\n");
        if (ns_socket >= 0) close_socket(ns_socket);
        return;
    }
    
    Message msg;
    INIT_MESSAGE(msg);
    msg.operation = OP_VIEWREQUESTS;
    strncpy(msg.sender_id, current_username, MAX_USERNAME_LEN - 1);
    strncpy(msg.filename, filename, MAX_FILENAME_LEN - 1);
    
    send_message(ns_socket, &msg);
    
    Message response;
    if (receive_message(ns_socket, &response) <= 0) {
        printf("Error: Failed to receive response\n");
        close_socket(ns_socket);
        return;
    }
    
    close_socket(ns_socket);
    
    if (response.error_code == ERR_SUCCESS) {
        printf("Pending access requests for '%s':\n", filename);
        printf("%s\n", response.content);
        log_info("VIEWREQUESTS successful");
    } else {
        printf("Error: %s\n", response.content);
        log_error("VIEWREQUESTS failed: %s", get_error_message(response.error_code));
    }
}

// APPROVEREQUEST - Approve access request (Bonus: Access requests)
void cmd_approverequest(const char* filename, const char* username) {
    if (strlen(filename) == 0 || strlen(username) == 0) {
        printf("Error: Filename and username cannot be empty\n");
        printf("Usage: APPROVEREQUEST <filename> <username>\n");
        return;
    }
    
    log_info("APPROVEREQUEST: '%s' for user '%s'", filename, username);
    
    int ns_socket = create_socket();
    if (ns_socket < 0 || connect_to_server(ns_socket, NS_IP, NS_PORT) < 0) {
        printf("Error: Failed to connect to Name Server\n");
        if (ns_socket >= 0) close_socket(ns_socket);
        return;
    }
    
    Message msg;
    INIT_MESSAGE(msg);
    msg.operation = OP_APPROVEREQUEST;
    strncpy(msg.sender_id, current_username, MAX_USERNAME_LEN - 1);
    strncpy(msg.filename, filename, MAX_FILENAME_LEN - 1);
    strncpy(msg.content, username, MAX_CONTENT_LEN - 1);  // Username in content field
    
    send_message(ns_socket, &msg);
    
    Message response;
    if (receive_message(ns_socket, &response) <= 0) {
        printf("Error: Failed to receive response\n");
        close_socket(ns_socket);
        return;
    }
    
    close_socket(ns_socket);
    
    if (response.error_code == ERR_SUCCESS) {
        printf("%s\n", response.content);
        log_info("APPROVEREQUEST successful");
    } else {
        printf("Error: %s\n", response.content);
        log_error("APPROVEREQUEST failed: %s", get_error_message(response.error_code));
    }
}

// DENYREQUEST - Deny access request (Bonus: Access requests)
void cmd_denyrequest(const char* filename, const char* username) {
    if (strlen(filename) == 0 || strlen(username) == 0) {
        printf("Error: Filename and username cannot be empty\n");
        printf("Usage: DENYREQUEST <filename> <username>\n");
        return;
    }
    
    log_info("DENYREQUEST: '%s' for user '%s'", filename, username);
    
    int ns_socket = create_socket();
    if (ns_socket < 0 || connect_to_server(ns_socket, NS_IP, NS_PORT) < 0) {
        printf("Error: Failed to connect to Name Server\n");
        if (ns_socket >= 0) close_socket(ns_socket);
        return;
    }
    
    Message msg;
    INIT_MESSAGE(msg);
    msg.operation = OP_DENYREQUEST;
    strncpy(msg.sender_id, current_username, MAX_USERNAME_LEN - 1);
    strncpy(msg.filename, filename, MAX_FILENAME_LEN - 1);
    strncpy(msg.content, username, MAX_CONTENT_LEN - 1);  // Username in content field
    
    send_message(ns_socket, &msg);
    
    Message response;
    if (receive_message(ns_socket, &response) <= 0) {
        printf("Error: Failed to receive response\n");
        close_socket(ns_socket);
        return;
    }
    
    close_socket(ns_socket);
    
    if (response.error_code == ERR_SUCCESS) {
        printf("%s\n", response.content);
        log_info("DENYREQUEST successful");
    } else {
        printf("Error: %s\n", response.content);
        log_error("DENYREQUEST failed: %s", get_error_message(response.error_code));
    }
}

void cmd_help() {
    printf("\n");
    printf("Available Commands:\n");
    printf("════════════════════════════════════════════════════════════════════════\n");
    printf("  File Operations:\n");
    printf("    CREATE <filename>                - Create a new file\n");
    printf("    READ <filename>                  - Read and display file content\n");
    printf("    WRITE <filename> <sentence_num>  - Edit words in a sentence\n");
    printf("    DELETE <filename>                - Delete a file\n");
    printf("    UNDO <filename>                  - Restore file to previous state\n");
    printf("    STREAM <filename>                - Stream file content word-by-word\n");
    printf("    EXEC <filename>                  - Execute file content as command\n");
    printf("\n");
    printf("  File Information:\n");
    printf("    VIEW                             - List files you can access\n");
    printf("    VIEW -a                          - List all files in system\n");
    printf("    VIEW -l                          - List accessible files with details\n");
    printf("    VIEW -al                         - List all files with details\n");
    printf("    INFO <filename>                  - Show detailed file information\n");
    printf("\n");
    printf("  Access Control:\n");
    printf("    ADDACCESS -R <filename> <user>   - Grant read access to user\n");
    printf("    ADDACCESS -W <filename> <user>   - Grant write access to user\n");
    printf("    REMACCESS <filename> <user>      - Revoke all access from user\n");
    printf("\n");
    printf("  User Management:\n");
    printf("    LIST                             - Show all registered users\n");
    printf("\n");
    printf("  BONUS - Hierarchical Folders:\n");
    printf("    CREATEFOLDER <foldername>        - Create a new folder\n");
    printf("    MOVE <filename> <foldername>     - Move file to folder\n");
    printf("    VIEWFOLDER <foldername>          - List files in folder\n");
    printf("\n");
    printf("  BONUS - Checkpoints:\n");
    printf("    CHECKPOINT <file> <tag>          - Create a checkpoint\n");
    printf("    VIEWCHECKPOINT <file> <tag>      - View checkpoint content\n");
    printf("    REVERT <file> <tag>              - Revert to checkpoint\n");
    printf("    LISTCHECKPOINTS <file>           - List all checkpoints\n");
    printf("\n");
    printf("  BONUS - Access Requests:\n");
    printf("    REQUESTACCESS <filename>         - Request access to file\n");
    printf("    VIEWREQUESTS <filename>          - View pending requests (owner)\n");
    printf("    APPROVEREQUEST <file> <user>     - Approve access request\n");
    printf("    DENYREQUEST <file> <user>        - Deny access request\n");
    printf("\n");
    printf("  General:\n");
    printf("    HELP                             - Show this help message\n");
    printf("    EXIT                             - Exit the client\n");
    printf("════════════════════════════════════════════════════════════════════════\n");
    printf("\n");
    printf("Examples:\n");
    printf("  CREATE mydoc.txt\n");
    printf("  READ mydoc.txt\n");
    printf("  WRITE mydoc.txt 0\n");
    printf("  UNDO mydoc.txt\n");
    printf("  STREAM mydoc.txt\n");
    printf("  EXEC script.sh\n");
    printf("  VIEW -l\n");
    printf("  INFO mydoc.txt\n");
    printf("  ADDACCESS -R mydoc.txt alice\n");
    printf("  LIST\n");
    printf("  CREATEFOLDER reports\n");
    printf("  MOVE mydoc.txt reports\n");
    printf("  CHECKPOINT mydoc.txt v1\n");
    printf("  REQUESTACCESS mydoc.txt\n");
    printf("\n");
}

/* ============================================================================
 * MAIN
 * ============================================================================ */

int main() {
    // Initialize logger
    init_logger("CLIENT", "logs/client.log");
    
    // Setup signal handler for graceful shutdown
    signal(SIGINT, signal_handler);
    
    // Display banner
    printf("\n");
    printf("╔════════════════════════════════════════════════════════════╗\n");
    printf("║                    Docs++ Client                           ║\n");
    printf("║         Distributed Document Collaboration System          ║\n");
    printf("╚════════════════════════════════════════════════════════════╝\n");
    printf("\n");
    
    // Get username
    printf("Enter your username: ");
    fflush(stdout);
    
    if (fgets(current_username, sizeof(current_username), stdin) == NULL) {
        printf("Error reading username\n");
        return 1;
    }
    
    trim(current_username);
    
    if (strlen(current_username) == 0) {
        printf("Error: Username cannot be empty\n");
        return 1;
    }
    
    log_info("User logged in: %s", current_username);
    
    // Get local IP
    get_local_ip(local_ip);
    
    // Register with NS
    printf("Connecting to Name Server...\n");
    if (register_with_nameserver() < 0) {
        printf("Failed to connect to Name Server. Exiting.\n");
        close_logger();
        return 1;
    }
    
    printf("\n");
    printf("Type 'HELP' for list of commands\n");
    printf("\n");
    
    // Main command loop
    char input[MAX_INPUT];
    char command[MAX_FILENAME_LEN];
    char arg1[MAX_FILENAME_LEN];
    char arg2[MAX_FILENAME_LEN];
    char arg3[MAX_FILENAME_LEN];
    
    while (1) {
        printf("docs++> ");
        fflush(stdout);
        
        if (fgets(input, sizeof(input), stdin) == NULL) {
            break;
        }
        
        // Parse input
        if (!parse_input(input, command, arg1, arg2, arg3)) {
            continue;
        }
        
        if (strlen(command) == 0) {
            continue;
        }
        
        // Handle commands
        if (strcmp(command, "CREATE") == 0) {
            cmd_create(arg1);
        } else if (strcmp(command, "READ") == 0) {
            cmd_read(arg1);
        } else if (strcmp(command, "WRITE") == 0) {
            cmd_write(arg1, arg2);
        } else if (strcmp(command, "DELETE") == 0) {
            cmd_delete(arg1);
        } else if (strcmp(command, "UNDO") == 0) {
            cmd_undo(arg1);
        } else if (strcmp(command, "STREAM") == 0) {
            cmd_stream(arg1);
        } else if (strcmp(command, "EXEC") == 0) {
            cmd_exec(arg1);
        } else if (strcmp(command, "LIST") == 0) {
            cmd_list();
        } else if (strcmp(command, "VIEW") == 0) {
            cmd_view(arg1);
        } else if (strcmp(command, "INFO") == 0) {
            cmd_info(arg1);
        } else if (strcmp(command, "ADDACCESS") == 0) {
            cmd_addaccess(arg1, arg2, arg3);
        } else if (strcmp(command, "REMACCESS") == 0) {
            cmd_remaccess(arg1, arg2);
        } else if (strcmp(command, "CREATEFOLDER") == 0) {
            cmd_createfolder(arg1);
        } else if (strcmp(command, "MOVE") == 0) {
            cmd_move(arg1, arg2);
        } else if (strcmp(command, "VIEWFOLDER") == 0) {
            cmd_viewfolder(arg1);
        } else if (strcmp(command, "CHECKPOINT") == 0) {
            cmd_checkpoint(arg1, arg2);
        } else if (strcmp(command, "VIEWCHECKPOINT") == 0) {
            cmd_viewcheckpoint(arg1, arg2);
        } else if (strcmp(command, "REVERT") == 0) {
            cmd_revert(arg1, arg2);
        } else if (strcmp(command, "LISTCHECKPOINTS") == 0) {
            cmd_listcheckpoints(arg1);
        } else if (strcmp(command, "REQUESTACCESS") == 0) {
            cmd_requestaccess(arg1);
        } else if (strcmp(command, "VIEWREQUESTS") == 0) {
            cmd_viewrequests(arg1);
        } else if (strcmp(command, "APPROVEREQUEST") == 0) {
            cmd_approverequest(arg1, arg2);
        } else if (strcmp(command, "DENYREQUEST") == 0) {
            cmd_denyrequest(arg1, arg2);
        } else if (strcmp(command, "HELP") == 0) {
            cmd_help();
        } else if (strcmp(command, "EXIT") == 0 || strcmp(command, "QUIT") == 0) {
            printf("Goodbye, %s!\n", current_username);
            log_info("User logged out: %s", current_username);
            break;
        } else {
            printf("Unknown command: %s\n", command);
            printf("Type 'HELP' for list of commands\n");
        }
        
        printf("\n");
    }
    
    close_logger();
    return 0;
}