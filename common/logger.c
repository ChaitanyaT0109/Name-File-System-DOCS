/*
 * common/logger.c
 * 
 * Implementation of logging utilities
 * Copy this EXACTLY into common/logger.c
 */

#include "logger.h"
#include <stdlib.h>

/* Global logger state */
static FILE* log_file_ptr = NULL;
static char component_name[32] = "UNKNOWN";
static LogLevel min_log_level = LOG_DEBUG;

/* ============================================================================
 * INITIALIZATION
 * ============================================================================ */

int init_logger(const char* component, const char* log_file) {
    if (component == NULL) {
        fprintf(stderr, "Error: Component name is NULL\n");
        return -1;
    }
    
    // Store component name
    strncpy(component_name, component, sizeof(component_name) - 1);
    component_name[sizeof(component_name) - 1] = '\0';
    
    // Open log file if specified
    if (log_file != NULL) {
        log_file_ptr = fopen(log_file, "a");  // Append mode
        if (log_file_ptr == NULL) {
            perror("Failed to open log file");
            return -1;
        }
    }
    
    log_info("Logger initialized for component: %s", component);
    return 0;
}

void close_logger() {
    if (log_file_ptr != NULL) {
        log_info("Logger closing");
        fclose(log_file_ptr);
        log_file_ptr = NULL;
    }
}

/* ============================================================================
 * TIMESTAMP HELPER
 * ============================================================================ */

void get_timestamp(char* buffer) {
    time_t now = time(NULL);
    struct tm* tm_info = localtime(&now);
    strftime(buffer, 32, "%Y-%m-%d %H:%M:%S", tm_info);
}

/* ============================================================================
 * LOG LEVEL HELPER
 * ============================================================================ */

const char* get_log_level_string(LogLevel level) {
    switch(level) {
        case LOG_DEBUG:    return "DEBUG";
        case LOG_INFO:     return "INFO";
        case LOG_WARNING:  return "WARNING";
        case LOG_ERROR:    return "ERROR";
        case LOG_CRITICAL: return "CRITICAL";
        default:           return "UNKNOWN";
    }
}

/* ============================================================================
 * CORE LOGGING FUNCTION
 * ============================================================================ */

void log_message(LogLevel level, const char* format, ...) {
    if (level < min_log_level) {
        return;  // Skip if below minimum level
    }
    
    char timestamp[32];
    get_timestamp(timestamp);
    
    // Build the log message
    char message[1024];
    va_list args;
    va_start(args, format);
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);
    
    // Format: [TIMESTAMP] [COMPONENT] [LEVEL] MESSAGE
    char log_line[1200];
    snprintf(log_line, sizeof(log_line), "[%s] [%s] [%s] %s\n",
             timestamp, component_name, get_log_level_string(level), message);
    
    // Print to console
    printf("%s", log_line);
    fflush(stdout);
    
    // Write to file if available
    if (log_file_ptr != NULL) {
        fprintf(log_file_ptr, "%s", log_line);
        fflush(log_file_ptr);  // Ensure immediate write
    }
}

/* ============================================================================
 * CONVENIENCE FUNCTIONS
 * ============================================================================ */

void log_debug(const char* format, ...) {
    char message[1024];
    va_list args;
    va_start(args, format);
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);
    log_message(LOG_DEBUG, "%s", message);
}

void log_info(const char* format, ...) {
    char message[1024];
    va_list args;
    va_start(args, format);
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);
    log_message(LOG_INFO, "%s", message);
}

void log_warning(const char* format, ...) {
    char message[1024];
    va_list args;
    va_start(args, format);
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);
    log_message(LOG_WARNING, "%s", message);
}

void log_error(const char* format, ...) {
    char message[1024];
    va_list args;
    va_start(args, format);
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);
    log_message(LOG_ERROR, "%s", message);
}

void log_critical(const char* format, ...) {
    char message[1024];
    va_list args;
    va_start(args, format);
    vsnprintf(message, sizeof(message), format, args);
    va_end(args);
    log_message(LOG_CRITICAL, "%s", message);
}

/* ============================================================================
 * SPECIALIZED LOGGING FOR PROJECT REQUIREMENTS
 * ============================================================================ */

void log_operation(const char* operation, const char* filename, 
                   const char* username, const char* ip, int port, 
                   const char* result) {
    log_info("OPERATION: %s | FILE: %s | USER: %s | FROM: %s:%d | RESULT: %s",
             operation, 
             filename ? filename : "N/A",
             username ? username : "N/A",
             ip ? ip : "N/A",
             port,
             result);
}

void log_request(const char* from, const char* operation, const char* details) {
    log_info("REQUEST from %s: %s | %s", from, operation, details);
}

void log_response(const char* to, const char* operation, const char* result) {
    log_info("RESPONSE to %s: %s | %s", to, operation, result);
}

void log_ack(const char* to, const char* operation, const char* status) {
    log_info("ACK to %s: %s | %s", to, operation, status);
}