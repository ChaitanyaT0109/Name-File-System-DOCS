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
#include <pthread.h>
#include <sys/select.h>
#include <sys/socket.h> // Required for setsockopt
#include <errno.h>
#include "socket_utils.h"
#include "logger.h"
#include "protocol.h"
#include "acl.h"
#include "access_requests.h"

#define NS_PORT 8080
#define MAX_CONNECTIONS 50
#define MAX_FILES 1000
#define HASH_TABLE_SIZE 1009  // Prime number for better distribution
#define ACL_DATA_FILE "acl_data.db"  // Persistent ACL storage (in current working directory)

/* ============================================================================
 * INPUT VALIDATION
 * ============================================================================ */

// Check if string contains invalid characters for ACL persistence
int contains_invalid_chars(const char* str) {
    if (str == NULL) return 1;
    while (*str) {
        if (*str == '|' || *str == ',') {
            return 1;  // Contains delimiter characters
        }
        str++;
    }
    return 0;
}

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
 * LRU CACHE FOR EFFICIENT SEARCH
 * ============================================================================ */

#define LRU_CACHE_SIZE 100

typedef struct LRUNode {
    char filename[MAX_FILENAME_LEN];
    int ss_index;
    struct LRUNode* prev;
    struct LRUNode* next;
} LRUNode;

typedef struct {
    LRUNode* head;  // Most recently used
    LRUNode* tail;  // Least recently used
    int size;
    int hits;
    int misses;
} LRUCache;

LRUCache lru_cache;

/* ============================================================================
 * ACCESS CONTROL LIST (ACL)
 * ============================================================================ */

AclTable acl_table;

/* ============================================================================
 * GLOBAL REGISTRY
 * ============================================================================ */

ExtendedSSInfo storage_servers[MAX_STORAGE_SERVERS];
int ss_count = 0;

ClientInfo clients[MAX_CONCURRENT_CLIENTS];
int client_count = 0;

/* ============================================================================
 * REPLICATION MAPPING (Fault Tolerance Bonus)
 * ============================================================================ */

typedef struct {
    char filename[MAX_FILENAME_LEN];
    int primary_ss;      // Primary storage server index
    int replica_ss;      // Backup storage server index (-1 if none)
} ReplicationEntry;

ReplicationEntry replication_map[MAX_FILES];
int replication_count = 0;

// Find replica for a file
int find_replica_ss(const char* filename) {
    for (int i = 0; i < replication_count; i++) {
        if (strcmp(replication_map[i].filename, filename) == 0) {
            return replication_map[i].replica_ss;
        }
    }
    return -1;
}

// Add replication entry
void add_replication_entry(const char* filename, int primary, int replica) {
    if (replication_count >= MAX_FILES) return;
    
    // Check if entry exists
    for (int i = 0; i < replication_count; i++) {
        if (strcmp(replication_map[i].filename, filename) == 0) {
            replication_map[i].replica_ss = replica;
            return;
        }
    }
    
    // Add new entry
    strncpy(replication_map[replication_count].filename, filename, MAX_FILENAME_LEN - 1);
    replication_map[replication_count].primary_ss = primary;
    replication_map[replication_count].replica_ss = replica;
    replication_count++;
}

/* ============================================================================
 * HELPER FUNCTIONS
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
    
    // Initialize LRU Cache
    memset(&lru_cache, 0, sizeof(LRUCache));
    lru_cache.head = NULL;
    lru_cache.tail = NULL;
    lru_cache.size = 0;
    lru_cache.hits = 0;
    lru_cache.misses = 0;
    
    // Initialize ACL
    acl_init(&acl_table);
    
    // Try to load ACL from persistent storage
    if (acl_load(&acl_table, ACL_DATA_FILE) == 0) {
        log_info("ACL data loaded from %s", ACL_DATA_FILE);
        acl_stats(&acl_table);
    } else {
        log_info("Starting with empty ACL (no saved data)");
    }
    
    // Initialize Access Requests System
    request_init();
    log_info("Access request system initialized");
}

/* ============================================================================
 * LRU CACHE IMPLEMENTATION
 * ============================================================================ */

// Move node to front (most recently used)
void lru_move_to_front(LRUNode* node) {
    if (node == lru_cache.head) return;
    
    // Remove from current position
    if (node->prev) node->prev->next = node->next;
    if (node->next) node->next->prev = node->prev;
    if (node == lru_cache.tail) lru_cache.tail = node->prev;
    
    // Move to front
    node->prev = NULL;
    node->next = lru_cache.head;
    if (lru_cache.head) lru_cache.head->prev = node;
    lru_cache.head = node;
    if (!lru_cache.tail) lru_cache.tail = node;
}

// Get from cache (returns ss_index or -1)
int lru_get(const char* filename) {
    LRUNode* current = lru_cache.head;
    while (current) {
        if (strcmp(current->filename, filename) == 0) {
            lru_cache.hits++;
            lru_move_to_front(current);
            return current->ss_index;
        }
        current = current->next;
    }
    lru_cache.misses++;
    return -1;
}

// Add to cache
void lru_put(const char* filename, int ss_index) {
    // Check if already exists
    LRUNode* current = lru_cache.head;
    while (current) {
        if (strcmp(current->filename, filename) == 0) {
            current->ss_index = ss_index;
            lru_move_to_front(current);
            return;
        }
        current = current->next;
    }
    
    // Create new node
    LRUNode* node = (LRUNode*)malloc(sizeof(LRUNode));
    if (!node) return;
    
    strncpy(node->filename, filename, MAX_FILENAME_LEN - 1);
    node->filename[MAX_FILENAME_LEN - 1] = '\0';
    node->ss_index = ss_index;
    node->prev = NULL;
    node->next = lru_cache.head;
    
    if (lru_cache.head) lru_cache.head->prev = node;
    lru_cache.head = node;
    if (!lru_cache.tail) lru_cache.tail = node;
    
    lru_cache.size++;
    
    // Evict LRU if over capacity
    if (lru_cache.size > LRU_CACHE_SIZE) {
        LRUNode* old = lru_cache.tail;
        lru_cache.tail = old->prev;
        if (lru_cache.tail) lru_cache.tail->next = NULL;
        else lru_cache.head = NULL;
        free(old);
        lru_cache.size--;
    }
}

// Invalidate cache entry
void lru_invalidate(const char* filename) {
    LRUNode* current = lru_cache.head;
    while (current) {
        if (strcmp(current->filename, filename) == 0) {
            if (current->prev) current->prev->next = current->next;
            if (current->next) current->next->prev = current->prev;
            if (current == lru_cache.head) lru_cache.head = current->next;
            if (current == lru_cache.tail) lru_cache.tail = current->prev;
            free(current);
            lru_cache.size--;
            return;
        }
        current = current->next;
    }
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
    
    // Invalidate cache entry (file mapping changed)
    lru_invalidate(filename);
    
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
    
    // Invalidate cache entry
    lru_invalidate(filename);
    
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
 * FILE LOOKUP FUNCTIONS (LRU Cache + HashMap)
 * ============================================================================ */

int find_ss_for_file(const char* filename) {
    // Check LRU cache first
    int cached = lru_get(filename);
    if (cached >= 0 && storage_servers[cached].base.is_alive) return cached;
    
    // Cache miss - check HashMap
    int ss_index = hashmap_find(filename);
    
    // Failover: If primary SS is dead, try replica
    if (ss_index >= 0 && !storage_servers[ss_index].base.is_alive) {
        int replica = find_replica_ss(filename);
        if (replica >= 0 && storage_servers[replica].base.is_alive) {
            log_info("Failover: Using replica SS[%d] for file '%s' (primary SS[%d] is down)", 
                     replica, filename, ss_index);
            ss_index = replica;
        } else {
            log_warning("File '%s' unavailable - primary SS[%d] is down and no replica available", 
                        filename, ss_index);
            return -1;
        }
    }
    
    if (ss_index >= 0) lru_put(filename, ss_index);
    
    return ss_index;
}

int find_available_ss() {
    static int last_used = -1;  // Track last used SS for round-robin
    
    if (ss_count == 0) return -1;
    
    // Round-robin: Try starting from next SS after last used
    int start_idx = (last_used + 1) % ss_count;
    
    for (int i = 0; i < ss_count; i++) {
        int idx = (start_idx + i) % ss_count;
        if (storage_servers[idx].base.is_alive) {
            last_used = idx;  // Remember this for next time
            log_info("Selected SS[%d] for new file (round-robin)", idx);
            return idx;
        }
    }
    
    return -1;  // No alive storage servers
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

// Helper to check if a user is registered
int user_exists(const char* username) {
    for (int i = 0; i < client_count; i++) {
        if (strcmp(clients[i].username, username) == 0) {
            return 1;  // User found
        }
    }
    return 0;  // User not found
}

// Helper to mark a client as disconnected by IP and port
void mark_client_disconnected(const char* client_ip, int client_port) {
    for (int i = 0; i < client_count; i++) {
        if (clients[i].is_active && 
            strcmp(clients[i].ip, client_ip) == 0 && 
            clients[i].port == client_port) {
            clients[i].is_active = 0;
            time_t session_duration = time(NULL) - clients[i].connected_time;
            log_info("Client '%s' disconnected from %s:%d (session duration: %ld seconds)", 
                     clients[i].username, client_ip, client_port, session_duration);
            return;
        }
    }
}

/* ============================================================================
 * REGISTRATION HANDLERS (Modified for file sync)
 * ============================================================================ */

void handle_ss_registration(int ss_socket, Message* msg) {
    // Check if this is a reconnecting SS (recovery)
    int existing_ss = -1;
    for (int i = 0; i < ss_count; i++) {
        if (strcmp(storage_servers[i].base.ip, msg->ip_address) == 0 &&
            storage_servers[i].base.nm_port == msg->nm_port) {
            existing_ss = i;
            break;
        }
    }
    
    if (existing_ss >= 0) {
        // SS Recovery: Reconnecting after failure
        log_info("Storage Server SS[%d] reconnecting (recovery)", existing_ss);
        storage_servers[existing_ss].base.is_alive = 1;
        storage_servers[existing_ss].base.last_heartbeat = time(NULL);
        storage_servers[existing_ss].base.client_port = msg->client_port;
        
        // Rescan files from recovered SS
        char temp_content[MAX_CONTENT_LEN];
        strncpy(temp_content, msg->content, MAX_CONTENT_LEN - 1);
        temp_content[MAX_CONTENT_LEN - 1] = '\0';
        
        char* token = strtok(temp_content, "|");
        int files_resynced = 0;
        
        while (token != NULL && files_resynced < MAX_FILES) {
            strncpy(storage_servers[existing_ss].files[files_resynced], token, MAX_FILENAME_LEN - 1);
            storage_servers[existing_ss].files[files_resynced][MAX_FILENAME_LEN - 1] = '\0';
            hashmap_add(token, existing_ss);
            files_resynced++;
            token = strtok(NULL, "|");
        }
        
        storage_servers[existing_ss].file_count = files_resynced;
        
        Message response;
        INIT_MESSAGE(response);
        response.operation = OP_ACK;
        response.error_code = ERR_SUCCESS;
        snprintf(response.content, sizeof(response.content), 
                 "Storage Server recovered as SS[%d] with %d files", 
                 existing_ss, files_resynced);
        send_message(ss_socket, &response);
        
        log_info("SS[%d] recovery complete - resynced %d files", existing_ss, files_resynced);
        return;
    }
    
    // New SS registration
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
    
    // Process the file list sent in the content field
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
    // Validate username for special characters
    if (contains_invalid_chars(msg->sender_id)) {
        log_warning("Invalid username '%s' - contains | or ,", msg->sender_id);
        Message response;
        INIT_MESSAGE(response);
        response.operation = OP_ACK;
        response.error_code = ERR_BAD_REQUEST;
        snprintf(response.content, sizeof(response.content), 
                 "Invalid username: cannot contain | or , characters");
        send_message(client_socket, &response);
        return;
    }
    
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
 * REQUEST HANDLERS
 * ============================================================================ */

void handle_create_request(int client_socket, Message* msg) {
    log_info("CREATE request from %s for file '%s'", msg->sender_id, msg->filename);
    
    // Validate filename for special characters
    if (contains_invalid_chars(msg->filename)) {
        log_warning("Invalid filename '%s' - contains | or ,", msg->filename);
        Message response;
        INIT_MESSAGE(response);
        response.operation = OP_ACK;
        response.error_code = ERR_BAD_REQUEST;
        snprintf(response.content, sizeof(response.content), 
                 "Invalid filename: cannot contain | or , characters");
        send_message(client_socket, &response);
        return;
    }
    
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
        
        // Add to ACL with creator as owner
        if (acl_add_file(&acl_table, msg->filename, msg->sender_id) == 0) {
            log_info("File '%s' added to ACL with owner '%s'", msg->filename, msg->sender_id);
            // Persist ACL changes
            acl_save(&acl_table, ACL_DATA_FILE);
        }
        
        log_info("File '%s' created successfully on SS[%d]", msg->filename, ss_index);
        
        // Replication: Find backup SS and replicate asynchronously
        if (ss_count > 1) {
            int replica_ss = find_available_ss();
            if (replica_ss >= 0 && replica_ss != ss_index) {
                add_replication_entry(msg->filename, ss_index, replica_ss);
                log_info("Replication: File '%s' will be replicated to SS[%d]", 
                         msg->filename, replica_ss);
                
                // Send async replication request (don't wait for response)
                int rep_socket = create_socket();
                if (rep_socket >= 0) {
                    if (connect_to_server(rep_socket, storage_servers[replica_ss].base.ip,
                                         storage_servers[replica_ss].base.nm_port) == 0) {
                        Message rep_msg;
                        INIT_MESSAGE(rep_msg);
                        rep_msg.operation = OP_REPLICATE;
                        strncpy(rep_msg.filename, msg->filename, MAX_FILENAME_LEN - 1);
                        strncpy(rep_msg.sender_id, msg->sender_id, MAX_USERNAME_LEN - 1);
                        send_message(rep_socket, &rep_msg);
                        add_file_to_ss(replica_ss, msg->filename);
                    }
                    close_socket(rep_socket);
                }
            }
        }
    }
    
    send_message(client_socket, &ss_response);
}

void handle_read_request(int client_socket, Message* msg) {
    log_info("READ request from %s for file '%s'", msg->sender_id, msg->filename);
    
    // FIRST: Check if file exists (before checking permissions)
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
    
    // SECOND: Check if user has read access
    if (!acl_check_read(&acl_table, msg->filename, msg->sender_id)) {
        log_warning("Access denied: %s attempted to read '%s'", msg->sender_id, msg->filename);
        Message response;
        INIT_MESSAGE(response);
        response.operation = OP_ACK;
        response.error_code = ERR_ACCESS_DENIED;
        snprintf(response.content, sizeof(response.content),
                 "Access denied: You don't have permission to read '%s'", msg->filename);
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
    
    // Check if user has write access (needed for delete)
    if (!acl_check_write(&acl_table, msg->filename, msg->sender_id)) {
        log_warning("Access denied: %s attempted to delete '%s'", msg->sender_id, msg->filename);
        Message response;
        INIT_MESSAGE(response);
        response.operation = OP_ACK;
        response.error_code = ERR_ACCESS_DENIED;
        snprintf(response.content, sizeof(response.content),
                 "Access denied: You don't have permission to delete '%s'", msg->filename);
        send_message(client_socket, &response);
        return;
    }
    
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
        
        // Remove from ACL
        if (acl_remove_file(&acl_table, msg->filename) == 0) {
            log_info("Removed file '%s' from ACL", msg->filename);
            // Persist ACL changes
            acl_save(&acl_table, ACL_DATA_FILE);
        }
        
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

void handle_write_request(int client_socket, Message* msg) {
    log_info("WRITE request from %s for file '%s', sentence %d", 
             msg->sender_id, msg->filename, msg->sentence_num);
    
    // Check if file exists
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
    
    // Check if user has write access
    if (!acl_check_write(&acl_table, msg->filename, msg->sender_id)) {
        log_warning("Access denied: %s attempted to write '%s'", msg->sender_id, msg->filename);
        Message response;
        INIT_MESSAGE(response);
        response.operation = OP_ACK;
        response.error_code = ERR_ACCESS_DENIED;
        snprintf(response.content, sizeof(response.content),
                 "Access denied: You don't have permission to write '%s'", msg->filename);
        send_message(client_socket, &response);
        return;
    }
    
    // Return SS routing info (client will connect directly to SS)
    log_info("Routing WRITE to SS[%d] at %s:%d", 
             ss_index, storage_servers[ss_index].base.ip, 
             storage_servers[ss_index].base.client_port);
    
    Message response;
    INIT_MESSAGE(response);
    response.operation = OP_ROUTE_INFO;
    response.error_code = ERR_SUCCESS;
    strncpy(response.ss_ip, storage_servers[ss_index].base.ip, MAX_IP_LEN - 1);
    response.ss_port = storage_servers[ss_index].base.client_port;
    snprintf(response.content, sizeof(response.content),
             "Connect to SS at %s:%d for WRITE", response.ss_ip, response.ss_port);
    
    send_message(client_socket, &response);
}

void handle_undo_request(int client_socket, Message* msg) {
    log_info("UNDO request from %s for file '%s'", msg->sender_id, msg->filename);
    
    // Check if file exists
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
    
    // Check if user has write access
    if (!acl_check_write(&acl_table, msg->filename, msg->sender_id)) {
        log_warning("Access denied: %s attempted to undo '%s'", msg->sender_id, msg->filename);
        Message response;
        INIT_MESSAGE(response);
        response.operation = OP_ACK;
        response.error_code = ERR_ACCESS_DENIED;
        snprintf(response.content, sizeof(response.content),
                 "Access denied: You don't have permission to undo '%s'", msg->filename);
        send_message(client_socket, &response);
        return;
    }
    
    // Return SS routing info (client will connect directly to SS)
    log_info("Routing UNDO to SS[%d]", ss_index);
    
    Message response;
    INIT_MESSAGE(response);
    response.operation = OP_ROUTE_INFO;
    response.error_code = ERR_SUCCESS;
    strncpy(response.ss_ip, storage_servers[ss_index].base.ip, MAX_IP_LEN - 1);
    response.ss_port = storage_servers[ss_index].base.client_port;
    snprintf(response.content, sizeof(response.content),
             "Connect to SS at %s:%d for UNDO", response.ss_ip, response.ss_port);
    
    send_message(client_socket, &response);
}

void handle_stream_request(int client_socket, Message* msg) {
    log_info("STREAM request from %s for file '%s'", msg->sender_id, msg->filename);
    
    // Check if file exists
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
    
    // Check if user has read access
    if (!acl_check_read(&acl_table, msg->filename, msg->sender_id)) {
        log_warning("Access denied: %s attempted to stream '%s'", msg->sender_id, msg->filename);
        Message response;
        INIT_MESSAGE(response);
        response.operation = OP_ACK;
        response.error_code = ERR_ACCESS_DENIED;
        snprintf(response.content, sizeof(response.content),
                 "Access denied: You don't have permission to read '%s'", msg->filename);
        send_message(client_socket, &response);
        return;
    }
    
    // Return SS routing info (client will connect directly to SS)
    log_info("Routing STREAM to SS[%d] at %s:%d", 
             ss_index, storage_servers[ss_index].base.ip, 
             storage_servers[ss_index].base.client_port);
    
    Message response;
    INIT_MESSAGE(response);
    response.operation = OP_ROUTE_INFO;
    response.error_code = ERR_SUCCESS;
    strncpy(response.ss_ip, storage_servers[ss_index].base.ip, MAX_IP_LEN - 1);
    response.ss_port = storage_servers[ss_index].base.client_port;
    snprintf(response.content, sizeof(response.content),
             "Connect to SS at %s:%d for STREAM", response.ss_ip, response.ss_port);
    
    send_message(client_socket, &response);
}

void handle_exec_request(int client_socket, Message* msg) {
    log_info("EXEC request from %s for file '%s'", msg->sender_id, msg->filename);
    
    // Check if file exists
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
    
    // Check if user has read access
    if (!acl_check_read(&acl_table, msg->filename, msg->sender_id)) {
        log_warning("Access denied: %s attempted to exec '%s'", msg->sender_id, msg->filename);
        Message response;
        INIT_MESSAGE(response);
        response.operation = OP_ACK;
        response.error_code = ERR_ACCESS_DENIED;
        snprintf(response.content, sizeof(response.content),
                 "Access denied: You don't have permission to read '%s'", msg->filename);
        send_message(client_socket, &response);
        return;
    }
    
    // Request file content from SS
    int ss_socket = create_socket();
    if (ss_socket < 0 || connect_to_server(ss_socket, storage_servers[ss_index].base.ip, 
                                            storage_servers[ss_index].base.nm_port) < 0) {
        log_error("Failed to connect to SS for EXEC");
        Message response;
        INIT_MESSAGE(response);
        response.operation = OP_ACK;
        response.error_code = ERR_SS_UNAVAILABLE;
        strcpy(response.content, "Storage server unavailable");
        send_message(client_socket, &response);
        if (ss_socket >= 0) close_socket(ss_socket);
        return;
    }
    
    // Request READ from SS
    Message read_req;
    INIT_MESSAGE(read_req);
    read_req.operation = OP_READ;
    strncpy(read_req.filename, msg->filename, MAX_FILENAME_LEN - 1);
    send_message(ss_socket, &read_req);
    
    Message ss_response;
    if (receive_message(ss_socket, &ss_response) <= 0 || ss_response.error_code != ERR_SUCCESS) {
        close_socket(ss_socket);
        Message response;
        INIT_MESSAGE(response);
        response.operation = OP_ACK;
        response.error_code = ERR_SERVER_ERROR;
        strcpy(response.content, "Failed to read file from SS");
        send_message(client_socket, &response);
        return;
    }
    
    close_socket(ss_socket);
    
    // Execute file content as command
    log_info("Executing content of file '%s'", msg->filename);
    
    char command[MAX_CONTENT_LEN];
    strncpy(command, ss_response.content, MAX_CONTENT_LEN - 1);
    command[MAX_CONTENT_LEN - 1] = '\0';
    
    // Use popen to capture output
    FILE* fp = popen(command, "r");
    if (fp == NULL) {
        log_error("Failed to execute command");
        Message response;
        INIT_MESSAGE(response);
        response.operation = OP_ACK;
        response.error_code = ERR_SERVER_ERROR;
        strcpy(response.content, "Failed to execute command");
        send_message(client_socket, &response);
        return;
    }
    
    // Read command output
    char output[MAX_CONTENT_LEN] = "";
    char line[256];
    size_t total_len = 0;
    
    while (fgets(line, sizeof(line), fp) != NULL) {
        size_t line_len = strlen(line);
        if (total_len + line_len < MAX_CONTENT_LEN - 1) {
            strcat(output, line);
            total_len += line_len;
        } else {
            break;  // Buffer full
        }
    }
    
    int exit_status = pclose(fp);
    
    // Send result to client
    Message response;
    INIT_MESSAGE(response);
    response.operation = OP_ACK;
    
    if (exit_status == 0) {
        response.error_code = ERR_SUCCESS;
        if (strlen(output) > 0) {
            strncpy(response.content, output, MAX_CONTENT_LEN - 1);
        } else {
            strcpy(response.content, "(command produced no output)");
        }
    } else {
        response.error_code = ERR_SERVER_ERROR;
        snprintf(response.content, sizeof(response.content),
                 "Command failed with exit code %d. Output: %s", 
                 exit_status, strlen(output) > 0 ? output : "(none)");
    }
    
    send_message(client_socket, &response);
    log_info("EXEC completed with status %d", exit_status);
}

/* ============================================================================
 * ACCESS CONTROL COMMAND HANDLERS
 * ============================================================================ */

void handle_addaccess_request(int client_socket, Message* msg) {
    log_info("ADDACCESS request from %s for file '%s', target user '%s', access type %d",
             msg->sender_id, msg->filename, msg->target_user, msg->access_type);
    
    Message response;
    INIT_MESSAGE(response);
    response.operation = OP_ACK;
    
    // Validate username for special characters
    if (contains_invalid_chars(msg->target_user)) {
        log_warning("Invalid username '%s' - contains | or ,", msg->target_user);
        response.error_code = ERR_BAD_REQUEST;
        snprintf(response.content, sizeof(response.content), 
                 "Invalid username: cannot contain | or , characters");
        send_message(client_socket, &response);
        return;
    }
    
    // Check if target user exists (is registered)
    if (!user_exists(msg->target_user)) {
        log_warning("User '%s' does not exist - cannot grant access", msg->target_user);
        response.error_code = ERR_BAD_REQUEST;
        snprintf(response.content, sizeof(response.content), 
                 "User '%s' does not exist. User must be registered to grant access.", 
                 msg->target_user);
        send_message(client_socket, &response);
        return;
    }
    
    // Check if requester is the owner
    if (!acl_check_owner(&acl_table, msg->filename, msg->sender_id)) {
        log_warning("Access denied: %s is not owner of '%s'", msg->sender_id, msg->filename);
        response.error_code = ERR_ACCESS_DENIED;
        snprintf(response.content, sizeof(response.content),
                 "Access denied: Only the owner can grant access to '%s'", msg->filename);
        send_message(client_socket, &response);
        return;
    }
    
    // Validate access type
    if (msg->access_type != ACCESS_READ && msg->access_type != ACCESS_WRITE) {
        log_warning("Invalid access type: %d", msg->access_type);
        response.error_code = ERR_BAD_REQUEST;
        strcpy(response.content, "Invalid access type. Use -R for read or -W for write");
        send_message(client_socket, &response);
        return;
    }
    
    // Add access
    if (acl_add_access(&acl_table, msg->filename, msg->target_user, msg->access_type) < 0) {
        log_error("Failed to add access for user '%s' to file '%s'", 
                  msg->target_user, msg->filename);
        response.error_code = ERR_SERVER_ERROR;
        snprintf(response.content, sizeof(response.content),
                 "Failed to grant access to '%s'", msg->target_user);
        send_message(client_socket, &response);
        return;
    }
    
    // Persist changes
    acl_save(&acl_table, ACL_DATA_FILE);
    
    response.error_code = ERR_SUCCESS;
    snprintf(response.content, sizeof(response.content),
             "Granted %s access to '%s' for user '%s'",
             msg->access_type == ACCESS_READ ? "READ" : "WRITE",
             msg->filename, msg->target_user);
    send_message(client_socket, &response);
    
    log_info("Successfully granted access to '%s' for '%s'", msg->target_user, msg->filename);
}

void handle_remaccess_request(int client_socket, Message* msg) {
    log_info("REMACCESS request from %s for file '%s', target user '%s'",
             msg->sender_id, msg->filename, msg->target_user);
    
    Message response;
    INIT_MESSAGE(response);
    response.operation = OP_ACK;
    
    // Check if requester is the owner
    if (!acl_check_owner(&acl_table, msg->filename, msg->sender_id)) {
        log_warning("Access denied: %s is not owner of '%s'", msg->sender_id, msg->filename);
        response.error_code = ERR_ACCESS_DENIED;
        snprintf(response.content, sizeof(response.content),
                 "Access denied: Only the owner can revoke access to '%s'", msg->filename);
        send_message(client_socket, &response);
        return;
    }
    
    // Remove access
    if (acl_remove_access(&acl_table, msg->filename, msg->target_user) < 0) {
        log_warning("User '%s' had no access to file '%s'", msg->target_user, msg->filename);
        response.error_code = ERR_NOT_FOUND;
        snprintf(response.content, sizeof(response.content),
                 "User '%s' doesn't have access to '%s'", msg->target_user, msg->filename);
        send_message(client_socket, &response);
        return;
    }
    
    // Persist changes
    acl_save(&acl_table, ACL_DATA_FILE);
    
    response.error_code = ERR_SUCCESS;
    snprintf(response.content, sizeof(response.content),
             "Revoked access to '%s' for user '%s'", msg->filename, msg->target_user);
    send_message(client_socket, &response);
    
    log_info("Successfully revoked access for '%s' from '%s'", msg->target_user, msg->filename);
}

/* ============================================================================
 * METADATA COMMAND HANDLERS
 * ============================================================================ */

// Helper function for handle_list_request to check duplicates
static int is_user_duplicate(const char* username, char users[][MAX_USERNAME_LEN], int count) {
    for (int i = 0; i < count; i++) {
        if (strcmp(users[i], username) == 0) {
            return 1;
        }
    }
    return 0;
}

void handle_list_request(int client_socket, Message* msg) {
    log_info("LIST request from %s", msg->sender_id);
    
    Message response;
    INIT_MESSAGE(response);
    response.operation = OP_ACK;
    response.error_code = ERR_SUCCESS;
    
    // Get all users from ACL (persistent users who have created/accessed files)
    char acl_users[MAX_CONCURRENT_CLIENTS][MAX_USERNAME_LEN];
    int acl_user_count = acl_get_all_users(&acl_table, acl_users, MAX_CONCURRENT_CLIENTS);
    
    // Also include currently connected clients
    // Create a combined unique list
    char all_users[MAX_CONCURRENT_CLIENTS * 2][MAX_USERNAME_LEN];
    int total_users = 0;
    
    // Add ACL users
    for (int i = 0; i < acl_user_count; i++) {
        strncpy(all_users[total_users], acl_users[i], MAX_USERNAME_LEN - 1);
        all_users[total_users][MAX_USERNAME_LEN - 1] = '\0';
        total_users++;
    }
    
    // Add currently connected clients (if not already in list)
    for (int i = 0; i < client_count && total_users < MAX_CONCURRENT_CLIENTS * 2; i++) {
        if (clients[i].is_active && !is_user_duplicate(clients[i].username, all_users, total_users)) {
            strncpy(all_users[total_users], clients[i].username, MAX_USERNAME_LEN - 1);
            all_users[total_users][MAX_USERNAME_LEN - 1] = '\0';
            total_users++;
        }
    }
    
    // Build list
    char user_list[MAX_CONTENT_LEN] = "";
    int offset = 0;
    
    offset += snprintf(user_list + offset, sizeof(user_list) - offset,
                      "Users in system (%d):\n", total_users);
    
    for (int i = 0; i < total_users && offset < MAX_CONTENT_LEN - 100; i++) {
        offset += snprintf(user_list + offset, sizeof(user_list) - offset,
                         "  - %s\n", all_users[i]);
    }
    
    strncpy(response.content, user_list, sizeof(response.content) - 1);
    response.content[sizeof(response.content) - 1] = '\0';
    
    send_message(client_socket, &response);
    log_info("LIST: Sent %d users to %s", total_users, msg->sender_id);
}

void handle_view_request(int client_socket, Message* msg) {
    log_info("VIEW request from %s with flags: %d", msg->sender_id, msg->view_flags);
    
    Message response;
    INIT_MESSAGE(response);
    response.operation = OP_ACK;
    response.error_code = ERR_SUCCESS;
    
    char file_list[MAX_CONTENT_LEN] = "";
    int offset = 0;
    char files[MAX_FILES][MAX_FILENAME_LEN];
    int file_count;
    
    // Determine which files to show
    if (msg->view_flags & VIEW_FLAG_ALL) {
        // VIEW -a or VIEW -al: show all files
        file_count = acl_get_all_files(&acl_table, files, MAX_FILES);
        offset += snprintf(file_list + offset, sizeof(file_list) - offset,
                          "All files (%d):\n", file_count);
    } else {
        // VIEW or VIEW -l: show only accessible files
        file_count = acl_get_accessible_files(&acl_table, msg->sender_id, files, MAX_FILES);
        offset += snprintf(file_list + offset, sizeof(file_list) - offset,
                          "Accessible files (%d):\n", file_count);
    }
    
    // Check if we need metadata (-l flag)
    if (msg->view_flags & VIEW_FLAG_LONG) {
        // VIEW -l or VIEW -al: include metadata
        offset += snprintf(file_list + offset, sizeof(file_list) - offset,
                          "%-30s %-10s %-20s %-10s %-10s\n",
                          "Filename", "Size", "Modified", "Words", "Chars");
        offset += snprintf(file_list + offset, sizeof(file_list) - offset,
                          "----------------------------------------------------------------------------\n");
        
        for (int i = 0; i < file_count && offset < MAX_CONTENT_LEN - 200; i++) {
            // Find which SS has the file
            int ss_index = find_ss_for_file(files[i]);
            if (ss_index < 0) continue;
            
            // Request metadata from SS
            int ss_socket = create_socket();
            if (ss_socket < 0 || 
                connect_to_server(ss_socket, storage_servers[ss_index].base.ip,
                                 storage_servers[ss_index].base.nm_port) < 0) {
                log_warning("Failed to connect to SS for metadata");
                if (ss_socket >= 0) close_socket(ss_socket);
                continue;
            }
            
            Message metadata_req;
            INIT_MESSAGE(metadata_req);
            metadata_req.operation = OP_GET_METADATA;
            strncpy(metadata_req.filename, files[i], MAX_FILENAME_LEN - 1);
            
            if (send_message(ss_socket, &metadata_req) < 0) {
                close_socket(ss_socket);
                continue;
            }
            
            Message metadata_resp;
            if (receive_message(ss_socket, &metadata_resp) <= 0) {
                close_socket(ss_socket);
                continue;
            }
            
            close_socket(ss_socket);
            
            if (metadata_resp.error_code == ERR_SUCCESS) {
                // Parse metadata from content (format: "size|word_count|char_count|modified_time")
                long size;
                int word_count, char_count;
                time_t mod_time;
                
                if (sscanf(metadata_resp.content, "%ld|%d|%d|%ld",
                          &size, &word_count, &char_count, &mod_time) == 4) {
                    
                    char time_str[20];
                    struct tm* tm_info = localtime(&mod_time);
                    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M", tm_info);
                    
                    offset += snprintf(file_list + offset, sizeof(file_list) - offset,
                                      "%-30s %-10ld %-20s %-10d %-10d\n",
                                      files[i], size, time_str, word_count, char_count);
                }
            }
        }
    } else {
        // Simple list (no metadata)
        for (int i = 0; i < file_count && offset < MAX_CONTENT_LEN - 100; i++) {
            offset += snprintf(file_list + offset, sizeof(file_list) - offset,
                             "  - %s\n", files[i]);
        }
    }
    
    strncpy(response.content, file_list, sizeof(response.content) - 1);
    response.content[sizeof(response.content) - 1] = '\0';
    
    send_message(client_socket, &response);
    log_info("VIEW: Sent %d files to %s", file_count, msg->sender_id);
}

void handle_info_request(int client_socket, Message* msg) {
    log_info("INFO request from %s for file '%s'", msg->sender_id, msg->filename);
    
    Message response;
    INIT_MESSAGE(response);
    response.operation = OP_ACK;
    
    // Check if user has read access
    if (!acl_check_read(&acl_table, msg->filename, msg->sender_id)) {
        log_warning("Access denied: %s attempted INFO on '%s'", msg->sender_id, msg->filename);
        response.error_code = ERR_ACCESS_DENIED;
        snprintf(response.content, sizeof(response.content),
                 "Access denied: You don't have permission to view info for '%s'", msg->filename);
        send_message(client_socket, &response);
        return;
    }
    
    // Check if file exists
    int ss_index = find_ss_for_file(msg->filename);
    if (ss_index < 0) {
        log_warning("File '%s' not found", msg->filename);
        response.error_code = ERR_NOT_FOUND;
        snprintf(response.content, sizeof(response.content),
                 "File '%s' not found", msg->filename);
        send_message(client_socket, &response);
        return;
    }
    
    // Get ACL information
    char owner[MAX_USERNAME_LEN];
    char access_list[512];  // Reduced size to avoid truncation warning
    
    if (acl_get_owner(&acl_table, msg->filename, owner, sizeof(owner)) < 0) {
        strcpy(owner, "unknown");
    }
    
    if (acl_get_access_list(&acl_table, msg->filename, access_list, sizeof(access_list)) < 0) {
        strcpy(access_list, "No access information");
    }
    
    // Request metadata from SS
    int ss_socket = create_socket();
    if (ss_socket < 0 || 
        connect_to_server(ss_socket, storage_servers[ss_index].base.ip,
                         storage_servers[ss_index].base.nm_port) < 0) {
        log_error("Failed to connect to SS for metadata");
        response.error_code = ERR_SS_UNAVAILABLE;
        strcpy(response.content, "Failed to retrieve file metadata");
        send_message(client_socket, &response);
        if (ss_socket >= 0) close_socket(ss_socket);
        return;
    }
    
    Message metadata_req;
    INIT_MESSAGE(metadata_req);
    metadata_req.operation = OP_GET_METADATA;
    strncpy(metadata_req.filename, msg->filename, MAX_FILENAME_LEN - 1);
    
    if (send_message(ss_socket, &metadata_req) < 0 ||
        receive_message(ss_socket, &response) <= 0) {
        log_error("Failed to get metadata from SS");
        close_socket(ss_socket);
        response.error_code = ERR_SERVER_ERROR;
        strcpy(response.content, "Failed to retrieve file metadata");
        send_message(client_socket, &response);
        return;
    }
    
    close_socket(ss_socket);
    
    if (response.error_code != ERR_SUCCESS) {
        send_message(client_socket, &response);
        return;
    }
    
    // Parse metadata from SS response
    long size;
    int word_count, char_count;
    time_t created_time, mod_time, access_time;
    
    if (sscanf(response.content, "%ld|%d|%d|%ld|%ld|%ld",
              &size, &word_count, &char_count, &created_time, &mod_time, &access_time) == 6) {
        
        char created_str[30], modified_str[30], accessed_str[30];
        struct tm* tm_info;
        
        tm_info = localtime(&created_time);
        strftime(created_str, sizeof(created_str), "%Y-%m-%d %H:%M:%S", tm_info);
        
        tm_info = localtime(&mod_time);
        strftime(modified_str, sizeof(modified_str), "%Y-%m-%d %H:%M:%S", tm_info);
        
        tm_info = localtime(&access_time);
        strftime(accessed_str, sizeof(accessed_str), "%Y-%m-%d %H:%M:%S", tm_info);
        
        // Format comprehensive info
        snprintf(response.content, sizeof(response.content),
                "File Information: %s\n"
                "==========================================\n"
                "Owner:          %s\n"
                "Access:         %s\n"
                "Size:           %ld bytes\n"
                "Word Count:     %d\n"
                "Char Count:     %d\n"
                "Created:        %s\n"
                "Last Modified:  %s\n"
                "Last Accessed:  %s\n"
                "Location:       SS[%d] at %s:%d\n",
                msg->filename, owner, access_list, size, word_count, char_count,
                created_str, modified_str, accessed_str,
                ss_index, storage_servers[ss_index].base.ip,
                storage_servers[ss_index].base.client_port);
    }
    
    response.error_code = ERR_SUCCESS;
    send_message(client_socket, &response);
    log_info("INFO: Sent file info for '%s' to %s", msg->filename, msg->sender_id);
}

/* ============================================================================
 * ADDITIONAL FEATURES - Request Handlers
 * ============================================================================ */

// Handle CREATEFOLDER request (Bonus: Hierarchical folders)
void handle_createfolder_request(int client_socket, Message* msg) {
    log_info("CREATEFOLDER request from %s for folder '%s'", msg->sender_id, msg->folder_name);
    
    Message response;
    INIT_MESSAGE(response);
    response.operation = OP_ACK;
    
    // For folder creation, any registered user can create folders
    // No ACL check needed since folders are a new entity
    // Just need to pick an available storage server
    
    // Find any available storage server (use first one)
    int ss_index = -1;
    for (int i = 0; i < ss_count; i++) {
        if (storage_servers[i].base.is_alive) {
            ss_index = i;
            break;
        }
    }
    
    if (ss_index < 0) {
        log_error("No active storage servers available for CREATEFOLDER");
        response.error_code = ERR_SS_UNAVAILABLE;
        strcpy(response.content, "No storage servers available");
        send_message(client_socket, &response);
        return;
    }
    
    // Forward to storage server
    int ss_socket = create_socket();
    if (ss_socket < 0 ||
        connect_to_server(ss_socket, storage_servers[ss_index].base.ip,
                         storage_servers[ss_index].base.nm_port) < 0) {
        log_error("Failed to connect to SS for CREATEFOLDER");
        response.error_code = ERR_SS_UNAVAILABLE;
        strcpy(response.content, "Storage server unavailable");
        send_message(client_socket, &response);
        if (ss_socket >= 0) close_socket(ss_socket);
        return;
    }
    
    send_message(ss_socket, msg);
    receive_message(ss_socket, &response);
    close_socket(ss_socket);
    
    send_message(client_socket, &response);
    log_info("CREATEFOLDER: Forwarded to SS[%d], result: %s", 
             ss_index, get_error_message(response.error_code));
}

// Handle MOVE request (Bonus: Move files between folders)
void handle_move_request(int client_socket, Message* msg) {
    log_info("MOVE request from %s: '%s' -> '%s'", msg->sender_id, msg->filename, msg->folder_name);
    
    Message response;
    INIT_MESSAGE(response);
    response.operation = OP_ACK;
    
    // Check permissions
    if (!acl_is_owner(&acl_table, msg->filename, msg->sender_id)) {
        response.error_code = ERR_PERMISSION_DENIED;
        snprintf(response.content, sizeof(response.content),
                "Permission denied: Only owner can move files");
        send_message(client_socket, &response);
        return;
    }
    
    // Find storage server
    int ss_index = hashmap_find( msg->filename);
    if (ss_index < 0) {
        response.error_code = ERR_NOT_FOUND;
        snprintf(response.content, sizeof(response.content), "File not found");
        send_message(client_socket, &response);
        return;
    }
    
    // Forward to storage server
    int ss_socket = create_socket();
    if (ss_socket < 0 ||
        connect_to_server(ss_socket, storage_servers[ss_index].base.ip,
                         storage_servers[ss_index].base.nm_port) < 0) {
        response.error_code = ERR_SS_UNAVAILABLE;
        strcpy(response.content, "Storage server unavailable");
        send_message(client_socket, &response);
        if (ss_socket >= 0) close_socket(ss_socket);
        return;
    }
    
    send_message(ss_socket, msg);
    receive_message(ss_socket, &response);
    close_socket(ss_socket);
    
    send_message(client_socket, &response);
    log_info("MOVE: Forwarded to SS[%d], result: %s", 
             ss_index, get_error_message(response.error_code));
}

// Handle VIEWFOLDER request (Bonus: List folder contents)
void handle_viewfolder_request(int client_socket, Message* msg) {
    log_info("VIEWFOLDER request from %s for '%s'", msg->sender_id, msg->folder_name);
    
    Message response;
    INIT_MESSAGE(response);
    response.operation = OP_ACK;
    
    // For viewing folders, any registered user can view any folder
    // Folders are public entities, no ACL check needed
    
    // Find any available storage server (folders are on first SS)
    int ss_index = -1;
    for (int i = 0; i < ss_count; i++) {
        if (storage_servers[i].base.is_alive) {
            ss_index = i;
            break;
        }
    }
    
    if (ss_index < 0) {
        response.error_code = ERR_SS_UNAVAILABLE;
        snprintf(response.content, sizeof(response.content), "No storage servers available");
        send_message(client_socket, &response);
        return;
    }
    
    // Forward to storage server
    int ss_socket = create_socket();
    if (ss_socket < 0 ||
        connect_to_server(ss_socket, storage_servers[ss_index].base.ip,
                         storage_servers[ss_index].base.nm_port) < 0) {
        response.error_code = ERR_SS_UNAVAILABLE;
        strcpy(response.content, "Storage server unavailable");
        send_message(client_socket, &response);
        if (ss_socket >= 0) close_socket(ss_socket);
        return;
    }
    
    send_message(ss_socket, msg);
    receive_message(ss_socket, &response);
    close_socket(ss_socket);
    
    send_message(client_socket, &response);
    log_info("VIEWFOLDER: Sent folder listing to %s", msg->sender_id);
}

// Handle REQUESTACCESS (Bonus: Access request system)
void handle_requestaccess_request(int client_socket, Message* msg) {
    log_info("REQUESTACCESS from %s for file '%s'", msg->sender_id, msg->filename);
    
    Message response;
    INIT_MESSAGE(response);
    response.operation = OP_ACK;
    
    // Check if file exists
    int ss_index = hashmap_find( msg->filename);
    if (ss_index < 0) {
        response.error_code = ERR_NOT_FOUND;
        snprintf(response.content, sizeof(response.content), "File not found");
        send_message(client_socket, &response);
        return;
    }
    
    // Check if already has access
    if (acl_can_access(&acl_table, msg->filename, msg->sender_id)) {
        response.error_code = ERR_ALREADY_EXISTS;
        snprintf(response.content, sizeof(response.content), 
                "You already have access to this file");
        send_message(client_socket, &response);
        return;
    }
    
    // Get owner
    char owner[MAX_USERNAME_LEN];
    if (acl_get_owner(&acl_table, msg->filename, owner, sizeof(owner)) < 0) {
        response.error_code = ERR_NOT_FOUND;
        snprintf(response.content, sizeof(response.content), "File owner not found");
        send_message(client_socket, &response);
        return;
    }
    
    // Add request
    int result = request_add(&global_request_table, msg->filename, msg->sender_id, 
                            owner, ACCESS_READ);
    if (result == ERR_SUCCESS) {
        response.error_code = ERR_SUCCESS;
        snprintf(response.content, sizeof(response.content),
                "Access request submitted. Owner will be notified.");
        log_info("Access request added: %s -> %s", msg->sender_id, msg->filename);
    } else {
        response.error_code = ERR_SERVER_ERROR;
        snprintf(response.content, sizeof(response.content), "Failed to submit request");
    }
    
    send_message(client_socket, &response);
}

// Handle VIEWREQUESTS (Bonus: View pending access requests)
void handle_viewrequests_request(int client_socket, Message* msg) {
    log_info("VIEWREQUESTS from %s for file '%s'", msg->sender_id, msg->filename);
    
    Message response;
    INIT_MESSAGE(response);
    response.operation = OP_ACK;
    
    // Check if requester is owner
    if (!acl_is_owner(&acl_table, msg->filename, msg->sender_id)) {
        response.error_code = ERR_PERMISSION_DENIED;
        snprintf(response.content, sizeof(response.content),
                "Permission denied: Only owner can view requests");
        send_message(client_socket, &response);
        return;
    }
    
    // Get requests
    AccessRequest requests[100];
    int count = request_get_for_owner(&global_request_table, msg->sender_id, 
                                      requests, 100);
    
    response.error_code = ERR_SUCCESS;
    if (count == 0) {
        snprintf(response.content, sizeof(response.content), 
                "No pending access requests for this file");
    } else {
        char* ptr = response.content;
        int remaining = sizeof(response.content);
        int written = 0;
        
        for (int i = 0; i < count && remaining > 50; i++) {
            if (strcmp(requests[i].filename, msg->filename) == 0) {
                written = snprintf(ptr, remaining, "%s requests access to %s\n",
                                 requests[i].requester, requests[i].filename);
                ptr += written;
                remaining -= written;
            }
        }
        
        if (ptr == response.content) {
            snprintf(response.content, sizeof(response.content),
                    "No pending requests for this specific file");
        }
    }
    
    send_message(client_socket, &response);
    log_info("VIEWREQUESTS: Sent %d requests to %s", count, msg->sender_id);
}

// Handle APPROVEREQUEST (Bonus: Approve access request)
void handle_approverequest_request(int client_socket, Message* msg) {
    log_info("APPROVEREQUEST from %s: grant '%s' access to '%s'",
             msg->sender_id, msg->content, msg->filename);
    
    Message response;
    INIT_MESSAGE(response);
    response.operation = OP_ACK;
    
    // Check if requester is owner
    if (!acl_is_owner(&acl_table, msg->filename, msg->sender_id)) {
        response.error_code = ERR_PERMISSION_DENIED;
        snprintf(response.content, sizeof(response.content),
                "Permission denied: Only owner can approve requests");
        send_message(client_socket, &response);
        return;
    }
    
    // Approve request
    if (request_approve(&global_request_table, msg->filename, msg->content, msg->sender_id) == ERR_SUCCESS) {
        // Grant actual access
        if (acl_add_user(&acl_table, msg->filename, msg->content) == 0) {
            response.error_code = ERR_SUCCESS;
            snprintf(response.content, sizeof(response.content),
                    "Access granted to %s", msg->content);
            log_info("Access granted: %s can now access '%s'", msg->content, msg->filename);
        } else {
            response.error_code = ERR_SERVER_ERROR;
            snprintf(response.content, sizeof(response.content), "Failed to grant access");
        }
    } else {
        response.error_code = ERR_NOT_FOUND;
        snprintf(response.content, sizeof(response.content), "Request not found");
    }
    
    send_message(client_socket, &response);
}

// Handle DENYREQUEST (Bonus: Deny access request)
void handle_denyrequest_request(int client_socket, Message* msg) {
    log_info("DENYREQUEST from %s: deny '%s' access to '%s'",
             msg->sender_id, msg->content, msg->filename);
    
    Message response;
    INIT_MESSAGE(response);
    response.operation = OP_ACK;
    
    // Check if requester is owner
    if (!acl_is_owner(&acl_table, msg->filename, msg->sender_id)) {
        response.error_code = ERR_PERMISSION_DENIED;
        snprintf(response.content, sizeof(response.content),
                "Permission denied: Only owner can deny requests");
        send_message(client_socket, &response);
        return;
    }
    
    // Deny request
    if (request_deny(&global_request_table, msg->filename, msg->content, msg->sender_id) == ERR_SUCCESS) {
        response.error_code = ERR_SUCCESS;
        snprintf(response.content, sizeof(response.content),
                "Request denied for %s", msg->content);
        log_info("Access request denied: %s for file '%s'", msg->content, msg->filename);
    } else {
        response.error_code = ERR_NOT_FOUND;
        snprintf(response.content, sizeof(response.content), "Request not found");
    }
    
    send_message(client_socket, &response);
}

/* ============================================================================
 * SEARCH HANDLER - Forward search request to all storage servers
 * ============================================================================ */

void handle_search_request(int client_socket, Message* msg) {
    log_info("SEARCH request from %s for word '%s'", msg->sender_id, msg->content);
    
    Message response;
    INIT_MESSAGE(response);
    response.operation = OP_ACK;
    
    // Collect results from all alive storage servers
    char all_results[MAX_CONTENT_LEN * 2] = "";
    int total_files = 0;
    int ss_queried = 0;
    
    for (int i = 0; i < ss_count; i++) {
        if (!storage_servers[i].base.is_alive) {
            log_debug("Skipping dead SS %s:%d", 
                     storage_servers[i].base.ip, 
                     storage_servers[i].base.nm_port);
            continue;
        }
        
        // Forward search request to this SS
        Message search_msg;
        INIT_MESSAGE(search_msg);
        search_msg.operation = OP_SEARCH;
        strncpy(search_msg.sender_id, msg->sender_id, MAX_USERNAME_LEN - 1);
        strncpy(search_msg.content, msg->content, MAX_CONTENT_LEN - 1);
        
        int ss_socket = create_socket();
        if (ss_socket < 0) {
            log_warning("Failed to create socket for SS %d", i);
            continue;
        }
        
        if (connect_to_server(ss_socket, storage_servers[i].base.ip, 
                             storage_servers[i].base.client_port) < 0) {
            log_warning("Failed to connect to SS %s:%d", 
                       storage_servers[i].base.ip, 
                       storage_servers[i].base.client_port);
            close_socket(ss_socket);
            continue;
        }
        
        if (send_message(ss_socket, &search_msg) < 0) {
            log_warning("Failed to send search to SS %d", i);
            close_socket(ss_socket);
            continue;
        }
        
        // Receive search results
        Message ss_response;
        if (receive_message(ss_socket, &ss_response) > 0) {
            if (ss_response.error_code == ERR_SUCCESS) {
                // Append results
                if (strlen(all_results) > 0) {
                    strcat(all_results, "\n");
                }
                strcat(all_results, ss_response.content + strlen("Files containing '") );
                // Skip the header line
                char* newline = strchr(ss_response.content, '\n');
                if (newline) {
                    strcat(all_results, newline);
                    total_files++;
                }
            }
        }
        
        close_socket(ss_socket);
        ss_queried++;
    }
    
    if (ss_queried == 0) {
        response.error_code = ERR_SS_UNAVAILABLE;
        strcpy(response.content, "No storage servers available");
    } else if (total_files == 0) {
        response.error_code = ERR_NOT_FOUND;
        snprintf(response.content, sizeof(response.content),
                "No files found containing '%s'", msg->content);
    } else {
        response.error_code = ERR_SUCCESS;
        snprintf(response.content, sizeof(response.content),
                "Files containing '%s':\n%s", msg->content, all_results);
    }
    
    send_message(client_socket, &response);
    log_info("SEARCH response sent: %d files found across %d servers", 
             total_files, ss_queried);
}

/* ============================================================================
 * HEARTBEAT HANDLER - Update SS heartbeat timestamp for fault tolerance
 * ============================================================================ */

void handle_heartbeat(int client_socket, Message* msg) {
    Message response;
    INIT_MESSAGE(response);
    response.operation = OP_ACK;
    
    // Find the storage server by IP and port
    int ss_index = -1;
    for (int i = 0; i < ss_count; i++) {
        if (strcmp(storage_servers[i].base.ip, msg->ip_address) == 0 &&
            storage_servers[i].base.nm_port == msg->nm_port) {
            ss_index = i;
            break;
        }
    }
    
    if (ss_index == -1) {
        log_warning("Heartbeat from unregistered SS: %s:%d", msg->ip_address, msg->nm_port);
        response.error_code = ERR_NOT_FOUND;
        snprintf(response.content, sizeof(response.content), 
                "Storage server not registered");
    } else {
        // Update heartbeat timestamp
        storage_servers[ss_index].base.last_heartbeat = time(NULL);
        storage_servers[ss_index].base.is_alive = 1;
        
        response.error_code = ERR_SUCCESS;
        log_debug("Heartbeat received from SS %s:%d", msg->ip_address, msg->nm_port);
    }
    
    send_message(client_socket, &response);
}

/* ============================================================================
 * REQUEST HANDLER - Process client/SS requests
 * ============================================================================ */

void handle_connection(int conn_socket, char* client_ip, int client_port) {
    log_info("New connection from %s:%d on socket %d", client_ip, client_port, conn_socket);
    
    Message msg;
    int bytes = receive_message(conn_socket, &msg);
    
    if (bytes <= 0) {
        if (bytes == 0) {
            // Clean disconnection (client closed connection)
            log_info("Connection closed by %s:%d (socket %d)", client_ip, client_port, conn_socket);
            mark_client_disconnected(client_ip, client_port);
        } else {
            // Error receiving data
            log_error("Failed to receive message from %s:%d (socket %d)", client_ip, client_port, conn_socket);
        }
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
            
        case OP_UNREGISTER_CLIENT:
            log_info("Client unregister request from %s", msg.sender_id);
            mark_client_disconnected(msg.ip_address, msg.nm_port);
            // No response needed - client is disconnecting
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
            
        case OP_WRITE:
            handle_write_request(conn_socket, &msg);
            close_socket(conn_socket);
            break;
            
        case OP_UNDO:
            handle_undo_request(conn_socket, &msg);
            close_socket(conn_socket);
            break;
            
        case OP_ADDACCESS:
            handle_addaccess_request(conn_socket, &msg);
            close_socket(conn_socket);
            break;
            
        case OP_REMACCESS:
            handle_remaccess_request(conn_socket, &msg);
            close_socket(conn_socket);
            break;
            
        case OP_LIST:
            handle_list_request(conn_socket, &msg);
            close_socket(conn_socket);
            break;
            
        case OP_VIEW:
            handle_view_request(conn_socket, &msg);
            close_socket(conn_socket);
            break;
            
        case OP_INFO:
            handle_info_request(conn_socket, &msg);
            close_socket(conn_socket);
            break;
            
        case OP_STREAM:
            handle_stream_request(conn_socket, &msg);
            close_socket(conn_socket);
            break;
            
        case OP_EXEC:
            handle_exec_request(conn_socket, &msg);
            close_socket(conn_socket);
            break;
            
        case OP_CREATEFOLDER:
            handle_createfolder_request(conn_socket, &msg);
            close_socket(conn_socket);
            break;
            
        case OP_MOVE:
            handle_move_request(conn_socket, &msg);
            close_socket(conn_socket);
            break;
            
        case OP_VIEWFOLDER:
            handle_viewfolder_request(conn_socket, &msg);
            close_socket(conn_socket);
            break;
            
        case OP_REQUESTACCESS:
            handle_requestaccess_request(conn_socket, &msg);
            close_socket(conn_socket);
            break;
            
        case OP_VIEWREQUESTS:
            handle_viewrequests_request(conn_socket, &msg);
            close_socket(conn_socket);
            break;
            
        case OP_APPROVEREQUEST:
            handle_approverequest_request(conn_socket, &msg);
            close_socket(conn_socket);
            break;
            
        case OP_DENYREQUEST:
            handle_denyrequest_request(conn_socket, &msg);
            close_socket(conn_socket);
            break;
            
        case OP_HEARTBEAT:
            handle_heartbeat(conn_socket, &msg);
            close_socket(conn_socket);
            break;
            
        case OP_SEARCH:
            handle_search_request(conn_socket, &msg);
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
 * FAILURE DETECTION THREAD - Monitor SS heartbeats and detect failures
 * ============================================================================ */

#define HEARTBEAT_TIMEOUT 15  // Mark SS as dead after 15 seconds without heartbeat

void* failure_detection_thread(void* arg) {
    (void)arg;
    log_info("Failure detection thread started - checking every 10 seconds");
    
    while (1) {
        sleep(10);  // Check every 10 seconds
        
        time_t current_time = time(NULL);
        
        for (int i = 0; i < ss_count; i++) {
            if (storage_servers[i].base.is_alive) {
                time_t time_since_heartbeat = current_time - storage_servers[i].base.last_heartbeat;
                
                if (time_since_heartbeat > HEARTBEAT_TIMEOUT) {
                    log_warning("Storage Server %s:%d marked as DEAD (no heartbeat for %ld seconds)",
                              storage_servers[i].base.ip, 
                              storage_servers[i].base.nm_port,
                              time_since_heartbeat);
                    
                    storage_servers[i].base.is_alive = 0;
                    
                    // Failover handled automatically by find_ss_for_file()
                    log_info("Clients will be redirected to replica SS for files on SS[%d]", i);
                }
            }
        }
    }
    
    return NULL;
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
    pthread_t fd_thread;
    
    init_logger("NS", "logs/nameserver.log");
    
    log_info("==========================================================");
    log_info("Name Server Starting - Docs++ Distributed Document System");
    log_info("==========================================================");
    
    init_registries();
    log_info("Registries initialized with HashMap (O(1) lookup)");
    
    // Start failure detection thread
    if (pthread_create(&fd_thread, NULL, failure_detection_thread, NULL) != 0) {
        log_warning("Failed to create failure detection thread (non-critical)");
    } else {
        log_info("Failure detection thread started - monitoring SS heartbeats");
    }
    
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
