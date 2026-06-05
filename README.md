# Docs++ - Distributed Document Collaboration System

Docs++ is a high-performance, distributed file system built from scratch in C, inspired by the collaborative features of Google Docs. It supports concurrent editing, text streaming, granular access control, file versioning, and more. 

---

## Architecture Overview

The system utilizes a distributed, multi-node architecture that divides responsibilities to ensure stability, speed, and safety:

- **Name Server (NS):** The central coordinator (running on port 8080). It manages client discovery, user registration, maintains directory routing information, and stores persistent Access Control Lists (ACL).
- **Storage Server (SS):** The file storage hubs (e.g., ports 8081, 8082). They execute the raw file operations, enforce concurrent editing locks, and manage advanced local features like Undo Buffers and Checkpoint versions.
- **Client:** The user interface. Clients connect directly to the Name Server to locate files and validate credentials, and then communicate directly with the Storage Servers to perform file operations.

**Communication Model:**
Clients and servers communicate using a custom TCP protocol capable of sending standard HTTP-like status codes and specific command headers (`READ`, `WRITE_START`, `STREAM`, etc.).
---

## High-Level Features & Implementation

Docs++ employs numerous Operating System and Networking concepts to allow seamless synchronization and data integrity.

### 1. Granular Access Control (ACL)
- **What you can do:** Restrict who can read or write to your files. Request access to other people's files and approve/deny requests dynamically.
- **How it's implemented:** Backed by an in-memory Hash Map situated on the Name Server. Each file maintains dynamically growing lists of authorized readers and writers. Access requests are queued, allowing the file owner to asynchronously approve (`APPROVEREQUEST`) or deny (`DENYREQUEST`) them. Data persists on disk to quickly reload permissions if the Name Server restarts. 

### 2. Sentence-Level Locking (Concurrent Editing)
- **What you can do:** Multiple users can safely edit the exact same document at the same time without overwriting each other's changes.
- **How it's implemented:** Instead of locking the entire file, the Storage Servers lock data at the **sentence level**. A concurrent Hash Table enforced with POSIX `pthread_mutex` primitives tracks which sentence is being edited by whom. A background garbage collector thread purges stale locks (e.g., if a client disconnects unexpectedly) to prevent deadlocks.

### 3. File Versioning & Checkpointing
- **What you can do:** Save named versions of your files manually, view history, and instantly revert a file back to any previous checkpoint. 
- **How it's implemented:** A Git-like version control flow backing up distinct snapshots into a `/storage_dir` segment. It manages versions and allows reverting to previous states.

### 4. Real-time File Streaming
- **What you can do:** Watch a file continuously and see updates exactly as they happen, much like standard `tail -f` or live streaming.
- **How it's implemented:** Maintains open stream descriptors in the Storage Server pushing buffer updates immediately back over the TCP socket pipe to connected clients on changes.

### 5. Undo Operations
- **What you can do:** Instantly revert your most recent edit to a file.
- **How it's implemented:** Uses a thread-safe Hash Table mappings approach `UndoTable` managing a distinct, volatile state memory buffer (up to 1MB) limit per active file. Prior to committing a write, the prior state is kept in the buffer. Upon an `UNDO` routine, the memory byte array is written back to the active file.

### 6. Fast Content Traversal & Inverted Index 
- **What you can do:** Execute deep file search efficiently and organize files deeply inside folders.
- **How it's implemented:** A content tracking system maps specific words directly to an array of files via a prime-numbered Hash Map, achieving $O(1)$ fast lookup times. Further, operations like `CREATEFOLDER`, `MOVE`, and `VIEWFOLDER` maintain a virtual hierarchical directory path.

---

## Quick Start

### 1. Compilation
Build the system using the provided Makefile:
```bash
make clean
make
```

### 2. Running the System

You will need to open three separate terminal windows to simulate the distributed environment.

**Start the Name Server:**
```bash
./nameserver/nameserver
```

**Start a Storage Server:**
```bash
./storage_server/storage_server 127.0.0.1 8081 8082
```

**Launch the Client:**
```bash
./client/client
```
*(Enter a username when prompted to register or login)*

---

*This repository relies intricately on POSIX thread implementations, socket programming, and custom robust networking logic built natively in C.*

---

## Usage Examples

### Basic Operations
```bash
docs++> CREATE mydoc.txt
docs++> WRITE mydoc.txt 0
1 Hello world.
ETIRW

docs++> READ mydoc.txt
docs++> INFO mydoc.txt
docs++> DELETE mydoc.txt
```

### Access Control
```bash
docs++> ADDACCESS -R mydoc.txt alice
docs++> ADDACCESS -W mydoc.txt bob
docs++> REMACCESS mydoc.txt alice
```

### Bonus Features
```bash
# Folders
docs++> CREATEFOLDER reports
docs++> MOVE mydoc.txt reports
docs++> VIEWFOLDER reports

# Checkpoints
docs++> CHECKPOINT mydoc.txt v1
docs++> VIEWCHECKPOINT mydoc.txt v1
docs++> REVERT mydoc.txt v1
docs++> LISTCHECKPOINTS mydoc.txt

# Access Requests
docs++> REQUESTACCESS mydoc.txt
docs++> VIEWREQUESTS mydoc.txt
docs++> APPROVEREQUEST mydoc.txt alice
```

## Technical Details

### Concurrency
- **Name Server:** `select()` multiplexing for concurrent connections
- **Storage Server:** Dual threads (NS inbound + client server)
- **Write Operations:** Sentence-level locking prevents conflicts
- **ACL Operations:** Thread-safe with pthread_mutex

### Data Structures
- **HashMap:** O(1) file lookup (1009 buckets, DJB2 hash)
- **LRU Cache:** 100-entry cache for recent searches
- **ACL Table:** Persistent access control with file-based storage
- **Lock Table:** Per-sentence locks with client tracking

### Error Handling
Universal error codes (HTTP-like):
- 200-202: Success codes
- 400-499: Client errors (bad request, unauthorized, not found, etc.)
- 500-599: Server errors

### Logging
All operations logged with:
- Timestamp (YYYY-MM-DD HH:MM:SS)
- Component (NS/SS/CLIENT)
- Operation type
- Username, IP, port
- Result status

---

## Performance Characteristics

- **File Lookup:** O(1) average (HashMap + LRU cache)
- **Concurrent Reads:** Unlimited (no locking)
- **Concurrent Writes:** Sentence-level granularity
- **Persistence:** All files and ACL data survive restarts
- **Scalability:** Multiple storage servers supported