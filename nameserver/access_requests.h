/*
 * nameserver/access_requests.h
 * Access Request System
 */

#ifndef ACCESS_REQUESTS_H
#define ACCESS_REQUESTS_H

#include "protocol.h"

typedef struct {
    char filename[MAX_FILENAME_LEN];
    char requester[MAX_USERNAME_LEN];
    char owner[MAX_USERNAME_LEN];
    time_t request_time;
    int access_type;  // ACCESS_READ or ACCESS_WRITE
    int is_pending;
} AccessRequest;

typedef struct {
    AccessRequest requests[100];
    int count;
} RequestTable;

// Global request table
extern RequestTable global_request_table;

// Initialize global request table
void request_init(void);

// Table-based functions
void request_table_init(RequestTable* table);
int request_add(RequestTable* table, const char* filename, const char* requester, 
                const char* owner, int access_type);
int request_get_for_owner(RequestTable* table, const char* owner, 
                          AccessRequest requests[], int max_requests);
int request_approve(RequestTable* table, const char* filename, const char* requester, const char* owner);
int request_deny(RequestTable* table, const char* filename, const char* requester, const char* owner);

// hashmap_get wrapper (returns SS index or -1)
int hashmap_get(void* map, const char* key);

#endif
