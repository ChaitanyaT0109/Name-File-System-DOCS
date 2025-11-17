/*
 * storage_server/sentence_lock.h
 * 
 * Sentence-level locking for concurrent WRITE operations
 * Prevents two clients from modifying the same sentence simultaneously
 */

#ifndef SENTENCE_LOCK_H
#define SENTENCE_LOCK_H

#include <time.h>
#include <pthread.h>
#include "protocol.h"

#define LOCK_HASH_SIZE 509  // Prime for better distribution
#define LOCK_TIMEOUT 60     // Seconds before auto-release

typedef struct LockEntry {
    char filename[MAX_FILENAME_LEN];
    int sentence_num;
    char client_id[MAX_USERNAME_LEN];
    time_t timestamp;
    struct LockEntry* next;
} LockEntry;

typedef struct {
    LockEntry* buckets[LOCK_HASH_SIZE];
    pthread_mutex_t mutex;
} LockTable;

// Initialize lock table
void lock_table_init(LockTable* table);

// Acquire lock (returns ERR_SUCCESS or ERR_LOCKED)
int lock_acquire(LockTable* table, const char* filename, int sentence_num, const char* client_id);

// Release lock (returns ERR_SUCCESS or ERR_NOT_FOUND)
int lock_release(LockTable* table, const char* filename, int sentence_num, const char* client_id);

// Check if locked (returns 1 if locked by someone else, 0 if free or locked by you)
int lock_is_locked(LockTable* table, const char* filename, int sentence_num, const char* client_id);

// Cleanup expired locks (called by background thread)
int lock_cleanup_expired(LockTable* table);

// Destroy lock table
void lock_table_destroy(LockTable* table);

#endif
