/*
 * nameserver/access_requests.c
 * Access Request System
 */

#include <stdio.h>
#include <string.h>
#include <time.h>
#include "access_requests.h"
#include "logger.h"

// Global request table
RequestTable global_request_table;

void request_init(void) {
    request_table_init(&global_request_table);
    log_info("Global request table initialized");
}

void request_table_init(RequestTable* table) {
    memset(table, 0, sizeof(RequestTable));
    table->count = 0;
}

int request_add(RequestTable* table, const char* filename, const char* requester, 
                const char* owner, int access_type) {
    if (table->count >= 100) {
        log_error("Request table full");
        return ERR_SERVER_ERROR;
    }
    
    // Check for duplicate request
    for (int i = 0; i < table->count; i++) {
        if (table->requests[i].is_pending &&
            strcmp(table->requests[i].filename, filename) == 0 &&
            strcmp(table->requests[i].requester, requester) == 0) {
            log_warning("Duplicate access request from %s for %s", requester, filename);
            return ERR_CONFLICT;
        }
    }
    
    AccessRequest* req = &table->requests[table->count];
    strncpy(req->filename, filename, MAX_FILENAME_LEN - 1);
    strncpy(req->requester, requester, MAX_USERNAME_LEN - 1);
    strncpy(req->owner, owner, MAX_USERNAME_LEN - 1);
    req->request_time = time(NULL);
    req->access_type = access_type;
    req->is_pending = 1;
    table->count++;
    
    log_info("Access request added: %s requests %s access to %s (owner: %s)",
             requester, (access_type == ACCESS_READ) ? "READ" : "WRITE", filename, owner);
    return ERR_SUCCESS;
}

int request_get_for_owner(RequestTable* table, const char* owner, 
                          AccessRequest requests[], int max_requests) {
    int count = 0;
    for (int i = 0; i < table->count && count < max_requests; i++) {
        if (table->requests[i].is_pending && 
            strcmp(table->requests[i].owner, owner) == 0) {
            memcpy(&requests[count], &table->requests[i], sizeof(AccessRequest));
            count++;
        }
    }
    return count;
}

int request_approve(RequestTable* table, const char* filename, const char* requester, const char* owner) {
    for (int i = 0; i < table->count; i++) {
        if (table->requests[i].is_pending &&
            strcmp(table->requests[i].filename, filename) == 0 &&
            strcmp(table->requests[i].requester, requester) == 0 &&
            strcmp(table->requests[i].owner, owner) == 0) {
            table->requests[i].is_pending = 0;
            log_info("Access request approved: %s can access %s", requester, filename);
            return ERR_SUCCESS;
        }
    }
    return ERR_NOT_FOUND;
}

int request_deny(RequestTable* table, const char* filename, const char* requester, const char* owner) {
    for (int i = 0; i < table->count; i++) {
        if (table->requests[i].is_pending &&
            strcmp(table->requests[i].filename, filename) == 0 &&
            strcmp(table->requests[i].requester, requester) == 0 &&
            strcmp(table->requests[i].owner, owner) == 0) {
            table->requests[i].is_pending = 0;
            log_info("Access request denied: %s cannot access %s", requester, filename);
            return ERR_SUCCESS;
        }
    }
    return ERR_NOT_FOUND;
}
