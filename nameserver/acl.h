/*
 * nameserver/acl.h
 * 
 * Access Control List (ACL) System for Docs++
 * Manages file ownership and permissions (read/write access)
 * 
 * Design:
 * - HashMap-based structure for O(1) access checks
 * - Each file has: owner, list of users with read access, list with write access
 * - Owner always has both read and write permissions
 * - Write access implies read access
 * - Persistent storage to survive NS restarts
 */

#ifndef ACL_H
#define ACL_H

#include <time.h>
#include "protocol.h"

/* ============================================================================
 * ACL DATA STRUCTURES
 * ============================================================================ */

#define ACL_MAX_USERS 50  // Max users per access list
#define ACL_HASH_SIZE 1009  // Prime number for hash table

// Access Control Entry - permissions for a single file
typedef struct {
    char filename[MAX_FILENAME_LEN];
    char owner[MAX_USERNAME_LEN];
    
    // Read access list
    char read_users[ACL_MAX_USERS][MAX_USERNAME_LEN];
    int read_count;
    
    // Write access list (write implies read)
    char write_users[ACL_MAX_USERS][MAX_USERNAME_LEN];
    int write_count;
    
    time_t created_time;
} AclEntry;

// Hash node for chaining collisions
typedef struct AclNode {
    AclEntry entry;
    struct AclNode* next;
} AclNode;

// ACL Hash Table
typedef struct {
    AclNode* buckets[ACL_HASH_SIZE];
    int file_count;
} AclTable;

/* ============================================================================
 * ACL FUNCTIONS
 * ============================================================================ */

/**
 * Initialize the ACL system
 * Must be called before any other ACL operations
 */
void acl_init(AclTable* acl);

/**
 * Add a new file to ACL with owner
 * Owner automatically gets read and write access
 * 
 * @param acl ACL table
 * @param filename Name of the file
 * @param owner Username of the owner
 * @return 0 on success, -1 on error
 */
int acl_add_file(AclTable* acl, const char* filename, const char* owner);

/**
 * Check if a user has read access to a file
 * Returns true if user is owner, in read_users, or in write_users
 * 
 * @param acl ACL table
 * @param filename Name of the file
 * @param username User to check
 * @return 1 if has access, 0 if not
 */
int acl_check_read(AclTable* acl, const char* filename, const char* username);

/**
 * Check if a user has write access to a file
 * Returns true if user is owner or in write_users
 * 
 * @param acl ACL table
 * @param filename Name of the file
 * @param username User to check
 * @return 1 if has access, 0 if not
 */
int acl_check_write(AclTable* acl, const char* filename, const char* username);

/**
 * Check if a user is the owner of a file
 * 
 * @param acl ACL table
 * @param filename Name of the file
 * @param username User to check
 * @return 1 if is owner, 0 if not
 */
int acl_check_owner(AclTable* acl, const char* filename, const char* username);

/**
 * Add access permission to a file
 * Only owner can grant access
 * 
 * @param acl ACL table
 * @param filename Name of the file
 * @param username User to grant access to
 * @param access_type ACCESS_READ or ACCESS_WRITE
 * @return 0 on success, -1 on error
 */
int acl_add_access(AclTable* acl, const char* filename, const char* username, int access_type);

/**
 * Remove access permission from a file
 * Only owner can revoke access
 * Removes user from both read and write lists
 * 
 * @param acl ACL table
 * @param filename Name of the file
 * @param username User to revoke access from
 * @return 0 on success, -1 on error
 */
int acl_remove_access(AclTable* acl, const char* filename, const char* username);

/**
 * Remove a file from ACL (when file is deleted)
 * 
 * @param acl ACL table
 * @param filename Name of the file
 * @return 0 on success, -1 if file not found
 */
int acl_remove_file(AclTable* acl, const char* filename);

/**
 * Get the owner of a file
 * 
 * @param acl ACL table
 * @param filename Name of the file
 * @param owner_buf Buffer to store owner username
 * @param buf_size Size of owner_buf
 * @return 0 on success, -1 if file not found
 */
int acl_get_owner(AclTable* acl, const char* filename, char* owner_buf, size_t buf_size);

/**
 * Get formatted access list for a file
 * Format: "owner (RW), user1 (R), user2 (RW)"
 * 
 * @param acl ACL table
 * @param filename Name of the file
 * @param buf Buffer to store formatted list
 * @param buf_size Size of buffer
 * @return 0 on success, -1 if file not found
 */
int acl_get_access_list(AclTable* acl, const char* filename, char* buf, size_t buf_size);

/**
 * Get all files in the ACL
 * 
 * @param acl ACL table
 * @param files Array to store filenames
 * @param max_files Maximum number of files to return
 * @return Number of files returned
 */
int acl_get_all_files(AclTable* acl, char files[][MAX_FILENAME_LEN], int max_files);

/**
 * Get files accessible by a user (has at least read access)
 * 
 * @param acl ACL table
 * @param username Username to check
 * @param files Array to store filenames
 * @param max_files Maximum number of files to return
 * @return Number of files returned
 */
int acl_get_accessible_files(AclTable* acl, const char* username, 
                              char files[][MAX_FILENAME_LEN], int max_files);

/**
 * Save ACL to persistent storage
 * 
 * @param acl ACL table
 * @param filepath Path to save file
 * @return 0 on success, -1 on error
 */
int acl_save(AclTable* acl, const char* filepath);

/**
 * Load ACL from persistent storage
 * 
 * @param acl ACL table (must be initialized)
 * @param filepath Path to load file
 * @return 0 on success, -1 on error
 */
int acl_load(AclTable* acl, const char* filepath);

/**
 * Print ACL statistics (for debugging)
 * 
 * @param acl ACL table
 */
void acl_stats(AclTable* acl);

#endif /* ACL_H */
