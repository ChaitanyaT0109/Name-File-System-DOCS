/*
 * storage_server/checkpoint_manager.h
 * 
 * Checkpoint System for File Versioning
 */

#ifndef CHECKPOINT_MANAGER_H
#define CHECKPOINT_MANAGER_H

#include "protocol.h"

#define MAX_CHECKPOINTS 50
#define MAX_TAG_LEN 64
#define MAX_PATH_LEN 512

// External storage directory (defined in storage_server.c)
extern char STORAGE_DIR[MAX_PATH_LEN];

// Initialize checkpoint system
void checkpoint_init();

// Create a checkpoint with given tag
int checkpoint_create(const char* filename, const char* tag, const char* owner);

// View checkpoint content
int checkpoint_view(const char* filename, const char* tag, char* content, size_t content_size);

// Revert file to checkpoint
int checkpoint_revert(const char* filename, const char* tag, const char* owner);

// List all checkpoints for a file
int checkpoint_list(const char* filename, char tags[][MAX_TAG_LEN], int max_tags);

// Check if checkpoint exists
int checkpoint_exists(const char* filename, const char* tag);

// Cleanup old checkpoints (keep last 10 per file)
void checkpoint_cleanup();

#endif // CHECKPOINT_MANAGER_H
