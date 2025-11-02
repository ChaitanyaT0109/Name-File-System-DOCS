# Makefile for Docs++ Project
# 
# Quick commands:
#   make all      - Build everything
#   make test     - Run test programs
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

# Common object files
COMMON_OBJS = $(COMMON_DIR)/socket_utils.o $(COMMON_DIR)/logger.o

# Test programs
TEST_SERVER = $(TEST_DIR)/test_server
TEST_CLIENT = $(TEST_DIR)/test_client

# Main programs (to be added later)
CLIENT = $(CLIENT_DIR)/client
NAMESERVER = $(NS_DIR)/nameserver
STORAGESERVER = $(SS_DIR)/storage_server

# ============================================================================
# Default target
# ============================================================================
all: $(LOG_DIR) common tests
	@echo ""
	@echo "=========================================="
	@echo "Build completed successfully!"
	@echo "=========================================="
	@echo "Next steps:"
	@echo "  1. Run tests: make test"
	@echo "  2. Or manually:"
	@echo "     Terminal 1: make test-server"
	@echo "     Terminal 2: make test-client"
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
# Create logs directory
# ============================================================================
$(LOG_DIR):
	@echo "Creating logs directory..."
	@mkdir -p $(LOG_DIR)

# ============================================================================
# Clean build files
# ============================================================================
clean:
	@echo "Cleaning build files..."
	@rm -f $(COMMON_DIR)/*.o
	@rm -f $(TEST_SERVER) $(TEST_CLIENT)
	@rm -f $(CLIENT) $(NAMESERVER) $(STORAGESERVER)
	@echo "✓ Clean complete"

# Clean everything including logs
cleanall: clean
	@echo "Cleaning logs..."
	@rm -rf $(LOG_DIR)
	@echo "✓ Clean all complete"

# ============================================================================
# Run tests
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
# Show logs
# ============================================================================
logs:
	@echo "=========================================="
	@echo "SERVER LOG (logs/test_server.log)"
	@echo "=========================================="
	@cat $(LOG_DIR)/test_server.log 2>/dev/null || echo "No server log found"
	@echo ""
	@echo "=========================================="
	@echo "CLIENT LOG (logs/test_client.log)"
	@echo "=========================================="
	@cat $(LOG_DIR)/test_client.log 2>/dev/null || echo "No client log found"
	@echo ""

# Clean logs only
clean-logs:
	@echo "Cleaning log files..."
	@rm -f $(LOG_DIR)/*.log
	@echo "✓ Logs cleaned"

# ============================================================================
# Help target
# ============================================================================
help:
	@echo "=========================================="
	@echo "Docs++ Project Makefile"
	@echo "=========================================="
	@echo ""
	@echo "Available targets:"
	@echo "  make all          - Build all components"
	@echo "  make common       - Build common utilities only"
	@echo "  make tests        - Build test programs"
	@echo "  make test         - Run automated tests"
	@echo "  make test-server  - Run test server (manual)"
	@echo "  make test-client  - Run test client (manual)"
	@echo "  make logs         - Display log files"
	@echo "  make clean-logs   - Remove log files only"
	@echo "  make clean        - Remove compiled files"
	@echo "  make cleanall     - Remove everything including logs"
	@echo "  make help         - Show this help message"
	@echo ""

# ============================================================================
# Phony targets (not actual files)
# ============================================================================
.PHONY: all common tests clean cleanall test test-server test-client logs clean-logs help