# Makefile for Docs++ Project
# 
# Quick commands:
#   make all      - Build everything
#   make test     - Run test programs
#   make run      - Run all main programs (NS, SS, Client in background)
#   make clean    - Remove compiled files
#   make cleanall - Remove everything including logs

# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -g -I./common
LDFLAGS = 

# Directories
COMMON_DIR = common
TEST_DIR = tests
LOG_DIR = logs
CLIENT_DIR = client
NS_DIR = nameserver
SS_DIR = storage_server
STORAGE_DIR = $(SS_DIR)/storage

# Common object files
COMMON_OBJS = $(COMMON_DIR)/socket_utils.o $(COMMON_DIR)/logger.o

# Test programs
TEST_SERVER = $(TEST_DIR)/test_server
TEST_CLIENT = $(TEST_DIR)/test_client

# Main programs
CLIENT = $(CLIENT_DIR)/client
NAMESERVER = $(NS_DIR)/nameserver
STORAGESERVER = $(SS_DIR)/storage_server

# ============================================================================
# Default target - Build everything
# ============================================================================
all: $(LOG_DIR) $(STORAGE_DIR) common tests main
	@echo ""
	@echo "=========================================="
	@echo "✓ Build completed successfully!"
	@echo "=========================================="
	@echo ""
	@echo "Available commands:"
	@echo "  make test         - Run test programs"
	@echo "  make run-ns       - Run Name Server"
	@echo "  make run-ss       - Run Storage Server"
	@echo "  make run-client   - Run Client"
	@echo "  make logs-main    - View main program logs"
	@echo "  make clean        - Clean compiled files"
	@echo ""

# ============================================================================
# Build common utilities
# ============================================================================
common: $(COMMON_OBJS)
	@echo "✓ Common utilities built successfully!"

$(COMMON_DIR)/%.o: $(COMMON_DIR)/%.c $(COMMON_DIR)/%.h
	@echo "Compiling $<..."
	$(CC) $(CFLAGS) -c $< -o $@

# ============================================================================
# Build test programs
# ============================================================================
tests: $(TEST_SERVER) $(TEST_CLIENT)
	@echo "✓ Test programs built successfully!"

$(TEST_SERVER): $(TEST_DIR)/test_server.c $(COMMON_OBJS)
	@echo "Building test_server..."
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

$(TEST_CLIENT): $(TEST_DIR)/test_client.c $(COMMON_OBJS)
	@echo "Building test_client..."
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

# ============================================================================
# Build main programs (Name Server, Storage Server, Client)
# ============================================================================
main: nameserver storageserver client
	@echo "✓ Main programs built successfully!"

nameserver: $(NAMESERVER)

$(NAMESERVER): $(NS_DIR)/nameserver.c $(COMMON_OBJS)
	@echo "Building nameserver..."
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

storageserver: $(STORAGESERVER)

$(STORAGESERVER): $(SS_DIR)/storage_server.c $(COMMON_OBJS)
	@echo "Building storage_server..."
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS) -lpthread

client: $(CLIENT)

$(CLIENT): $(CLIENT_DIR)/client.c $(COMMON_OBJS)
	@echo "Building client..."
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

# ============================================================================
# Create necessary directories
# ============================================================================
$(LOG_DIR):
	@echo "Creating logs directory..."
	@mkdir -p $(LOG_DIR)

$(STORAGE_DIR):
	@echo "Creating storage directory..."
	@mkdir -p $(STORAGE_DIR)

# ============================================================================
# Run main programs
# ============================================================================
run-ns: $(NAMESERVER) $(LOG_DIR)
	@echo "=========================================="
	@echo "Starting Name Server..."
	@echo "=========================================="
	@echo "Press Ctrl+C to stop"
	@echo ""
	./$(NAMESERVER)

run-ss: $(STORAGESERVER) $(LOG_DIR) $(STORAGE_DIR)
	@echo "=========================================="
	@echo "Starting Storage Server..."
	@echo "=========================================="
	@echo "Press Ctrl+C to stop"
	@echo ""
	./$(STORAGESERVER)

run-client: $(CLIENT) $(LOG_DIR)
	@echo "=========================================="
	@echo "Starting Client..."
	@echo "=========================================="
	@echo ""
	./$(CLIENT)

# ============================================================================
# Clean build files
# ============================================================================
clean:
	@echo "Cleaning build files..."
	@rm -f $(COMMON_DIR)/*.o
	@rm -f $(TEST_SERVER) $(TEST_CLIENT)
	@rm -f $(CLIENT) $(NAMESERVER) $(STORAGESERVER)
	@echo "✓ Clean complete"

# Clean everything including logs and storage
cleanall: clean
	@echo "Cleaning logs and storage..."
	@rm -rf $(LOG_DIR)
	@rm -rf $(STORAGE_DIR)
	@echo "✓ Clean all complete"

# Clean only storage files
clean-storage:
	@echo "Cleaning storage files..."
	@rm -rf $(STORAGE_DIR)/*
	@echo "✓ Storage cleaned"

# ============================================================================
# Test targets
# ============================================================================
test: $(TEST_SERVER) $(TEST_CLIENT) $(LOG_DIR)
	@echo ""
	@echo "=========================================="
	@echo "Running socket tests..."
	@echo "=========================================="
	@echo ""
	@echo "Starting test server in background..."
	@./$(TEST_SERVER) &
	@sleep 2
	@echo ""
	@echo "Running test client..."
	@./$(TEST_CLIENT)
	@echo ""
	@sleep 1
	@echo "Stopping test server..."
	@pkill -f test_server || true
	@echo ""
	@echo "=========================================="
	@echo "Tests completed! Check logs/ for details"
	@echo "=========================================="
	@echo ""

# Run server only (for manual testing)
test-server: $(TEST_SERVER) $(LOG_DIR)
	@echo "Starting test server..."
	@echo "Press Ctrl+C to stop"
	@echo ""
	./$(TEST_SERVER)

# Run client only (for manual testing)
test-client: $(TEST_CLIENT) $(LOG_DIR)
	@echo "Running test client..."
	@echo ""
	./$(TEST_CLIENT)

# ============================================================================
# View logs
# ============================================================================
logs-test:
	@echo "=========================================="
	@echo "TEST SERVER LOG"
	@echo "=========================================="
	@cat $(LOG_DIR)/test_server.log 2>/dev/null || echo "No test server log found"
	@echo ""
	@echo "=========================================="
	@echo "TEST CLIENT LOG"
	@echo "=========================================="
	@cat $(LOG_DIR)/test_client.log 2>/dev/null || echo "No test client log found"
	@echo ""

logs-main:
	@echo "=========================================="
	@echo "NAME SERVER LOG"
	@echo "=========================================="
	@cat $(LOG_DIR)/nameserver.log 2>/dev/null || echo "No nameserver log found"
	@echo ""
	@echo "=========================================="
	@echo "STORAGE SERVER LOG"
	@echo "=========================================="
	@cat $(LOG_DIR)/storage_server.log 2>/dev/null || echo "No storage server log found"
	@echo ""
	@echo "=========================================="
	@echo "CLIENT LOG"
	@echo "=========================================="
	@cat $(LOG_DIR)/client.log 2>/dev/null || echo "No client log found"
	@echo ""

logs: logs-main

# Clean logs only
clean-logs:
	@echo "Cleaning log files..."
	@rm -f $(LOG_DIR)/*.log
	@echo "✓ Logs cleaned"

# ============================================================================
# Quick start - Run entire system
# ============================================================================
start-system: $(NAMESERVER) $(STORAGESERVER) $(CLIENT) $(LOG_DIR) $(STORAGE_DIR)
	@echo "=========================================="
	@echo "Starting Docs++ System..."
	@echo "=========================================="
	@echo ""
	@echo "Starting Name Server in background..."
	@./$(NAMESERVER) > /dev/null 2>&1 &
	@sleep 1
	@echo "Starting Storage Server in background..."
	@./$(STORAGESERVER) > /dev/null 2>&1 &
	@sleep 1
	@echo ""
	@echo "System started! Components running:"
	@echo "  - Name Server (check logs/nameserver.log)"
	@echo "  - Storage Server (check logs/storage_server.log)"
	@echo ""
	@echo "To connect as client, run:"
	@echo "  make run-client"
	@echo ""
	@echo "To stop system:"
	@echo "  make stop-system"
	@echo ""

stop-system:
	@echo "Stopping Docs++ System..."
	@pkill -f nameserver || true
	@pkill -f storage_server || true
	@echo "✓ System stopped"

# ============================================================================
# Status check
# ============================================================================
status:
	@echo "=========================================="
	@echo "Docs++ System Status"
	@echo "=========================================="
	@echo ""
	@echo "Running processes:"
	@ps aux | grep -E "(nameserver|storage_server|client)" | grep -v grep || echo "  No processes running"
	@echo ""
	@echo "Log files:"
	@ls -lh $(LOG_DIR)/*.log 2>/dev/null || echo "  No log files"
	@echo ""
	@echo "Storage files:"
	@ls -lh $(STORAGE_DIR)/* 2>/dev/null || echo "  No storage files"
	@echo ""

# ============================================================================
# Help target
# ============================================================================
help:
	@echo "=========================================="
	@echo "Docs++ Project Makefile"
	@echo "=========================================="
	@echo ""
	@echo "BUILD TARGETS:"
	@echo "  make all          - Build all components"
	@echo "  make common       - Build common utilities only"
	@echo "  make tests        - Build test programs"
	@echo "  make main         - Build main programs (NS, SS, Client)"
	@echo "  make nameserver   - Build name server only"
	@echo "  make storageserver- Build storage server only"
	@echo "  make client       - Build client only"
	@echo ""
	@echo "RUN TARGETS:"
	@echo "  make run-ns       - Run Name Server"
	@echo "  make run-ss       - Run Storage Server"
	@echo "  make run-client   - Run Client"
	@echo "  make start-system - Start NS and SS in background"
	@echo "  make stop-system  - Stop all running components"
	@echo ""
	@echo "TEST TARGETS:"
	@echo "  make test         - Run automated tests"
	@echo "  make test-server  - Run test server (manual)"
	@echo "  make test-client  - Run test client (manual)"
	@echo ""
	@echo "LOG TARGETS:"
	@echo "  make logs         - Display main program logs"
	@echo "  make logs-main    - Display main program logs"
	@echo "  make logs-test    - Display test program logs"
	@echo ""
	@echo "CLEAN TARGETS:"
	@echo "  make clean        - Remove compiled files"
	@echo "  make clean-logs   - Remove log files only"
	@echo "  make clean-storage- Remove storage files only"
	@echo "  make cleanall     - Remove everything"
	@echo ""
	@echo "OTHER TARGETS:"
	@echo "  make status       - Show system status"
	@echo "  make help         - Show this help message"
	@echo ""

# ============================================================================
# Phony targets (not actual files)
# ============================================================================
.PHONY: all common tests main nameserver storageserver client \
        clean cleanall clean-logs clean-storage \
        test test-server test-client \
        run-ns run-ss run-client start-system stop-system \
        logs logs-main logs-test status help