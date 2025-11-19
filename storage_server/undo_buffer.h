/*
 * storage_server/undo_buffer.h
 * 
 * Per-file undo buffer for WRITE operations
 * Maintains one undo buffer per file (hash table based)
 * Each file can have ONE undo operation available
 */

#ifndef UNDO_BUFFER_H
#define UNDO_BUFFER_H

#include "protocol.h"
#include <pthread.h>

#define MAX_UNDO_SIZE (1024 * 1024)  // 1MB max undo buffer per file
#define UNDO_HASH_SIZE 509  // Prime number for hash table

// Single undo entry for one file
typedef struct {
    char filename[MAX_FILENAME_LEN];
    char* content;
    size_t size;
    int valid;  // 1 if undo available, 0 if empty
} UndoEntry;

// Hash node for collision chaining
typedef struct UndoNode {
    UndoEntry entry;
    struct UndoNode* next;
} UndoNode;

// Undo table - manages undo buffers for all files
typedef struct {
    UndoNode* buckets[UNDO_HASH_SIZE];
    pthread_mutex_t mutex;
    int entry_count;
} UndoTable;

// Initialize undo table
void undo_table_init(UndoTable* table);

// Save file to undo buffer (returns ERR_SUCCESS or error code)
// Creates or updates undo entry for the specified file
int undo_save(UndoTable* table, const char* filename, const char* content, size_t size);

// Restore file from undo buffer (returns ERR_SUCCESS or error code)
// Also clears the undo entry after restore
int undo_restore(UndoTable* table, const char* filename, char** content_out, size_t* size_out);

// Check if undo available for file
int undo_available(UndoTable* table, const char* filename);

// Clear undo buffer for a specific file
void undo_clear_file(UndoTable* table, const char* filename);

// Destroy entire undo table
void undo_table_destroy(UndoTable* table);

#endif
