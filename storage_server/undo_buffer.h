/*
 * storage_server/undo_buffer.h
 * 
 * Single-level undo buffer for WRITE operations
 * Stores file backup before atomic write commit
 */

#ifndef UNDO_BUFFER_H
#define UNDO_BUFFER_H

#include "protocol.h"

#define MAX_UNDO_SIZE (1024 * 1024)  // 1MB max undo buffer

typedef struct {
    char filename[MAX_FILENAME_LEN];
    char* content;
    size_t size;
    int valid;  // 1 if undo available, 0 if empty
} UndoBuffer;

// Initialize undo buffer
void undo_buffer_init(UndoBuffer* undo);

// Save file to undo buffer (returns ERR_SUCCESS or error code)
int undo_save(UndoBuffer* undo, const char* filename, const char* content, size_t size);

// Restore file from undo buffer (returns ERR_SUCCESS or error code)
int undo_restore(UndoBuffer* undo, const char* filename);

// Check if undo available for file
int undo_available(UndoBuffer* undo, const char* filename);

// Clear undo buffer
void undo_clear(UndoBuffer* undo);

// Destroy undo buffer
void undo_buffer_destroy(UndoBuffer* undo);

#endif
