/*
 * common/protocol.h
 * 
 * Protocol Design for Docs++ Distributed Document System
 * 
 * This defines the communication protocol between:
 * - Client ↔ Name Server
 * - Name Server ↔ Storage Server
 * - Client ↔ Storage Server
 */

#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>
#include <time.h>

/* ============================================================================
 * OPERATION CODES - Define what action is being requested
 * ============================================================================ */

// File Operations (10-19)
#define OP_CREATE           10
#define OP_READ             11
#define OP_WRITE            12
#define OP_DELETE           13
#define OP_STREAM           14
#define OP_UNDO             15

// Metadata Operations (20-29)
#define OP_VIEW             20
#define OP_INFO             21
#define OP_LIST             22

// Access Control Operations (30-39)
#define OP_ADDACCESS        30
#define OP_REMACCESS        31

// System Operations (40-49)
#define OP_EXEC             40

// Registration Operations (50-59)
#define OP_REGISTER_SS      50
#define OP_REGISTER_CLIENT  51
#define OP_UNREGISTER_CLIENT 52  // Client disconnection

// Internal Operations (60-69)
#define OP_ROUTE_INFO       60  // NS sends SS connection info to client
#define OP_ACK              61  // Acknowledgment
#define OP_STOP             62  // End of transmission
#define OP_LOCK_SENTENCE    63  // Request sentence lock
#define OP_UNLOCK_SENTENCE  64  // Release sentence lock
#define OP_GET_METADATA     65  // NS requests metadata from SS

// Write Operations (70-79)
#define OP_WRITE_START      70  // Start WRITE session
#define OP_WRITE_UPDATE     71  // Send word update
#define OP_WRITE_COMMIT     72  // ETIRW - commit changes

// Bonus: Folder Operations (80-89)
#define OP_CREATEFOLDER     80  // Create folder
#define OP_MOVE             81  // Move file to folder
#define OP_VIEWFOLDER       82  // List files in folder

// Bonus: Checkpoint Operations (90-99)
#define OP_CHECKPOINT       90  // Create checkpoint
#define OP_VIEWCHECKPOINT   91  // View checkpoint content
#define OP_REVERT           92  // Revert to checkpoint
#define OP_LISTCHECKPOINTS  93  // List all checkpoints

// Bonus: Access Request Operations (100-109)
#define OP_REQUESTACCESS    100 // Request access to file
#define OP_VIEWREQUESTS     101 // View pending requests
#define OP_APPROVEREQUEST   102 // Approve access request
#define OP_DENYREQUEST      103 // Deny access request

// Bonus: Replication Operations (110-119)
#define OP_REPLICATE        110 // Replicate file to backup SS
#define OP_HEARTBEAT        111 // SS heartbeat
#define OP_SEARCH           112 // Content search

/* ============================================================================
 * ERROR CODES - Standard HTTP-like error codes
 * ============================================================================ */

// Success Codes (200-299)
#define ERR_SUCCESS         200
#define ERR_CREATED         201
#define ERR_ACCEPTED        202

// Client Errors (400-499)
#define ERR_BAD_REQUEST     400  // Invalid request format
#define ERR_UNAUTHORIZED    401  // Not registered
#define ERR_ACCESS_DENIED   403  // No permission
#define ERR_PERMISSION_DENIED 403  // Alias for ACCESS_DENIED
#define ERR_NOT_FOUND       404  // File/User not found
#define ERR_CONFLICT        409  // File already exists
#define ERR_ALREADY_EXISTS  409  // Alias for CONFLICT
#define ERR_LOCKED          423  // Sentence locked by another user
#define ERR_INDEX_ERROR     450  // Invalid sentence/word index

// Server Errors (500-599)
#define ERR_SERVER_ERROR    500  // Internal server error
#define ERR_NOT_IMPLEMENTED 501  // Feature not implemented
#define ERR_SS_UNAVAILABLE  503  // Storage Server unavailable

/* ============================================================================
 * ACCESS FLAGS - For access control
 * ============================================================================ */

#define ACCESS_NONE         0
#define ACCESS_READ         1
#define ACCESS_WRITE        2  // Write implies read
#define ACCESS_OWNER        3  // Owner has all permissions

/* ============================================================================
 * VIEW FLAGS - For VIEW command
 * ============================================================================ */

#define VIEW_FLAG_NONE      0
#define VIEW_FLAG_ALL       1  // -a flag
#define VIEW_FLAG_LONG      2  // -l flag
#define VIEW_FLAG_ALL_LONG  3  // -al flag

/* ============================================================================
 * MESSAGE STRUCTURE - Core communication unit
 * ============================================================================ */

#define MAX_FILENAME_LEN    256
#define MAX_USERNAME_LEN    64
#define MAX_CONTENT_LEN     4096
#define MAX_IP_LEN          16
#define MAX_PATH_LEN        512

typedef struct {
    /* Message Type & Control */
    int operation;              // Operation code (OP_*)
    int error_code;             // Error code (ERR_*)
    
    /* Sender/Receiver Info */
    char sender_id[MAX_USERNAME_LEN];   // Who sent this message
    char target_user[MAX_USERNAME_LEN]; // For access control operations
    
    /* File Operations */
    char filename[MAX_FILENAME_LEN];    // Target file
    int sentence_num;                   // For WRITE operations
    int word_index;                     // For WRITE operations
    char content[MAX_CONTENT_LEN];      // File content or word content
    
    /* Access Control */
    int access_type;                    // ACCESS_READ, ACCESS_WRITE, etc.
    int view_flags;                     // For VIEW command flags
    
    /* Bonus: Folder Operations */
    char folder_name[MAX_FILENAME_LEN]; // For folder operations
    char checkpoint_tag[MAX_FILENAME_LEN]; // For checkpoint operations
    
    /* Bonus: Replication */
    int replica_index;                  // Which replica to use
    int is_primary;                     // Primary vs backup SS
    
    /* Routing Info (for NS responses) */
    char ss_ip[MAX_IP_LEN];            // Storage Server IP
    int ss_port;                        // Storage Server port for clients
    
    /* Metadata */
    long timestamp;                     // Message timestamp
    size_t content_length;              // Actual length of content
    int sequence_num;                   // For ordered delivery
    
    /* Registration Info */
    char ip_address[MAX_IP_LEN];       // For registration
    int nm_port;                        // Port for NM communication
    int client_port;                    // Port for client communication
    
} Message;

/* ============================================================================
 * FILE METADATA STRUCTURE - For INFO and VIEW -l commands
 * ============================================================================ */

typedef struct {
    char filename[MAX_FILENAME_LEN];
    char owner[MAX_USERNAME_LEN];
    time_t created_time;
    time_t modified_time;
    time_t accessed_time;
    size_t file_size;           // In bytes
    int word_count;
    int char_count;
    int sentence_count;
    char accessed_by[MAX_USERNAME_LEN];  // Last accessed by whom
} FileMetadata;

/* ============================================================================
 * STORAGE SERVER INFO - For NS to track SS
 * ============================================================================ */

typedef struct {
    char ip[MAX_IP_LEN];
    int nm_port;                // Port for NS communication
    int client_port;            // Port for client communication
    int is_alive;               // Heartbeat status
    time_t last_heartbeat;
    int file_count;
    char files[100][MAX_FILENAME_LEN];  // List of files on this SS
} StorageServerInfo;

/* ============================================================================
 * CLIENT INFO - For NS to track clients
 * ============================================================================ */

typedef struct {
    char username[MAX_USERNAME_LEN];
    char ip[MAX_IP_LEN];
    int port;
    time_t connected_time;
    int is_active;
} ClientInfo;

/* ============================================================================
 * SENTENCE LOCK INFO - For SS to track locks
 * ============================================================================ */

typedef struct {
    char filename[MAX_FILENAME_LEN];
    int sentence_num;
    char locked_by[MAX_USERNAME_LEN];
    time_t lock_time;
    int is_locked;
} SentenceLock;

/* ============================================================================
 * HELPER MACROS
 * ============================================================================ */

// Initialize a message
#define INIT_MESSAGE(msg) do { \
    memset(&(msg), 0, sizeof(Message)); \
    (msg).timestamp = time(NULL); \
    (msg).error_code = ERR_SUCCESS; \
} while(0)

// Check if operation is a file operation
#define IS_FILE_OP(op) ((op) >= 10 && (op) <= 19)

// Check if operation needs authentication
#define NEEDS_AUTH(op) ((op) >= 10 && (op) <= 49)

// Check if error indicates success
#define IS_SUCCESS(err) ((err) >= 200 && (err) < 300)

// Check if error is client error
#define IS_CLIENT_ERROR(err) ((err) >= 400 && (err) < 500)

// Check if error is server error
#define IS_SERVER_ERROR(err) ((err) >= 500 && (err) < 600)

/* ============================================================================
 * PROTOCOL CONSTANTS
 * ============================================================================ */

#define DEFAULT_NS_PORT         8080
#define DEFAULT_SS_NM_PORT      8081
#define DEFAULT_SS_CLIENT_PORT  8082
#define DEFAULT_CLIENT_PORT     8083

#define LOCK_TIMEOUT_SECONDS    60      // Lock expires after 60 seconds
#define MAX_CONCURRENT_CLIENTS  100
#define MAX_STORAGE_SERVERS     10
#define STREAM_WORD_DELAY_MS    100     // 0.1 seconds = 100 milliseconds

#define BUFFER_SIZE             4096
#define MAX_SENTENCE_LENGTH     10000
#define MAX_WORD_LENGTH         256

/* ============================================================================
 * ERROR MESSAGES - Human readable error messages
 * ============================================================================ */

__attribute__((unused))
static const char* get_error_message(int error_code) {
    switch(error_code) {
        case ERR_SUCCESS:           return "Success";
        case ERR_CREATED:           return "Created successfully";
        case ERR_ACCEPTED:          return "Request accepted";
        
        case ERR_BAD_REQUEST:       return "Bad request format";
        case ERR_UNAUTHORIZED:      return "Not authorized - register first";
        case ERR_ACCESS_DENIED:     return "Access denied - insufficient permissions";
        case ERR_NOT_FOUND:         return "File or resource not found";
        case ERR_CONFLICT:          return "Resource already exists";
        case ERR_LOCKED:            return "Sentence locked by another user";
        case ERR_INDEX_ERROR:       return "Invalid sentence or word index";
        
        case ERR_SERVER_ERROR:      return "Internal server error";
        case ERR_NOT_IMPLEMENTED:   return "Feature not implemented";
        case ERR_SS_UNAVAILABLE:    return "Storage server unavailable";
        
        default:                    return "Unknown error";
    }
}

/* ============================================================================
 * OPERATION NAMES - For logging and debugging
 * ============================================================================ */

__attribute__((unused))
static const char* get_operation_name(int operation) {
    switch(operation) {
        case OP_CREATE:             return "CREATE";
        case OP_READ:               return "READ";
        case OP_WRITE:              return "WRITE";
        case OP_DELETE:             return "DELETE";
        case OP_STREAM:             return "STREAM";
        case OP_UNDO:               return "UNDO";
        
        case OP_VIEW:               return "VIEW";
        case OP_INFO:               return "INFO";
        case OP_LIST:               return "LIST";
        
        case OP_ADDACCESS:          return "ADDACCESS";
        case OP_REMACCESS:          return "REMACCESS";
        
        case OP_EXEC:               return "EXEC";
        
        case OP_REGISTER_SS:        return "REGISTER_SS";
        case OP_REGISTER_CLIENT:    return "REGISTER_CLIENT";
        
        case OP_ROUTE_INFO:         return "ROUTE_INFO";
        case OP_ACK:                return "ACK";
        case OP_STOP:               return "STOP";
        case OP_LOCK_SENTENCE:      return "LOCK_SENTENCE";
        case OP_UNLOCK_SENTENCE:    return "UNLOCK_SENTENCE";
        case OP_GET_METADATA:       return "GET_METADATA";
        
        case OP_WRITE_START:        return "WRITE_START";
        case OP_WRITE_UPDATE:       return "WRITE_UPDATE";
        case OP_WRITE_COMMIT:       return "WRITE_COMMIT";
        
        case OP_CREATEFOLDER:       return "CREATEFOLDER";
        case OP_MOVE:               return "MOVE";
        case OP_VIEWFOLDER:         return "VIEWFOLDER";
        
        case OP_CHECKPOINT:         return "CHECKPOINT";
        case OP_VIEWCHECKPOINT:     return "VIEWCHECKPOINT";
        case OP_REVERT:             return "REVERT";
        case OP_LISTCHECKPOINTS:    return "LISTCHECKPOINTS";
        
        case OP_REQUESTACCESS:      return "REQUESTACCESS";
        case OP_VIEWREQUESTS:       return "VIEWREQUESTS";
        case OP_APPROVEREQUEST:     return "APPROVEREQUEST";
        case OP_DENYREQUEST:        return "DENYREQUEST";
        
        case OP_REPLICATE:          return "REPLICATE";
        case OP_HEARTBEAT:          return "HEARTBEAT";
        
        case OP_SEARCH:             return "SEARCH";
        
        default:                    return "UNKNOWN";
    }
}


/* ============================================================================
 * USAGE EXAMPLES - How to use this protocol
 * ============================================================================ */

/*

EXAMPLE 1: Client sends CREATE request to NS
---------------------------------------------
Message msg;
INIT_MESSAGE(msg);
msg.operation = OP_CREATE;
strcpy(msg.sender_id, "user1");
strcpy(msg.filename, "test.txt");
send_message(ns_socket, &msg);


EXAMPLE 2: NS responds with SUCCESS
------------------------------------
Message response;
INIT_MESSAGE(response);
response.operation = OP_ACK;
response.error_code = ERR_CREATED;
send_message(client_socket, &response);


EXAMPLE 3: Client requests READ, NS sends routing info
-------------------------------------------------------
// Client to NS
Message request;
INIT_MESSAGE(request);
request.operation = OP_READ;
strcpy(request.sender_id, "user1");
strcpy(request.filename, "test.txt");
send_message(ns_socket, &request);

// NS to Client (routing info)
Message route;
INIT_MESSAGE(route);
route.operation = OP_ROUTE_INFO;
strcpy(route.ss_ip, "127.0.0.1");
route.ss_port = 8082;
send_message(client_socket, &route);

// Client then connects to SS directly


EXAMPLE 4: WRITE operation with sentence lock
----------------------------------------------
// Client requests lock
Message lock_req;
INIT_MESSAGE(lock_req);
lock_req.operation = OP_LOCK_SENTENCE;
strcpy(lock_req.filename, "test.txt");
lock_req.sentence_num = 1;
strcpy(lock_req.sender_id, "user1");
send_message(ss_socket, &lock_req);

// If lock acquired, send updates
Message update;
INIT_MESSAGE(update);
update.operation = OP_WRITE_UPDATE;
strcpy(update.filename, "test.txt");
update.sentence_num = 1;
update.word_index = 3;
strcpy(update.content, "hello");
send_message(ss_socket, &update);

// Commit changes
Message commit;
INIT_MESSAGE(commit);
commit.operation = OP_WRITE_COMMIT;
strcpy(commit.filename, "test.txt");
commit.sentence_num = 1;
send_message(ss_socket, &commit);


EXAMPLE 5: Error handling
--------------------------
Message response;
receive_message(socket, &response);

if (!IS_SUCCESS(response.error_code)) {
    printf("Error: %s\n", get_error_message(response.error_code));
    return -1;
}


EXAMPLE 6: Access control
--------------------------
Message access_req;
INIT_MESSAGE(access_req);
access_req.operation = OP_ADDACCESS;
strcpy(access_req.sender_id, "user1");  // Owner
strcpy(access_req.filename, "test.txt");
access_req.access_type = ACCESS_READ;
send_message(ns_socket, &access_req);

*/

#endif /* PROTOCOL_H */