/*
 * storage_server/storage_server.c
 * * Storage Server - Handles file storage and retrieval
 * Registers with Name Server and serves clients directly
 * * Compile: gcc -o storage_server storage_server.c ../common/socket_utils.c ../common/logger.c -I../common -pthread
 * Run: ./storage_server
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h> // Required for setsockopt
#include <dirent.h>
#include <errno.h>
#include "socket_utils.h"
#include "logger.h"
#include "protocol.h"
#include "sentence_lock.h"
#include "sentence_parser.h"
#include "undo_buffer.h"
#include "folder_manager.h"
#include "checkpoint_manager.h"

// Configurable via command-line arguments
static char NS_IP[MAX_IP_LEN] = "192.168.1.187";  // Name Server IP
static int NS_PORT = 8080;                         // Name Server port
static int SS_NM_PORT = 8081;                      // Port for NS communication (inbound)
static int SS_CLIENT_PORT = 8082;                  // Port for client communication (inbound)
char STORAGE_DIR[MAX_PATH_LEN] = "./storage";  // Storage directory (non-static for module access)

// Global state
int ns_socket = -1;             // Outbound socket to NS (for registration/heartbeat)
int nm_server_socket = -1;      // Inbound socket for NS forwarded requests (8081)
int client_server_socket = -1;  // Inbound socket for client requests (8082)
char local_ip[MAX_IP_LEN];

// WRITE operation state
LockTable lock_table;
UndoTable undo_table;  // Changed from UndoBuffer to UndoTable

/* ============================================================================
 * FILE OPERATIONS (Original code retained)
 * ============================================================================ */

int ensure_storage_dir() {
    struct stat st = {0};
    
    if (stat(STORAGE_DIR, &st) == -1) {
        log_info("Creating storage directory: %s", STORAGE_DIR);
        if (mkdir(STORAGE_DIR, 0755) == -1) {
            log_error("Failed to create storage directory: %s", strerror(errno));
            return -1;
        }
    }
    return 0;
}

void get_file_path(const char* filename, char* filepath) {
    snprintf(filepath, MAX_PATH_LEN, "%s/%s", STORAGE_DIR, filename);
}

int file_exists(const char* filename) {
    char filepath[MAX_PATH_LEN];
    get_file_path(filename, filepath);
    
    struct stat st;
    return (stat(filepath, &st) == 0);
}

int create_file(const char* filename, const char* owner) {
    char filepath[MAX_PATH_LEN];
    get_file_path(filename, filepath);
    
    (void)owner;  // Reserved for future use (metadata tracking)
    
    if (file_exists(filename)) {
        log_warning("File '%s' already exists", filename);
        return ERR_CONFLICT;
    }
    
    FILE* fp = fopen(filepath, "w");
    if (fp == NULL) {
        log_error("Failed to create file '%s': %s", filename, strerror(errno));
        return ERR_SERVER_ERROR;
    }
    
    fclose(fp);
    log_info("File '%s' created successfully", filename);
    return ERR_CREATED;
}

int read_file(const char* filename, char* buffer, size_t buffer_size) {
    char filepath[MAX_PATH_LEN];
    get_file_path(filename, filepath);
    
    if (!file_exists(filename)) {
        log_warning("File '%s' not found", filename);
        return ERR_NOT_FOUND;
    }
    
    FILE* fp = fopen(filepath, "r");
    if (fp == NULL) {
        log_error("Failed to open file '%s': %s", filename, strerror(errno));
        return ERR_SERVER_ERROR;
    }
    
    size_t bytes_read = fread(buffer, 1, buffer_size - 1, fp);
    buffer[bytes_read] = '\0';
    
    fclose(fp);
    log_info("File '%s' read successfully (%zu bytes)", filename, bytes_read);
    
    return ERR_SUCCESS;
}

int delete_file(const char* filename) {
    char filepath[MAX_PATH_LEN];
    get_file_path(filename, filepath);
    
    if (!file_exists(filename)) {
        log_warning("File '%s' not found", filename);
        return ERR_NOT_FOUND;
    }
    
    if (unlink(filepath) == -1) {
        log_error("Failed to delete file '%s': %s", filename, strerror(errno));
        return ERR_SERVER_ERROR;
    }
    
    log_info("File '%s' deleted successfully", filename);
    return ERR_SUCCESS;
}

int get_file_metadata(const char* filename, char* metadata_str, size_t str_size) {
    char filepath[MAX_PATH_LEN];
    get_file_path(filename, filepath);
    
    if (!file_exists(filename)) {
        log_warning("File '%s' not found for metadata", filename);
        return ERR_NOT_FOUND;
    }
    
    struct stat st;
    if (stat(filepath, &st) != 0) {
        log_error("Failed to stat file '%s': %s", filename, strerror(errno));
        return ERR_SERVER_ERROR;
    }
    
    // Calculate word count and character count
    FILE* fp = fopen(filepath, "r");
    if (fp == NULL) {
        log_error("Failed to open file '%s' for metadata: %s", filename, strerror(errno));
        return ERR_SERVER_ERROR;
    }
    
    int word_count = 0;
    int char_count = 0;
    int in_word = 0;
    int c;
    
    while ((c = fgetc(fp)) != EOF) {
        char_count++;
        
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            in_word = 0;
        } else {
            if (!in_word) {
                word_count++;
                in_word = 1;
            }
        }
    }
    
    fclose(fp);
    
    // Format: size|word_count|char_count|created_time|modified_time|accessed_time
    snprintf(metadata_str, str_size, "%ld|%d|%d|%ld|%ld|%ld",
             (long)st.st_size, word_count, char_count,
             (long)st.st_ctime, (long)st.st_mtime, (long)st.st_atime);
    
    log_info("Metadata for '%s': size=%ld, words=%d, chars=%d",
             filename, (long)st.st_size, word_count, char_count);
    
    return ERR_SUCCESS;
}

int scan_storage_files(char files[][MAX_FILENAME_LEN], int max_files) {
    DIR* dir;
    struct dirent* entry;
    int count = 0;
    
    dir = opendir(STORAGE_DIR);
    if (dir == NULL) {
        log_warning("Could not open storage directory");
        return 0;
    }
    
    while ((entry = readdir(dir)) != NULL && count < max_files) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        
        char filepath[MAX_PATH_LEN];
        snprintf(filepath, sizeof(filepath), "%s/%s", STORAGE_DIR, entry->d_name);
        
        struct stat st;
        if (stat(filepath, &st) == 0 && S_ISREG(st.st_mode)) {
            strncpy(files[count], entry->d_name, MAX_FILENAME_LEN - 1);
            files[count][MAX_FILENAME_LEN - 1] = '\0';
            count++;
            log_info("Found existing file: %s", entry->d_name);
        }
    }
    
    closedir(dir);
    return count;
}


/* ============================================================================
 * REGISTRATION WITH NAME SERVER (Modified for file sync)
 * ============================================================================ */

int register_with_nameserver(char files[][MAX_FILENAME_LEN], int file_count) {
    log_info("Registering with Name Server at %s:%d", NS_IP, NS_PORT);
    
    // Create socket for NS communication
    ns_socket = create_socket();
    if (ns_socket < 0) {
        log_error("Failed to create socket for NS connection");
        return -1;
    }
    
    // Connect to NS
    if (connect_to_server(ns_socket, NS_IP, NS_PORT) < 0) {
        log_error("Failed to connect to Name Server");
        close_socket(ns_socket);
        return -1;
    }
    
    log_info("Connected to Name Server");
    
    // Prepare registration message
    Message reg_msg;
    INIT_MESSAGE(reg_msg);
    reg_msg.operation = OP_REGISTER_SS;
    strncpy(reg_msg.ip_address, local_ip, MAX_IP_LEN - 1);
    reg_msg.nm_port = SS_NM_PORT;
    reg_msg.client_port = SS_CLIENT_PORT;
    
    // NEW LOGIC: Serialize file list into content field using '|' delimiter
    char file_list_str[MAX_CONTENT_LEN] = "";
    int current_len = 0;
    
    for (int i = 0; i < file_count; i++) {
        // Append filename, followed by a delimiter
        int required = snprintf(file_list_str + current_len, 
                                sizeof(file_list_str) - current_len, 
                                "%s|", files[i]);
        
        // Safety check: Stop if the content buffer is full
        if (current_len + (size_t)required >= sizeof(file_list_str)) {
            log_warning("File list truncated during serialization for NS registration.");
            break;
        }
        current_len += required;
    }
    
    // Remove trailing delimiter if any
    if (current_len > 0 && file_list_str[current_len - 1] == '|') {
        file_list_str[current_len - 1] = '\0';
    }

    strncpy(reg_msg.content, file_list_str, MAX_CONTENT_LEN - 1);
    reg_msg.content[MAX_CONTENT_LEN - 1] = '\0';
    log_info("Serialized %d files into registration content.", file_count);
    
    log_info("Sending registration message");
    
    // Send registration
    if (send_message(ns_socket, &reg_msg) < 0) {
        log_error("Failed to send registration message");
        close_socket(ns_socket);
        return -1;
    }
    
    // Wait for acknowledgment
    Message ack;
    if (receive_message(ns_socket, &ack) <= 0) {
        log_error("Failed to receive registration acknowledgment");
        close_socket(ns_socket);
        return -1;
    }
    
    if (ack.error_code != ERR_SUCCESS) {
        log_error("Registration failed: %s", ack.content);
        close_socket(ns_socket);
        return -1;
    }
    
    log_info("Registration successful: %s", ack.content);
    
    // Close the registration socket after successful ACK (it's not needed for inbound ops)
    close_socket(ns_socket); 
    ns_socket = -1;
    
    return 0;
}


/* ============================================================================
 * WRITE OPERATION - Sentence-level atomic write with locking
 * ============================================================================ */

typedef struct {
    char filename[MAX_FILENAME_LEN];
    int sentence_num;
    char client_id[MAX_USERNAME_LEN];
    char** words;
    int word_count;
    int active;
} WriteSession;

WriteSession write_session = {0};

void handle_write_session(int client_socket, Message* msg) {
    Message response;
    INIT_MESSAGE(response);
    
    if (msg->operation == OP_WRITE_START) {
        // Start WRITE session - acquire lock
        log_info("WRITE_START: file=%s, sentence=%d, client=%s", 
                 msg->filename, msg->sentence_num, msg->sender_id);
        
        // Check file exists
        if (!file_exists(msg->filename)) {
            response.error_code = ERR_NOT_FOUND;
            strcpy(response.content, "File not found");
            send_message(client_socket, &response);
            return;
        }
        
        // Try to acquire lock
        int lock_result = lock_acquire(&lock_table, msg->filename, msg->sentence_num, msg->sender_id);
        
        if (lock_result == ERR_LOCKED) {
            response.error_code = ERR_LOCKED;
            strcpy(response.content, "Sentence locked by another user");
            send_message(client_socket, &response);
            return;
        }
        
        // Read file and parse sentences
        char file_content[MAX_CONTENT_LEN];
        int read_result = read_file(msg->filename, file_content, sizeof(file_content));
        
        if (read_result != ERR_SUCCESS) {
            lock_release(&lock_table, msg->filename, msg->sentence_num, msg->sender_id);
            response.error_code = read_result;
            strcpy(response.content, "Failed to read file");
            send_message(client_socket, &response);
            return;
        }
        
        char** sentences;
        int sentence_count = parse_sentences(file_content, &sentences);
        
        // Special case: Allow WRITE 0 on empty file (sentence_count == 0)
        if (msg->sentence_num < 0 || (sentence_count > 0 && msg->sentence_num >= sentence_count) || 
            (sentence_count == 0 && msg->sentence_num > 0)) {
            free_sentences(sentences, sentence_count);
            lock_release(&lock_table, msg->filename, msg->sentence_num, msg->sender_id);
            response.error_code = ERR_INDEX_ERROR;
            snprintf(response.content, sizeof(response.content),
                    "Invalid sentence number %d (file has %d sentences)", 
                    msg->sentence_num, sentence_count);
            send_message(client_socket, &response);
            return;
        }
        
        // Parse target sentence into words
        char** words;
        int word_count;
        
        if (sentence_count == 0 && msg->sentence_num == 0) {
            // Empty file - initialize with empty word list
            words = NULL;
            word_count = 0;
        } else {
            word_count = parse_words(sentences[msg->sentence_num], &words);
        }
        
        // Initialize write session
        strncpy(write_session.filename, msg->filename, MAX_FILENAME_LEN - 1);
        write_session.sentence_num = msg->sentence_num;
        strncpy(write_session.client_id, msg->sender_id, MAX_USERNAME_LEN - 1);
        write_session.words = words;
        write_session.word_count = word_count;
        write_session.active = 1;
        
        free_sentences(sentences, sentence_count);
        
        // Save to undo buffer (per-file)
        undo_save(&undo_table, msg->filename, file_content, strlen(file_content));
        
        response.error_code = ERR_SUCCESS;
        snprintf(response.content, sizeof(response.content),
                "Lock acquired. Sentence has %d words. Send word updates.", word_count);
        send_message(client_socket, &response);
        log_info("WRITE session started successfully");
        
    } else if (msg->operation == OP_WRITE_UPDATE) {
        // Update word in session
        if (!write_session.active) {
            response.error_code = ERR_BAD_REQUEST;
            strcpy(response.content, "No active WRITE session");
            send_message(client_socket, &response);
            return;
        }
        
        int word_idx = msg->word_index;
        const char* new_word = msg->content;
        
        log_info("WRITE_UPDATE: word_idx=%d, new_word='%s'", word_idx, new_word);
        
        // Validate index
        if (word_idx < 0 || word_idx > write_session.word_count) {
            response.error_code = ERR_INDEX_ERROR;
            snprintf(response.content, sizeof(response.content),
                    "Invalid word index %d (sentence has %d words)", 
                    word_idx, write_session.word_count);
            send_message(client_socket, &response);
            return;
        }
        
        // Insert word (creates new array)
        char** new_words = malloc((write_session.word_count + 1) * sizeof(char*));
        
        for (int i = 0; i < word_idx; i++) {
            new_words[i] = strdup(write_session.words[i]);
        }
        
        new_words[word_idx] = strdup(new_word);
        
        for (int i = word_idx; i < write_session.word_count; i++) {
            new_words[i + 1] = strdup(write_session.words[i]);
        }
        
        // Free old words
        free_words(write_session.words, write_session.word_count);
        
        write_session.words = new_words;
        write_session.word_count++;
        
        response.error_code = ERR_SUCCESS;
        snprintf(response.content, sizeof(response.content),
                "Word inserted at index %d. Sentence now has %d words.", 
                word_idx, write_session.word_count);
        send_message(client_socket, &response);
        log_info("Word update applied to session");
        
    } else if (msg->operation == OP_WRITE_COMMIT) {
        // ETIRW - commit changes atomically
        if (!write_session.active) {
            response.error_code = ERR_BAD_REQUEST;
            strcpy(response.content, "No active WRITE session");
            send_message(client_socket, &response);
            return;
        }
        
        log_info("WRITE_COMMIT: Committing changes atomically");
        
        // Read full file again
        char file_content[MAX_CONTENT_LEN];
        read_file(write_session.filename, file_content, sizeof(file_content));
        
        char** sentences;
        int sentence_count = parse_sentences(file_content, &sentences);
        
        // Rebuild sentence from words
        char new_sentence[MAX_CONTENT_LEN] = "";
        for (int i = 0; i < write_session.word_count; i++) {
            strcat(new_sentence, write_session.words[i]);
            if (i < write_session.word_count - 1) {
                strcat(new_sentence, " ");
            }
        }
        
        // Special case: Empty file (sentence_count == 0)
        if (sentence_count == 0) {
            // Add the new sentence as the first sentence
            if (has_delimiter(new_sentence)) {
                // Split by delimiters
                char** parts;
                int part_count = split_by_delimiters(new_sentence, &parts);
                sentences = parts;
                sentence_count = part_count;
            } else {
                // Single sentence
                sentences = malloc(sizeof(char*));
                sentences[0] = strdup(new_sentence);
                sentence_count = 1;
            }
        }
        // Check for delimiters in new sentence - handle splits
        else if (has_delimiter(new_sentence)) {
            char** parts;
            int part_count = split_by_delimiters(new_sentence, &parts);
            
            log_info("Sentence split into %d parts due to delimiters", part_count);
            
            // Replace old sentence with new parts
            char** new_sentences = malloc((sentence_count - 1 + part_count) * sizeof(char*));
            int new_idx = 0;
            
            for (int i = 0; i < write_session.sentence_num; i++) {
                new_sentences[new_idx++] = strdup(sentences[i]);
            }
            
            for (int i = 0; i < part_count; i++) {
                new_sentences[new_idx++] = strdup(parts[i]);
            }
            
            for (int i = write_session.sentence_num + 1; i < sentence_count; i++) {
                new_sentences[new_idx++] = strdup(sentences[i]);
            }
            
            free_sentences(sentences, sentence_count);
            free_sentences(parts, part_count);
            
            sentences = new_sentences;
            sentence_count = new_idx;
        } else {
            // No split - simple replacement
            free(sentences[write_session.sentence_num]);
            sentences[write_session.sentence_num] = strdup(new_sentence);
        }
        
        // Rebuild file
        char* new_content = rebuild_file(sentences, sentence_count);
        
        // Write atomically using temp file
        char filepath[MAX_PATH_LEN];
        char temp_filepath[MAX_PATH_LEN];
        get_file_path(write_session.filename, filepath);
        snprintf(temp_filepath, sizeof(temp_filepath), "%s.tmp", filepath);
        
        FILE* fp = fopen(temp_filepath, "w");
        if (!fp) {
            free(new_content);
            free_sentences(sentences, sentence_count);
            lock_release(&lock_table, write_session.filename, write_session.sentence_num, write_session.client_id);
            write_session.active = 0;
            response.error_code = ERR_SERVER_ERROR;
            strcpy(response.content, "Failed to write temp file");
            send_message(client_socket, &response);
            return;
        }
        
        fwrite(new_content, 1, strlen(new_content), fp);
        fclose(fp);
        
        // Atomic rename
        rename(temp_filepath, filepath);
        
        free(new_content);
        free_sentences(sentences, sentence_count);
        
        // Release lock
        lock_release(&lock_table, write_session.filename, write_session.sentence_num, write_session.client_id);
        
        // Clean up session
        free_words(write_session.words, write_session.word_count);
        write_session.active = 0;
        
        response.error_code = ERR_SUCCESS;
        strcpy(response.content, "WRITE committed successfully");
        send_message(client_socket, &response);
        log_info("WRITE committed and lock released");
    }
}

void handle_undo_request(int client_socket, Message* msg) {
    log_info("UNDO request for file '%s'", msg->filename);
    
    Message response;
    INIT_MESSAGE(response);
    
    // Check if undo is available for this file
    if (!undo_available(&undo_table, msg->filename)) {
        response.error_code = ERR_NOT_FOUND;
        strcpy(response.content, "No undo available for this file");
        send_message(client_socket, &response);
        return;
    }
    
    // Restore content from undo buffer
    char* restore_content = NULL;
    size_t restore_size = 0;
    
    int restore_result = undo_restore(&undo_table, msg->filename, &restore_content, &restore_size);
    
    if (restore_result != ERR_SUCCESS) {
        response.error_code = restore_result;
        strcpy(response.content, "Failed to restore from undo buffer");
        send_message(client_socket, &response);
        return;
    }
    
    // Write restored content back to file
    char filepath[MAX_PATH_LEN];
    get_file_path(msg->filename, filepath);
    
    FILE* fp = fopen(filepath, "w");
    if (!fp) {
        free(restore_content);
        response.error_code = ERR_SERVER_ERROR;
        strcpy(response.content, "Failed to write restored file");
        send_message(client_socket, &response);
        return;
    }
    
    fwrite(restore_content, 1, restore_size, fp);
    fclose(fp);
    
    free(restore_content);
    
    response.error_code = ERR_SUCCESS;
    strcpy(response.content, "File restored from undo buffer");
    send_message(client_socket, &response);
    log_info("UNDO completed for file '%s'", msg->filename);
}

void handle_stream_request(int client_socket, Message* msg) {
    log_info("STREAM request from client for file '%s'", msg->filename);
    
    Message response;
    INIT_MESSAGE(response);
    
    // Check if file exists
    if (!file_exists(msg->filename)) {
        response.operation = OP_ACK;
        response.error_code = ERR_NOT_FOUND;
        strcpy(response.content, "File not found");
        send_message(client_socket, &response);
        return;
    }
    
    // Read file content
    char file_content[MAX_CONTENT_LEN];
    int read_result = read_file(msg->filename, file_content, sizeof(file_content));
    
    if (read_result != ERR_SUCCESS) {
        response.operation = OP_ACK;
        response.error_code = read_result;
        strcpy(response.content, "Failed to read file");
        send_message(client_socket, &response);
        return;
    }
    
    // Send ACK first
    response.operation = OP_ACK;
    response.error_code = ERR_SUCCESS;
    strcpy(response.content, "Starting stream");
    send_message(client_socket, &response);
    
    // Parse content into words
    char** words;
    int word_count = parse_words(file_content, &words);
    
    if (word_count == 0) {
        // Empty file - send STOP
        Message stop;
        INIT_MESSAGE(stop);
        stop.operation = OP_STOP;
        send_message(client_socket, &stop);
        log_info("STREAM completed: empty file");
        return;
    }
    
    log_info("Streaming %d words from file '%s'", word_count, msg->filename);
    
    // Stream words one by one with 0.1s delay
    for (int i = 0; i < word_count; i++) {
        Message word_msg;
        INIT_MESSAGE(word_msg);
        word_msg.operation = OP_STREAM;
        word_msg.error_code = ERR_SUCCESS;
        strncpy(word_msg.content, words[i], MAX_CONTENT_LEN - 1);
        word_msg.word_index = i;  // Track word position
        
        int send_result = send_message(client_socket, &word_msg);
        if (send_result <= 0) {
            log_warning("Client disconnected during stream at word %d/%d", i + 1, word_count);
            free_words(words, word_count);
            return;
        }
        
        // 0.1 second delay between words
        usleep(100000);  // 100,000 microseconds = 0.1 seconds
    }
    
    free_words(words, word_count);
    
    // Send STOP to indicate stream complete
    Message stop;
    INIT_MESSAGE(stop);
    stop.operation = OP_STOP;
    send_message(client_socket, &stop);
    
    log_info("STREAM completed successfully: %d words sent", word_count);
}

/* ============================================================================
 * REQUEST HANDLERS (Original code retained)
 * ============================================================================ */

void handle_read_from_client(int client_socket, Message* msg) {
    log_info("READ request from client for file '%s'", msg->filename);
    
    char content[MAX_CONTENT_LEN];
    int result = read_file(msg->filename, content, sizeof(content));
    
    Message response;
    INIT_MESSAGE(response);
    response.operation = OP_ACK;
    response.error_code = result;
    
    if (result == ERR_SUCCESS) {
        // Only send content if read was successful
        strncpy(response.content, content, sizeof(response.content) - 1);
        response.content[sizeof(response.content) - 1] = '\0';
        response.content_length = strlen(response.content);
    } else {
        snprintf(response.content, sizeof(response.content),
                 "File '%s' %s", msg->filename, 
                 (result == ERR_NOT_FOUND) ? "not found" : "read failed");
    }
    
    send_message(client_socket, &response);
    
    // Send STOP packet
    Message stop;
    INIT_MESSAGE(stop);
    stop.operation = OP_STOP;
    send_message(client_socket, &stop);
    
    log_info("Sent READ response to client: %s", get_error_message(result));
}

/* ============================================================================
 * BONUS: FOLDER OPERATIONS HANDLERS
 * ============================================================================ */

void handle_createfolder_request(int socket, Message* msg) {
    log_info("CREATEFOLDER request for folder '%s'", msg->folder_name);
    
    int result = folder_create(msg->folder_name, msg->sender_id);
    
    Message response;
    INIT_MESSAGE(response);
    response.operation = OP_ACK;
    response.error_code = result;
    
    if (result == ERR_CREATED) {
        snprintf(response.content, sizeof(response.content),
                 "Folder '%s' created successfully", msg->folder_name);
    } else {
        snprintf(response.content, sizeof(response.content),
                 "Failed to create folder: %s", get_error_message(result));
    }
    
    send_message(socket, &response);
    log_info("CREATEFOLDER response sent: %s", get_error_message(result));
}

void handle_move_request(int socket, Message* msg) {
    log_info("MOVE request: file '%s' to folder '%s'", msg->filename, msg->folder_name);
    
    int result = folder_move_file(msg->filename, msg->folder_name, msg->sender_id);
    
    Message response;
    INIT_MESSAGE(response);
    response.operation = OP_ACK;
    response.error_code = result;
    
    if (result == ERR_SUCCESS) {
        snprintf(response.content, sizeof(response.content),
                 "File '%s' moved to folder '%s'", msg->filename, msg->folder_name);
    } else {
        snprintf(response.content, sizeof(response.content),
                 "Failed to move file: %s", get_error_message(result));
    }
    
    send_message(socket, &response);
    log_info("MOVE response sent: %s", get_error_message(result));
}

void handle_viewfolder_request(int socket, Message* msg) {
    log_info("VIEWFOLDER request for folder '%s'", msg->folder_name);
    
    char files[MAX_FILES_PER_FOLDER][MAX_FILENAME_LEN];
    int file_count = folder_list_files(msg->folder_name, files, MAX_FILES_PER_FOLDER);
    
    Message response;
    INIT_MESSAGE(response);
    response.operation = OP_ACK;
    
    if (file_count < 0) {
        response.error_code = ERR_NOT_FOUND;
        snprintf(response.content, sizeof(response.content),
                 "Folder '%s' not found", msg->folder_name);
    } else {
        response.error_code = ERR_SUCCESS;
        int offset = 0;
        offset += snprintf(response.content + offset, sizeof(response.content) - offset,
                          "Files in folder '%s' (%d):\n", msg->folder_name, file_count);
        
        for (int i = 0; i < file_count && offset < MAX_CONTENT_LEN - 50; i++) {
            offset += snprintf(response.content + offset, sizeof(response.content) - offset,
                             "  - %s\n", files[i]);
        }
    }
    
    send_message(socket, &response);
    log_info("VIEWFOLDER response sent: %d files", file_count);
}

/* ============================================================================
 * BONUS: CHECKPOINT OPERATIONS HANDLERS
 * ============================================================================ */

void handle_checkpoint_request(int socket, Message* msg) {
    log_info("CHECKPOINT request: file '%s', tag '%s'", msg->filename, msg->checkpoint_tag);
    
    int result = checkpoint_create(msg->filename, msg->checkpoint_tag, msg->sender_id);
    
    Message response;
    INIT_MESSAGE(response);
    response.operation = OP_ACK;
    response.error_code = result;
    
    if (result == ERR_CREATED) {
        snprintf(response.content, sizeof(response.content),
                 "Checkpoint '%s' created for file '%s'", msg->checkpoint_tag, msg->filename);
    } else {
        snprintf(response.content, sizeof(response.content),
                 "Failed to create checkpoint: %s", get_error_message(result));
    }
    
    send_message(socket, &response);
    log_info("CHECKPOINT response sent: %s", get_error_message(result));
}

void handle_viewcheckpoint_request(int socket, Message* msg) {
    log_info("VIEWCHECKPOINT request: file '%s', tag '%s'", msg->filename, msg->checkpoint_tag);
    
    char content[MAX_CONTENT_LEN];
    int result = checkpoint_view(msg->filename, msg->checkpoint_tag, content, sizeof(content));
    
    Message response;
    INIT_MESSAGE(response);
    response.operation = OP_ACK;
    response.error_code = result;
    
    if (result == ERR_SUCCESS) {
        strncpy(response.content, content, sizeof(response.content) - 1);
        response.content[sizeof(response.content) - 1] = '\0';
    } else {
        snprintf(response.content, sizeof(response.content),
                 "Failed to view checkpoint: %s", get_error_message(result));
    }
    
    send_message(socket, &response);
    log_info("VIEWCHECKPOINT response sent: %s", get_error_message(result));
}

void handle_revert_request(int socket, Message* msg) {
    log_info("REVERT request: file '%s' to checkpoint '%s'", msg->filename, msg->checkpoint_tag);
    
    int result = checkpoint_revert(msg->filename, msg->checkpoint_tag, msg->sender_id);
    
    Message response;
    INIT_MESSAGE(response);
    response.operation = OP_ACK;
    response.error_code = result;
    
    if (result == ERR_SUCCESS) {
        snprintf(response.content, sizeof(response.content),
                 "File '%s' reverted to checkpoint '%s'", msg->filename, msg->checkpoint_tag);
    } else {
        snprintf(response.content, sizeof(response.content),
                 "Failed to revert: %s", get_error_message(result));
    }
    
    send_message(socket, &response);
    log_info("REVERT response sent: %s", get_error_message(result));
}

void handle_listcheckpoints_request(int socket, Message* msg) {
    log_info("LISTCHECKPOINTS request for file '%s'", msg->filename);
    
    char tags[MAX_CHECKPOINTS][MAX_TAG_LEN];
    int count = checkpoint_list(msg->filename, tags, MAX_CHECKPOINTS);
    
    Message response;
    INIT_MESSAGE(response);
    response.operation = OP_ACK;
    response.error_code = ERR_SUCCESS;
    
    int offset = 0;
    offset += snprintf(response.content + offset, sizeof(response.content) - offset,
                      "Checkpoints for '%s' (%d):\n", msg->filename, count);
    
    for (int i = 0; i < count && offset < MAX_CONTENT_LEN - 50; i++) {
        offset += snprintf(response.content + offset, sizeof(response.content) - offset,
                         "  - %s\n", tags[i]);
    }
    
    send_message(socket, &response);
    log_info("LISTCHECKPOINTS response sent: %d checkpoints", count);
}


/* ============================================================================
 * SERVER THREADS (Modified)
 * ============================================================================ */

// NEW THREAD: Handles NS-forwarded requests (CREATE/DELETE) on SS_NM_PORT (8081)
void* ns_inbound_thread(void* arg) {
    (void)arg;  // Unused parameter (required by pthread signature)
    log_info("NS inbound thread started on port %d", SS_NM_PORT);
    
    while (1) {
        char ns_ip[MAX_IP_LEN];
        int ns_port;
        
        // Accept connection from NS
        int ns_conn_socket = accept_connection(nm_server_socket, ns_ip, &ns_port);
        if (ns_conn_socket < 0) {
            log_error("Failed to accept NS forwarding connection");
            continue;
        }
        
        log_info("NS connected for forwarding from %s:%d", ns_ip, ns_port);
        
        // Receive request
        Message msg;
        int bytes = receive_message(ns_conn_socket, &msg);
        if (bytes <= 0) {
            log_error("Failed to receive forwarded message from NS");
            close_socket(ns_conn_socket);
            continue;
        }
        
        log_info("Received forwarded request from NS: %s for file '%s'", 
                 get_operation_name(msg.operation), msg.filename);
        
        int result;
        if (msg.operation == OP_CREATE) {
            result = create_file(msg.filename, msg.sender_id);
        } else if (msg.operation == OP_DELETE) {
            result = delete_file(msg.filename);
        } else if (msg.operation == OP_GET_METADATA) {
            // Handle metadata request
            char metadata[MAX_CONTENT_LEN];
            result = get_file_metadata(msg.filename, metadata, sizeof(metadata));
            
            // Send response back to NS
            Message response;
            INIT_MESSAGE(response);
            response.operation = OP_ACK;
            response.error_code = result;
            
            if (result == ERR_SUCCESS) {
                strncpy(response.content, metadata, sizeof(response.content) - 1);
                response.content[sizeof(response.content) - 1] = '\0';
            } else {
                snprintf(response.content, sizeof(response.content),
                        "Failed to get metadata: %s", get_error_message(result));
            }
            
            send_message(ns_conn_socket, &response);
            log_info("Sent metadata response to NS");
            close_socket(ns_conn_socket);
            continue;
        } else if (msg.operation == OP_CREATEFOLDER) {
            handle_createfolder_request(&msg, ns_conn_socket);
            close_socket(ns_conn_socket);
            continue;
        } else if (msg.operation == OP_MOVE) {
            handle_move_request(&msg, ns_conn_socket);
            close_socket(ns_conn_socket);
            continue;
        } else if (msg.operation == OP_VIEWFOLDER) {
            handle_viewfolder_request(&msg, ns_conn_socket);
            close_socket(ns_conn_socket);
            continue;
        } else {
            log_warning("Unknown operation from NS: %d", msg.operation);
            result = ERR_NOT_IMPLEMENTED;
        }

        // Send response back to NS
        Message response;
        INIT_MESSAGE(response);
        response.operation = OP_ACK;
        response.error_code = result;

        if (result == ERR_CREATED || result == ERR_SUCCESS) {
             snprintf(response.content, sizeof(response.content), "Operation successful");
        } else {
            snprintf(response.content, sizeof(response.content), 
                     "Operation failed: %s", get_error_message(result));
        }

        send_message(ns_conn_socket, &response);
        log_info("Sent forwarded response to NS: %s", get_error_message(result));

        close_socket(ns_conn_socket);
    }
    return NULL;
}


// Thread to handle client connections on SS_CLIENT_PORT (8082)
void* client_server_thread(void* arg) {
    (void)arg;  // Unused parameter (required by pthread signature)
    log_info("Client server thread started on port %d", SS_CLIENT_PORT);
    
    while (1) {
        char client_ip[MAX_IP_LEN];
        int client_port;
        
        // Accept client connection
        int client_socket = accept_connection(client_server_socket, client_ip, &client_port);
        if (client_socket < 0) {
            log_error("Failed to accept client connection");
            continue;
        }
        
        log_info("Client connected from %s:%d", client_ip, client_port);
        
        // Receive request
        Message msg;
        int bytes = receive_message(client_socket, &msg);
        
        if (bytes <= 0) {
            log_error("Failed to receive message from client");
            close_socket(client_socket);
            continue;
        }
        
        log_info("Received request from client: %s", get_operation_name(msg.operation));
        
        // Special handling for WRITE sessions - keep connection alive
        if (msg.operation == OP_WRITE_START) {
            // Handle WRITE_START
            handle_write_session(client_socket, &msg);
            
            // Keep connection alive for subsequent WRITE_UPDATE and WRITE_COMMIT
            while (1) {
                Message write_msg;
                int bytes = receive_message(client_socket, &write_msg);
                
                if (bytes <= 0) {
                    log_warning("Client disconnected during WRITE session");
                    // Clean up session if still active
                    if (write_session.active) {
                        lock_release(&lock_table, write_session.filename, 
                                   write_session.sentence_num, write_session.client_id);
                        free_words(write_session.words, write_session.word_count);
                        write_session.active = 0;
                    }
                    break;
                }
                
                log_info("Received WRITE operation: %s", get_operation_name(write_msg.operation));
                
                if (write_msg.operation == OP_WRITE_UPDATE) {
                    handle_write_session(client_socket, &write_msg);
                } else if (write_msg.operation == OP_WRITE_COMMIT) {
                    handle_write_session(client_socket, &write_msg);
                    // WRITE_COMMIT ends the session
                    break;
                } else {
                    log_warning("Unexpected operation during WRITE session: %d", write_msg.operation);
                    break;
                }
            }
        } else {
            // Handle other operations normally (single message per connection)
            switch (msg.operation) {
                case OP_READ:
                    handle_read_from_client(client_socket, &msg);
                    break;
                    
                case OP_WRITE_UPDATE:
                case OP_WRITE_COMMIT:
                    // These should only come after WRITE_START
                    log_warning("Received %s without WRITE_START", get_operation_name(msg.operation));
                    Message response;
                    INIT_MESSAGE(response);
                    response.operation = OP_ACK;
                    response.error_code = ERR_BAD_REQUEST;
                    strcpy(response.content, "No active WRITE session");
                    send_message(client_socket, &response);
                    break;
                    
                case OP_UNDO:
                    handle_undo_request(client_socket, &msg);
                    break;
                    
                case OP_STREAM:
                    handle_stream_request(client_socket, &msg);
                    break;
                    
                case OP_CHECKPOINT:
                    handle_checkpoint_request(&msg, client_socket);
                    break;
                    
                case OP_VIEWCHECKPOINT:
                    handle_viewcheckpoint_request(&msg, client_socket);
                    break;
                    
                case OP_REVERT:
                    handle_revert_request(&msg, client_socket);
                    break;
                    
                case OP_LISTCHECKPOINTS:
                    handle_listcheckpoints_request(&msg, client_socket);
                    break;
                    
                default:
                    log_warning("Unknown operation from client: %d", msg.operation);
                    Message err_response;
                    INIT_MESSAGE(err_response);
                    err_response.operation = OP_ACK;
                    err_response.error_code = ERR_NOT_IMPLEMENTED;
                    strcpy(err_response.content, "Operation not implemented");
                    send_message(client_socket, &err_response);
                    break;
            }
        }
        
        close_socket(client_socket);
        log_info("Client connection closed");
    }
    
    return NULL;
}

// Background thread to cleanup expired locks
void* lock_cleanup_thread(void* arg) {
    (void)arg;
    log_info("Lock cleanup thread started");
    
    while (1) {
        sleep(30);  // Check every 30 seconds
        lock_cleanup_expired(&lock_table);
    }
    
    return NULL;
}


/* ============================================================================
 * MAIN (Modified for Correct Order and SO_REUSEADDR)
 * ============================================================================ */

int main(int argc, char* argv[]) {
    pthread_t ns_thread, client_thread, cleanup_thread;
    int opt = 1; // Used for SO_REUSEADDR option
    
    // Parse command-line arguments
    if (argc >= 4) {
        // Format: ./storage_server <ns_ip> <nm_port> <client_port> [storage_dir]
        strncpy(NS_IP, argv[1], MAX_IP_LEN - 1);
        SS_NM_PORT = atoi(argv[2]);
        SS_CLIENT_PORT = atoi(argv[3]);
        
        if (argc >= 5) {
            strncpy(STORAGE_DIR, argv[4], MAX_PATH_LEN - 1);
        } else {
            // Auto-generate storage directory based on port
            snprintf(STORAGE_DIR, MAX_PATH_LEN, "./storage_%d", SS_NM_PORT);
        }
        
        printf("=============================================================\n");
        printf("Storage Server - Custom Configuration\n");
        printf("=============================================================\n");
        printf("Name Server IP:       %s:%d\n", NS_IP, NS_PORT);
        printf("NS Communication:     Port %d\n", SS_NM_PORT);
        printf("Client Communication: Port %d\n", SS_CLIENT_PORT);
        printf("Storage Directory:    %s\n", STORAGE_DIR);
        printf("=============================================================\n");
    } else {
        printf("=============================================================\n");
        printf("Storage Server - Default Configuration\n");
        printf("=============================================================\n");
        printf("Name Server IP:       %s:%d\n", NS_IP, NS_PORT);
        printf("NS Communication:     Port %d\n", SS_NM_PORT);
        printf("Client Communication: Port %d\n", SS_CLIENT_PORT);
        printf("Storage Directory:    %s\n", STORAGE_DIR);
        printf("=============================================================\n");
        printf("Usage: %s <ns_ip> <nm_port> <client_port> [storage_dir]\n", argv[0]);
        printf("Example: %s 192.168.1.187 9081 9082 ./storage_9081\n", argv[0]);
        printf("=============================================================\n\n");
    }
    
    // Initialize logger and prerequisites
    char log_filename[512];
    snprintf(log_filename, sizeof(log_filename), "logs/storage_server_%d.log", SS_NM_PORT);
    init_logger("SS", log_filename);
    log_info("============================================================");
    log_info("Storage Server Starting - Docs++ Distributed Document System");
    log_info("============================================================");
    log_info("Configuration: NS_IP=%s NS_PORT=%d SS_NM_PORT=%d SS_CLIENT_PORT=%d", 
             NS_IP, NS_PORT, SS_NM_PORT, SS_CLIENT_PORT);
    log_info("Storage Directory: %s", STORAGE_DIR);
    
    // Initialize WRITE components
    lock_table_init(&lock_table);
    undo_table_init(&undo_table);  // Changed from undo_buffer_init
    
    get_local_ip(local_ip);
    log_info("Local IP: %s", local_ip);
    
    if (ensure_storage_dir() < 0) {
        log_critical("Failed to create storage directory");
        return 1;
    }
    log_info("Storage directory: %s", STORAGE_DIR);
    
    // Scan for existing files
    char existing_files[100][MAX_FILENAME_LEN];
    int file_count = scan_storage_files(existing_files, 100);
    log_info("Found %d existing files in storage", file_count);
    
    // --------------------------------------------------------------------------
    // STEP 1: Setup NS Inbound Listener (SS_NM_PORT: 8081)
    // --------------------------------------------------------------------------
    nm_server_socket = create_socket();
    if (nm_server_socket < 0) {
        log_critical("Failed to create NS listener socket");
        return 1;
    }
    
    // FIX: Set SO_REUSEADDR
    if (setsockopt(nm_server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        log_warning("setsockopt(SO_REUSEADDR) failed for NS listener");
    }
    
    if (bind_socket(nm_server_socket, SS_NM_PORT) < 0) {
        log_critical("Failed to bind NS listener socket to port %d", SS_NM_PORT);
        close_socket(nm_server_socket);
        return 1;
    }
    
    if (listen_socket(nm_server_socket, 10) < 0) {
        log_critical("Failed to listen on NS listener socket");
        close_socket(nm_server_socket);
        return 1;
    }
    log_info("NS listener listening on port %d", SS_NM_PORT);

    // --------------------------------------------------------------------------
    // STEP 2: Setup Client Listener (SS_CLIENT_PORT: 8082)
    // --------------------------------------------------------------------------
    client_server_socket = create_socket();
    if (client_server_socket < 0) {
        log_critical("Failed to create client server socket");
        return 1;
    }

    // FIX: Set SO_REUSEADDR
    if (setsockopt(client_server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        log_warning("setsockopt(SO_REUSEADDR) failed for Client listener");
    }
    
    if (bind_socket(client_server_socket, SS_CLIENT_PORT) < 0) {
        log_critical("Failed to bind client server socket to port %d", SS_CLIENT_PORT);
        close_socket(client_server_socket);
        return 1;
    }
    
    if (listen_socket(client_server_socket, 10) < 0) {
        log_critical("Failed to listen on client server socket");
        close_socket(client_server_socket);
        return 1;
    }
    log_info("Client server listening on port %d", SS_CLIENT_PORT);
    
    // --------------------------------------------------------------------------
    // STEP 3: Register with Name Server (Now both listener ports are ready)
    // --------------------------------------------------------------------------
    if (register_with_nameserver(existing_files, file_count) < 0) {
        log_critical("Failed to register with Name Server");
        return 1;
    }
    
    // --------------------------------------------------------------------------
    // STEP 4: Start Inbound Threads (Start listeners AFTER registration)
    // --------------------------------------------------------------------------

    // Start NS Inbound Thread
    if (pthread_create(&ns_thread, NULL, ns_inbound_thread, NULL) != 0) {
        log_critical("Failed to create NS inbound thread");
        return 1;
    }
    log_info("NS inbound thread created (listening on %d)", SS_NM_PORT);
    
    // Start client server thread
    if (pthread_create(&client_thread, NULL, client_server_thread, NULL) != 0) {
        log_critical("Failed to create client server thread");
        return 1;
    }
    log_info("Client server thread created (listening on %d)", SS_CLIENT_PORT);
    
    // Start lock cleanup thread
    if (pthread_create(&cleanup_thread, NULL, lock_cleanup_thread, NULL) != 0) {
        log_warning("Failed to create lock cleanup thread (non-critical)");
    } else {
        log_info("Lock cleanup thread created");
    }
    
    log_info("============================================================");
    log_info("Storage Server running and ready to serve requests");
    log_info("============================================================");
    
    pthread_join(ns_thread, NULL);
    pthread_join(client_thread, NULL);
    
    // Cleanup
    lock_table_destroy(&lock_table);
    undo_table_destroy(&undo_table);  // Changed from undo_buffer_destroy
    close_socket(nm_server_socket);
    close_socket(client_server_socket);
    close_logger();
    
    return 0;
}