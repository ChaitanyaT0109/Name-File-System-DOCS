/*
 * nameserver/acl.c
 * 
 * Implementation of Access Control List (ACL) System
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include "acl.h"
#include "logger.h"

/* ============================================================================
 * MUTEX FOR THREAD-SAFE SAVE
 * ============================================================================ */

static pthread_mutex_t acl_save_mutex = PTHREAD_MUTEX_INITIALIZER;

/* ============================================================================
 * HASH FUNCTION
 * ============================================================================ */

static unsigned int acl_hash(const char* str) {
    unsigned long hash = 5381;
    int c;
    
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c;
    }
    
    return hash % ACL_HASH_SIZE;
}

/* ============================================================================
 * INTERNAL HELPER FUNCTIONS
 * ============================================================================ */

// Find ACL entry for a file
static AclEntry* acl_find_entry(AclTable* acl, const char* filename) {
    unsigned int index = acl_hash(filename);
    AclNode* current = acl->buckets[index];
    
    while (current != NULL) {
        if (strcmp(current->entry.filename, filename) == 0) {
            return &current->entry;
        }
        current = current->next;
    }
    
    return NULL;
}

// Check if username is in array
static int is_in_list(const char list[][MAX_USERNAME_LEN], int count, const char* username) {
    for (int i = 0; i < count; i++) {
        if (strcmp(list[i], username) == 0) {
            return 1;
        }
    }
    return 0;
}

// Add username to array (no duplicates)
static int add_to_list(char list[][MAX_USERNAME_LEN], int* count, int max_count, const char* username) {
    // Check if already exists
    if (is_in_list(list, *count, username)) {
        return 0;  // Already exists
    }
    
    // Check if we have space
    if (*count >= max_count) {
        return -1;  // List full
    }
    
    strncpy(list[*count], username, MAX_USERNAME_LEN - 1);
    list[*count][MAX_USERNAME_LEN - 1] = '\0';
    (*count)++;
    
    return 1;  // Added
}

// Remove username from array
static int remove_from_list(char list[][MAX_USERNAME_LEN], int* count, const char* username) {
    for (int i = 0; i < *count; i++) {
        if (strcmp(list[i], username) == 0) {
            // Shift remaining elements
            for (int j = i; j < *count - 1; j++) {
                strcpy(list[j], list[j + 1]);
            }
            (*count)--;
            return 1;  // Removed
        }
    }
    return 0;  // Not found
}

/* ============================================================================
 * PUBLIC ACL FUNCTIONS
 * ============================================================================ */

void acl_init(AclTable* acl) {
    memset(acl, 0, sizeof(AclTable));
    for (int i = 0; i < ACL_HASH_SIZE; i++) {
        acl->buckets[i] = NULL;
    }
    acl->file_count = 0;
    log_info("ACL system initialized");
}

int acl_add_file(AclTable* acl, const char* filename, const char* owner) {
    // Check if file already exists
    if (acl_find_entry(acl, filename) != NULL) {
        log_warning("ACL: File '%s' already exists", filename);
        return -1;
    }
    
    // Create new node
    AclNode* new_node = (AclNode*)malloc(sizeof(AclNode));
    if (new_node == NULL) {
        log_error("ACL: Failed to allocate memory for new entry");
        return -1;
    }
    
    // Initialize entry
    memset(&new_node->entry, 0, sizeof(AclEntry));
    strncpy(new_node->entry.filename, filename, MAX_FILENAME_LEN - 1);
    new_node->entry.filename[MAX_FILENAME_LEN - 1] = '\0';
    strncpy(new_node->entry.owner, owner, MAX_USERNAME_LEN - 1);
    new_node->entry.owner[MAX_USERNAME_LEN - 1] = '\0';
    new_node->entry.read_count = 0;
    new_node->entry.write_count = 0;
    new_node->entry.created_time = time(NULL);
    
    // Insert into hash table
    unsigned int index = acl_hash(filename);
    new_node->next = acl->buckets[index];
    acl->buckets[index] = new_node;
    acl->file_count++;
    
    log_info("ACL: Added file '%s' with owner '%s'", filename, owner);
    return 0;
}

int acl_check_read(AclTable* acl, const char* filename, const char* username) {
    AclEntry* entry = acl_find_entry(acl, filename);
    if (entry == NULL) {
        return 0;  // File not in ACL
    }
    
    // Owner always has read access
    if (strcmp(entry->owner, username) == 0) {
        return 1;
    }
    
    // Check read list
    if (is_in_list(entry->read_users, entry->read_count, username)) {
        return 1;
    }
    
    // Check write list (write implies read)
    if (is_in_list(entry->write_users, entry->write_count, username)) {
        return 1;
    }
    
    return 0;
}

int acl_check_write(AclTable* acl, const char* filename, const char* username) {
    AclEntry* entry = acl_find_entry(acl, filename);
    if (entry == NULL) {
        return 0;  // File not in ACL
    }
    
    // Owner always has write access
    if (strcmp(entry->owner, username) == 0) {
        return 1;
    }
    
    // Check write list
    if (is_in_list(entry->write_users, entry->write_count, username)) {
        return 1;
    }
    
    return 0;
}

int acl_check_owner(AclTable* acl, const char* filename, const char* username) {
    AclEntry* entry = acl_find_entry(acl, filename);
    if (entry == NULL) {
        return 0;
    }
    
    return strcmp(entry->owner, username) == 0 ? 1 : 0;
}

int acl_add_access(AclTable* acl, const char* filename, const char* username, int access_type) {
    AclEntry* entry = acl_find_entry(acl, filename);
    if (entry == NULL) {
        log_warning("ACL: Cannot add access - file '%s' not found", filename);
        return -1;
    }
    
    int result = 0;
    
    if (access_type == ACCESS_READ) {
        // Add to read list only
        result = add_to_list(entry->read_users, &entry->read_count, ACL_MAX_USERS, username);
        if (result > 0) {
            log_info("ACL: Granted READ access to '%s' for file '%s'", username, filename);
        }
    } else if (access_type == ACCESS_WRITE) {
        // Add to write list (write implies read, so add to write list only)
        result = add_to_list(entry->write_users, &entry->write_count, ACL_MAX_USERS, username);
        if (result > 0) {
            log_info("ACL: Granted WRITE access to '%s' for file '%s'", username, filename);
        }
    } else {
        log_warning("ACL: Invalid access type %d", access_type);
        return -1;
    }
    
    return result >= 0 ? 0 : -1;
}

int acl_remove_access(AclTable* acl, const char* filename, const char* username) {
    AclEntry* entry = acl_find_entry(acl, filename);
    if (entry == NULL) {
        log_warning("ACL: Cannot remove access - file '%s' not found", filename);
        return -1;
    }
    
    // Remove from both read and write lists
    int removed_read = remove_from_list(entry->read_users, &entry->read_count, username);
    int removed_write = remove_from_list(entry->write_users, &entry->write_count, username);
    
    if (removed_read || removed_write) {
        log_info("ACL: Revoked access for '%s' on file '%s'", username, filename);
        return 0;
    }
    
    log_warning("ACL: User '%s' had no access to file '%s'", username, filename);
    return -1;
}

int acl_remove_file(AclTable* acl, const char* filename) {
    unsigned int index = acl_hash(filename);
    AclNode* current = acl->buckets[index];
    AclNode* prev = NULL;
    
    while (current != NULL) {
        if (strcmp(current->entry.filename, filename) == 0) {
            // Found it - remove from list
            if (prev == NULL) {
                acl->buckets[index] = current->next;
            } else {
                prev->next = current->next;
            }
            
            log_info("ACL: Removed file '%s'", filename);
            free(current);
            acl->file_count--;
            return 0;
        }
        prev = current;
        current = current->next;
    }
    
    return -1;  // Not found
}

int acl_get_owner(AclTable* acl, const char* filename, char* owner_buf, size_t buf_size) {
    AclEntry* entry = acl_find_entry(acl, filename);
    if (entry == NULL) {
        return -1;
    }
    
    strncpy(owner_buf, entry->owner, buf_size - 1);
    owner_buf[buf_size - 1] = '\0';
    return 0;
}

int acl_get_access_list(AclTable* acl, const char* filename, char* buf, size_t buf_size) {
    AclEntry* entry = acl_find_entry(acl, filename);
    if (entry == NULL) {
        return -1;
    }
    
    char temp[MAX_CONTENT_LEN];
    int offset = 0;
    
    // Add owner (always RW)
    offset += snprintf(temp + offset, sizeof(temp) - offset, "%s (RW)", entry->owner);
    
    // Add write users (RW)
    for (int i = 0; i < entry->write_count; i++) {
        offset += snprintf(temp + offset, sizeof(temp) - offset, ", %s (RW)", entry->write_users[i]);
    }
    
    // Add read-only users (R)
    for (int i = 0; i < entry->read_count; i++) {
        // Skip if user is also in write list
        if (!is_in_list(entry->write_users, entry->write_count, entry->read_users[i])) {
            offset += snprintf(temp + offset, sizeof(temp) - offset, ", %s (R)", entry->read_users[i]);
        }
    }
    
    strncpy(buf, temp, buf_size - 1);
    buf[buf_size - 1] = '\0';
    return 0;
}

int acl_get_all_files(AclTable* acl, char files[][MAX_FILENAME_LEN], int max_files) {
    int count = 0;
    
    for (int i = 0; i < ACL_HASH_SIZE && count < max_files; i++) {
        AclNode* current = acl->buckets[i];
        while (current != NULL && count < max_files) {
            strncpy(files[count], current->entry.filename, MAX_FILENAME_LEN - 1);
            files[count][MAX_FILENAME_LEN - 1] = '\0';
            count++;
            current = current->next;
        }
    }
    
    return count;
}

int acl_get_accessible_files(AclTable* acl, const char* username, 
                              char files[][MAX_FILENAME_LEN], int max_files) {
    int count = 0;
    
    for (int i = 0; i < ACL_HASH_SIZE && count < max_files; i++) {
        AclNode* current = acl->buckets[i];
        while (current != NULL && count < max_files) {
            AclEntry* entry = &current->entry;
            
            // Check if user has access
            if (strcmp(entry->owner, username) == 0 ||
                is_in_list(entry->read_users, entry->read_count, username) ||
                is_in_list(entry->write_users, entry->write_count, username)) {
                
                strncpy(files[count], entry->filename, MAX_FILENAME_LEN - 1);
                files[count][MAX_FILENAME_LEN - 1] = '\0';
                count++;
            }
            
            current = current->next;
        }
    }
    
    return count;
}

int acl_save(AclTable* acl, const char* filepath) {
    pthread_mutex_lock(&acl_save_mutex);
    
    FILE* fp = fopen(filepath, "w");
    if (fp == NULL) {
        log_error("ACL: Failed to open '%s' for writing", filepath);
        pthread_mutex_unlock(&acl_save_mutex);
        return -1;
    }
    
    // Write header
    fprintf(fp, "# ACL Data File - Docs++\n");
    fprintf(fp, "# Format: filename|owner|read_users|write_users\n");
    fprintf(fp, "# Version: 1.0\n");
    fprintf(fp, "file_count=%d\n", acl->file_count);
    
    // Write each ACL entry
    for (int i = 0; i < ACL_HASH_SIZE; i++) {
        AclNode* current = acl->buckets[i];
        while (current != NULL) {
            AclEntry* entry = &current->entry;
            
            // Write filename and owner
            fprintf(fp, "%s|%s|", entry->filename, entry->owner);
            
            // Write read users
            for (int j = 0; j < entry->read_count; j++) {
                fprintf(fp, "%s", entry->read_users[j]);
                if (j < entry->read_count - 1) fprintf(fp, ",");
            }
            fprintf(fp, "|");
            
            // Write write users
            for (int j = 0; j < entry->write_count; j++) {
                fprintf(fp, "%s", entry->write_users[j]);
                if (j < entry->write_count - 1) fprintf(fp, ",");
            }
            fprintf(fp, "\n");
            
            current = current->next;
        }
    }
    
    // Ensure data is written to disk
    fflush(fp);
    fsync(fileno(fp));
    fclose(fp);
    
    log_info("ACL: Saved %d entries to '%s'", acl->file_count, filepath);
    pthread_mutex_unlock(&acl_save_mutex);
    return 0;
}

int acl_load(AclTable* acl, const char* filepath) {
    FILE* fp = fopen(filepath, "r");
    if (fp == NULL) {
        log_warning("ACL: File '%s' not found, starting with empty ACL", filepath);
        return -1;
    }
    
    char line[MAX_CONTENT_LEN];
    int loaded = 0;
    
    while (fgets(line, sizeof(line), fp)) {
        // Skip comments and empty lines
        if (line[0] == '#' || line[0] == '\n') continue;
        
        // Skip header line
        if (strncmp(line, "file_count=", 11) == 0) continue;
        
        // Parse entry: filename|owner|read_users|write_users
        char filename[MAX_FILENAME_LEN];
        char owner[MAX_USERNAME_LEN];
        char read_str[MAX_CONTENT_LEN];
        char write_str[MAX_CONTENT_LEN];
        
        int parsed = sscanf(line, "%255[^|]|%63[^|]|%4095[^|]|%4095[^\n]",
                           filename, owner, read_str, write_str);
        
        if (parsed < 2) continue;  // Need at least filename and owner
        
        // Add file to ACL
        if (acl_add_file(acl, filename, owner) == 0) {
            loaded++;
            
            // Parse and add read users (using thread-safe strtok_r)
            if (parsed >= 3 && strlen(read_str) > 0) {
                char* saveptr1;
                char* token = strtok_r(read_str, ",", &saveptr1);
                while (token != NULL) {
                    acl_add_access(acl, filename, token, ACCESS_READ);
                    token = strtok_r(NULL, ",", &saveptr1);
                }
            }
            
            // Parse and add write users (using thread-safe strtok_r)
            if (parsed >= 4 && strlen(write_str) > 0) {
                char* saveptr2;
                char* token = strtok_r(write_str, ",", &saveptr2);
                while (token != NULL) {
                    acl_add_access(acl, filename, token, ACCESS_WRITE);
                    token = strtok_r(NULL, ",", &saveptr2);
                }
            }
        } else {
            log_error("ACL: Failed to load entry for '%s' (owner: %s)", filename, owner);
        }
    }
    
    fclose(fp);
    log_info("ACL: Loaded %d entries from '%s'", loaded, filepath);
    return 0;
}

void acl_stats(AclTable* acl) {
    int used_buckets = 0;
    int max_chain = 0;
    
    for (int i = 0; i < ACL_HASH_SIZE; i++) {
        if (acl->buckets[i] != NULL) {
            used_buckets++;
            
            int chain_len = 0;
            AclNode* current = acl->buckets[i];
            while (current != NULL) {
                chain_len++;
                current = current->next;
            }
            
            if (chain_len > max_chain) {
                max_chain = chain_len;
            }
        }
    }
    
    double load_factor = (double)acl->file_count / ACL_HASH_SIZE;
    log_info("ACL Stats: %d files, %d/%d buckets (%.2f%%), max chain: %d, load: %.3f",
             acl->file_count, used_buckets, ACL_HASH_SIZE,
             (used_buckets * 100.0 / ACL_HASH_SIZE), max_chain, load_factor);
}

// Helper function for acl_get_all_users
static int is_user_in_acl_list(const char* username, char users[][MAX_USERNAME_LEN], int count) {
    for (int i = 0; i < count; i++) {
        if (strcmp(users[i], username) == 0) {
            return 1;
        }
    }
    return 0;
}

int acl_get_all_users(AclTable* acl, char users[][MAX_USERNAME_LEN], int max_users) {
    int user_count = 0;
    
    // Iterate through all ACL entries
    for (int i = 0; i < ACL_HASH_SIZE && user_count < max_users; i++) {
        AclNode* current = acl->buckets[i];
        while (current != NULL && user_count < max_users) {
            AclEntry* entry = &current->entry;
            
            // Add owner if not already in list
            if (!is_user_in_acl_list(entry->owner, users, user_count)) {
                strncpy(users[user_count], entry->owner, MAX_USERNAME_LEN - 1);
                users[user_count][MAX_USERNAME_LEN - 1] = '\0';
                user_count++;
            }
            
            // Add read users
            for (int j = 0; j < entry->read_count && user_count < max_users; j++) {
                if (!is_user_in_acl_list(entry->read_users[j], users, user_count)) {
                    strncpy(users[user_count], entry->read_users[j], MAX_USERNAME_LEN - 1);
                    users[user_count][MAX_USERNAME_LEN - 1] = '\0';
                    user_count++;
                }
            }
            
            // Add write users
            for (int j = 0; j < entry->write_count && user_count < max_users; j++) {
                if (!is_user_in_acl_list(entry->write_users[j], users, user_count)) {
                    strncpy(users[user_count], entry->write_users[j], MAX_USERNAME_LEN - 1);
                    users[user_count][MAX_USERNAME_LEN - 1] = '\0';
                    user_count++;
                }
            }
            
            current = current->next;
        }
    }
    
    return user_count;
}
