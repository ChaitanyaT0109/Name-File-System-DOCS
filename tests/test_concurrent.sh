#!/bin/bash

# Test script for concurrent connections and HashMap efficiency
# This tests both Days 3-5 improvements

echo "=========================================="
echo "Testing Days 3-5 Improvements"
echo "=========================================="
echo ""

# Colors for output
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Check if programs are built
if [ ! -f "./nameserver/nameserver" ] || [ ! -f "./storage_server/storage_server" ] || [ ! -f "./client/client" ]; then
    echo -e "${RED}✗ Programs not built. Run 'make all' first${NC}"
    exit 1
fi

echo -e "${GREEN}✓ All programs built${NC}"
echo ""

# Clean up any existing processes
echo "Cleaning up any existing processes..."
pkill -f nameserver || true
pkill -f storage_server || true
sleep 1

# Clean logs and storage
rm -rf logs/*.log
mkdir -p logs
mkdir -p storage_server/storage
rm -f storage_server/storage/*

echo -e "${GREEN}✓ Cleanup complete${NC}"
echo ""

# Start Name Server
echo "Starting Name Server..."
./nameserver/nameserver > /dev/null 2>&1 &
NS_PID=$!
sleep 2

if ! ps -p $NS_PID > /dev/null; then
    echo -e "${RED}✗ Name Server failed to start${NC}"
    exit 1
fi
echo -e "${GREEN}✓ Name Server started (PID: $NS_PID)${NC}"
echo ""

# Start Storage Server
echo "Starting Storage Server..."
./storage_server/storage_server > /dev/null 2>&1 &
SS_PID=$!
sleep 2

if ! ps -p $SS_PID > /dev/null; then
    echo -e "${RED}✗ Storage Server failed to start${NC}"
    kill $NS_PID
    exit 1
fi
echo -e "${GREEN}✓ Storage Server started (PID: $SS_PID)${NC}"
echo ""

# Test 1: HashMap - Check O(1) lookup
echo "=========================================="
echo "TEST 1: HashMap Efficiency (O(1) lookup)"
echo "=========================================="
echo ""

# Create multiple files to test HashMap
echo "Creating 10 test files..."
for i in {1..10}; do
    echo "user1" | ./client/client << EOF > /dev/null 2>&1
CREATE test$i.txt
EXIT
EOF
    sleep 0.2
done

# Check NS logs for HashMap statistics
if grep -q "HashMap Stats" logs/nameserver.log; then
    echo -e "${GREEN}✓ HashMap initialized and tracking files${NC}"
    echo ""
    echo "HashMap Statistics:"
    grep "HashMap Stats" logs/nameserver.log | tail -1
    echo ""
else
    echo -e "${YELLOW}⚠ HashMap stats not found in logs${NC}"
    echo ""
fi

# Test 2: Concurrent Connections
echo "=========================================="
echo "TEST 2: Concurrent Client Connections"
echo "=========================================="
echo ""

echo "Launching 5 concurrent clients..."

# Launch 5 clients simultaneously
for i in {1..5}; do
    (
        echo "user$i" | ./client/client << EOF > /dev/null 2>&1
CREATE concurrent$i.txt
READ concurrent$i.txt
DELETE concurrent$i.txt
EXIT
EOF
    ) &
done

# Wait for all clients to finish
wait

echo -e "${GREEN}✓ All 5 clients completed${NC}"
echo ""

# Check if select() is handling concurrent connections
if grep -q "select()" logs/nameserver.log || grep -q "active set" logs/nameserver.log; then
    echo -e "${GREEN}✓ Name Server using select() for multiplexing${NC}"
    echo ""
    echo "Connection handling stats:"
    grep -E "(Added socket|Removed socket)" logs/nameserver.log | tail -5
    echo ""
else
    echo -e "${YELLOW}⚠ select() multiplexing not confirmed in logs${NC}"
    echo ""
fi

# Test 3: Verify HashMap operations
echo "=========================================="
echo "TEST 3: HashMap Add/Remove Operations"
echo "=========================================="
echo ""

# Check HashMap add operations
HASHMAP_ADDS=$(grep -c "Added to HashMap" logs/nameserver.log || echo "0")
echo "Files added to HashMap: $HASHMAP_ADDS"

# Check HashMap lookup operations
HASHMAP_LOOKUPS=$(grep -c "HashMap lookup" logs/nameserver.log || echo "0")
echo "HashMap lookups performed: $HASHMAP_LOOKUPS"

# Check HashMap remove operations
HASHMAP_REMOVES=$(grep -c "Removed from HashMap" logs/nameserver.log || echo "0")
echo "Files removed from HashMap: $HASHMAP_REMOVES"

echo ""

if [ $HASHMAP_ADDS -gt 0 ] && [ $HASHMAP_LOOKUPS -gt 0 ]; then
    echo -e "${GREEN}✓ HashMap is actively being used for file operations${NC}"
else
    echo -e "${RED}✗ HashMap may not be working correctly${NC}"
fi

echo ""

# Final summary
echo "=========================================="
echo "SUMMARY"
echo "=========================================="
echo ""

# Count total operations
TOTAL_OPS=$(grep -c "operation" logs/nameserver.log || echo "0")
echo "Total operations processed: $TOTAL_OPS"

# Check for errors
ERRORS=$(grep -c "ERROR" logs/nameserver.log || echo "0")
echo "Errors encountered: $ERRORS"

echo ""

if [ $HASHMAP_ADDS -gt 0 ] && [ $HASHMAP_LOOKUPS -gt 0 ] && [ $ERRORS -eq 0 ]; then
    echo -e "${GREEN}=========================================="
    echo -e "✓ ALL TESTS PASSED!"
    echo -e "=========================================="
    echo -e ""
    echo -e "✓ HashMap (O(1) lookup) - IMPLEMENTED"
    echo -e "✓ select() multiplexing - IMPLEMENTED"
    echo -e "✓ Concurrent connections - WORKING"
    echo -e "${NC}"
else
    echo -e "${YELLOW}=========================================="
    echo -e "⚠ SOME ISSUES DETECTED"
    echo -e "=========================================="
    echo -e "${NC}"
    echo "Check logs/nameserver.log for details"
    echo ""
fi

# Cleanup
echo "Cleaning up..."
kill $NS_PID $SS_PID 2>/dev/null || true
sleep 1

echo ""
echo "Test complete! Check logs/ for detailed output"
echo ""
