# 📋 Docs++ Manual Testing Guide
## Comprehensive Testing for Days 3-7 Implementation

**Date:** November 16, 2025  
**Status:** Days 3-7 Complete ✅  
**Features to Test:** CREATE, READ, DELETE, VIEW, INFO, LIST, ADDACCESS, REMACCESS

---

## 🎯 Testing Overview

### ✅ **What's Implemented:**
- **Days 3-5:** Core file operations (CREATE, READ, DELETE)
- **Days 6-7:** Access control & metadata (VIEW, INFO, LIST, ADDACCESS, REMACCESS)

### 📊 **Testing Categories:**
1. Basic Operations Testing
2. Access Control Testing
3. Metadata & Information Testing
4. Multi-User Testing
5. Error Handling Testing
6. Data Persistence Testing

---

## 🚀 PART 1: SYSTEM STARTUP

### **Step 1.1: Clean Start**
```bash
# Navigate to project directory
cd "/home/chai404/OSN/Course Project/course-project-osn-3-0"

# Clean previous build
make clean

# Build everything
make all

# Expected Output: ✓ Build completed successfully!
```

### **Step 1.2: Start Name Server (Terminal 1)**
```bash
# Terminal 1
make run-ns

# Expected Output:
# ==========================================================
# Name Server Starting - Docs++ Distributed Document System
# ==========================================================
# Registries initialized with HashMap (O(1) lookup)
# Server socket created (fd: X)
# Socket bound to port 8080
# Server listening on port 8080 with select() multiplexing
# Ready to handle concurrent SS and Client connections
```

**✅ Checkpoint:** Name Server should be running without errors

### **Step 1.3: Start Storage Server (Terminal 2)**
```bash
# Terminal 2
make run-ss

# Expected Output:
# ============================================================
# Storage Server Starting - Docs++ Distributed Document System
# ============================================================
# Storage directory ensured: ./storage
# Found existing file: [any existing files]
# Registering with Name Server at 127.0.0.1:8080
# Connected to Name Server
# Serialized X files into registration content
# Registration successful: [confirmation message]
```

**✅ Checkpoint:** Storage Server connected to Name Server successfully

### **Step 1.4: Verify Logs**
```bash
# Terminal 3 (or new terminal)
make logs-main

# Check for:
# - Name Server: Listening messages
# - Storage Server: Registration success
# - No error messages
```

---

## 📝 PART 2: BASIC OPERATIONS TESTING (Days 3-5)

### **Test 2.1: Client Registration & CREATE**

#### **Terminal 3: Start Client as user1**
```bash
make run-client

# When prompted:
Enter your username: user1

# Expected Output:
# ╔════════════════════════════════════════════════════════════╗
# ║                    Docs++ Client                           ║
# ║         Distributed Document Collaboration System          ║
# ╚════════════════════════════════════════════════════════════╝
# 
# Connecting to Name Server...
# ✓ Client 'user1' registered successfully
# 
# Type 'HELP' for list of commands
```

**✅ Checkpoint:** Client registered successfully

#### **Test: HELP Command**
```bash
docs++> HELP

# Expected: Display all available commands with descriptions
```

#### **Test: CREATE Command**
```bash
docs++> CREATE testfile1.txt
# Expected: ✓ File 'testfile1.txt' created successfully!

docs++> CREATE document.txt
# Expected: ✓ File 'document.txt' created successfully!

docs++> CREATE mydata.txt
# Expected: ✓ File 'mydata.txt' created successfully!
```

**✅ Checkpoint:** Files created without errors

#### **Test: CREATE Duplicate File (Error Handling)**
```bash
docs++> CREATE testfile1.txt
# Expected: ✗ Error: File 'testfile1.txt' already exists
```

**✅ Checkpoint:** Duplicate detection works

---

### **Test 2.2: READ Operation**

#### **Test: READ Empty File**
```bash
docs++> READ testfile1.txt

# Expected Output:
# ─────────────────────────────────────────────────────────────
# File: testfile1.txt
# ─────────────────────────────────────────────────────────────
# (empty file)
# ─────────────────────────────────────────────────────────────
```

**✅ Checkpoint:** Can read empty files

#### **Test: READ Non-existent File (Error Handling)**
```bash
docs++> READ nonexistent.txt
# Expected: ✗ Error: File 'nonexistent.txt' not found
```

**✅ Checkpoint:** Error handling for missing files works

---

### **Test 2.2A: Concurrent READ Operations (Spec Requirement!)**

**Purpose:** Verify "Files support concurrent access for both reading and writing... allows multiple users to view or edit the file simultaneously"

#### **Setup: Create test file**
```bash
docs++> CREATE concurrent_read.txt

# Add content via storage server directly (for testing)
# Or use WRITE command if implemented
# Content: "This is test content for concurrent reads."
```

#### **Test: Multiple Users Reading Simultaneously**

**Terminal 3 (user1):**
```bash
docs++> READ concurrent_read.txt
# Expected: This is test content for concurrent reads.
```

**Terminal 4 (user2) - SIMULTANEOUSLY:**
```bash
# Start second client
make run-client
# Username: user2

# Grant access first via user1:
# ADDACCESS -R concurrent_read.txt user2

docs++> READ concurrent_read.txt
# Expected: This is test content for concurrent reads.
```

**Terminal 5 (user3) - SIMULTANEOUSLY:**
```bash
# Start third client
make run-client
# Username: user3

# Grant access first via user1:
# ADDACCESS -R concurrent_read.txt user3

docs++> READ concurrent_read.txt
# Expected: This is test content for concurrent reads.
```

**✅ PASS CRITERIA:**
- All 3 users can READ simultaneously
- No lock errors (READ doesn't lock)
- All get same content
- No "file busy" errors

**Note:** READ operations should NOT block each other - only WRITE locks sentences

---

### **Test 2.2B: READ During WRITE (Different Sentences)**

**Purpose:** Verify READ works while WRITE locks a specific sentence

**Terminal 3 (user1):**
```bash
# Start WRITE on sentence 0 (locks ONLY sentence 0)
docs++> WRITE concurrent_read.txt 0
# Don't commit yet - keep lock active
```

**Terminal 4 (user2) - WHILE user1 holds lock:**
```bash
docs++> READ concurrent_read.txt
# Expected: SUCCESS - READ should work even when sentence is locked
# Shows ORIGINAL content (before user1's uncommitted changes)
```

**✅ PASS CRITERIA:**
- READ succeeds during WRITE
- Shows pre-WRITE content (uncommitted changes not visible)
- No blocking or errors

---

### **Test 2.3: DELETE Operation**

#### **Test: DELETE Existing File**
```bash
docs++> CREATE temp.txt
# Expected: ✓ File 'temp.txt' created successfully!

docs++> DELETE temp.txt
# Expected: ✓ File 'temp.txt' deleted successfully!

docs++> READ temp.txt
# Expected: ✗ Error: File 'temp.txt' not found
```

**✅ Checkpoint:** DELETE works and file is removed

#### **Test: DELETE Non-existent File (Error Handling)**
```bash
docs++> DELETE nonexistent.txt
# Expected: ✗ Error: File not found or access denied
```

**✅ Checkpoint:** Error handling for DELETE works

---

## 👥 PART 3: MULTI-USER & ACCESS CONTROL TESTING (Days 6-7)

### **Test 3.1: Multiple Users Setup**

#### **Terminal 4: Start Client as user2**
```bash
# Terminal 4 (new terminal)
cd "/home/chai404/OSN/Course Project/course-project-osn-3-0"
make run-client

# When prompted:
Enter your username: user2
```

#### **Terminal 5: Start Client as user3**
```bash
# Terminal 5 (new terminal)
cd "/home/chai404/OSN/Course Project/course-project-osn-3-0"
make run-client

# When prompted:
Enter your username: user3
```

**✅ Checkpoint:** Three clients running simultaneously

---

### **Test 3.2: LIST Command**

#### **As user1 (Terminal 3):**
```bash
docs++> LIST

# Expected Output:
# Registered users (3):
#   - user1
#   - user2
#   - user3
```

#### **As user2 (Terminal 4):**
```bash
docs++> LIST

# Expected: Same list of all registered users
```

**✅ Checkpoint:** LIST shows all registered users

---

### **Test 3.3: VIEW Command (File Visibility)**

#### **As user1 (Terminal 3):**
```bash
docs++> VIEW

# Expected Output:
# Accessible files (X):
#   - testfile1.txt
#   - document.txt
#   - mydata.txt
# (All files owned by user1)
```

#### **As user2 (Terminal 4):**
```bash
docs++> VIEW

# Expected Output:
# Accessible files (0):
# (user2 has no files yet)
```

**✅ Checkpoint:** VIEW shows only accessible files per user

---

### **Test 3.4: VIEW -a (View All Files)**

#### **As user2 (Terminal 4):**
```bash
docs++> VIEW -a

# Expected Output:
# All files (3):
#   - testfile1.txt
#   - document.txt
#   - mydata.txt
# (Shows all files in system, regardless of ownership)
```

**✅ Checkpoint:** VIEW -a shows all system files

---

### **Test 3.5: INFO Command**

#### **As user1 (Terminal 3):**
```bash
docs++> INFO testfile1.txt

# Expected Output:
# File Information: testfile1.txt
# ==========================================
# Owner:          user1
# Access:         Owner (Read, Write)
# Size:           0 bytes
# Word Count:     0
# Char Count:     0
# Created:        2025-11-16 21:XX:XX
# Last Modified:  2025-11-16 21:XX:XX
# Last Accessed:  2025-11-16 21:XX:XX
# Location:       SS[0] at 127.0.0.1:8082
```

**✅ Checkpoint:** INFO displays comprehensive file metadata

---

### **Test 3.6: ADDACCESS Command (Grant Read Permission)**

#### **As user1 (Terminal 3) - Grant read access to user2:**
```bash
docs++> ADDACCESS -R testfile1.txt user2

# Expected: ✓ Granted read access to 'testfile1.txt' for user 'user2'
```

#### **Verify as user2 (Terminal 4):**
```bash
docs++> VIEW

# Expected Output:
# Accessible files (1):
#   - testfile1.txt
# (Now user2 can see this file)

docs++> READ testfile1.txt

# Expected: Successfully reads the file (empty file)
```

**✅ Checkpoint:** Read access grant works

---

### **Test 3.7: ADDACCESS Command (Grant Write Permission)**

#### **As user1 (Terminal 3) - Grant write access to user3:**
```bash
docs++> ADDACCESS -W document.txt user3

# Expected: ✓ Granted write access to 'document.txt' for user 'user3'
```

#### **Verify as user3 (Terminal 5):**
```bash
docs++> VIEW

# Expected Output:
# Accessible files (1):
#   - document.txt
# (Write access implies read access)

docs++> INFO document.txt

# Expected Output:
# Owner:          user1
# Access:         user3 (Read, Write)
# [... metadata ...]
```

**✅ Checkpoint:** Write access grant works

---

### **Test 3.8: Access Control Enforcement**

#### **Test: Unauthorized READ (As user3)**
```bash
docs++> READ mydata.txt

# Expected: ✗ Error: Access denied - insufficient permissions
```

#### **Test: Unauthorized INFO (As user2)**
```bash
docs++> INFO document.txt

# Expected: ✗ Error: Access denied - insufficient permissions
# (user2 doesn't have access to document.txt)
```

**✅ Checkpoint:** Access control prevents unauthorized access

---

### **Test 3.9: VIEW -l (View with Metadata)**

#### **As user1 (Terminal 3):**
```bash
docs++> VIEW -l

# Expected Output:
# Accessible files (3):
# Filename                       Size       Modified             Words      Chars
# ----------------------------------------------------------------------------
# testfile1.txt                  0          2025-11-16 21:XX     0          0
# document.txt                   0          2025-11-16 21:XX     0          0
# mydata.txt                     0          2025-11-16 21:XX     0          0
```

**✅ Checkpoint:** VIEW -l displays metadata table

---

### **Test 3.10: VIEW -al (All Files with Metadata)**

#### **As user2 (Terminal 4):**
```bash
docs++> VIEW -al

# Expected: All system files with metadata in table format
```

**✅ Checkpoint:** VIEW -al combines both flags

---

### **Test 3.10A: VIEW Flag Edge Cases**

**Purpose:** Verify robust error handling for VIEW command flags

#### **Test 3.10A.1: Invalid Flags**
```bash
docs++> VIEW -x
# Expected: ✗ Error: Invalid flag '-x'. Use -a (all) or -l (long)

docs++> VIEW -R
# Expected: ✗ Error: Invalid flag '-R'. Use -a (all) or -l (long)

docs++> VIEW --all
# Expected: ✗ Error: Invalid flag '--all'. Use -a (all) or -l (long)
```

**✅ PASS CRITERIA:**
- Clear error messages for invalid flags
- No system crashes
- Helpful usage hint provided

---

#### **Test 3.10A.2: Flag Order Variations**
```bash
docs++> VIEW -la
# Expected: Same as -al (order shouldn't matter)

docs++> VIEW -a -l
# Expected: Should work or show clear error if space-separated not supported
```

**✅ PASS CRITERIA:**
- Either supports both orders or gives clear error
- Consistent behavior

---

#### **Test 3.10A.3: Empty File System VIEW**
```bash
# Fresh system with NO files
docs++> VIEW
# Expected: No accessible files OR clear message "No files found"

docs++> VIEW -a
# Expected: No files on system OR clear message "No files in system"

docs++> VIEW -l
# Expected: Empty table with headers OR "No files to display"

docs++> VIEW -al
# Expected: Empty table with headers OR "No files to display"
```

**✅ PASS CRITERIA:**
- No crashes on empty file system
- User-friendly messages
- Clean output formatting

---

### **Test 3.11: REMACCESS Command**

#### **As user1 (Terminal 3) - Revoke user2's access:**
```bash
docs++> REMACCESS testfile1.txt user2

# Expected: ✓ Revoked access to 'testfile1.txt' for user 'user2'
```

#### **Verify as user2 (Terminal 4):**
```bash
docs++> VIEW

# Expected Output:
# Accessible files (0):
# (testfile1.txt should no longer appear)

docs++> READ testfile1.txt

# Expected: ✗ Error: Access denied - insufficient permissions
```

**✅ Checkpoint:** Access revocation works

---

### **Test 3.12: Unauthorized ADDACCESS (Error Handling)**

#### **As user2 (Terminal 4) - Try to grant access to user1's file:**
```bash
docs++> ADDACCESS -R testfile1.txt user3

# Expected: ✗ Error: Access denied - only owner can modify access
```

**✅ Checkpoint:** Non-owners cannot modify ACL

---

### **Test 3.13: Invalid ADDACCESS Syntax**

#### **As user1 (Terminal 3):**
```bash
docs++> ADDACCESS -X testfile1.txt user2

# Expected: Error: Invalid flag '-X'. Use -R for read or -W for write
```

**✅ Checkpoint:** Invalid flag detection works

---

## 📊 PART 4: METADATA TESTING

### **Test 4.1: File Size Calculation**

Currently, files are empty. To test metadata properly, you'd need to add content (requires WRITE operation which isn't implemented yet).

#### **As user1 (Terminal 3):**
```bash
docs++> INFO testfile1.txt

# Verify all metadata fields are present:
# - Owner ✓
# - Access info ✓
# - Size ✓
# - Word count ✓
# - Char count ✓
# - Timestamps (created, modified, accessed) ✓
# - Location (SS info) ✓
```

**✅ Checkpoint:** All metadata fields displayed correctly

---

## 🔄 PART 5: DATA PERSISTENCE TESTING

### **Test 5.1: ACL Persistence**

#### **Step 1: Set up ACL (As user1 - Terminal 3):**
```bash
docs++> CREATE persistent.txt
docs++> ADDACCESS -R persistent.txt user2
docs++> ADDACCESS -W persistent.txt user3
```

#### **Step 2: Stop and Restart Name Server**
```bash
# In Terminal 1 (Name Server):
# Press Ctrl+C to stop

# Then restart:
make run-ns
```

#### **Step 3: Restart Clients and Verify**
```bash
# Restart user2 client (Terminal 4)
# After reconnecting:

docs++> VIEW
# Expected: persistent.txt should still be in the list

docs++> INFO persistent.txt
# Expected: Access info should show user2 has read access, user3 has write access
```

**✅ Checkpoint:** ACL data persists across Name Server restarts

---

### **Test 5.2: File Persistence**

#### **Step 1: Verify files exist on disk**
```bash
# In a new terminal:
ls -la ./storage_server/storage/

# Expected: See all created files (testfile1.txt, document.txt, etc.)
```

#### **Step 2: Stop and Restart Storage Server**
```bash
# In Terminal 2 (Storage Server):
# Press Ctrl+C to stop

# Then restart:
make run-ss

# Expected Output should include:
# Found existing file: testfile1.txt
# Found existing file: document.txt
# [...other files...]
```

**✅ Checkpoint:** Files persist across Storage Server restarts

---

## 🐛 PART 6: ERROR HANDLING & EDGE CASES

### **Test 6.1: Empty Username**
```bash
# Start new client
make run-client

# When prompted:
Enter your username: [press Enter without typing]

# Expected: Error: Username cannot be empty
```

**✅ Checkpoint:** Empty username rejected

---

### **Test 6.2: Special Characters in Filename**
```bash
docs++> CREATE test@file#1.txt
# Test if system handles special characters

docs++> CREATE test file.txt
# Test with space in filename
```

**📝 Note:** Document behavior for special characters

---

### **Test 6.3: Very Long Filename**
```bash
docs++> CREATE thisIsAVeryLongFilenameToTestIfTheSystemCanHandleItProperly.txt

# Expected: Should work if within MAX_FILENAME_LEN (256 chars)
```

**✅ Checkpoint:** Long filenames handled

---

### **Test 6.4: Case Sensitivity**
```bash
docs++> CREATE TestFile.txt
docs++> CREATE testfile.txt

# Expected: Should create two different files (case-sensitive)
```

**✅ Checkpoint:** Case sensitivity tested

---

### **Test 6.5: Network Disconnection Handling**

#### **Test: Client disconnects**
```bash
# In any client terminal, press Ctrl+C

# In Name Server log (Terminal 1):
# Should see: Connection closed message
```

#### **Test: Reconnection**
```bash
# Restart the same client
make run-client
# Enter same username

# Expected: Should reconnect successfully
```

**✅ Checkpoint:** Disconnection handled gracefully

---

## 📈 PART 7: CONCURRENT OPERATIONS

### **Test 7.1: Simultaneous File Creation**

#### **As user1 (Terminal 3):**
```bash
docs++> CREATE concurrent1.txt
```

#### **Immediately after, as user2 (Terminal 4):**
```bash
docs++> CREATE concurrent2.txt
```

#### **As user3 (Terminal 5):**
```bash
docs++> CREATE concurrent3.txt
```

**✅ Checkpoint:** All three files created successfully

---

### **Test 7.2: Concurrent READ Operations**

#### **All three users simultaneously:**
```bash
# user1, user2, user3 all execute:
docs++> VIEW -l
```

**✅ Checkpoint:** No errors, all get responses

---

### **Test 7.3: Concurrent Access Modifications**

#### **As user1 (owner of multiple files):**
```bash
docs++> ADDACCESS -R concurrent1.txt user2
docs++> ADDACCESS -R concurrent2.txt user2
docs++> ADDACCESS -R concurrent3.txt user2
```

#### **As user2 (verify immediately):**
```bash
docs++> VIEW

# Expected: Should see all three files
```

**✅ Checkpoint:** Concurrent ACL modifications work

---

## 📝 PART 8: LOGGING VERIFICATION

### **Test 8.1: Check Name Server Logs**
```bash
cat logs/nameserver.log | tail -50

# Verify logs contain:
# - Client registration entries
# - SS registration entries
# - File operation logs (CREATE, READ, DELETE)
# - ACL modification logs
# - Access control enforcement logs
```

**✅ Checkpoint:** Comprehensive logging present

---

### **Test 8.2: Check Storage Server Logs**
```bash
cat logs/storage_server.log | tail -50

# Verify logs contain:
# - Registration with NS
# - File creation operations
# - File read operations
# - File deletion operations
# - Metadata calculation logs
```

**✅ Checkpoint:** Storage Server logging works

---

### **Test 8.3: Check Client Logs**
```bash
cat logs/client.log | tail -50

# Verify logs contain:
# - User login
# - Command execution logs
# - Success/failure indicators
# - Logout messages
```

**✅ Checkpoint:** Client logging works

---

## 📊 PART 9: FINAL VERIFICATION CHECKLIST

### **✅ Days 3-5: Core Operations**
- [ ] CREATE works correctly
- [ ] READ works correctly
- [ ] DELETE works correctly
- [ ] Error handling for all operations
- [ ] Files persist after restart

### **✅ Days 6-7: Access Control & Metadata**
- [ ] LIST shows all users
- [ ] VIEW shows only accessible files
- [ ] VIEW -a shows all files
- [ ] VIEW -l shows files with metadata
- [ ] VIEW -al combines both flags
- [ ] INFO shows comprehensive metadata
- [ ] ADDACCESS -R grants read access
- [ ] ADDACCESS -W grants write access
- [ ] REMACCESS revokes access
- [ ] Access control enforced on READ
- [ ] Only owners can modify ACL
- [ ] ACL persists across NS restart

### **✅ Additional Requirements**
- [ ] Multiple concurrent clients work
- [ ] Logging is comprehensive
- [ ] Error messages are clear
- [ ] System handles edge cases
- [ ] No memory leaks (run with valgrind)
- [ ] Code compiles without warnings

---

## 🎯 SUCCESS CRITERIA

### **All Tests Should Pass If:**
1. ✅ All file operations (CREATE, READ, DELETE) work correctly
2. ✅ Access control prevents unauthorized access
3. ✅ VIEW commands show correct files based on permissions
4. ✅ INFO displays all metadata correctly
5. ✅ ADDACCESS/REMACCESS modify permissions correctly
6. ✅ Data persists across system restarts
7. ✅ Error handling works for all invalid operations
8. ✅ Multiple clients can operate concurrently
9. ✅ Logging captures all operations
10. ✅ System is stable under normal operation

---

## 🐛 TROUBLESHOOTING

### **Issue: Cannot connect to Name Server**
**Solution:** 
- Check if Name Server is running on port 8080
- Verify no other process is using port 8080: `lsof -i :8080`

### **Issue: Files not persisting**
**Solution:**
- Check `./storage_server/storage/` directory exists
- Verify file permissions on storage directory

### **Issue: ACL not persisting**
**Solution:**
- Check if `acl_data.db` file is created in project root
- Verify write permissions

### **Issue: Compilation warnings**
**Solution:**
- All warnings were fixed in the latest build
- Run `make clean && make all` to rebuild

---

## 📄 TEST REPORT TEMPLATE

```
=================================================
Docs++ Testing Report - Days 3-7
=================================================
Date: November 16, 2025
Tester: [Your Name]
Environment: Linux/Ubuntu

BASIC OPERATIONS (Days 3-5):
  CREATE:     [PASS/FAIL] - Notes: ___________
  READ:       [PASS/FAIL] - Notes: ___________
  DELETE:     [PASS/FAIL] - Notes: ___________

ACCESS CONTROL (Days 6-7):
  LIST:       [PASS/FAIL] - Notes: ___________
  VIEW:       [PASS/FAIL] - Notes: ___________
  VIEW -a:    [PASS/FAIL] - Notes: ___________
  VIEW -l:    [PASS/FAIL] - Notes: ___________
  VIEW -al:   [PASS/FAIL] - Notes: ___________
  INFO:       [PASS/FAIL] - Notes: ___________
  ADDACCESS:  [PASS/FAIL] - Notes: ___________
  REMACCESS:  [PASS/FAIL] - Notes: ___________

DATA PERSISTENCE:
  Files:      [PASS/FAIL] - Notes: ___________
  ACL:        [PASS/FAIL] - Notes: ___________

ERROR HANDLING:
  Invalid ops:[PASS/FAIL] - Notes: ___________
  Auth checks:[PASS/FAIL] - Notes: ___________

CONCURRENT OPERATIONS:
  Multi-user: [PASS/FAIL] - Notes: ___________

OVERALL STATUS: [PASS/FAIL]
ESTIMATED MARKS: ___/120 (Days 3-7)

Additional Notes:
_________________________________________________
_________________________________________________
```

---

## 🎓 CONCLUSION

This manual test suite covers **all implemented features from Days 3-7**. Complete all tests systematically and document any issues found.

**Estimated Time:** 45-60 minutes for complete testing

**Next Steps After Testing:**
1. Fix any bugs discovered
2. Document test results
3. Prepare for Days 8-11 implementation (WRITE operation)

---

## 💪 PART 10: STRESS TESTING

### **⚠️ CRITICAL: Run These Before Evaluation Day**

These tests simulate extreme conditions to ensure your system won't crash during evaluation. **If any stress test fails, fix it immediately!**

---

### **Stress Test 10.1: Rapid Sequential Operations**

**Purpose:** Test system stability under rapid command execution

#### **Setup: Single User Rapid Fire (Terminal 3 - user1)**
```bash
# Execute these commands as fast as possible (copy-paste)
CREATE stress1.txt
CREATE stress2.txt
CREATE stress3.txt
CREATE stress4.txt
CREATE stress5.txt
VIEW
VIEW -a
VIEW -l
VIEW -al
INFO stress1.txt
INFO stress2.txt
INFO stress3.txt
LIST
ADDACCESS -R stress1.txt user2
ADDACCESS -R stress2.txt user2
ADDACCESS -W stress3.txt user2
REMACCESS stress1.txt user2
DELETE stress4.txt
DELETE stress5.txt
VIEW
```

**✅ Expected:** All commands execute successfully, no crashes, no hung processes

**❌ If fails:** Check for race conditions in command handling

---

### **Stress Test 10.2: Concurrent Client Commands**

**Purpose:** Test simultaneous operations from multiple clients

#### **Setup: 3 Clients Execute Simultaneously**

**Terminal 3 (user1):**
```bash
CREATE concurrent_test1.txt
ADDACCESS -R concurrent_test1.txt user2
ADDACCESS -W concurrent_test1.txt user3
```

**Terminal 4 (user2) - Execute at SAME TIME:**
```bash
CREATE concurrent_test2.txt
VIEW
VIEW -l
```

**Terminal 5 (user3) - Execute at SAME TIME:**
```bash
CREATE concurrent_test3.txt
LIST
VIEW -a
```

**✅ Expected:** All operations complete, no file corruption, all clients get responses

**❌ If fails:** Check `select()` implementation and socket handling in NS

---

### **Stress Test 10.3: Mass File Creation**

**Purpose:** Test system with large number of files

#### **As user1 (Terminal 3):**
```bash
# Create 50 files rapidly
CREATE file01.txt
CREATE file02.txt
CREATE file03.txt
CREATE file04.txt
CREATE file05.txt
CREATE file06.txt
CREATE file07.txt
CREATE file08.txt
CREATE file09.txt
CREATE file10.txt
CREATE file11.txt
CREATE file12.txt
CREATE file13.txt
CREATE file14.txt
CREATE file15.txt
CREATE file16.txt
CREATE file17.txt
CREATE file18.txt
CREATE file19.txt
CREATE file20.txt
CREATE file21.txt
CREATE file22.txt
CREATE file23.txt
CREATE file24.txt
CREATE file25.txt
CREATE file26.txt
CREATE file27.txt
CREATE file28.txt
CREATE file29.txt
CREATE file30.txt
CREATE file31.txt
CREATE file32.txt
CREATE file33.txt
CREATE file34.txt
CREATE file35.txt
CREATE file36.txt
CREATE file37.txt
CREATE file38.txt
CREATE file39.txt
CREATE file40.txt
CREATE file41.txt
CREATE file42.txt
CREATE file43.txt
CREATE file44.txt
CREATE file45.txt
CREATE file46.txt
CREATE file47.txt
CREATE file48.txt
CREATE file49.txt
CREATE file50.txt

# Now test VIEW performance
VIEW
VIEW -l
VIEW -a
VIEW -al
```

**✅ Expected:** All files created, VIEW commands return quickly (< 2 seconds), no memory errors

**❌ If fails:** Check HashMap implementation, memory allocation, buffer sizes

**Performance Benchmark:**
- VIEW with 50 files: Should complete in < 1 second
- VIEW -l with 50 files: Should complete in < 3 seconds (metadata calculation)

---

### **Stress Test 10.4: Mass Access Control Operations**

**Purpose:** Test ACL system under heavy load

#### **As user1 (Terminal 3) - Grant access to all 50 files:**
```bash
ADDACCESS -R file01.txt user2
ADDACCESS -R file02.txt user2
ADDACCESS -R file03.txt user2
ADDACCESS -R file04.txt user2
ADDACCESS -R file05.txt user2
ADDACCESS -R file06.txt user2
ADDACCESS -R file07.txt user2
ADDACCESS -R file08.txt user2
ADDACCESS -R file09.txt user2
ADDACCESS -R file10.txt user2
ADDACCESS -R file11.txt user2
ADDACCESS -R file12.txt user2
ADDACCESS -R file13.txt user2
ADDACCESS -R file14.txt user2
ADDACCESS -R file15.txt user2
ADDACCESS -R file16.txt user2
ADDACCESS -R file17.txt user2
ADDACCESS -R file18.txt user2
ADDACCESS -R file19.txt user2
ADDACCESS -R file20.txt user2
ADDACCESS -R file21.txt user2
ADDACCESS -R file22.txt user2
ADDACCESS -R file23.txt user2
ADDACCESS -R file24.txt user2
ADDACCESS -R file25.txt user2
# Continue for all 50 files...
```

#### **As user2 (Terminal 4) - Verify access:**
```bash
VIEW
# Expected: Should show all 50 files

VIEW -l
# Expected: Should display metadata for all 50 files
```

**✅ Expected:** All access grants succeed, VIEW shows all files, ACL lookups are fast

**❌ If fails:** Check ACL HashMap efficiency, `acl_check_read()` performance

---

### **Stress Test 10.5: Rapid Connection/Disconnection**

**Purpose:** Test system stability with clients connecting and disconnecting

#### **Terminal 3:**
```bash
# Start client, execute command, exit
make run-client
# Username: stress_user1
CREATE quick1.txt
EXIT

# Immediately start again
make run-client
# Username: stress_user2
CREATE quick2.txt
EXIT

# Repeat 5 times with different usernames
# stress_user3, stress_user4, stress_user5
```

#### **Then verify from stable client:**
```bash
make run-client
# Username: verifier
LIST
# Expected: Should show all stress_userX users

VIEW -a
# Expected: Should show all quickX.txt files
```

**✅ Expected:** All operations succeed, no zombie connections in NS

**❌ If fails:** Check client cleanup in NS, socket closure handling

---

### **Stress Test 10.6: Long-Running Stability Test**

**Purpose:** Ensure no memory leaks or resource exhaustion over time

#### **Setup: Automated Loop Test**

Create a test script:
```bash
# Create file: stress_loop_test.sh
cat > stress_loop_test.sh << 'EOF'
#!/bin/bash
echo "Running 100 iterations of CREATE/READ/DELETE cycle..."
for i in {1..100}; do
    echo "Iteration $i/100"
    echo "CREATE stress_loop_$i.txt" | timeout 5 ./client
    echo "READ stress_loop_$i.txt" | timeout 5 ./client
    echo "DELETE stress_loop_$i.txt" | timeout 5 ./client
    sleep 0.1
done
echo "Stress loop completed!"
EOF

chmod +x stress_loop_test.sh
```

#### **Run the test:**
```bash
./stress_loop_test.sh
```

**Monitor NS and SS terminals for:**
- Memory usage (use `top` or `htop` in another terminal)
- Error messages
- Slowdowns

**✅ Expected:** All 100 iterations complete, memory usage stable, no errors

**❌ If fails:** Check for memory leaks with valgrind:
```bash
valgrind --leak-check=full --show-leak-kinds=all ./nameserver
```

---

### **Stress Test 10.7: Invalid Input Flooding**

**Purpose:** Test error handling doesn't crash the system

#### **As any user, execute invalid commands rapidly:**
```bash
INVALID_COMMAND
CREATE
READ
DELETE
ADDACCESS file.txt
REMACCESS
VIEW -z
VIEW -l -a
INFO
LIST extra args
CREATE file|with|pipes.txt
READ ../../../etc/passwd
DELETE *
ADDACCESS -R file.txt
REMACCESS file.txt
HELP me
CREATE file.txt file.txt
```

**✅ Expected:** All invalid commands return error messages, system remains stable, no crashes

**❌ If fails:** Check input validation in client command parser

---

### **Stress Test 10.8: Buffer Overflow Tests**

**Purpose:** Ensure no buffer overflows with large inputs

#### **Test: Very Long Filenames**
```bash
CREATE aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa.txt

# Expected: Should fail gracefully with "filename too long" error
# Should NOT crash or cause buffer overflow
```

#### **Test: Very Long Username**
```bash
# Start new client
make run-client
# Enter: aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa

# Expected: Should reject with error or truncate safely
```

**✅ Expected:** System handles gracefully, no segfaults, no buffer overflows

**❌ If fails:** Review all `strcpy()` calls, use `strncpy()`, check buffer sizes

---

### **Stress Test 10.9: Network Stress - Rapid Fire Commands**

**Purpose:** Test socket handling under load

#### **Setup: Script to send 1000 commands**
```bash
# Create file: network_stress.sh
cat > network_stress.sh << 'EOF'
#!/bin/bash
echo "Sending 1000 LIST commands..."
for i in {1..1000}; do
    echo "LIST" | timeout 2 ./client 2>/dev/null &
    if [ $((i % 100)) -eq 0 ]; then
        echo "Sent $i/1000 commands"
        wait  # Wait for batch to complete
    fi
done
wait
echo "Network stress test complete!"
EOF

chmod +x network_stress.sh
```

#### **Run the test:**
```bash
./network_stress.sh
```

**Monitor for:**
- "Too many open files" errors
- Connection refused errors
- NS or SS crashes

**✅ Expected:** All commands complete successfully, no resource exhaustion

**❌ If fails:** 
- Increase file descriptor limits: `ulimit -n 4096`
- Check socket closure in NS
- Ensure proper cleanup after each request

---

### **Stress Test 10.10: Persistence Under Stress**

**Purpose:** Ensure data survives crashes and restarts under load

#### **Phase 1: Create heavy load**
```bash
# As user1 (Terminal 3):
# Create 20 files with complex ACLs
for i in {1..20}; do
    echo "CREATE persist_$i.txt"
    echo "ADDACCESS -R persist_$i.txt user2"
    echo "ADDACCESS -W persist_$i.txt user3"
done | ./client
```

#### **Phase 2: Hard restart NS and SS**
```bash
# In NS terminal (Terminal 1):
# Press Ctrl+C (hard kill)

# In SS terminal (Terminal 2):
# Press Ctrl+C (hard kill)

# Wait 2 seconds, then restart both:
make run-ns    # Terminal 1
make run-ss    # Terminal 2
```

#### **Phase 3: Verify data integrity**
```bash
# Start fresh client
make run-client
# Username: verifier

VIEW -a
# Expected: All 20 persist_X.txt files present

# Start as user2
make run-client
# Username: user2

VIEW
# Expected: Should see all 20 files (has read access)

# Start as user3
make run-client
# Username: user3

VIEW
# Expected: Should see all 20 files (has write access)
```

**✅ Expected:** All files and ACLs restored correctly, no data loss

**❌ If fails:** Check `acl_save()` is called after every ACL modification, verify storage directory persistence

---

### **Stress Test 10.11: Memory Leak Detection**

**Purpose:** Find memory leaks before evaluation

#### **Setup: Run with Valgrind**
```bash
# Terminal 1: Start NS with Valgrind
valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes --log-file=valgrind_ns.log ./nameserver

# Terminal 2: Start SS with Valgrind
valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes --log-file=valgrind_ss.log ./storage_server

# Terminal 3: Start client with Valgrind
valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes --log-file=valgrind_client.log ./client
```

#### **Run typical operations:**
```bash
CREATE leak_test.txt
READ leak_test.txt
VIEW
VIEW -l
INFO leak_test.txt
LIST
ADDACCESS -R leak_test.txt user2
REMACCESS leak_test.txt user2
DELETE leak_test.txt
EXIT
```

#### **Check results:**
```bash
# After exiting:
cat valgrind_ns.log | grep "LEAK SUMMARY"
cat valgrind_ss.log | grep "LEAK SUMMARY"
cat valgrind_client.log | grep "LEAK SUMMARY"

# Expected:
# All heap blocks freed -- no leaks are possible
# OR
# Still reachable: [small amount from libraries]
```

**✅ Expected:** Zero "definitely lost" or "indirectly lost" memory

**❌ If fails:** Review all `malloc()` calls, ensure matching `free()`, check socket closures

---

### **Stress Test 10.12: System Resource Monitoring**

**Purpose:** Monitor system resources during operation

#### **Setup: Monitor script**
```bash
# Create file: monitor_resources.sh
cat > monitor_resources.sh << 'EOF'
#!/bin/bash
echo "Monitoring NS and SS processes..."
echo "Press Ctrl+C to stop"
echo ""
while true; do
    clear
    echo "=========================================="
    echo "System Resource Usage"
    echo "=========================================="
    echo ""
    
    # Find NS and SS PIDs
    NS_PID=$(pgrep -f "./nameserver")
    SS_PID=$(pgrep -f "./storage_server")
    
    if [ ! -z "$NS_PID" ]; then
        echo "Name Server (PID: $NS_PID):"
        ps -p $NS_PID -o %cpu,%mem,vsz,rss,etime | tail -1
        echo "  Open files: $(lsof -p $NS_PID 2>/dev/null | wc -l)"
        echo ""
    else
        echo "Name Server: NOT RUNNING"
        echo ""
    fi
    
    if [ ! -z "$SS_PID" ]; then
        echo "Storage Server (PID: $SS_PID):"
        ps -p $SS_PID -o %cpu,%mem,vsz,rss,etime | tail -1
        echo "  Open files: $(lsof -p $SS_PID 2>/dev/null | wc -l)"
        echo ""
    else
        echo "Storage Server: NOT RUNNING"
        echo ""
    fi
    
    echo "Files in storage: $(ls -1 ./storage_server/storage/ 2>/dev/null | wc -l)"
    echo "ACL database size: $(du -h acl_data.db 2>/dev/null | cut -f1)"
    echo ""
    echo "Press Ctrl+C to stop monitoring"
    
    sleep 2
done
EOF

chmod +x monitor_resources.sh
```

#### **Run monitoring in separate terminal:**
```bash
./monitor_resources.sh
```

#### **While monitoring, run heavy operations:**
```bash
# Execute stress tests 10.3 and 10.4 while monitoring
```

**✅ Expected:**
- CPU usage < 50% during normal operations
- Memory usage stable (not continuously increasing)
- Open files count reasonable (< 100)
- No unusual spikes

**❌ If fails:** Investigate memory leaks, excessive file descriptor usage, CPU-intensive loops

---

## 🎯 STRESS TEST SUMMARY & SCORING

### **Critical Tests (Must Pass 100%):**
- ✅ **10.1** - Rapid Sequential Operations
- ✅ **10.2** - Concurrent Client Commands
- ✅ **10.5** - Rapid Connection/Disconnection
- ✅ **10.7** - Invalid Input Flooding
- ✅ **10.10** - Persistence Under Stress

### **Important Tests (Should Pass 90%+):**
- ✅ **10.3** - Mass File Creation
- ✅ **10.4** - Mass Access Control
- ✅ **10.8** - Buffer Overflow Tests
- ✅ **10.11** - Memory Leak Detection

### **Performance Tests (Benchmarks):**
- ✅ **10.6** - Long-Running Stability
- ✅ **10.9** - Network Stress
- ✅ **10.12** - Resource Monitoring

---

## 🚨 PRE-EVALUATION DAY CHECKLIST

**48 Hours Before Evaluation:**
```
[ ] Run all 12 stress tests
[ ] Fix any crashes or memory leaks
[ ] Verify all tests pass on clean system
[ ] Test on evaluation lab machines (if possible)
[ ] Document known limitations
[ ] Prepare demo script
```

**24 Hours Before Evaluation:**
```
[ ] Run stress tests again
[ ] Clean code, remove debug prints
[ ] Verify logs are comprehensive
[ ] Test fresh clone from GitHub
[ ] Run valgrind on all components
[ ] Get good sleep (seriously!)
```

**On Evaluation Day:**
```
[ ] Arrive early to test on actual machine
[ ] Run quick smoke test (Parts 2-3)
[ ] Have backup plan if something fails
[ ] Know your code architecture
[ ] Be ready to explain design decisions
```

---

## 📊 STRESS TEST REPORT TEMPLATE

```
=================================================
Docs++ STRESS TESTING REPORT
=================================================
Date: November 16, 2025
Tester: [Your Name]
Duration: [Total testing time]

STRESS TEST RESULTS:
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

10.1 Rapid Sequential:        [PASS/FAIL] - Time: ___s
10.2 Concurrent Commands:      [PASS/FAIL] - Time: ___s
10.3 Mass File Creation:       [PASS/FAIL] - Time: ___s
     Performance: ___s for 50 files
10.4 Mass Access Control:      [PASS/FAIL] - Time: ___s
10.5 Rapid Connect/Disconnect: [PASS/FAIL] - Cycles: ___
10.6 Long-Running Stability:   [PASS/FAIL] - Iterations: ___
10.7 Invalid Input Flooding:   [PASS/FAIL] - Commands: ___
10.8 Buffer Overflow Tests:    [PASS/FAIL] - No crashes: ___
10.9 Network Stress:           [PASS/FAIL] - Commands: ___/1000
10.10 Persistence Under Stress:[PASS/FAIL] - Data intact: ___
10.11 Memory Leak Detection:   [PASS/FAIL] - Leaks: ___ bytes
10.12 Resource Monitoring:     [PASS/FAIL] - Peak CPU: ___%

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

CRITICAL TESTS:  ___/5 passed (Must be 5/5)
IMPORTANT TESTS: ___/4 passed (Should be 4/4)
PERFORMANCE:     [EXCELLENT/GOOD/POOR]

ISSUES FOUND:
1. _______________________________________________
2. _______________________________________________
3. _______________________________________________

FIXES APPLIED:
1. _______________________________________________
2. _______________________________________________
3. _______________________________________________

SYSTEM STABILITY: [PRODUCTION-READY / NEEDS WORK / CRITICAL ISSUES]

EVALUATION READINESS: [READY / NOT READY]

Tester Signature: _______________  Date: ___________
```

---

## 💡 TIPS FOR HANDLING STRESS TEST FAILURES

### **If System Crashes:**
1. **Check logs immediately** - Last operation before crash
2. **Run with gdb** - `gdb ./nameserver`, then `run`, reproduce crash, `bt` for backtrace
3. **Check for null pointers** - Most common crash cause
4. **Verify bounds checking** - Array indices, buffer sizes

### **If System Hangs:**
1. **Check for deadlocks** - Look for infinite loops in `select()` or connection handling
2. **Verify socket cleanup** - Ensure all sockets are closed properly
3. **Check blocking operations** - Ensure no operations block indefinitely

### **If Memory Grows:**
1. **Run valgrind** - Identify leak source
2. **Check malloc/free pairs** - Every `malloc()` needs a `free()`
3. **Verify socket closure** - Unclosed sockets leak memory
4. **Check string operations** - `strdup()` without `free()`

### **If Performance Degrades:**
1. **Profile with gprof** - Find bottlenecks
2. **Check O(n²) algorithms** - Linear search in loops
3. **Verify HashMap usage** - Should be O(1), not O(n)
4. **Check file I/O** - Excessive disk operations

---

## ⚡ EMERGENCY FIX GUIDE

**If evaluator finds a bug during demo:**

1. **Stay Calm** - Acknowledge the issue professionally
2. **Explain** - "That's an edge case we can handle with..."
3. **Show Logs** - Demonstrate system is logging the error
4. **Quick Fix** - If simple, offer to fix in 2 minutes
5. **Workaround** - Show alternative way to achieve goal
6. **Document** - "We'll add this to known limitations"

**Remember:** Perfect software doesn't exist. How you handle issues matters!

---

**🎯 YOUR SYSTEM IS STRESS-TEST READY WHEN:**
- ✅ All 12 stress tests pass
- ✅ No crashes, hangs, or memory leaks
- ✅ Performance meets benchmarks
- ✅ Data persists correctly
- ✅ Error handling is robust
- ✅ You can explain every failure scenario

---

**Happy Stress Testing! May your system be unbreakable! 💪🚀**

# WRITE Operation Testing Guide

## Implementation Complete ✓

Successfully implemented Days 8-11 WRITE operation with the following components:

### Components Implemented:

1. **Sentence Locking System** (`sentence_lock.c/h`)
   - HashMap-based lock table for O(1) lookup
   - 60-second auto-timeout with background cleanup thread
   - Per-sentence granular locking

2. **Sentence Parsing** (`sentence_parser.c/h`)
   - Splits files by . ! ? delimiters (EVERY occurrence)
   - Word-level manipulation within sentences
   - Handles delimiter splits (e.g., "Umm!" creates 2 sentences)

3. **Undo Buffer** (`undo_buffer.c/h`)
   - Single-level undo (1MB max)
   - Saved before every WRITE commit
   - UNDO command restores previous state

4. **Storage Server WRITE Logic** (`storage_server.c`)
   - OP_WRITE_START: Acquire lock on sentence
   - OP_WRITE_UPDATE: Buffer word insertions
   - OP_WRITE_COMMIT: Atomic write with temp file + rename
   - Delimiter handling: auto-split sentences if new content has . ! ?

5. **Nameserver Routing** (`nameserver.c`)
   - handle_write_request(): Check write permissions, return SS info
   - handle_undo_request(): Check write permissions, return SS info

6. **Client Interface** (`client.c`)
   - cmd_write(filename, sentence_num): Interactive WRITE session
   - cmd_undo(filename): Restore from undo buffer
   - Commands: WRITE <file> <sentence> / UNDO <file>

## Quick Test Steps:

### Test 1: Basic WRITE Operation
```bash
# Terminal 1: Start NS
make run-ns

# Terminal 2: Start SS
make run-ss

# Terminal 3: Client (user1)
make run-client
# Login as: user1
CREATE test.txt
WRITE test.txt 0
  # Type: 0 Hello
  # Type: 1 World
  # Type: ETIRW
READ test.txt
```

### Test 2: Delimiter Handling
```bash
# After creating file with content "This is a test"
WRITE test.txt 0
  # Type: 2 Umm!
  # Type: ETIRW
READ test.txt
# Expected: "This is Umm! a test" → splits into 2 sentences
```

### Test 3: Lock Contention
```bash
# Terminal 3: user1
WRITE test.txt 0
  # (don't commit yet - ETIRW)

# Terminal 4: user2
WRITE test.txt 0
# Expected: Error "Sentence locked by another user"
```

### Test 4: UNDO
```bash
CREATE file.txt
# (add some content)
WRITE file.txt 0
  # (make changes)
  # Type: ETIRW
UNDO file.txt
READ file.txt
# Expected: Previous content restored
```

### Test 5: Lock Timeout
```bash
WRITE test.txt 0
  # Wait 65 seconds without committing
  # Type: ETIRW
# Expected: Lock expired, may fail or new lock acquired
```

## Usage:

### WRITE Command
```
WRITE <filename> <sentence_number>

Interactive session:
  <word_index> <word_content>   - Insert word at index
  ETIRW                         - Commit changes
  CANCEL                        - Abort (lock will timeout)
```

### UNDO Command
```
UNDO <filename>
```

## Edge Cases Covered:

✓ Multiple delimiters: "!!!" → 3 empty sentences
✓ Abbreviations: "e.g." → 2 sentences (as per requirement)
✓ Empty files: sentence 0 = empty sentence
✓ Word index validation: Negative or > word_count = error
✓ Lock expiry: Background thread cleans every 30 seconds
✓ Atomic writes: Uses temp file + rename to prevent corruption
✓ Concurrent access: Only one client can modify same sentence

## Compilation Status:

✓ 0 errors
✓ 1 minor warning (format truncation - non-critical)
✓ All modules compiled successfully

## Next Steps:

1. Test basic WRITE end-to-end
2. Test delimiter handling ("Umm!", "e.g.", multiple !!!)
3. Test lock contention with 2 clients
4. Test UNDO operation
5. Test lock timeout (wait 60+ seconds)
6. Stress test: Rapid writes, many concurrent users

---

## Implementation Notes:

- Lock table: 509 buckets (prime for distribution)
- Lock timeout: 60 seconds
- Cleanup thread: Runs every 30 seconds
- Undo buffer: 1MB max per file
- Sentence delimiters: . ! ? (EVERY occurrence)
- Atomic write: temp file → rename (POSIX atomic)

## Architecture:

```
Client → NS (check write access) → Client → SS (direct connection)
         ↓                                    ↓
      Return SS info                    OP_WRITE_START (lock)
                                         OP_WRITE_UPDATE (buffer)
                                         OP_WRITE_COMMIT (atomic write)
```

## File Changes:

- storage_server/sentence_lock.{c,h} (NEW)
- storage_server/sentence_parser.{c,h} (NEW)
- storage_server/undo_buffer.{c,h} (NEW)
- storage_server/storage_server.c (MODIFIED - 260 lines added)
- nameserver/nameserver.c (MODIFIED - 100 lines added)
- client/client.c (MODIFIED - 250 lines added)
- Makefile (MODIFIED - new modules)

Total new code: ~850 lines

---

# 🔥 PART 11: STRESS TESTING - DAYS 8-11 WRITE OPERATION

## ⚠️ CRITICAL: Run These Tests Before Evaluation!

**Purpose:** Verify WRITE operation stability under extreme conditions  
**Time Required:** 30-45 minutes  
**Priority:** MANDATORY before evaluation

---

## 📋 PRE-TEST SETUP

### **Ensure System is Running**
```bash
# Terminal 1: Name Server
make run-ns

# Terminal 2: Storage Server
make run-ss

# Wait 2 seconds for startup
sleep 2
```

---

## 🧪 STRESS TEST 1: Basic WRITE Verification

**Purpose:** Verify basic WRITE operation works correctly

### **Terminal 3: Client (user1)**
```bash
make run-client
# Username: user1

# Create test file
CREATE write_test.txt

# Perform basic WRITE
WRITE write_test.txt 0
0 Hello
1 World
2 from
3 Docs++
ETIRW

# Verify content
READ write_test.txt
```

**✅ Expected Output:**
```
Lock acquired. Sentence has 0 words. Send word updates.
✓ Word inserted at index 0. Sentence now has 1 words.
✓ Word inserted at index 1. Sentence now has 2 words.
✓ Word inserted at index 2. Sentence now has 3 words.
✓ Word inserted at index 3. Sentence now has 4 words.
✓ Changes committed successfully!
  Total updates made: 4

─────────────────────────────────────────────────────────────
File: write_test.txt
─────────────────────────────────────────────────────────────
Hello World from Docs++
─────────────────────────────────────────────────────────────
```

**❌ If fails:** Check lock_acquire(), write_session structure, commit logic

---

## 🔒 STRESS TEST 2: Lock Contention (Concurrent Write to Same Sentence)

**Purpose:** Verify lock prevents simultaneous edits

### **Terminal 3: Client (user1)**
```bash
# Start WRITE but DON'T commit
WRITE write_test.txt 0
# (Don't type anything yet - leave session open)
```

### **Terminal 4: Client (user2) - IMMEDIATELY OPEN**
```bash
make run-client
# Username: user2

# Grant write access first (if needed)
# (Have user1 do: ADDACCESS -W write_test.txt user2)

# Try to WRITE same sentence
WRITE write_test.txt 0
```

**✅ Expected Output (user2):**
```
Error: Sentence locked by another user
```

**✅ Expected Behavior:**
- user1 holds the lock
- user2 gets ERR_LOCKED (423)
- user2 cannot edit until user1 commits or lock times out

### **Complete the test:**
```bash
# In Terminal 3 (user1):
CANCEL

# In Terminal 4 (user2):
WRITE write_test.txt 0
0 Now
1 I
2 can
3 edit
ETIRW

# Verify
READ write_test.txt
```

**❌ If fails:** Check lock_acquire() logic, lock contention detection

---

## ⏰ STRESS TEST 3: Lock Timeout (60 Second Expiry)

**Purpose:** Verify locks auto-expire after 60 seconds

### **Terminal 3: Client (user1)**
```bash
CREATE timeout_test.txt

# Start WRITE session
WRITE timeout_test.txt 0
0 Testing
ETIRW

# Start another WRITE but DON'T commit
WRITE timeout_test.txt 0
# (Leave this open - don't type anything)
```

### **Wait 65 seconds**
```bash
# In another terminal, monitor logs:
tail -f logs/storage_server.log

# You should see after ~60 seconds:
# "Cleaned expired lock: timeout_test.txt:S0 (was held by user1)"
```

### **Terminal 4: Client (user2) - After 65 seconds**
```bash
# Grant access if needed
# Then try to write
WRITE timeout_test.txt 0
0 Lock
1 expired
ETIRW

READ timeout_test.txt
```

**✅ Expected Behavior:**
- user1's lock expires after 60 seconds
- Cleanup thread removes it within next 30 seconds
- user2 can acquire lock after expiry
- user1's session fails when trying to commit (lock lost)

**❌ If fails:** Check lock timeout logic, cleanup thread, timestamp comparison

---

## 💥 STRESS TEST 4: Delimiter Splitting - Single Delimiter

**Purpose:** Verify sentence splits on . ! ? delimiters

### **Test 4A: Period Delimiter**
```bash
# Terminal 3 (user1)
CREATE delimiter_test.txt

WRITE delimiter_test.txt 0
0 Hello
1 World.
2 This
3 is
4 new
ETIRW

READ delimiter_test.txt
```

**✅ Expected Output:**
```
Hello World. This is new
```

**Verification:** Check sentence count
```bash
# The file should now have 2 sentences:
# Sentence 0: "Hello World."
# Sentence 1: " This is new"
```

### **Test 4B: Exclamation Delimiter**
```bash
CREATE exclamation_test.txt

WRITE exclamation_test.txt 0
0 Umm!
1 That
2 works
ETIRW

READ exclamation_test.txt
```

**✅ Expected Output:**
```
Umm! That works
```

**Verification:** 2 sentences created: "Umm!" and " That works"

### **Test 4C: Question Mark Delimiter**
```bash
CREATE question_test.txt

WRITE question_test.txt 0
0 Really?
1 Yes!
2 Cool.
ETIRW

READ question_test.txt
```

**✅ Expected Output:**
```
Really? Yes! Cool.
```

**Verification:** 3 sentences created: "Really?", " Yes!", " Cool."

**❌ If fails:** Check `has_delimiter()`, `split_by_delimiters()` functions

---

## 🔥 STRESS TEST 5: Multiple Consecutive Delimiters

**Purpose:** Verify "..." creates 3 empty sentences

### **Test 5A: Triple Period**
```bash
CREATE triple_test.txt

WRITE triple_test.txt 0
0 Wait...
1 OK
ETIRW

READ triple_test.txt
```

**✅ Expected Output:**
```
Wait... OK
```

**Verification:** Should create 4 sentences:
- "Wait."
- "."
- "."
- " OK"

### **Test 5B: Mixed Delimiters**
```bash
CREATE mixed_test.txt

WRITE mixed_test.txt 0
0 What?!?
1 Crazy
ETIRW

READ mixed_test.txt
```

**✅ Expected Output:**
```
What?!? Crazy
```

**Verification:** Should create 4 sentences:
- "What?"
- "!"
- "?"
- " Crazy"

**❌ If fails:** Check delimiter parsing in `parse_sentences()`

---

## 📝 STRESS TEST 6: "e.g." Edge Case

**Purpose:** Verify EVERY delimiter creates sentence (even in abbreviations)

### **Test 6: Abbreviation Handling**
```bash
CREATE abbreviation_test.txt

WRITE abbreviation_test.txt 0
0 For
1 example
2 e.g.
3 this
4 works
ETIRW

READ abbreviation_test.txt
```

**✅ Expected Output:**
```
For example e.g. this works
```

**Verification:** Should create 3 sentences:
- "For example e."
- "g."
- " this works"

**Note:** This is CORRECT behavior per spec: "EVERY delimiter creates new sentence (even in e.g.)"

**❌ If fails:** Check delimiter detection logic

---

## ↩️ STRESS TEST 7: UNDO Operation

**Purpose:** Verify UNDO restores previous file state

### **Test 7A: Basic UNDO**
```bash
CREATE undo_test.txt

# Write initial content
WRITE undo_test.txt 0
0 Original
1 content
2 here
ETIRW

READ undo_test.txt

# Modify it
WRITE undo_test.txt 0
0 Modified
1 content
2 now
ETIRW

READ undo_test.txt

# UNDO to restore
UNDO undo_test.txt

READ undo_test.txt
```

**✅ Expected Output:**
```
After first WRITE: Original content here
After second WRITE: Modified content now
After UNDO: Original content here
```

### **Test 7B: UNDO Without Previous WRITE**
```bash
CREATE empty_undo.txt

# Try UNDO without any WRITE
UNDO empty_undo.txt
```

**✅ Expected Output:**
```
Error: No undo available
```

### **Test 7C: UNDO After Multiple WRITEs**
```bash
CREATE multi_write.txt

WRITE multi_write.txt 0
0 First
ETIRW

WRITE multi_write.txt 0
0 Second
ETIRW

WRITE multi_write.txt 0
0 Third
ETIRW

# UNDO restores to "Second" (single-level)
UNDO multi_write.txt

READ multi_write.txt
```

**✅ Expected Output:**
```
Second
```

**Note:** Only single-level undo, so restores to state before LAST commit only

**❌ If fails:** Check `undo_save()`, `undo_restore()`, buffer management

---

## 🚀 STRESS TEST 8: Rapid Sequential WRITEs (Single User)

**Purpose:** Verify system stability under rapid operations

### **Terminal 3: Client (user1)**
```bash
CREATE rapid_test.txt

# Execute these as fast as possible (copy-paste)
WRITE rapid_test.txt 0
0 Word1
ETIRW

WRITE rapid_test.txt 0
1 Word2
ETIRW

WRITE rapid_test.txt 0
2 Word3
ETIRW

WRITE rapid_test.txt 0
3 Word4
ETIRW

WRITE rapid_test.txt 0
4 Word5
ETIRW

READ rapid_test.txt
```

**✅ Expected Output:**
```
Word1 Word2 Word3 Word4 Word5
```

**✅ Expected Behavior:**
- All WRITEs complete successfully
- No lock errors
- File integrity maintained

**❌ If fails:** Check lock release timing, session cleanup

---

## 👥 STRESS TEST 9: Multiple Users, Different Sentences (Concurrent)

**Purpose:** Verify concurrent writes to DIFFERENT sentences work

### **Setup: Create file with multiple sentences**
```bash
# Terminal 3 (user1)
CREATE concurrent_test.txt

WRITE concurrent_test.txt 0
0 Sentence
1 one.
2 Sentence
3 two.
4 Sentence
5 three.
ETIRW

READ concurrent_test.txt
```

**Expected:** "Sentence one. Sentence two. Sentence three."

### **Concurrent Edits:**

**Terminal 3 (user1) - Edit Sentence 0:**
```bash
WRITE concurrent_test.txt 0
2 MODIFIED
ETIRW
```

**Terminal 4 (user2) - Edit Sentence 1 (SIMULTANEOUSLY):**
```bash
# Grant access first: user1 does "ADDACCESS -W concurrent_test.txt user2"

WRITE concurrent_test.txt 1
2 CHANGED
ETIRW
```

**Terminal 5 (user3) - Edit Sentence 2 (SIMULTANEOUSLY):**
```bash
# Grant access first: user1 does "ADDACCESS -W concurrent_test.txt user3"

WRITE concurrent_test.txt 2
2 UPDATED
ETIRW
```

### **Verify all changes:**
```bash
# Any user:
READ concurrent_test.txt
```

**✅ Expected Output:**
```
Sentence MODIFIED. Sentence CHANGED. Sentence UPDATED.
```

**✅ Expected Behavior:**
- All three WRITEs succeed (different sentences = different locks)
- No lock contention
- All changes preserved

**❌ If fails:** Check lock key generation (filename:sentence_num), lock isolation

---

## 💾 STRESS TEST 10: Large File with Many Sentences

**Purpose:** Verify system handles files with many sentences

### **Terminal 3 (user1)**
```bash
CREATE large_file.txt

# Create file with 20 sentences
WRITE large_file.txt 0
0 S1.
1 S2.
2 S3.
3 S4.
4 S5.
5 S6.
6 S7.
7 S8.
8 S9.
9 S10.
10 S11.
11 S12.
12 S13.
13 S14.
14 S15.
15 S16.
16 S17.
17 S18.
18 S19.
19 S20.
ETIRW

READ large_file.txt

# Edit sentence 15 (middle of file)
WRITE large_file.txt 15
0 MIDDLE
ETIRW

READ large_file.txt

# Edit sentence 19 (last sentence)
WRITE large_file.txt 19
0 LAST
ETIRW

READ large_file.txt
```

**✅ Expected Behavior:**
- File parses correctly into 20 sentences
- Can edit any sentence by index
- Changes preserved correctly
- No index errors

**❌ If fails:** Check sentence parsing, indexing logic

---

## 🔄 STRESS TEST 11: WRITE → UNDO → WRITE → UNDO Cycle

**Purpose:** Verify undo buffer updates correctly

### **Terminal 3 (user1)**
```bash
CREATE cycle_test.txt

# Cycle 1
WRITE cycle_test.txt 0
0 State1
ETIRW

READ cycle_test.txt

# Cycle 2
WRITE cycle_test.txt 0
0 State2
ETIRW

READ cycle_test.txt

UNDO cycle_test.txt
READ cycle_test.txt

# Cycle 3
WRITE cycle_test.txt 0
0 State3
ETIRW

READ cycle_test.txt

UNDO cycle_test.txt
READ cycle_test.txt
```

**✅ Expected Outputs:**
```
After State1: State1
After State2: State2
After UNDO: State1
After State3: State3
After UNDO: State2
```

**✅ Expected Behavior:**
- Undo buffer updates before each commit
- Each UNDO restores to previous state
- Buffer cleared after restore

**❌ If fails:** Check undo_save() timing, buffer replacement logic

---

## ⚠️ STRESS TEST 12: Invalid Operations (Error Handling)

**Purpose:** Verify proper error handling

### **Test 12A: Invalid Sentence Number**
```bash
CREATE error_test.txt

WRITE error_test.txt 0
0 Content
ETIRW

# Try to edit non-existent sentence
WRITE error_test.txt 99
```

**✅ Expected Output:**
```
Error: Invalid sentence number 99 (file has 1 sentences)
```

### **Test 12B: Invalid Word Index**
```bash
WRITE error_test.txt 0
999 Word
```

**✅ Expected Output:**
```
Error: Invalid word index 999 (sentence has 1 words)
```

### **Test 12C: Negative Indices**
```bash
WRITE error_test.txt -1
```

**✅ Expected Output:**
```
Error: Invalid sentence number
```

### **Test 12D: Empty Word Content**
```bash
WRITE error_test.txt 0
0
```

**✅ Expected Output:**
```
Error: Word content cannot be empty
```

### **Test 12E: WRITE Without Permission**
```bash
# Terminal 4 (user2)
# Try to WRITE user1's file without permission
WRITE error_test.txt 0
```

**✅ Expected Output:**
```
Error: Access denied: You don't have permission to write 'error_test.txt'
```

**❌ If fails:** Check input validation, error code returns

---

## 🔍 STRESS TEST 13: File Integrity After Crash Simulation

**Purpose:** Verify atomic writes prevent corruption

### **Test 13A: Kill During WRITE (Simulated)**
```bash
CREATE crash_test.txt

WRITE crash_test.txt 0
0 Original
1 content
ETIRW

READ crash_test.txt

# Start WRITE but don't commit
WRITE crash_test.txt 0
0 Incomplete
# Press Ctrl+C to kill client
```

### **Verify file integrity:**
```bash
# Terminal 4 (new client)
READ crash_test.txt
```

**✅ Expected Output:**
```
Original content
```

**✅ Expected Behavior:**
- File unchanged (no commit occurred)
- Lock times out after 60 seconds
- No partial writes in file

### **Test 13B: UNDO After Crash**
```bash
# After previous test:
UNDO crash_test.txt
```

**✅ Expected Output:**
```
Error: No undo available
```

**Note:** Undo buffer remains from last successful commit

**❌ If fails:** Check atomic rename logic, temp file handling

---

## 📊 STRESS TEST 14: Performance Benchmark

**Purpose:** Measure WRITE operation performance

### **Test 14A: 10 Sequential WRITEs**
```bash
CREATE perf_test.txt

# Time this sequence:
time {
WRITE perf_test.txt 0
0 W1
ETIRW

WRITE perf_test.txt 0
1 W2
ETIRW

WRITE perf_test.txt 0
2 W3
ETIRW

WRITE perf_test.txt 0
3 W4
ETIRW

WRITE perf_test.txt 0
4 W5
ETIRW

WRITE perf_test.txt 0
5 W6
ETIRW

WRITE perf_test.txt 0
6 W7
ETIRW

WRITE perf_test.txt 0
7 W8
ETIRW

WRITE perf_test.txt 0
8 W9
ETIRW

WRITE perf_test.txt 0
9 W10
ETIRW
}
```

**✅ Expected Performance:**
- Total time: < 10 seconds (average 1s per WRITE)
- No timeouts
- No errors

**Benchmark:** If > 15 seconds, investigate performance issues

---

## 🔢 STRESS TEST 14A: Sentence Index Updates After WRITE (Spec Requirement!)

**Purpose:** "After each WRITE completion, the sentence index update. So, care must be taken for ensuring concurrent WRITEs are handled correctly."

### **Test 14A.1: Index Updates Within Session**

```bash
CREATE index_test.txt

# Start with 2 sentences
WRITE index_test.txt 0
0 First.
1 Second
ETIRW

READ index_test.txt
# Expected: First. Second
# File has 2 sentences now

# Edit sentence 1 and add delimiter - creates NEW sentence!
WRITE index_test.txt 1
1 sentence.
2 Third
ETIRW

READ index_test.txt
# Expected: First. Second sentence. Third
# File NOW has 3 sentences (sentence 1 was split)

# Now try to edit what WAS sentence 1, but is NOW sentence 2
WRITE index_test.txt 2
1 modified
ETIRW

READ index_test.txt
# Expected: First. Second sentence. Third modified
```

**✅ PASS CRITERIA:**
- Sentence indices updated after delimiter insertion
- Can access newly created sentences
- No index-out-of-range errors

---

### **Test 14A.2: Multiple Delimiter Insertions Create Multiple Sentences**

```bash
CREATE multi_delim.txt

WRITE multi_delim.txt 0
0 One!
1 Two?
2 Three.
3 Four
ETIRW

READ multi_delim.txt
# Expected: One! Two? Three. Four
# File has 4 sentences (each delimiter creates one)
```

**✅ PASS CRITERIA:**
- Multiple delimiters create multiple sentences
- Each . ! ? is treated as delimiter (even in "e.g.")
- File has correct number of sentences

---

### **Test 14A.3: Concurrent WRITE After Index Change**

```bash
# Terminal 3 (user1)
CREATE concurrent_index.txt
WRITE concurrent_index.txt 0
0 Sent1.
1 Sent2
ETIRW

ADDACCESS -W concurrent_index.txt user2

# user1: Add delimiter to split sentence
WRITE concurrent_index.txt 1
0 Now.
1 Three
ETIRW

# File now has 3 sentences: [Sent1.] [Now.] [Three]

# Terminal 4 (user2) - Try to edit sentence that doesn't exist anymore
WRITE concurrent_index.txt 3
# Should fail - only 3 sentences exist (0, 1, 2)
```

**✅ Expected:**
```
Error: Invalid sentence number 3 (file has 3 sentences)
```

**✅ PASS CRITERIA:**
- Concurrent writes handle index changes correctly
- Clear error for invalid indices
- No crashes or corruption

---

## 📝 STRESS TEST 14B: Newline Character Support (From Doubts Doc)

**Purpose:** "Content can have \\n to signify new line"

### **Test 14B.1: Multi-Line File Content**

```bash
CREATE multiline.txt

WRITE multiline.txt 0
0 Line1\n
1 Line2\n
2 Line3
ETIRW

READ multiline.txt
```

**✅ Expected Output:**
```
Line1
Line2
Line3
```

**✅ PASS CRITERIA:**
- \\n is interpreted as newline character
- File displays on multiple lines when read
- Content preserved correctly across operations

---

### **Test 14B.2: Newline in EXEC File**

```bash
CREATE exec_multiline.txt

WRITE exec_multiline.txt 0
0 echo
1 "Line
2 1"\necho
3 "Line
4 2"
ETIRW

EXEC exec_multiline.txt
```

**✅ Expected Output:**
```
Line 1
Line 2
```

**✅ PASS CRITERIA:**
- Newlines work in executable files
- Multiple commands execute properly
- Output formatted correctly

---

## 🧹 STRESS TEST 15: Lock Cleanup Thread Verification

**Purpose:** Verify background cleanup thread works

### **Test Setup:**
```bash
# Monitor Storage Server logs in real-time
tail -f logs/storage_server.log | grep -i "lock\|cleanup"
```

### **Create Multiple Expired Locks:**

**Terminal 3 (user1):**
```bash
CREATE cleanup1.txt
WRITE cleanup1.txt 0
# Leave open (don't commit)
```

**Terminal 4 (user2):**
```bash
CREATE cleanup2.txt
WRITE cleanup2.txt 0
# Leave open (don't commit)
```

**Terminal 5 (user3):**
```bash
CREATE cleanup3.txt
WRITE cleanup3.txt 0
# Leave open (don't commit)
```

### **Wait 90 seconds and observe logs:**

**✅ Expected Log Output:**
```
Lock cleanup thread started
Lock acquired: cleanup1.txt:S0 by user1
Lock acquired: cleanup2.txt:S0 by user2
Lock acquired: cleanup3.txt:S0 by user3
... (wait ~60 seconds) ...
Cleaned expired lock: cleanup1.txt:S0 (was held by user1)
Cleaned expired lock: cleanup2.txt:S0 (was held by user2)
Cleaned expired lock: cleanup3.txt:S0 (was held by user3)
Cleaned 3 expired locks
```

**✅ Expected Behavior:**
- Cleanup thread runs every 30 seconds
- Locks expire after 60 seconds
- All expired locks cleaned within 90 seconds

**❌ If fails:** Check lock_cleanup_thread(), pthread_create(), LOCK_TIMEOUT constant

---

## 📋 STRESS TEST 16: Memory Leak Check (Valgrind)

**Purpose:** Verify no memory leaks in WRITE operations

### **Run Storage Server with Valgrind:**
```bash
# Terminal 2 (replace normal SS startup):
valgrind --leak-check=full --show-leak-kinds=all --track-origins=yes --log-file=valgrind_ss_write.log ./storage_server/storage_server
```

### **Perform Multiple WRITE Operations:**
```bash
# Terminal 3:
CREATE leak_test.txt

WRITE leak_test.txt 0
0 Test1
ETIRW

WRITE leak_test.txt 0
1 Test2
ETIRW

UNDO leak_test.txt

WRITE leak_test.txt 0
2 Test3
ETIRW

DELETE leak_test.txt
EXIT
```

### **Stop Storage Server and Check Report:**
```bash
# Press Ctrl+C in Terminal 2

# Check valgrind report:
cat valgrind_ss_write.log | grep "LEAK SUMMARY" -A 10
```

**✅ Expected Output:**
```
LEAK SUMMARY:
   definitely lost: 0 bytes in 0 blocks
   indirectly lost: 0 bytes in 0 blocks
   possibly lost: 0 bytes in 0 blocks
   still reachable: [small amount from libraries]
```

**❌ If fails:** Check for missing `free()` calls in:
- sentence_lock.c
- sentence_parser.c
- undo_buffer.c
- write_session cleanup

---

## ✅ STRESS TEST SUMMARY & CHECKLIST

### **Critical Tests (Must Pass 100%):**
- [ ] Test 1: Basic WRITE operation
- [ ] Test 2: Lock contention
- [ ] Test 3: Lock timeout
- [ ] Test 7: UNDO operation
- [ ] Test 12: Error handling
- [ ] Test 13: File integrity

### **Important Tests (Should Pass 90%+):**
- [ ] Test 4: Single delimiter splitting
- [ ] Test 5: Multiple delimiters
- [ ] Test 6: "e.g." edge case
- [ ] Test 8: Rapid sequential writes
- [ ] Test 9: Concurrent writes (different sentences)

### **Performance Tests (Benchmarks):**
- [ ] Test 10: Large file handling
- [ ] Test 14: Performance benchmark
- [ ] Test 15: Cleanup thread
- [ ] Test 16: Memory leak check

---

## 🚨 PRE-EVALUATION DAY CHECKLIST

**1 Hour Before Evaluation:**
```
[ ] Run Stress Tests 1-3 (Basic + Locking)
[ ] Run Stress Test 7 (UNDO)
[ ] Run Stress Test 12 (Error Handling)
[ ] Verify all logs show no errors
[ ] Check file integrity with READ commands
[ ] Test on fresh system if possible
```

**30 Minutes Before:**
```
[ ] Clean restart: pkill -f nameserver; pkill -f storage_server
[ ] make clean && make all
[ ] Verify compilation: 0 errors
[ ] Start NS and SS fresh
[ ] Run quick smoke test (Test 1)
```

**10 Minutes Before:**
```
[ ] Have Manual_test.md open
[ ] Know your lock timeout value (60s)
[ ] Know cleanup thread interval (30s)
[ ] Know undo buffer size (1MB)
[ ] Be ready to explain atomic write (temp file + rename)
```

---

## 🎯 QUICK SMOKE TEST (5 Minutes)

**Run this right before evaluation to verify everything works:**

```bash
# Terminal 1: Start NS
make run-ns

# Terminal 2: Start SS (wait 2s)
make run-ss

# Terminal 3: Client
make run-client
# Username: testuser

CREATE smoke.txt
WRITE smoke.txt 0
0 Hello
1 World.
2 Testing
ETIRW
READ smoke.txt

UNDO smoke.txt
READ smoke.txt

DELETE smoke.txt
EXIT
```

**✅ Expected:** All operations succeed, no errors

**If anything fails → STOP and fix before evaluation!**

---

## 💡 DEBUGGING TIPS

### **If WRITE fails:**
1. Check logs: `tail -100 logs/storage_server.log`
2. Verify lock_acquire() return value
3. Check write_session.active flag

### **If lock contention fails:**
1. Verify lock key generation (filename:sentence_num)
2. Check pthread_mutex_lock() usage
3. Test with `lock_is_locked()` function

### **If delimiter parsing fails:**
1. Check `is_delimiter()` function
2. Verify `parse_sentences()` logic
3. Test with simple strings in isolation

### **If UNDO fails:**
1. Verify `undo_save()` called before commit
2. Check undo_buffer.valid flag
3. Verify filename matching

### **If cleanup thread fails:**
1. Check `pthread_create()` return value
2. Verify thread is running: `ps -T -p $(pgrep storage_server)`
3. Check sleep(30) in loop

---

## 📞 EVALUATION DAY TIPS

### **What Evaluators Will Test:**
1. Basic WRITE with word insertion
2. Lock contention (2 clients, same sentence)
3. Delimiter handling (especially ".")
4. UNDO operation
5. Error handling (invalid indices)

### **How to Demo:**
1. Show Basic WRITE (Test 1)
2. Show Lock Contention (Test 2)
3. Show Delimiter Split (Test 4A)
4. Show UNDO (Test 7A)
5. Show Error Handling (Test 12)

### **Common Questions & Answers:**
**Q: What's your lock timeout?**  
A: 60 seconds, cleaned every 30 seconds by background thread

**Q: How is atomic write implemented?**  
A: Write to temp file, then POSIX rename() for atomic operation

**Q: Does "e.g." create 2 sentences?**  
A: Yes, per specification: EVERY delimiter creates new sentence

**Q: How many levels of undo?**  
A: Single-level, 1MB max buffer per file

**Q: What if two clients edit same sentence?**  
A: Second client gets ERR_LOCKED (423), must wait for lock release

---

## 🎓 SUCCESS CRITERIA

**Your WRITE operation is working correctly if:**
- ✅ All Critical Tests pass (1, 2, 3, 7, 12, 13)
- ✅ No segmentation faults
- ✅ No memory leaks (valgrind clean)
- ✅ File integrity maintained after operations
- ✅ Locks prevent concurrent edits correctly
- ✅ Delimiter parsing follows specification
- ✅ UNDO restores previous state
- ✅ Error messages are clear and correct

**Estimated Marks:** 30/30 for Days 8-11 if all tests pass! 🎉

---

# � PART 11A: UNDO OPERATION VERIFICATION (CRITICAL!)

## ⚠️ Per-File UNDO Testing - Bug Fix Verification

**Background:** We fixed a critical bug where only ONE global undo buffer existed.  
Now each file has its own undo buffer in a hash table. **This MUST be tested!**

**Requirement:** "The undo-es are file specific, and not user specific. So, if user A makes a change, and user B wants to undo it, user B can also do it. The undo history is maintained by the storage server."

---

## TEST 21.1: Multi-File UNDO Independence ⭐ CRITICAL!

**Purpose:** Verify UNDO works independently for EACH file

```bash
# Terminal 3: Client (user1)

# Create and modify File 1
CREATE undo_file1.txt
WRITE undo_file1.txt 0
0 File1
1 Original
ETIRW

# Create and modify File 2
CREATE undo_file2.txt
WRITE undo_file2.txt 0
0 File2
1 Original
ETIRW

# Modify File 1 again (this should save undo)
WRITE undo_file1.txt 0
2 Modified
ETIRW

# Modify File 2 again (this should save undo)
WRITE undo_file2.txt 0
2 Modified
ETIRW

# Now both files should have undo available!
READ undo_file1.txt  # Expected: File1 Original Modified
READ undo_file2.txt  # Expected: File2 Original Modified

# UNDO File 1
UNDO undo_file1.txt

# Verify File 1 reverted, but File 2 unchanged!
READ undo_file1.txt  # Expected: File1 Original (reverted)
READ undo_file2.txt  # Expected: File2 Original Modified (NOT reverted)

# UNDO File 2
UNDO undo_file2.txt

# Verify File 2 now reverted
READ undo_file2.txt  # Expected: File2 Original (reverted)
```

**✅ PASS CRITERIA:**
- ✅ Both files can be undone independently
- ✅ Undoing file1 doesn't affect file2
- ✅ Each file maintains its own undo history
- ✅ **This proves the bug fix works!**

**❌ OLD BUG (Fixed!):** Only the last written file could be undone

---

## TEST 21.2: UNDO After Multiple Files Modified

**Purpose:** Ensure undo buffer per file survives other file operations

```bash
# Create 3 files
CREATE undo_a.txt
WRITE undo_a.txt 0
0 Content
1 A
ETIRW

CREATE undo_b.txt
WRITE undo_b.txt 0
0 Content
1 B
ETIRW

CREATE undo_c.txt
WRITE undo_c.txt 0
0 Content
1 C
ETIRW

# Modify all 3
WRITE undo_a.txt 0
2 ModifiedA
ETIRW

WRITE undo_b.txt 0
2 ModifiedB
ETIRW

WRITE undo_c.txt 0
2 ModifiedC
ETIRW

# Now UNDO any file in any order - all should work!
UNDO undo_b.txt  # Should revert B
READ undo_b.txt  # Expected: Content B (no ModifiedB)

UNDO undo_a.txt  # Should revert A
READ undo_a.txt  # Expected: Content A (no ModifiedA)

UNDO undo_c.txt  # Should revert C
READ undo_c.txt  # Expected: Content C (no ModifiedC)
```

**✅ PASS CRITERIA:**
- All 3 files can be undone in any order
- Each undo is independent
- No undo buffer conflicts

---

## TEST 21.3: Cross-User UNDO (Spec Requirement!)

**Purpose:** "if user A makes a change, and user B wants to undo it, user B can also do it"

```bash
# Terminal 3: Client as user1
CREATE crossundo.txt
WRITE crossundo.txt 0
0 User1
1 wrote
2 this
ETIRW

ADDACCESS -W crossundo.txt user2
READ crossundo.txt  # Expected: User1 wrote this

# Now user2 modifies
EXIT
make run-client
# Username: user2

WRITE crossundo.txt 0
3 and
4 user2
5 modified
ETIRW

READ crossundo.txt  # Expected: User1 wrote this and user2 modified

# Now user1 can undo user2's change!
EXIT
make run-client
# Username: user1

UNDO crossundo.txt  # user1 undoing user2's change
READ crossundo.txt  # Expected: User1 wrote this (user2's changes gone)
```

**✅ PASS CRITERIA:**
- ✅ user1 can undo user2's modifications
- ✅ UNDO is file-specific, not user-specific
- ✅ Matches spec requirement exactly

---

## TEST 21.4: UNDO Permission Check

**Purpose:** Verify write permission required for UNDO

```bash
# Terminal 3: Client as user1
CREATE undoperm.txt
WRITE undoperm.txt 0
0 Original
ETIRW

ADDACCESS -R undoperm.txt user3  # Read-only for user3

WRITE undoperm.txt 0
1 Modified
ETIRW

# Try UNDO as user3 (read-only)
EXIT
make run-client
# Username: user3

UNDO undoperm.txt  # Should FAIL - no write permission
```

**✅ Expected:**
```
Error: Access denied: You don't have permission to undo 'undoperm.txt'
```

**✅ PASS CRITERIA:**
- UNDO requires write permission
- Clear error message for read-only users

---

## 🎯 UNDO Test Summary

**If all 4 tests pass:**
- ✅ Per-file undo works correctly (bug fixed!)
- ✅ Multiple files can maintain independent undo
- ✅ Cross-user undo works per spec
- ✅ Permission checks in place

**Estimated Marks:** 15/15 for UNDO feature! 🎉

---

# �🚀 PART 12: DAYS 12-13 ADVANCED FEATURES

## Overview

**Days 12-13 Requirements:**
- ✅ **STREAM Operation** (15 marks): Word-by-word streaming with 0.1s delay
- ✅ **EXEC Operation** (15 marks): Execute file content as command, capture output
- ✅ **Dynamic SS Addition** (LOW): Add storage servers anytime (already supported)
- ⚠️ **Efficient Search** (15 marks): LRU cache/Trie for fast file lookups (optional optimization)

**Total Available Marks:** 45 marks (30 for STREAM+EXEC, 15 for Search optimization)

---

## 📊 TEST 17: STREAM Operation - Word-by-Word Streaming

### **Test 17.1: Basic STREAM Test**

**Setup:**
```bash
# Terminal 1: Name Server (should already be running)
# Terminal 2: Storage Server (should already be running)
# Terminal 3: Client (user1)
```

**Create test file with content:**
```bash
# In client terminal (user1):
CREATE streamtest.txt

# Note: We've pre-created this file with content:
# "This is a test file for streaming. The quick brown fox jumps over the lazy dog. This sentence contains multiple words."
```

**Test STREAM command:**
```bash
# In client terminal (user1):
STREAM streamtest.txt

# Expected Output:
# ─────────────────────────────────────────────────────────────
# Streaming: streamtest.txt
# ─────────────────────────────────────────────────────────────
# This is a test file for streaming The quick 
# brown fox jumps over the lazy dog This sentence 
# contains multiple words 
# ─────────────────────────────────────────────────────────────
# ✓ Stream complete: 23 words received

# Note: Each word appears with ~0.1s delay (visible in real-time)
```

**✅ PASS Criteria:**
- Words appear one at a time with visible delay (~0.1s between words)
- All words from file are streamed
- Stream completes with success message
- Word count matches file content

---

### **Test 17.2: STREAM Large File**

**Create large file:**
```bash
# In storage server storage directory, create a large file:
# (Run this in a separate terminal, not in client)
cd "/home/chai404/OSN/Course Project/course-project-osn-3-0/storage_server/storage"
for i in {1..100}; do echo "Word$i" >> largefile.txt; done

# This creates a file with 100 words
```

**Register and stream:**
```bash
# In client terminal (user1):
CREATE largefile.txt  # First create via client

# Then stream:
STREAM largefile.txt

# Expected Output:
# Streaming should show all 100 words appearing one by one
# Takes approximately 10 seconds (100 words × 0.1s)
```

**✅ PASS Criteria:**
- All 100 words stream successfully
- Takes approximately 10 seconds
- No connection timeouts
- Stream completes without errors

---

### **Test 17.3: STREAM Non-Existent File**

```bash
# In client terminal (user1):
STREAM nonexistent.txt

# Expected Output:
# ✗ Error: File or resource not found
```

**✅ PASS Criteria:**
- Clear error message
- No crash or hang

---

### **Test 17.4: STREAM Access Control**

```bash
# Terminal 3: Client (user1)
CREATE private_stream.txt

# Terminal 4: Client (user2) - login as different user
# Try to stream file you don't have access to:
STREAM private_stream.txt

# Expected Output:
# ✗ Error: Access denied - insufficient permissions
```

**✅ PASS Criteria:**
- Access control enforced
- User2 cannot stream user1's file without permissions
- Clear error message

---

### **Test 17.5: STREAM with Permissions**

```bash
# Terminal 3: Client (user1)
# Grant read access to user2:
ADDACCESS -R private_stream.txt user2

# Terminal 4: Client (user2)
# Now try streaming:
STREAM private_stream.txt

# Expected Output:
# ✓ Stream complete: X words received
```

**✅ PASS Criteria:**
- After granting access, user2 can stream
- Words appear with proper delay
- Stream completes successfully

---

## ⚙️ TEST 18: EXEC Operation - Execute File Content

### **Test 18.1: Basic EXEC Test**

**Setup test script:**
```bash
# We've pre-created testscript.sh with:
# echo 'Hello from EXEC command!'; date; ls -l | head -3

# In client terminal (user1):
CREATE testscript.sh

# Then execute:
EXEC testscript.sh

# Expected Output:
# ─────────────────────────────────────────────────────────────
# Execution Output: testscript.sh
# ─────────────────────────────────────────────────────────────
# Hello from EXEC command!
# Sat Nov 16 22:XX:XX IST 2025
# total XX
# -rw-rw-r-- 1 chai404 chai404 ...
# ─────────────────────────────────────────────────────────────
# ✓ Command executed successfully
```

**✅ PASS Criteria:**
- Command executes successfully
- Output is captured and displayed
- Current date/time shown
- Directory listing appears

---

### **Test 18.2: EXEC Python Script**

**Create Python script:**
```bash
# In storage server storage directory:
cd "/home/chai404/OSN/Course Project/course-project-osn-3-0/storage_server/storage"
echo "print('Hello from Python!'); print('2 + 2 =', 2+2); import sys; print('Python version:', sys.version)" > pytest.py

# In client terminal (user1):
CREATE pytest.py

EXEC pytest.py

# Expected Output:
# ─────────────────────────────────────────────────────────────
# Execution Output: pytest.py
# ─────────────────────────────────────────────────────────────
# Hello from Python!
# 2 + 2 = 4
# Python version: 3.X.X (...)
# ─────────────────────────────────────────────────────────────
# ✓ Command executed successfully
```

**✅ PASS Criteria:**
- Python script executes
- All print statements appear
- Python version displayed

---

### **Test 18.3: EXEC with Errors**

**Create failing script:**
```bash
# In storage server storage directory:
cd "/home/chai404/OSN/Course Project/course-project-osn-3-0/storage_server/storage"
echo "ls /nonexistent_directory_12345; echo 'This will not appear if command fails'" > failscript.sh

# In client terminal (user1):
CREATE failscript.sh

EXEC failscript.sh

# Expected Output:
# ✗ Error: Command failed with exit code 2. Output: ls: cannot access '/nonexistent_directory_12345': No such file or directory
```

**✅ PASS Criteria:**
- Error captured correctly
- Exit code reported
- Error message from command shown

---

### **Test 18.4: EXEC Access Control**

```bash
# Terminal 3: Client (user1)
CREATE exec_private.sh

# Terminal 4: Client (user2)
# Try to execute without permissions:
EXEC exec_private.sh

# Expected Output:
# ✗ Error: Access denied - insufficient permissions
```

**✅ PASS Criteria:**
- Access control enforced for EXEC
- Clear error message

---

### **Test 18.5: EXEC Non-Existent File**

```bash
# In client terminal (user1):
EXEC nonexistent_script.sh

# Expected Output:
# ✗ Error: File or resource not found
```

**✅ PASS Criteria:**
- Clear error message
- No crash

---

## 🔍 TEST 19: Combined STREAM and EXEC Workflow

### **Test 19.1: Create, Stream, Execute Workflow**

```bash
# Terminal 3: Client (user1)

# 1. Create a script file
CREATE workflow_test.sh

# 2. Stream the file (to verify content is readable)
STREAM workflow_test.sh

# Expected: Words from script appear one by one

# 3. Execute the script
EXEC workflow_test.sh

# Expected: Script executes and output is shown

# 4. Verify with READ
READ workflow_test.sh

# Expected: Full script content displayed
```

**✅ PASS Criteria:**
- All four operations work in sequence
- STREAM shows word-by-word view
- EXEC executes the script
- READ shows full content
- No errors or crashes

---

### **Test 19.2: Multi-User STREAM and EXEC**

```bash
# Terminal 3: Client (user1)
CREATE shared_script.sh
ADDACCESS -R shared_script.sh user2

# Terminal 4: Client (user2)
# Both operations should work with read permission:
STREAM shared_script.sh  # ✓ Should work
EXEC shared_script.sh    # ✓ Should work (read permission is sufficient)
```

**✅ PASS Criteria:**
- Read permission allows both STREAM and EXEC
- Both users can access simultaneously
- No lock conflicts (read operations)

---

## 🎯 TEST 20: Performance and Stability

### **Test 20.1: STREAM Performance**

```bash
# Create very large file (1000 words):
cd "/home/chai404/OSN/Course Project/course-project-osn-3-0/storage_server/storage"
for i in {1..1000}; do echo "Word$i"; done > huge.txt

# In client terminal (user1):
CREATE huge.txt
time STREAM huge.txt

# Expected time: ~100 seconds (1000 words × 0.1s)
```

**✅ PASS Criteria:**
- Streams all 1000 words
- Takes approximately 100 seconds
- No memory leaks
- Connection stays stable

---

### **Test 20.2: EXEC Long-Running Command**

```bash
# Create long-running script:
cd "/home/chai404/OSN/Course Project/course-project-osn-3-0/storage_server/storage"
echo "sleep 2; echo 'Task 1 complete'; sleep 2; echo 'Task 2 complete'; echo 'All done!'" > longtask.sh

# In client terminal (user1):
CREATE longtask.sh
EXEC longtask.sh

# Expected: Waits 4 seconds total, then shows all output
```

**✅ PASS Criteria:**
- Command completes after ~4 seconds
- All output captured
- No timeout errors

---

### **Test 20.3: Concurrent STREAM Operations**

```bash
# Terminal 3: Client (user1)
STREAM largefile.txt

# Terminal 4: Client (user2) - while first stream is running
# Grant access first (in terminal 3):
# ADDACCESS -R largefile.txt user2

# Then in terminal 4:
STREAM largefile.txt

# Expected: Both streams work simultaneously
```

**✅ PASS Criteria:**
- Multiple clients can stream same file simultaneously
- Each gets independent stream
- No interference between streams

---

## 📋 SUMMARY: Days 12-13 Test Checklist

### **STREAM Operation (15 marks):**
- ✅ Test 17.1: Basic word-by-word streaming with 0.1s delay
- ✅ Test 17.2: Large file streaming (100+ words)
- ✅ Test 17.3: Error handling (non-existent file)
- ✅ Test 17.4: Access control enforcement
- ✅ Test 17.5: Permission-based access
- ✅ Test 20.1: Performance test (1000 words)
- ✅ Test 20.3: Concurrent streaming

### **EXEC Operation (15 marks):**
- ✅ Test 18.1: Basic shell command execution
- ✅ Test 18.2: Python script execution
- ✅ Test 18.3: Error handling (failed commands)
- ✅ Test 18.4: Access control enforcement
- ✅ Test 18.5: Non-existent file error
- ✅ Test 20.2: Long-running command handling

### **Integration Tests:**
- ✅ Test 19.1: Create → Stream → Exec → Read workflow
- ✅ Test 19.2: Multi-user access with permissions

### **Expected Marks:**
- **STREAM**: 15/15 if all tests pass
- **EXEC**: 15/15 if all tests pass
- **Dynamic SS**: Already implemented (Days 1-7)
- **Efficient Search**: Optional optimization (15 marks if implemented)

**Total: 30/30 marks for Days 12-13 core features! 🎉**

---

## 🔧 Debugging Tips for Days 12-13

**If STREAM not working:**
1. Check `handle_stream_request()` in storage_server.c
2. Verify `usleep(100000)` for 0.1s delay
3. Check word parsing with `parse_words()`
4. Ensure OP_STOP sent at end

**If EXEC not working:**
1. Check `handle_exec_request()` in nameserver.c
2. Verify `popen()` command execution
3. Check output buffer size (MAX_CONTENT_LEN)
4. Verify file fetching from SS before execution

**If Access Control fails:**
1. Check ACL checks in both STREAM and EXEC handlers
2. Verify read permission is checked
3. Test with different users

**If Performance issues:**
1. Check connection stability for long streams
2. Verify no memory leaks (valgrind)
3. Test network buffering

---

**Good luck with Days 12-13 evaluation! STREAM and EXEC are working! 💪🚀**

---

# � PART 13: EFFICIENT SEARCH VERIFICATION (15 marks!)

## ⚠️ HashMap and LRU Cache Testing - Critical for Performance!

**Requirement:** "Efficient Search: The Name Server should implement efficient search algorithms to quickly locate files based on their names or other metadata, minimizing latency in file access operations. Furthermore, caching should be implemented for recent searches to expedite subsequent requests for the same data."

**Note:** "An approach faster than O(N) time complexity is expected here. Efficient data structures like Tries, Hashmaps, etc. can be used."

---

## TEST 24.1: HashMap Statistics Verification ⭐ CRITICAL!

**Purpose:** Verify O(1) file lookup using HashMap

```bash
# Check Name Server logs for HashMap initialization
cat logs/nameserver.log | grep -i "hash"

# Expected log entries:
# "HashMap initialized (size: 1009)"
# "File added to HashMap: <filename> -> SS[0]"
```

**✅ PASS CRITERIA:**
- HashMap initialization logged on NS startup
- Prime number size (1009) for good distribution
- Files added to HashMap on CREATE/SS registration

---

### **Test 24.1A: HashMap Collision Handling**

```bash
# Create many files to test collision handling
for i in {1..50}; do
    echo "CREATE hashtest$i.txt" | ./client
done

# Check NS logs for HashMap statistics
cat logs/nameserver.log | grep "HashMap"
```

**✅ Expected:**
```
HashMap Stats: 50 entries, X buckets used, avg chain length: Y
```

**✅ PASS CRITERIA:**
- Multiple entries per bucket (collisions handled)
- Chain length reasonable (< 5 average)
- No crashes with many files

---

## TEST 24.2: Search Performance Test

**Purpose:** Verify sub-linear time complexity

```bash
# Create 100 files
for i in {1..100}; do
    echo "CREATE perffile$i.txt" | ./client
done

# Time file lookups
time {
    for i in {1..100}; do
        echo "INFO perffile$i.txt" | ./client
    done
}

# Expected: < 10 seconds for 100 lookups (< 0.1s per lookup)
```

**✅ PASS CRITERIA:**
- 100 lookups complete in < 10 seconds
- Average lookup time < 0.1 seconds
- No O(N) linear search detected

**❌ If > 15 seconds:** O(N) search - HashMap not working!

---

## TEST 24.3: LRU Cache Effectiveness

**Purpose:** Verify caching improves repeated access performance

```bash
# Access same file multiple times
INFO testfile.txt  # Cold - cache miss
INFO testfile.txt  # Warm - cache hit
INFO testfile.txt  # Warm - cache hit
INFO testfile.txt  # Warm - cache hit

# Check logs:
cat logs/nameserver.log | grep "testfile.txt" | grep -i "cache"
```

**✅ Expected:**
```
LRU Cache MISS: testfile.txt
LRU Cache HIT: testfile.txt
LRU Cache HIT: testfile.txt
LRU Cache HIT: testfile.txt
```

**✅ PASS CRITERIA:**
- First access is cache miss (not in LRU)
- Subsequent accesses are cache hits (found in LRU)
- Cache hit ratio > 70% for repeated access
- Cache size limit enforced (100 entries)

---

### **Test 24.3A: LRU Cache Eviction**

```bash
# Fill cache beyond capacity (>100 files)
for i in {1..120}; do
    echo "INFO cachetest$i.txt" | ./client
done

# Access first file again - should be evicted (MISS)
INFO cachetest1.txt

# Check logs for LRU eviction
cat logs/nameserver.log | grep "LRU.*evict"
```

**✅ Expected:**
```
LRU Cache FULL: Evicting least recently used entry
```

**✅ PASS CRITERIA:**
- LRU eviction happens when cache full
- Least recently used entries removed first
- No memory leaks from eviction

---

## 🎯 Efficient Search Test Summary

**If all tests pass:**
- ✅ HashMap provides O(1) file lookups
- ✅ LRU cache improves repeated access
- ✅ Sub-linear time complexity verified
- ✅ Performance meets spec requirements

**Estimated Marks:** 15/15 for Efficient Search! 🎉

---

# 🔧 PART 14: DATA PERSISTENCE & RECOVERY TESTING (Spec Requirement)

## ⚠️ Critical System Requirement Tests

**Requirement:** "Data Persistence: All files and their associated metadata (like access control lists) must be stored persistently. This ensures that data remains intact and accessible even after Storage Servers restart or fail."

---

## TEST 25.1: Storage Server Restart with File Recovery ⭐ CRITICAL!

**Purpose:** Verify files persist after SS restart

### **Step 1: Create Files**
```bash
# Terminal 3: Client (user1)
CREATE persist1.txt
WRITE persist1.txt 0
0 Data
1 before
2 restart
ETIRW

CREATE persist2.txt
WRITE persist2.txt 0
0 More
1 data
ETIRW

# Note current files
VIEW -a
```

### **Step 2: Stop Storage Server**
```bash
# Terminal 2: Press Ctrl+C to stop SS
^C

# Files should still exist in ./storage_server/storage/
ls -la ./storage_server/storage/
# Expected: persist1.txt, persist2.txt
```

### **Step 3: Restart Storage Server**
```bash
# Terminal 2:
make run-ss

# Expected log:
# "Found existing file: persist1.txt"
# "Found existing file: persist2.txt"
# "Registering 2 files with NS"
```

### **Step 4: Verify Files Accessible**
```bash
# Terminal 3:
VIEW -a
READ persist1.txt  # Expected: Data before restart
READ persist2.txt  # Expected: More data
```

**✅ PASS CRITERIA:**
- SS finds existing files on restart
- Re-registers files with NS
- Files remain accessible
- Content preserved exactly
- No data loss

---

## TEST 25.2: ACL Persistence After NS Restart ⭐ CRITICAL!

**Purpose:** Verify access control persists after NS restart

### **Step 1: Create File with ACL**
```bash
# Terminal 3: Client (user1)
CREATE acl_persist.txt
ADDACCESS -R acl_persist.txt user2
ADDACCESS -W acl_persist.txt user3

# Verify ACL
INFO acl_persist.txt
# Expected: user2 (R), user3 (RW)
```

### **Step 2: Stop Name Server**
```bash
# Terminal 1: Press Ctrl+C to stop NS
^C

# ACL data should exist in acl_data.db
ls -la acl_data.db
```

### **Step 3: Restart Name Server**
```bash
# Terminal 1:
make run-ns

# Expected log:
# "ACL data loaded from acl_data.db"
# "Loaded X ACL entries"
```

### **Step 4: Verify ACL Preserved**
```bash
# Restart client and verify ACL preserved
# Terminal 3:
EXIT
make run-client
# Username: user2

VIEW  # Should show acl_persist.txt
READ acl_persist.txt  # Should succeed (has read access)
WRITE acl_persist.txt 0  # Should fail (no write access)
```

**✅ PASS CRITERIA:**
- ACL data loads from acl_data.db on NS restart
- Permissions preserved exactly
- user2 still has read access
- user3 still has write access
- Owner permissions maintained

---

## TEST 25.3: UNDO Buffer NOT Persistent (By Design)

**Purpose:** Verify undo buffers are session-only (don't persist)

```bash
# Create and modify file
CREATE undo_persist.txt
WRITE undo_persist.txt 0
0 Original
ETIRW

WRITE undo_persist.txt 0
1 Modified
ETIRW

# Restart SS (Terminal 2: Ctrl+C, then make run-ss)

# Try to UNDO after restart
UNDO undo_persist.txt
```

**✅ Expected:**
```
Error: No undo available for this file
```

**✅ PASS CRITERIA:**
- UNDO buffer is NOT persistent (by design)
- Clear error message after SS restart
- File content persists (shows "Modified" state)

---

## 🎯 Data Persistence Test Summary

**If all tests pass:**
- ✅ Files persist across SS restarts
- ✅ ACL data persists across NS restarts
- ✅ System recovers gracefully
- ✅ No data loss or corruption

**Critical for Production:** These tests verify system reliability!

---

# 🔥 PART 15: FINAL PRE-EVALUATION CHECKLIST
---

# 🌟 PART 16: BONUS FEATURES TESTING (50 MARKS)

## Overview

This section tests the BONUS functionalities implementation:
1. **Hierarchical Folder Structure** (10 marks)
2. **Checkpoints** (15 marks)  
3. **Requesting Access** (5 marks)
4. **Fault Tolerance** (15 marks)
5. **The Unique Factor** (5 marks)

---

## 🗂️ TEST 16.1: Hierarchical Folder Structure (10 marks)

### **Test 16.1.1: Create Folder**

```bash
# Terminal 3: Client (user1)
CREATEFOLDER projects

# Expected Output:
✓ Folder 'projects' created successfully!
```

### **Test 16.1.2: Create Multiple Folders**

```bash
CREATEFOLDER documents
CREATEFOLDER archives
CREATEFOLDER temp

# Expected: All folders created
```

### **Test 16.1.3: Move File to Folder**

```bash
CREATE report.txt
MOVE report.txt projects

# Expected Output:
✓ File 'report.txt' moved to folder 'projects'
```

### **Test 16.1.4: View Files in Folder**

```bash
CREATE file1.txt
CREATE file2.txt
MOVE file1.txt projects
MOVE file2.txt projects

VIEWFOLDER projects

# Expected Output:
Files in folder 'projects' (3):
  - report.txt
  - file1.txt
  - file2.txt
```

### **Test 16.1.5: Error Cases**

```bash
# Duplicate folder
CREATEFOLDER projects
# Expected: Error: Folder 'projects' already exists

# Move non-existent file
MOVE nonexistent.txt projects
# Expected: Error: File 'nonexistent.txt' not found

# Move to non-existent folder
MOVE file1.txt invalid_folder
# Expected: Error: Folder 'invalid_folder' not found
```

**✅ PASS CRITERIA:**
- Folders are created in storage/.
- Files move correctly (not copied)
- VIEWFOLDER lists files in hierarchy
- Clear error messages for edge cases

---

## 📸 TEST 16.2: Checkpoint System (15 marks)

### **Test 16.2.1: Create Checkpoint**

```bash
CREATE checkpoint_test.txt
WRITE checkpoint_test.txt 0
0 Initial
1 content
ETIRW

CHECKPOINT checkpoint_test.txt v1

# Expected Output:
✓ Checkpoint 'v1' created for file 'checkpoint_test.txt'
```

### **Test 16.2.2: View Checkpoint**

```bash
VIEWCHECKPOINT checkpoint_test.txt v1

# Expected Output:
─────────────────────────────────────────────────────────────
Checkpoint: checkpoint_test.txt@v1
─────────────────────────────────────────────────────────────
Initial content
─────────────────────────────────────────────────────────────
```

### **Test 16.2.3: Revert to Checkpoint**

```bash
# Modify file
WRITE checkpoint_test.txt 0
2 modified
ETIRW

READ checkpoint_test.txt
# Expected: Initial content modified

REVERT checkpoint_test.txt v1

READ checkpoint_test.txt
# Expected: Initial content (original state restored)
```

### **Test 16.2.4: List Checkpoints**

```bash
CHECKPOINT checkpoint_test.txt v2
CHECKPOINT checkpoint_test.txt v3

LISTCHECKPOINTS checkpoint_test.txt

# Expected Output:
Checkpoints for 'checkpoint_test.txt' (3):
  - v1 (created: 2025-11-19 10:30:15)
  - v2 (created: 2025-11-19 10:32:20)
  - v3 (created: 2025-11-19 10:33:10)
```

### **Test 16.2.5: Checkpoint Error Cases**

```bash
# Duplicate checkpoint tag
CHECKPOINT checkpoint_test.txt v1
# Expected: Error: Checkpoint 'v1' already exists

# View non-existent checkpoint
VIEWCHECKPOINT checkpoint_test.txt invalid
# Expected: Error: Checkpoint 'invalid' not found

# Revert non-existent file
REVERT nonexistent.txt v1
# Expected: Error: File 'nonexistent.txt' not found
```

**✅ PASS CRITERIA:**
- Checkpoints stored in storage/.checkpoints/filename/
- Multiple checkpoints per file supported
- Revert restores exact state
- List shows all checkpoints with timestamps
- Clear error handling

---

## 📬 TEST 16.3: Access Request System (5 marks)

### **Test 16.3.1: Request Access**

```bash
# Terminal 3: user1 creates file
CREATE private.txt

# Terminal 4: user2 requests access
REQUESTACCESS private.txt READ

# Expected Output:
✓ Access request sent to file owner
```

### **Test 16.3.2: View Pending Requests (Owner)**

```bash
# Terminal 3: user1 checks requests
VIEWREQUESTS

# Expected Output:
Pending access requests (1):
  1. user2 requests READ access to 'private.txt' (received: 2025-11-19 10:40:15)
```

### **Test 16.3.3: Approve Request**

```bash
# Terminal 3: user1 approves
APPROVEREQUEST private.txt user2

# Expected Output:
✓ Access request approved for user2
✓ user2 now has READ access to 'private.txt'
```

### **Test 16.3.4: Verify Access Granted**

```bash
# Terminal 4: user2 can now read
READ private.txt

# Expected: Success (file content displayed)
```

### **Test 16.3.5: Deny Request**

```bash
# Terminal 5: user3 requests access
REQUESTACCESS private.txt WRITE

# Terminal 3: user1 denies
DENYREQUEST private.txt user3

# Expected Output:
✓ Access request denied for user3
```

**✅ PASS CRITERIA:**
- Users can request access they don't have
- Owners see pending requests
- Approve grants appropriate access
- Deny rejects without granting
- Requests don't duplicate

---

## 🛡️ TEST 16.4: Fault Tolerance (15 marks)

### **Test 16.4.1: File Replication**

```bash
# Start with 2 storage servers
# Terminal 2: SS1
make run-ss

# Terminal 6: SS2
./storage_server/storage_server 192.168.1.187 9081 9082 ./storage_9081

# Terminal 3: Create file (should replicate to SS2)
CREATE replicated.txt

# Verify replication
# Expected: File exists on both SS1 and SS2
ls storage/replicated.txt
ls storage_9081/replicated.txt
```

### **Test 16.4.2: Failure Detection**

```bash
# Terminal 2: Stop SS1 (Ctrl+C)

# Terminal 1: NS should detect failure
# Expected in NS log:
# Storage Server SS[0] heartbeat timeout
# SS[0] marked as unavailable
```

### **Test 16.4.3: Failover to Replica**

```bash
# With SS1 down, access file
# Terminal 3:
READ replicated.txt

# Expected: 
# - NS routes to SS2 (backup)
# - File content retrieved successfully
✓ Read successful from replica server
```

### **Test 16.4.4: SS Recovery**

```bash
# Terminal 2: Restart SS1
make run-ss

# Expected in NS log:
# Storage Server SS[0] reconnected
# Synchronizing with replica SS[1]...
# Sync complete: 5 files updated
```

### **Test 16.4.5: Write Replication**

```bash
# With both SS running
WRITE replicated.txt 0
0 Replicated
1 content
ETIRW

# Verify on both servers
# Terminal 2: Check SS1
cat storage/replicated.txt
# Expected: Replicated content

# Terminal 6: Check SS2
cat storage_9081/replicated.txt
# Expected: Replicated content (same)
```

**✅ PASS CRITERIA:**
- Files automatically replicate to backup SS
- NS detects SS failures via heartbeat
- Reads failover to replica seamlessly
- Writes replicate asynchronously
- SS recovery re-syncs data
- No data loss during failover

---

## 🔍 TEST 16.5: Unique Factor - Content Search (5 marks)

### **Test 16.5.1: Index Files**

```bash
CREATE doc1.txt
WRITE doc1.txt 0
0 Distributed
1 systems
2 are
3 complex
ETIRW

CREATE doc2.txt
WRITE doc2.txt 0
0 Operating
1 systems
2 manage
3 resources
ETIRW
```

### **Test 16.5.2: Search by Content**

```bash
SEARCH systems

# Expected Output:
Files containing 'systems' (2):
  - doc1.txt: "Distributed systems are complex"
  - doc2.txt: "Operating systems manage resources"
```

### **Test 16.5.3: Multi-Word Search**

```bash
SEARCH "Distributed systems"

# Expected Output:
Files containing 'Distributed systems' (1):
  - doc1.txt: "Distributed systems are complex"
```

### **Test 16.5.4: Search Performance**

```bash
# Create 100 files with random content
# (automated script)

SEARCH performance

# Expected:
# - Search completes in < 2 seconds
# - Results sorted by relevance
```

**✅ PASS CRITERIA:**
- Indexed search (not linear grep)
- Sub-second search for 100+ files
- Multi-word phrase support
- Results show context snippet
- Works with files in folders

---

## 🎯 BONUS FEATURES SUMMARY

### **Scoring Breakdown:**

| Feature | Max Points | Test Coverage |
|---------|-----------|---------------|
| Hierarchical Folders | 10 | CREATEFOLDER, MOVE, VIEWFOLDER |
| Checkpoints | 15 | CHECKPOINT, VIEW, REVERT, LIST |
| Access Requests | 5 | REQUEST, APPROVE, DENY |
| Fault Tolerance | 15 | Replication, Failover, Recovery |
| Unique Factor | 5 | Content Search |
| **TOTAL** | **50** | **15+ Commands** |

### **Integration Tests:**

```bash
# Test 1: Folder + Checkpoint
CREATEFOLDER projects
CREATE plan.txt
MOVE plan.txt projects
CHECKPOINT projects/plan.txt milestone1

# Test 2: Replication + Checkpoint
CREATE important.txt
CHECKPOINT important.txt backup
# Stop SS1, verify checkpoint accessible via SS2

# Test 3: Access Request + Folder
CREATEFOLDER shared
MOVE doc.txt shared
REQUESTACCESS shared/doc.txt READ
APPROVEREQUEST shared/doc.txt user2

# Test 4: Search + Folders
CREATEFOLDER archive
MOVE old_file.txt archive
SEARCH "old content"
# Expected: Finds file in archive folder
```

### **Edge Cases:**

```bash
# Checkpoint on moved file
CREATE file.txt
CHECKPOINT file.txt v1
MOVE file.txt folder
REVERT folder/file.txt v1
# Expected: Works with new path

# Request access to file in folder
REQUESTACCESS folder/private.txt READ
# Expected: Request sent correctly

# Search in folders
CREATEFOLDER deep/nested
MOVE file.txt deep/nested
SEARCH "content"
# Expected: Finds file.txt in nested folder

# Replicate folder structure
CREATEFOLDER project
MOVE code.c project
# Stop SS1, restart SS2
# Expected: project/ folder replicated
```

---

## 📊 BONUS TESTING REPORT TEMPLATE

```
=================================================
Docs++ BONUS FEATURES TESTING REPORT
=================================================
Date: _______________
Tester: _______________

BONUS FEATURE RESULTS:
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

Hierarchical Folders (10 marks):
  CREATEFOLDER:     [PASS/FAIL] - Notes: ___________
  MOVE:             [PASS/FAIL] - Notes: ___________
  VIEWFOLDER:       [PASS/FAIL] - Notes: ___________
  Error Handling:   [PASS/FAIL] - Notes: ___________
  Score: ___/10

Checkpoints (15 marks):
  CHECKPOINT:       [PASS/FAIL] - Notes: ___________
  VIEWCHECKPOINT:   [PASS/FAIL] - Notes: ___________
  REVERT:           [PASS/FAIL] - Notes: ___________
  LISTCHECKPOINTS:  [PASS/FAIL] - Notes: ___________
  Multiple CPs:     [PASS/FAIL] - Notes: ___________
  Score: ___/15

Access Requests (5 marks):
  REQUESTACCESS:    [PASS/FAIL] - Notes: ___________
  VIEWREQUESTS:     [PASS/FAIL] - Notes: ___________
  APPROVE:          [PASS/FAIL] - Notes: ___________
  DENY:             [PASS/FAIL] - Notes: ___________
  Score: ___/5

Fault Tolerance (15 marks):
  Replication:      [PASS/FAIL] - Notes: ___________
  Failure Detection:[PASS/FAIL] - Notes: ___________
  Failover:         [PASS/FAIL] - Notes: ___________
  Recovery:         [PASS/FAIL] - Notes: ___________
  Write Replication:[PASS/FAIL] - Notes: ___________
  Score: ___/15

Unique Factor - Search (5 marks):
  Index Build:      [PASS/FAIL] - Notes: ___________
  Basic Search:     [PASS/FAIL] - Notes: ___________
  Multi-word:       [PASS/FAIL] - Notes: ___________
  Performance:      [PASS/FAIL] - Notes: ___________
  Score: ___/5

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

TOTAL BONUS SCORE: ___/50

INTEGRATION TESTS: [PASS/FAIL]
EDGE CASES: [PASS/FAIL]

OVERALL BONUS STATUS: [PRODUCTION-READY / NEEDS WORK]

Additional Notes:
_________________________________________________
_________________________________________________
```

---

## 🚀 QUICK BONUS DEMO SCRIPT

**For rapid evaluation demonstration (5 minutes):**

```bash
# 1. Folders (30 seconds)
CREATEFOLDER demo
CREATE demo.txt
MOVE demo.txt demo
VIEWFOLDER demo

# 2. Checkpoints (45 seconds)
CREATE important.txt
CHECKPOINT important.txt backup
WRITE important.txt 0
0 Modified
ETIRW
LISTCHECKPOINTS important.txt
REVERT important.txt backup

# 3. Access Requests (30 seconds)
# (requires 2 clients)
REQUESTACCESS important.txt READ
VIEWREQUESTS
APPROVEREQUEST important.txt user2

# 4. Fault Tolerance (60 seconds)
# (requires 2 storage servers)
CREATE replicated.txt
# Stop SS1 (Ctrl+C)
READ replicated.txt  # Still works!
# Restart SS1 (auto-sync)

# 5. Search (30 seconds)
SEARCH "important"
# Shows: important.txt with context
```

**Expected Demo Time:** 3-5 minutes  
**Demonstrates:** All 5 bonus features working  
**Impact:** +50 marks if successful!

---

## ⚠️ CRITICAL BONUS TESTING NOTES

1. **Fault Tolerance requires 2+ Storage Servers**
   - Start SS1: `make run-ss`
   - Start SS2: `./storage_server/storage_server 192.168.1.187 9081 9082 ./storage_9081`

2. **Checkpoint storage location**
   - Stored in: `storage/.checkpoints/filename/tag.checkpoint`
   - Not visible in regular VIEW commands

3. **Access Requests need multiple users**
   - Start client1 as owner
   - Start client2 as requester
   - Owner approves/denies from client1

4. **Search index updates**
   - Re-index after every WRITE operation
   - Background thread for large files

5. **Folder paths in commands**
   - Use relative paths: `folder/file.txt`
   - Not absolute: `/storage/folder/file.txt`

---

**�� YOUR BONUS FEATURES ARE READY WHEN:**
- ✅ All 5 features implemented
- ✅ Integration tests pass
- ✅ Edge cases handled gracefully
- ✅ Performance meets benchmarks
- ✅ Demo script runs smoothly
- ✅ No crashes under stress
- ✅ Clear error messages

---

**CONGRATULATIONS! You've completed the comprehensive testing for all features including BONUS functionalities! 🎉**

**Total Possible Marks:**
- Main Features: 200 marks
- Bonus Features: 50 marks
- **GRAND TOTAL: 250 marks**

---

**Final Checklist Before Submission:**
- [ ] All main features tested (200 marks)
- [ ] All bonus features tested (50 marks)
- [ ] Stress tests passed
- [ ] Memory leak free (valgrind)
- [ ] Logs comprehensive
- [ ] Code documented
- [ ] README updated
- [ ] GitHub repo synchronized

**GOOD LUCK! 🚀**
