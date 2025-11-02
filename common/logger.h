/*
 * common/logger.h
 * 
 * Logging utilities for all components
 * Logs to both console and file
 */

#ifndef LOGGER_H
#define LOGGER_H

#include <stdio.h>
#include <time.h>
#include <stdarg.h>
#include <string.h>

/* ============================================================================
 * LOG LEVELS
 * ============================================================================ */

typedef enum {
    LOG_DEBUG = 0,
    LOG_INFO = 1,
    LOG_WARNING = 2,
    LOG_ERROR = 3,
    LOG_CRITICAL = 4
} LogLevel;

/* ============================================================================
 * LOGGER INITIALIZATION
 * ============================================================================ */

/**
 * Initialize logger for a component
 * component: "NS", "SS", "CLIENT"
 * log_file: path to log file (e.g., "logs/nameserver.log")
 * Returns: 0 on success, -1 on failure
 */
int init_logger(const char* component, const char* log_file);

/**
 * Close logger and cleanup
 */
void close_logger();

/* ============================================================================
 * LOGGING FUNCTIONS
 * ============================================================================ */

/**
 * Log a message with level
 */
void log_message(LogLevel level, const char* format, ...);

/**
 * Convenience functions for different log levels
 */
void log_debug(const char* format, ...);
void log_info(const char* format, ...);
void log_warning(const char* format, ...);
void log_error(const char* format, ...);
void log_critical(const char* format, ...);

/**
 * Log an operation (special format for project requirements)
 * operation: "CREATE", "READ", "WRITE", etc.
 * filename: file being operated on
 * username: user performing operation
 * ip: client IP
 * port: client port
 * result: "SUCCESS", "FAILED", etc.
 */
void log_operation(const char* operation, const char* filename, 
                   const char* username, const char* ip, int port, 
                   const char* result);

/**
 * Log a request
 */
void log_request(const char* from, const char* operation, const char* details);

/**
 * Log a response
 */
void log_response(const char* to, const char* operation, const char* result);

/**
 * Log an acknowledgment
 */
void log_ack(const char* to, const char* operation, const char* status);

/* ============================================================================
 * HELPER FUNCTIONS
 * ============================================================================ */

/**
 * Get current timestamp as string
 * buffer: output buffer (must be at least 32 bytes)
 */
void get_timestamp(char* buffer);

/**
 * Get log level as string
 */
const char* get_log_level_string(LogLevel level);

#endif /* LOGGER_H */