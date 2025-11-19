[![Review Assignment Due Date](https://classroom.github.com/assets/deadline-readme-button-22041afd0340ce965d47ae6ef1cefeee28c7c493a6346c4f15d667ab976d596c.svg)](https://classroom.github.com/a/0ek2UV58)

# Docs++ - Distributed Document Collaboration System

**Course Project - Operating Systems and Networks (OSN)**  
**Score: 230/250 marks (92%)**  
**Submission Date: 19 November 2025**

---

## 📋 Project Overview

Docs++ is a distributed file system similar to Google Docs, implemented from scratch in C with support for:
- Concurrent access and editing
- Access control and permissions
- Sentence-level locking
- Real-time file streaming
- Hierarchical folder structure
- File versioning with checkpoints
- Access request workflows

---

## 🏗️ Architecture

### Components
1. **Name Server (NS)** - Central coordinator running on port 8080
2. **Storage Servers (SS)** - File storage nodes (ports 8081-8082+)
3. **Clients** - User interface for file operations

### Communication Model
```
Client → Name Server → Storage Server
         (Routing)     (File Operations)
```

---

## ✅ Implemented Features (230/250 marks)

### Base Functionality (200/200 marks)

| Feature | Command | Marks | Status |
|---------|---------|-------|--------|
| View Files | `VIEW [-a] [-l]` | 10 | ✅ |
| Read File | `READ <filename>` | 10 | ✅ |
| Create File | `CREATE <filename>` | 10 | ✅ |
| Write File | `WRITE <filename> <sentence>` | 30 | ✅ |
| Undo Change | `UNDO <filename>` | 15 | ✅ |
| File Info | `INFO <filename>` | 10 | ✅ |
| Delete File | `DELETE <filename>` | 10 | ✅ |
| Stream Content | `STREAM <filename>` | 15 | ✅ |
| List Users | `LIST` | 10 | ✅ |
| Access Control | `ADDACCESS/REMACCESS` | 15 | ✅ |
| Execute File | `EXEC <filename>` | 15 | ✅ |

**System Requirements (40/40 marks):**
- ✅ Data Persistence (10 marks)
- ✅ Access Control (5 marks)
- ✅ Logging (5 marks)
- ✅ Error Handling (5 marks)
- ✅ Efficient Search - O(1) HashMap + LRU Cache (15 marks)

### Bonus Features (30/50 marks)

| Feature | Commands | Marks | Status |
|---------|----------|-------|--------|
| Hierarchical Folders | `CREATEFOLDER`, `MOVE`, `VIEWFOLDER` | 10 | ✅ |
| Checkpoints | `CHECKPOINT`, `VIEWCHECKPOINT`, `REVERT`, `LISTCHECKPOINTS` | 15 | ✅ |
| Access Requests | `REQUESTACCESS`, `VIEWREQUESTS`, `APPROVEREQUEST`, `DENYREQUEST` | 5 | ✅ |
| Fault Tolerance | Replication, heartbeat, recovery | 15 | ❌ |
| Content Search | Inverted index search | 5 | ❌ |

---

## 🚀 Quick Start

### Compilation
```bash
make clean
make
```

### Running the System

**1. Start Name Server:**
```bash
./nameserver/nameserver
```

**2. Start Storage Server:**
```bash
./storage_server/storage_server 127.0.0.1 8081 8082
```

**3. Start Client:**
```bash
./client/client
```
Enter username when prompted.

---

## 📖 Usage Examples

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

---

## 📁 Project Structure

```
├── client/
│   └── client.c                 # Client implementation
├── nameserver/
│   ├── nameserver.c             # Name server core
│   ├── acl.c/h                  # Access control lists
│   └── access_requests.c/h      # Access request system
├── storage_server/
│   ├── storage_server.c         # Storage server core
│   ├── sentence_lock.c/h        # Sentence-level locking
│   ├── sentence_parser.c/h      # Sentence/word parsing
│   ├── undo_buffer.c/h          # Undo functionality
│   ├── folder_manager.c/h       # Hierarchical folders (bonus)
│   └── checkpoint_manager.c/h   # Checkpoints (bonus)
├── common/
│   ├── protocol.h               # Message protocol
│   ├── socket_utils.c/h         # TCP socket utilities
│   └── logger.c/h               # Logging system
├── tests/
│   ├── test_acl_system.sh
│   ├── test_acl_metadata.sh
│   └── test_concurrent.sh
├── Manual_test(1).md            # Comprehensive test cases
├── COMPLIANCE_REPORT.md         # Detailed requirements compliance
└── Makefile
```

---

## 🔧 Technical Details

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

## 📊 Performance Characteristics

- **File Lookup:** O(1) average (HashMap + LRU cache)
- **Concurrent Reads:** Unlimited (no locking)
- **Concurrent Writes:** Sentence-level granularity
- **Persistence:** All files and ACL data survive restarts
- **Scalability:** Multiple storage servers supported

---

## 🧪 Testing

### Test Coverage
- **Manual_test(1).md:** 16 comprehensive test scenarios
- **Part 1-15:** Base functionality (200 marks)
- **Part 16:** Bonus features (500+ lines)
- **Scripts:** ACL, concurrency, metadata tests

### Running Tests
```bash
# ACL system test
bash tests/test_acl_system.sh

# ACL persistence test
bash tests/test_acl_metadata.sh

# Concurrent write test
bash tests/test_concurrent.sh
```

---

## 📝 Requirements Compliance

| Category | Score | Percentage |
|----------|-------|------------|
| User Functionalities | 150/150 | 100% |
| System Requirements | 40/40 | 100% |
| Specifications | 10/10 | 100% |
| Bonus Features | 30/50 | 60% |
| **TOTAL** | **230/250** | **92%** |

See [COMPLIANCE_REPORT.md](COMPLIANCE_REPORT.md) for detailed analysis.

---

## 🎯 Key Achievements

1. ✅ **100% Base Compliance** - All required features implemented
2. ✅ **O(1) Search** - HashMap + LRU exceeds O(N) requirement
3. ✅ **Robust Concurrency** - Sentence-level locking, thread-safe ACL
4. ✅ **Complete Persistence** - Files and metadata survive restarts
5. ✅ **Comprehensive Logging** - Detailed operation tracking
6. ✅ **3 Bonus Features** - Folders, checkpoints, access requests
7. ✅ **Extensive Testing** - 500+ lines of test documentation

---

## 🛠️ Build Requirements

- **Compiler:** GCC with C99 support
- **Libraries:** POSIX (pthread, sys/socket, dirent, time)
- **OS:** Linux (tested on Ubuntu)

---

## 📚 Documentation

- `Manual_test(1).md` - Complete testing guide
- `COMPLIANCE_REPORT.md` - Requirements verification
- `BONUS_FEATURES.md` - Bonus feature documentation
- `IMPLEMENTATION_STATUS.md` - Integration guide

---

## 👥 Team

**Course:** Operating Systems and Networks (OSN)  
**Institution:** IIIT Hyderabad  
**Semester:** Monsoon 2025

---

## 📄 License

Academic project for OSN course. All rights reserved.

---

**Note:** This project achieves 92% compliance (230/250 marks) with full implementation of all base requirements and 3 out of 5 bonus features.
