/*
 * storage_server/folder_manager.c
 * 
 * Hierarchical Folder Structure Management
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <errno.h>
#include "folder_manager.h"
#include "logger.h"

// External storage directory from storage_server.c
extern char STORAGE_DIR[MAX_PATH_LEN];

int folder_create(const char* folder_name, const char* owner) {
    char folder_path[MAX_PATH_LEN];
    
    (void)owner;  // Reserved for future ACL extension
    
    // Create folder path: storage/folder_name
    snprintf(folder_path, sizeof(folder_path), "%s/%s", STORAGE_DIR, folder_name);
    
    // Check if folder already exists
    struct stat st;
    if (stat(folder_path, &st) == 0 && S_ISDIR(st.st_mode)) {
        log_warning("Folder '%s' already exists", folder_name);
        return ERR_CONFLICT;
    }
    
    // Create directory
    if (mkdir(folder_path, 0755) == -1) {
        log_error("Failed to create folder '%s': %s", folder_name, strerror(errno));
        return ERR_SERVER_ERROR;
    }
    
    log_info("Created folder '%s' at '%s'", folder_name, folder_path);
    return ERR_CREATED;
}

int folder_move_file(const char* filename, const char* folder_name, const char* owner) {
    char src_path[MAX_PATH_LEN];
    char dest_path[MAX_PATH_LEN];
    char folder_path[MAX_PATH_LEN];
    
    (void)owner;  // Reserved for future ACL extension
    
    // Check if folder exists
    snprintf(folder_path, sizeof(folder_path), "%s/%s", STORAGE_DIR, folder_name);
    struct stat st;
    if (stat(folder_path, &st) != 0 || !S_ISDIR(st.st_mode)) {
        log_error("Folder '%s' does not exist", folder_name);
        return ERR_NOT_FOUND;
    }
    
    // Check if file exists in root storage
    snprintf(src_path, sizeof(src_path), "%s/%s", STORAGE_DIR, filename);
    if (stat(src_path, &st) != 0 || !S_ISREG(st.st_mode)) {
        log_error("File '%s' not found in root storage", filename);
        return ERR_NOT_FOUND;
    }
    
    // Build destination path
    snprintf(dest_path, sizeof(dest_path), "%s/%s/%s", STORAGE_DIR, folder_name, filename);
    
    // Check if file already exists in destination
    if (stat(dest_path, &st) == 0) {
        log_error("File '%s' already exists in folder '%s'", filename, folder_name);
        return ERR_CONFLICT;
    }
    
    // Move file (rename is atomic)
    if (rename(src_path, dest_path) == -1) {
        log_error("Failed to move file '%s' to folder '%s': %s", 
                  filename, folder_name, strerror(errno));
        return ERR_SERVER_ERROR;
    }
    
    log_info("Moved file '%s' to folder '%s'", filename, folder_name);
    return ERR_SUCCESS;
}

int folder_list_files(const char* folder_name, char files[][MAX_FILENAME_LEN], int max_files) {
    char folder_path[MAX_PATH_LEN];
    DIR* dir;
    struct dirent* entry;
    int count = 0;
    
    snprintf(folder_path, sizeof(folder_path), "%s/%s", STORAGE_DIR, folder_name);
    
    dir = opendir(folder_path);
    if (dir == NULL) {
        log_error("Cannot open folder '%s': %s", folder_name, strerror(errno));
        return -1;
    }
    
    while ((entry = readdir(dir)) != NULL && count < max_files) {
        // Skip . and ..
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        
        // Check if it's a regular file
        char file_path[MAX_PATH_LEN];
        snprintf(file_path, sizeof(file_path), "%s/%s", folder_path, entry->d_name);
        
        struct stat st;
        if (stat(file_path, &st) == 0 && S_ISREG(st.st_mode)) {
            strncpy(files[count], entry->d_name, MAX_FILENAME_LEN - 1);
            files[count][MAX_FILENAME_LEN - 1] = '\0';
            count++;
        }
    }
    
    closedir(dir);
    log_info("Listed %d files in folder '%s'", count, folder_name);
    return count;
}

int folder_exists(const char* folder_name) {
    char folder_path[MAX_PATH_LEN];
    struct stat st;
    
    snprintf(folder_path, sizeof(folder_path), "%s/%s", STORAGE_DIR, folder_name);
    
    return (stat(folder_path, &st) == 0 && S_ISDIR(st.st_mode));
}

void folder_get_file_path(const char* folder_name, const char* filename, char* full_path) {
    if (folder_name && strlen(folder_name) > 0) {
        snprintf(full_path, MAX_PATH_LEN, "%s/%s/%s", STORAGE_DIR, folder_name, filename);
    } else {
        snprintf(full_path, MAX_PATH_LEN, "%s/%s", STORAGE_DIR, filename);
    }
}
