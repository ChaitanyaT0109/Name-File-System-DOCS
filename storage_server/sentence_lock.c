/*
 * storage_server/sentence_lock.c
 * 
 * Implementation of sentence-level locking
 */

#include "sentence_lock.h"
#include "../common/logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Hash function for filename:sentence_num
static unsigned int hash_lock_key(const char* filename, int sentence_num) {
    unsigned int hash = 5381;
    const char* str = filename;
    
    while (*str) {
        hash = ((hash << 5) + hash) + (unsigned char)(*str++);
    }
    
    hash += (unsigned int)sentence_num * 31;
    return hash % LOCK_HASH_SIZE;
}

void lock_table_init(LockTable* table) {
    for (int i = 0; i < LOCK_HASH_SIZE; i++) {
        table->buckets[i] = NULL;
    }
    pthread_mutex_init(&table->mutex, NULL);
    log_info("Lock table initialized");
}

int lock_acquire(LockTable* table, const char* filename, int sentence_num, const char* client_id) {
    pthread_mutex_lock(&table->mutex);
    
    unsigned int idx = hash_lock_key(filename, sentence_num);
    LockEntry* entry = table->buckets[idx];
    
    // Check if already locked
    while (entry) {
        if (strcmp(entry->filename, filename) == 0 && entry->sentence_num == sentence_num) {
            // Check if lock expired
            time_t now = time(NULL);
            if (now - entry->timestamp >= LOCK_TIMEOUT) {
                // Lock expired, reuse entry
                strncpy(entry->client_id, client_id, MAX_USERNAME_LEN - 1);
                entry->timestamp = now;
                pthread_mutex_unlock(&table->mutex);
                log_info("Lock acquired (expired lock reused): %s:S%d by %s", filename, sentence_num, client_id);
                return ERR_SUCCESS;
            }
            
            // Check if same client
            if (strcmp(entry->client_id, client_id) == 0) {
                // Same client, refresh timestamp
                entry->timestamp = now;
                pthread_mutex_unlock(&table->mutex);
                log_info("Lock refreshed: %s:S%d by %s", filename, sentence_num, client_id);
                return ERR_SUCCESS;
            }
            
            // Locked by someone else
            pthread_mutex_unlock(&table->mutex);
            log_warning("Lock denied (already locked): %s:S%d locked by %s, requested by %s",
                       filename, sentence_num, entry->client_id, client_id);
            return ERR_LOCKED;
        }
        entry = entry->next;
    }
    
    // Lock is free, create new entry
    LockEntry* new_entry = malloc(sizeof(LockEntry));
    strncpy(new_entry->filename, filename, MAX_FILENAME_LEN - 1);
    new_entry->filename[MAX_FILENAME_LEN - 1] = '\0';
    new_entry->sentence_num = sentence_num;
    strncpy(new_entry->client_id, client_id, MAX_USERNAME_LEN - 1);
    new_entry->client_id[MAX_USERNAME_LEN - 1] = '\0';
    new_entry->timestamp = time(NULL);
    new_entry->next = table->buckets[idx];
    table->buckets[idx] = new_entry;
    
    pthread_mutex_unlock(&table->mutex);
    log_info("Lock acquired: %s:S%d by %s", filename, sentence_num, client_id);
    return ERR_SUCCESS;
}

int lock_release(LockTable* table, const char* filename, int sentence_num, const char* client_id) {
    pthread_mutex_lock(&table->mutex);
    
    unsigned int idx = hash_lock_key(filename, sentence_num);
    LockEntry* entry = table->buckets[idx];
    LockEntry* prev = NULL;
    
    while (entry) {
        if (strcmp(entry->filename, filename) == 0 && entry->sentence_num == sentence_num) {
            // Verify client owns the lock
            if (strcmp(entry->client_id, client_id) != 0) {
                pthread_mutex_unlock(&table->mutex);
                log_warning("Lock release denied: %s:S%d not owned by %s (owned by %s)",
                           filename, sentence_num, client_id, entry->client_id);
                return ERR_ACCESS_DENIED;
            }
            
            // Remove entry
            if (prev) {
                prev->next = entry->next;
            } else {
                table->buckets[idx] = entry->next;
            }
            
            free(entry);
            pthread_mutex_unlock(&table->mutex);
            log_info("Lock released: %s:S%d by %s", filename, sentence_num, client_id);
            return ERR_SUCCESS;
        }
        prev = entry;
        entry = entry->next;
    }
    
    pthread_mutex_unlock(&table->mutex);
    log_warning("Lock release failed: %s:S%d not found", filename, sentence_num);
    return ERR_NOT_FOUND;
}

int lock_is_locked(LockTable* table, const char* filename, int sentence_num, const char* client_id) {
    pthread_mutex_lock(&table->mutex);
    
    unsigned int idx = hash_lock_key(filename, sentence_num);
    LockEntry* entry = table->buckets[idx];
    
    while (entry) {
        if (strcmp(entry->filename, filename) == 0 && entry->sentence_num == sentence_num) {
            // Check if expired
            time_t now = time(NULL);
            if (now - entry->timestamp >= LOCK_TIMEOUT) {
                pthread_mutex_unlock(&table->mutex);
                return 0;  // Expired = free
            }
            
            // Check if same client
            if (strcmp(entry->client_id, client_id) == 0) {
                pthread_mutex_unlock(&table->mutex);
                return 0;  // You own it = free for you
            }
            
            pthread_mutex_unlock(&table->mutex);
            return 1;  // Locked by someone else
        }
        entry = entry->next;
    }
    
    pthread_mutex_unlock(&table->mutex);
    return 0;  // Not found = free
}

int lock_cleanup_expired(LockTable* table) {
    pthread_mutex_lock(&table->mutex);
    
    int cleaned = 0;
    time_t now = time(NULL);
    
    for (int i = 0; i < LOCK_HASH_SIZE; i++) {
        LockEntry* entry = table->buckets[i];
        LockEntry* prev = NULL;
        
        while (entry) {
            if (now - entry->timestamp >= LOCK_TIMEOUT) {
                // Remove expired entry
                LockEntry* to_free = entry;
                if (prev) {
                    prev->next = entry->next;
                    entry = entry->next;
                } else {
                    table->buckets[i] = entry->next;
                    entry = entry->next;
                }
                
                log_info("Cleaned expired lock: %s:S%d (was held by %s)",
                        to_free->filename, to_free->sentence_num, to_free->client_id);
                free(to_free);
                cleaned++;
            } else {
                prev = entry;
                entry = entry->next;
            }
        }
    }
    
    pthread_mutex_unlock(&table->mutex);
    
    if (cleaned > 0) {
        log_info("Cleaned %d expired locks", cleaned);
    }
    
    return cleaned;
}

void lock_table_destroy(LockTable* table) {
    pthread_mutex_lock(&table->mutex);
    
    for (int i = 0; i < LOCK_HASH_SIZE; i++) {
        LockEntry* entry = table->buckets[i];
        while (entry) {
            LockEntry* next = entry->next;
            free(entry);
            entry = next;
        }
        table->buckets[i] = NULL;
    }
    
    pthread_mutex_unlock(&table->mutex);
    pthread_mutex_destroy(&table->mutex);
    log_info("Lock table destroyed");
}
