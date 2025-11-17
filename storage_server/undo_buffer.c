/*
 * storage_server/undo_buffer.c
 * 
 * Implementation of single-level undo buffer
 */

#include "undo_buffer.h"
#include "../common/logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

void undo_buffer_init(UndoBuffer* undo) {
    undo->content = NULL;
    undo->size = 0;
    undo->valid = 0;
    undo->filename[0] = '\0';
    log_info("Undo buffer initialized");
}

int undo_save(UndoBuffer* undo, const char* filename, const char* content, size_t size) {
    if (size > MAX_UNDO_SIZE) {
        log_error("File too large for undo buffer: %zu bytes (max %d)", size, MAX_UNDO_SIZE);
        return ERR_SERVER_ERROR;
    }
    
    // Free old content if exists
    if (undo->content) {
        free(undo->content);
        undo->content = NULL;
    }
    
    // Allocate new buffer
    undo->content = malloc(size + 1);
    if (!undo->content) {
        log_error("Failed to allocate undo buffer: %zu bytes", size);
        undo->valid = 0;
        return ERR_SERVER_ERROR;
    }
    
    // Copy content
    memcpy(undo->content, content, size);
    undo->content[size] = '\0';
    undo->size = size;
    strncpy(undo->filename, filename, MAX_FILENAME_LEN - 1);
    undo->filename[MAX_FILENAME_LEN - 1] = '\0';
    undo->valid = 1;
    
    log_info("Undo saved for '%s': %zu bytes", filename, size);
    return ERR_SUCCESS;
}

int undo_restore(UndoBuffer* undo, const char* filename) {
    if (!undo->valid) {
        log_warning("No undo available");
        return ERR_NOT_FOUND;
    }
    
    if (strcmp(undo->filename, filename) != 0) {
        log_warning("Undo filename mismatch: requested '%s', have '%s'", filename, undo->filename);
        return ERR_NOT_FOUND;
    }
    
    // Write content back to file (caller should handle file path)
    // This function just provides the content, actual file write done by caller
    log_info("Undo restore ready for '%s': %zu bytes", filename, undo->size);
    return ERR_SUCCESS;
}

int undo_available(UndoBuffer* undo, const char* filename) {
    return (undo->valid && strcmp(undo->filename, filename) == 0);
}

void undo_clear(UndoBuffer* undo) {
    if (undo->content) {
        free(undo->content);
        undo->content = NULL;
    }
    undo->size = 0;
    undo->valid = 0;
    undo->filename[0] = '\0';
}

void undo_buffer_destroy(UndoBuffer* undo) {
    undo_clear(undo);
    log_info("Undo buffer destroyed");
}
