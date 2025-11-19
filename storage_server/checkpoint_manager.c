/*
 * storage_server/checkpoint_manager.c
 * 
 * Checkpoint System for File Versioning
 * BONUS FEATURE: 15 marks
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <time.h>
#include <errno.h>
#include "checkpoint_manager.h"
#include "logger.h"

// External storage directory
extern char STORAGE_DIR[MAX_PATH_LEN];

// Checkpoint directory structure: storage/.checkpoints/filename/tag
#define CHECKPOINT_DIR ".checkpoints"

void checkpoint_init() {
    char checkpoint_base[MAX_PATH_LEN];
    snprintf(checkpoint_base, sizeof(checkpoint_base), "%s/%s", STORAGE_DIR, CHECKPOINT_DIR);
    
    struct stat st;
    if (stat(checkpoint_base, &st) == -1) {
        if (mkdir(checkpoint_base, 0755) == -1) {
            log_error("Failed to create checkpoint directory: %s", strerror(errno));
        } else {
            log_info("Created checkpoint directory: %s", checkpoint_base);
        }
    }
}

static void get_checkpoint_dir(const char* filename, char* dir_path) {
    snprintf(dir_path, MAX_PATH_LEN, "%s/%s/%s", STORAGE_DIR, CHECKPOINT_DIR, filename);
}

static void get_checkpoint_path(const char* filename, const char* tag, char* checkpoint_path) {
    char dir_path[MAX_PATH_LEN];
    get_checkpoint_dir(filename, dir_path);
    snprintf(checkpoint_path, MAX_PATH_LEN, "%s/%s.checkpoint", dir_path, tag);
}

int checkpoint_create(const char* filename, const char* tag, const char* owner) {
    char src_path[MAX_PATH_LEN];
    char checkpoint_path[MAX_PATH_LEN];
    char dir_path[MAX_PATH_LEN];
    
    (void)owner;  // Reserved for future ACL extension
    
    // Check if source file exists
    snprintf(src_path, sizeof(src_path), "%s/%s", STORAGE_DIR, filename);
    struct stat st;
    if (stat(src_path, &st) != 0) {
        log_error("File '%s' not found for checkpoint", filename);
        return ERR_NOT_FOUND;
    }
    
    // Create checkpoint directory for this file if it doesn't exist
    get_checkpoint_dir(filename, dir_path);
    if (stat(dir_path, &st) == -1) {
        if (mkdir(dir_path, 0755) == -1) {
            log_error("Failed to create checkpoint directory for '%s': %s", 
                      filename, strerror(errno));
            return ERR_SERVER_ERROR;
        }
    }
    
    // Get checkpoint path
    get_checkpoint_path(filename, tag, checkpoint_path);
    
    // Check if checkpoint with same tag exists
    if (stat(checkpoint_path, &st) == 0) {
        log_warning("Checkpoint '%s' for file '%s' already exists", tag, filename);
        return ERR_CONFLICT;
    }
    
    // Copy file content to checkpoint
    FILE* src = fopen(src_path, "rb");
    if (!src) {
        log_error("Failed to open source file '%s': %s", filename, strerror(errno));
        return ERR_SERVER_ERROR;
    }
    
    FILE* dest = fopen(checkpoint_path, "wb");
    if (!dest) {
        log_error("Failed to create checkpoint file: %s", strerror(errno));
        fclose(src);
        return ERR_SERVER_ERROR;
    }
    
    // Copy content
    char buffer[4096];
    size_t bytes;
    while ((bytes = fread(buffer, 1, sizeof(buffer), src)) > 0) {
        fwrite(buffer, 1, bytes, dest);
    }
    
    fclose(src);
    fclose(dest);
    
    log_info("Created checkpoint '%s' for file '%s'", tag, filename);
    return ERR_CREATED;
}

int checkpoint_view(const char* filename, const char* tag, char* content, size_t content_size) {
    char checkpoint_path[MAX_PATH_LEN];
    
    get_checkpoint_path(filename, tag, checkpoint_path);
    
    // Check if checkpoint exists
    struct stat st;
    if (stat(checkpoint_path, &st) != 0) {
        log_error("Checkpoint '%s' for file '%s' not found", tag, filename);
        return ERR_NOT_FOUND;
    }
    
    // Read checkpoint content
    FILE* fp = fopen(checkpoint_path, "r");
    if (!fp) {
        log_error("Failed to open checkpoint '%s': %s", tag, strerror(errno));
        return ERR_SERVER_ERROR;
    }
    
    size_t bytes_read = fread(content, 1, content_size - 1, fp);
    content[bytes_read] = '\0';
    
    fclose(fp);
    
    log_info("Viewed checkpoint '%s' for file '%s' (%zu bytes)", tag, filename, bytes_read);
    return ERR_SUCCESS;
}

int checkpoint_revert(const char* filename, const char* tag, const char* owner) {
    char checkpoint_path[MAX_PATH_LEN];
    char file_path[MAX_PATH_LEN];
    
    (void)owner;  // Reserved for future ACL extension
    
    get_checkpoint_path(filename, tag, checkpoint_path);
    snprintf(file_path, sizeof(file_path), "%s/%s", STORAGE_DIR, filename);
    
    // Check if checkpoint exists
    struct stat st;
    if (stat(checkpoint_path, &st) != 0) {
        log_error("Checkpoint '%s' for file '%s' not found", tag, filename);
        return ERR_NOT_FOUND;
    }
    
    // Copy checkpoint content back to file
    FILE* src = fopen(checkpoint_path, "rb");
    if (!src) {
        log_error("Failed to open checkpoint '%s': %s", tag, strerror(errno));
        return ERR_SERVER_ERROR;
    }
    
    FILE* dest = fopen(file_path, "wb");
    if (!dest) {
        log_error("Failed to open file '%s' for writing: %s", filename, strerror(errno));
        fclose(src);
        return ERR_SERVER_ERROR;
    }
    
    // Copy content
    char buffer[4096];
    size_t bytes;
    while ((bytes = fread(buffer, 1, sizeof(buffer), src)) > 0) {
        fwrite(buffer, 1, bytes, dest);
    }
    
    fclose(src);
    fclose(dest);
    
    log_info("Reverted file '%s' to checkpoint '%s'", filename, tag);
    return ERR_SUCCESS;
}

int checkpoint_list(const char* filename, char tags[][MAX_TAG_LEN], int max_tags) {
    char dir_path[MAX_PATH_LEN];
    DIR* dir;
    struct dirent* entry;
    int count = 0;
    
    get_checkpoint_dir(filename, dir_path);
    
    dir = opendir(dir_path);
    if (dir == NULL) {
        // Directory doesn't exist means no checkpoints
        return 0;
    }
    
    while ((entry = readdir(dir)) != NULL && count < max_tags) {
        // Skip . and ..
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        
        // Check if it's a checkpoint file (.checkpoint extension)
        const char* ext = strstr(entry->d_name, ".checkpoint");
        if (ext && ext[11] == '\0') {  // Extension is at the end
            // Extract tag name (remove .checkpoint extension)
            size_t tag_len = ext - entry->d_name;
            if (tag_len < MAX_TAG_LEN) {
                strncpy(tags[count], entry->d_name, tag_len);
                tags[count][tag_len] = '\0';
                count++;
            }
        }
    }
    
    closedir(dir);
    log_info("Listed %d checkpoints for file '%s'", count, filename);
    return count;
}

int checkpoint_exists(const char* filename, const char* tag) {
    char checkpoint_path[MAX_PATH_LEN];
    struct stat st;
    
    get_checkpoint_path(filename, tag, checkpoint_path);
    return (stat(checkpoint_path, &st) == 0);
}

void checkpoint_cleanup() {
    // TODO: Implement cleanup of old checkpoints
    // Keep last 10 checkpoints per file, delete older ones
    log_info("Checkpoint cleanup not yet implemented");
}
