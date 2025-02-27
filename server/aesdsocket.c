// Socket program for AESD assignment 5
// Author: Eric Percin, 2/11/2025
// References: https://www.gta.ufrj.br/ensino/eel878/sockets

#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <syslog.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <signal.h>
#include <stdbool.h>



#define DATA_FILE_PATH "/var/tmp/aesdsocketdata"
#define INITIAL_BUFFER_SIZE 512

// Global variables to be closed in singal handler
int g_my_socket = -1;
int g_my_file_write = -1;
int g_exit_flag = 0;


// Helper function to close open resources before exiting
void cleanup() {
    if (g_my_file_write != -1) {
        close(g_my_file_write);
    }
    remove(DATA_FILE_PATH);
    closelog();
}


// Signal handlerer for sigint, sigterm
void signal_handler(int signo) {
    if (g_my_socket != -1) {
        shutdown(g_my_socket, SHUT_RDWR); 
        close(g_my_socket);
    }
    g_exit_flag = 1;
}


// Helper function to read data from client, write it to a file, then send entire file contents back to client
void handle_connection(int my_client, int g_my_file_write) {
    char *packet_buffer = NULL, *bigger_packet_buffer = NULL;
    size_t packet_length = 0;
    int my_file_read = -1;

    while (!g_exit_flag) {
        if (packet_buffer == NULL) {
            packet_buffer = malloc(INITIAL_BUFFER_SIZE);
            if (!packet_buffer) {
                perror("Call to malloc() failed");
                break;
            }
        }

        ssize_t bytes_received = recv(my_client, packet_buffer + packet_length, INITIAL_BUFFER_SIZE, 0);
        if (bytes_received <= 0) {
            perror("Call to recv() failed");
            break;
        }
        packet_length += bytes_received;

        char *newline = memchr(packet_buffer, '\n', packet_length);
        if (newline) {
            if (write(g_my_file_write, packet_buffer, packet_length) != (ssize_t)packet_length) {
                perror("Call to write() failed");
                break;
            }

            my_file_read = open(DATA_FILE_PATH, O_RDONLY);
            if (my_file_read < 0) {
                perror("Call to open() failed for reading");
                break;
            }

            char reader_buf[INITIAL_BUFFER_SIZE];
            ssize_t reader_bytes_read;
            while ((reader_bytes_read = read(my_file_read, reader_buf, sizeof(reader_buf))) > 0) {
                if (send(my_client, reader_buf, reader_bytes_read, 0) < 0) {
                    perror("Call to send() failed");
                    close(my_file_read);
                    break;
                }
            }
            close(my_file_read);
            if (reader_bytes_read < 0) {
                perror("Call to read() failed");
                break;
            }

            free(packet_buffer);
            packet_buffer = NULL;
            packet_length = 0;
            break;
        } else {
            bigger_packet_buffer = realloc(packet_buffer, packet_length * 2);
            if (!bigger_packet_buffer) {
                perror("Call to realloc() failed");
                break;
            }
            packet_buffer = bigger_packet_buffer;
        }
    }

    if (packet_buffer) {
        free(packet_buffer);
    }
    if (my_file_read != -1) {
        close(my_file_read);
    }
    close(my_client);
}


int main(int argc, char *argv[]) {

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    // Handle potential -d argument
    bool create_daemon = false; 
    if (argc == 2 && strcmp(argv[1], "-d") == 0) {
        create_daemon = true;
    }
    
    printf("Starting TCP server on port 9000...\n");

    int rc, my_client;
    struct sockaddr_in my_server_addr, my_client_addr;

    g_my_file_write = open(DATA_FILE_PATH, O_WRONLY | O_CREAT | O_APPEND, 0666);
    if (g_my_file_write < 0) {
        perror("Call to open() failed for writing");
        cleanup();
        return -1;
    }

    g_my_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (g_my_socket == -1) {
        perror("Call to socket() failed");
        cleanup();
        return -1;
    }

    rc = setsockopt(g_my_socket, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int));
    if (rc == -1) {
        perror("Call to setsockopt() failed");
        cleanup();
        return -1;
    }

    my_server_addr.sin_family = AF_INET;
    my_server_addr.sin_addr.s_addr = INADDR_ANY;
    my_server_addr.sin_port = htons(9000);

    rc = bind(g_my_socket, (struct sockaddr *)&my_server_addr, sizeof(my_server_addr));
    if (rc == -1) {
        perror("Call to bind() failed");
        cleanup();
        return -1;
    }

    rc = listen(g_my_socket, SOMAXCONN);
    if (rc == -1) {
        perror("Call to listen() failed");
        cleanup();
        return -1;
    }
    
    
    // Fork and exit the parent to create daemon
    if (create_daemon) {
        pid_t pid = fork();
        if (pid < 0) {
            perror("Call to fork() failed");
            cleanup();
            return -1;        
        }
        if (pid > 0) {
            exit(0);
        }
        if (setsid() < 0) {
            perror("Call to setsid() failed");
            cleanup();
            return -1;    
        }
        if (chdir("/") < 0) {
            perror("Call to chdir() failed");
            cleanup();
            return -1;
        }
        // Redirect output to /dev/null
        int devnull = open("/dev/null", O_RDWR);
        if (devnull < 0) {
            perror("Call to open() failed for /dev/null");
            cleanup();
            return -1;
        }
        close(STDIN_FILENO);
        close(STDOUT_FILENO);
        close(STDERR_FILENO);
        dup2(devnull, STDIN_FILENO);
        dup2(devnull, STDOUT_FILENO);
        dup2(devnull, STDERR_FILENO);
        close(devnull);
    }
    

    printf("Listening on port 9000...\n");
    openlog("aesdsocket", LOG_PID, LOG_USER);

    // Infinite loop to repeatedly accept and handle clients
    while (!g_exit_flag) {
        socklen_t client_addr_len = sizeof(my_client_addr);
        my_client = accept(g_my_socket, (struct sockaddr *)&my_client_addr, &client_addr_len);
        if (my_client == -1) {
            if (g_exit_flag) {
                break;
            }
            perror("Call to accept() failed");
            break;
        }

        syslog(LOG_INFO, "Accepted connection from %s", inet_ntoa(my_client_addr.sin_addr));
        printf("Accepted connection from %s\n", inet_ntoa(my_client_addr.sin_addr));

        handle_connection(my_client, g_my_file_write);

        syslog(LOG_INFO, "Closed connection from %s", inet_ntoa(my_client_addr.sin_addr));
        printf("Closed connection from %s\n", inet_ntoa(my_client_addr.sin_addr));
    }

    syslog(LOG_INFO, "Caught signal, exiting");
    printf("Caught signal, exiting\n");
    cleanup();
    
    return 0;
}

