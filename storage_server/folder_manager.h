/*
 * storage_server/folder_manager.h
 * 
 * Hierarchical Folder Structure Management
 */

#ifndef FOLDER_MANAGER_H
#define FOLDER_MANAGER_H

#include "protocol.h"

#define MAX_FOLDERS 100
#define MAX_FILES_PER_FOLDER 100
#define MAX_PATH_LEN 512

// External storage directory (defined in storage_server.c)
extern char STORAGE_DIR[MAX_PATH_LEN];

// Create a new folder
int folder_create(const char* folder_name, const char* owner);

// Move file to folder
int folder_move_file(const char* filename, const char* folder_name, const char* owner);

// List files in folder
int folder_list_files(const char* folder_name, char files[][MAX_FILENAME_LEN], int max_files);

// Check if folder exists
int folder_exists(const char* folder_name);

// Get full path for file in folder
void folder_get_file_path(const char* folder_name, const char* filename, char* full_path);

#endif // FOLDER_MANAGER_H
