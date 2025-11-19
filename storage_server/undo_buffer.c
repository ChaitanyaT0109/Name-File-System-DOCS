/*
 * storage_server/undo_buffer.c
 * 
 * Implementation of per-file undo buffer system
 * Uses hash table to maintain one undo buffer per file
 */

#include "undo_buffer.h"
#include "../common/logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

/* ============================================================================
 * HASH FUNCTION
 * ============================================================================ */

static unsigned int hash_filename(const char* filename) {
    unsigned int hash = 5381;
    int c;
    while ((c = *filename++)) {
        hash = ((hash << 5) + hash) + c; // hash * 33 + c
    }
    return hash % UNDO_HASH_SIZE;
}

/* ============================================================================
 * UNDO TABLE FUNCTIONS
 * ============================================================================ */

void undo_table_init(UndoTable* table) {
    memset(table->buckets, 0, sizeof(table->buckets));
    pthread_mutex_init(&table->mutex, NULL);
    table->entry_count = 0;
    log_info("Undo table initialized (hash size: %d)", UNDO_HASH_SIZE);
}

int undo_save(UndoTable* table, const char* filename, const char* content, size_t size) {
    if (size > MAX_UNDO_SIZE) {
        log_error("File too large for undo buffer: %zu bytes (max %d)", size, MAX_UNDO_SIZE);
        return ERR_SERVER_ERROR;
    }
    
    pthread_mutex_lock(&table->mutex);
    
    unsigned int bucket = hash_filename(filename);
    UndoNode* current = table->buckets[bucket];
    
    // Search for existing entry
    while (current) {
        if (strcmp(current->entry.filename, filename) == 0) {
            // Found - update existing entry
            if (current->entry.content) {
                free(current->entry.content);
            }
            
            current->entry.content = malloc(size + 1);
            if (!current->entry.content) {
                log_error("Failed to allocate undo buffer for '%s': %zu bytes", filename, size);
                pthread_mutex_unlock(&table->mutex);
                return ERR_SERVER_ERROR;
            }
            
            memcpy(current->entry.content, content, size);
            current->entry.content[size] = '\0';
            current->entry.size = size;
            current->entry.valid = 1;
            
            log_info("Undo updated for '%s': %zu bytes", filename, size);
            pthread_mutex_unlock(&table->mutex);
            return ERR_SUCCESS;
        }
        current = current->next;
    }
    
    // Not found - create new entry
    UndoNode* new_node = malloc(sizeof(UndoNode));
    if (!new_node) {
        log_error("Failed to allocate undo node for '%s'", filename);
        pthread_mutex_unlock(&table->mutex);
        return ERR_SERVER_ERROR;
    }
    
    strncpy(new_node->entry.filename, filename, MAX_FILENAME_LEN - 1);
    new_node->entry.filename[MAX_FILENAME_LEN - 1] = '\0';
    
    new_node->entry.content = malloc(size + 1);
    if (!new_node->entry.content) {
        free(new_node);
        log_error("Failed to allocate undo content for '%s': %zu bytes", filename, size);
        pthread_mutex_unlock(&table->mutex);
        return ERR_SERVER_ERROR;
    }
    
    memcpy(new_node->entry.content, content, size);
    new_node->entry.content[size] = '\0';
    new_node->entry.size = size;
    new_node->entry.valid = 1;
    
    // Insert at head of bucket
    new_node->next = table->buckets[bucket];
    table->buckets[bucket] = new_node;
    table->entry_count++;
    
    log_info("Undo created for '%s': %zu bytes (total entries: %d)", 
             filename, size, table->entry_count);
    
    pthread_mutex_unlock(&table->mutex);
    return ERR_SUCCESS;
}

int undo_restore(UndoTable* table, const char* filename, char** content_out, size_t* size_out) {
    pthread_mutex_lock(&table->mutex);
    
    unsigned int bucket = hash_filename(filename);
    UndoNode* current = table->buckets[bucket];
    
    // Search for entry
    while (current) {
        if (strcmp(current->entry.filename, filename) == 0) {
            if (!current->entry.valid) {
                log_warning("Undo entry for '%s' is invalid", filename);
                pthread_mutex_unlock(&table->mutex);
                return ERR_NOT_FOUND;
            }
            
            // Allocate and copy content for caller
            *content_out = malloc(current->entry.size + 1);
            if (!*content_out) {
                log_error("Failed to allocate restore buffer for '%s'", filename);
                pthread_mutex_unlock(&table->mutex);
                return ERR_SERVER_ERROR;
            }
            
            memcpy(*content_out, current->entry.content, current->entry.size);
            (*content_out)[current->entry.size] = '\0';
            *size_out = current->entry.size;
            
            // Clear this undo entry (one-time use)
            free(current->entry.content);
            current->entry.content = NULL;
            current->entry.size = 0;
            current->entry.valid = 0;
            
            log_info("Undo restored for '%s': %zu bytes (entry now cleared)", filename, *size_out);
            
            pthread_mutex_unlock(&table->mutex);
            return ERR_SUCCESS;
        }
        current = current->next;
    }
    
    log_warning("No undo available for '%s'", filename);
    pthread_mutex_unlock(&table->mutex);
    return ERR_NOT_FOUND;
}

int undo_available(UndoTable* table, const char* filename) {
    pthread_mutex_lock(&table->mutex);
    
    unsigned int bucket = hash_filename(filename);
    UndoNode* current = table->buckets[bucket];
    
    while (current) {
        if (strcmp(current->entry.filename, filename) == 0) {
            int available = current->entry.valid;
            pthread_mutex_unlock(&table->mutex);
            return available;
        }
        current = current->next;
    }
    
    pthread_mutex_unlock(&table->mutex);
    return 0;
}

void undo_clear_file(UndoTable* table, const char* filename) {
    pthread_mutex_lock(&table->mutex);
    
    unsigned int bucket = hash_filename(filename);
    UndoNode* current = table->buckets[bucket];
    UndoNode* prev = NULL;
    
    while (current) {
        if (strcmp(current->entry.filename, filename) == 0) {
            // Free content
            if (current->entry.content) {
                free(current->entry.content);
            }
            
            // Remove from list
            if (prev) {
                prev->next = current->next;
            } else {
                table->buckets[bucket] = current->next;
            }
            
            free(current);
            table->entry_count--;
            
            log_info("Undo cleared for '%s' (total entries: %d)", filename, table->entry_count);
            pthread_mutex_unlock(&table->mutex);
            return;
        }
        prev = current;
        current = current->next;
    }
    
    pthread_mutex_unlock(&table->mutex);
}

void undo_table_destroy(UndoTable* table) {
    pthread_mutex_lock(&table->mutex);
    
    for (int i = 0; i < UNDO_HASH_SIZE; i++) {
        UndoNode* current = table->buckets[i];
        while (current) {
            UndoNode* next = current->next;
            if (current->entry.content) {
                free(current->entry.content);
            }
            free(current);
            current = next;
        }
        table->buckets[i] = NULL;
    }
    
    table->entry_count = 0;
    
    pthread_mutex_unlock(&table->mutex);
    pthread_mutex_destroy(&table->mutex);
    
    log_info("Undo table destroyed");
}
