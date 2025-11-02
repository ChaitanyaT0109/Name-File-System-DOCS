/*
 * common/socket_utils.h
 * 
 * Socket utility functions for all components
 * Used by Client, Name Server, and Storage Server
 */

#ifndef SOCKET_UTILS_H
#define SOCKET_UTILS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include "protocol.h"

/* ============================================================================
 * SOCKET CREATION & CONNECTION FUNCTIONS
 * ============================================================================ */

/**
 * Create a TCP socket
 * Returns: socket file descriptor on success, -1 on failure
 */
int create_socket();

/**
 * Bind socket to a specific port
 * Returns: 0 on success, -1 on failure
 */
int bind_socket(int sockfd, int port);

/**
 * Set socket to listen mode
 * Returns: 0 on success, -1 on failure
 */
int listen_socket(int sockfd, int backlog);

/**
 * Connect to a server
 * Returns: 0 on success, -1 on failure
 */
int connect_to_server(int sockfd, const char* ip, int port);

/**
 * Accept a client connection
 * Returns: new socket fd on success, -1 on failure
 */
int accept_connection(int sockfd, char* client_ip, int* client_port);

/* ============================================================================
 * MESSAGE SENDING & RECEIVING FUNCTIONS
 * ============================================================================ */

/**
 * Send a message through socket
 * Returns: number of bytes sent, -1 on failure
 */
int send_message(int sockfd, Message* msg);

/**
 * Receive a message from socket
 * Returns: number of bytes received, -1 on failure, 0 on connection closed
 */
int receive_message(int sockfd, Message* msg);

/**
 * Send raw data through socket
 * Returns: number of bytes sent, -1 on failure
 */
int send_data(int sockfd, const void* data, size_t length);

/**
 * Receive raw data from socket
 * Returns: number of bytes received, -1 on failure, 0 on connection closed
 */
int receive_data(int sockfd, void* buffer, size_t length);

/* ============================================================================
 * SOCKET OPTION FUNCTIONS
 * ============================================================================ */

/**
 * Set socket to non-blocking mode
 * Returns: 0 on success, -1 on failure
 */
int set_nonblocking(int sockfd);

/**
 * Enable SO_REUSEADDR option (allows quick restart)
 * Returns: 0 on success, -1 on failure
 */
int enable_reuseaddr(int sockfd);

/**
 * Set socket timeout
 * Returns: 0 on success, -1 on failure
 */
int set_socket_timeout(int sockfd, int seconds);

/* ============================================================================
 * UTILITY FUNCTIONS
 * ============================================================================ */

/**
 * Get local IP address of this machine
 * Returns: 0 on success, -1 on failure
 */
int get_local_ip(char* ip_buffer);

/**
 * Close socket safely
 */
void close_socket(int sockfd);

#endif /* SOCKET_UTILS_H */