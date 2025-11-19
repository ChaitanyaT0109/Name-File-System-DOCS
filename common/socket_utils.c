/*
 * common/socket_utils.c
 * 
 * Implementation of socket utility functions
 * Copy this EXACTLY into common/socket_utils.c
 */

#include "socket_utils.h"
#include <fcntl.h>
#include <sys/time.h>

/* ============================================================================
 * SOCKET CREATION & CONNECTION FUNCTIONS
 * ============================================================================ */

int create_socket() {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("Socket creation failed");
        return -1;
    }
    
    // Enable address reuse (helps with quick restarts)
    if (enable_reuseaddr(sockfd) < 0) {
        close(sockfd);
        return -1;
    }
    
    return sockfd;
}

int bind_socket(int sockfd, int port) {
    struct sockaddr_in server_addr;
    
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;  // Bind to all interfaces
    server_addr.sin_port = htons(port);
    
    if (bind(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        return -1;
    }
    
    return 0;
}

int listen_socket(int sockfd, int backlog) {
    if (listen(sockfd, backlog) < 0) {
        perror("Listen failed");
        return -1;
    }
    return 0;
}

int connect_to_server(int sockfd, const char* ip, int port) {
    struct sockaddr_in server_addr;
    
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    
    // Convert IP address from string to binary
    if (inet_pton(AF_INET, ip, &server_addr.sin_addr) <= 0) {
        perror("Invalid IP address");
        return -1;
    }
    
    if (connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connection failed");
        return -1;
    }
    
    return 0;
}

int accept_connection(int sockfd, char* client_ip, int* client_port) {
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    
    int client_sockfd = accept(sockfd, (struct sockaddr*)&client_addr, &client_len);
    if (client_sockfd < 0) {
        perror("Accept failed");
        return -1;
    }
    
    // Store client IP and port if buffers provided
    if (client_ip != NULL) {
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, MAX_IP_LEN);
    }
    
    if (client_port != NULL) {
        *client_port = ntohs(client_addr.sin_port);
    }
    
    return client_sockfd;
}

/* ============================================================================
 * MESSAGE SENDING & RECEIVING FUNCTIONS
 * ============================================================================ */

int send_message(int sockfd, Message* msg) {
    if (msg == NULL) {
        fprintf(stderr, "Error: NULL message pointer\n");
        return -1;
    }
    
    // Send the entire Message struct in chunks if necessary
    size_t total_sent = 0;
    size_t msg_size = sizeof(Message);
    const char* ptr = (const char*)msg;
    
    while (total_sent < msg_size) {
        ssize_t bytes_sent = send(sockfd, ptr + total_sent, msg_size - total_sent, 0);
        
        if (bytes_sent < 0) {
            perror("Send failed");
            return -1;
        }
        
        if (bytes_sent == 0) {
            fprintf(stderr, "Warning: Connection closed during send\n");
            return total_sent;
        }
        
        total_sent += bytes_sent;
    }
    
    return total_sent;
}

int receive_message(int sockfd, Message* msg) {
    if (msg == NULL) {
        fprintf(stderr, "Error: NULL message pointer\n");
        return -1;
    }
    
    // Clear the message buffer first
    memset(msg, 0, sizeof(Message));
    
    // Receive the entire Message struct in chunks if necessary
    size_t total_received = 0;
    size_t msg_size = sizeof(Message);
    char* ptr = (char*)msg;
    
    while (total_received < msg_size) {
        ssize_t bytes_received = recv(sockfd, ptr + total_received, msg_size - total_received, 0);
        
        if (bytes_received < 0) {
            perror("Receive failed");
            return -1;
        }
        
        if (bytes_received == 0) {
            // Connection closed by peer
            if (total_received == 0) {
                return 0;  // Clean close before any data
            } else {
                fprintf(stderr, "Warning: Partial message received (%zu/%zu bytes)\n", 
                        total_received, msg_size);
                return -1;  // Incomplete message
            }
        }
        
        total_received += bytes_received;
    }
    
    return total_received;
}

int send_data(int sockfd, const void* data, size_t length) {
    if (data == NULL) {
        fprintf(stderr, "Error: NULL data pointer\n");
        return -1;
    }
    
    size_t total_sent = 0;
    const char* ptr = (const char*)data;
    
    // Send data in chunks until everything is sent
    while (total_sent < length) {
        ssize_t bytes_sent = send(sockfd, ptr + total_sent, length - total_sent, 0);
        
        if (bytes_sent < 0) {
            perror("Send data failed");
            return -1;
        }
        
        total_sent += bytes_sent;
    }
    
    return total_sent;
}

int receive_data(int sockfd, void* buffer, size_t length) {
    if (buffer == NULL) {
        fprintf(stderr, "Error: NULL buffer pointer\n");
        return -1;
    }
    
    size_t total_received = 0;
    char* ptr = (char*)buffer;
    
    // Receive data in chunks until everything is received
    while (total_received < length) {
        ssize_t bytes_received = recv(sockfd, ptr + total_received, 
                                     length - total_received, 0);
        
        if (bytes_received < 0) {
            perror("Receive data failed");
            return -1;
        }
        
        if (bytes_received == 0) {
            // Connection closed
            return total_received;
        }
        
        total_received += bytes_received;
    }
    
    return total_received;
}

/* ============================================================================
 * SOCKET OPTION FUNCTIONS
 * ============================================================================ */

int set_nonblocking(int sockfd) {
    int flags = fcntl(sockfd, F_GETFL, 0);
    if (flags < 0) {
        perror("fcntl F_GETFL failed");
        return -1;
    }
    
    if (fcntl(sockfd, F_SETFL, flags | O_NONBLOCK) < 0) {
        perror("fcntl F_SETFL failed");
        return -1;
    }
    
    return 0;
}

int enable_reuseaddr(int sockfd) {
    int optval = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, 
                   &optval, sizeof(optval)) < 0) {
        perror("setsockopt SO_REUSEADDR failed");
        return -1;
    }
    return 0;
}

int set_socket_timeout(int sockfd, int seconds) {
    struct timeval timeout;
    timeout.tv_sec = seconds;
    timeout.tv_usec = 0;
    
    // Set receive timeout
    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, 
                   &timeout, sizeof(timeout)) < 0) {
        perror("setsockopt SO_RCVTIMEO failed");
        return -1;
    }
    
    // Set send timeout
    if (setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, 
                   &timeout, sizeof(timeout)) < 0) {
        perror("setsockopt SO_SNDTIMEO failed");
        return -1;
    }
    
    return 0;
}

/* ============================================================================
 * UTILITY FUNCTIONS
 * ============================================================================ */

int get_local_ip(char* ip_buffer) {
    int sockfd;
    struct sockaddr_in server_addr;
    socklen_t addr_len = sizeof(server_addr);
    
    // Create a socket and connect to a public DNS server
    // This doesn't actually send any packets, just determines routing
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("Socket creation failed");
        return -1;
    }
    
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(53);  // DNS port
    inet_pton(AF_INET, "8.8.8.8", &server_addr.sin_addr);  // Google DNS
    
    if (connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connect failed");
        close(sockfd);
        
        // Fallback to localhost
        strcpy(ip_buffer, "127.0.0.1");
        return 0;
    }
    
    // Get the local address
    struct sockaddr_in local_addr;
    if (getsockname(sockfd, (struct sockaddr*)&local_addr, &addr_len) < 0) {
        perror("getsockname failed");
        close(sockfd);
        
        // Fallback to localhost
        strcpy(ip_buffer, "127.0.0.1");
        return 0;
    }
    
    inet_ntop(AF_INET, &local_addr.sin_addr, ip_buffer, MAX_IP_LEN);
    close(sockfd);
    
    return 0;
}

void close_socket(int sockfd) {
    if (sockfd >= 0) {
        close(sockfd);
    }
}