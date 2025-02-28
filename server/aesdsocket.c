// Socket program for AESD assignment 5
// Author: Eric Percin, 2/11/2025
// References: https://www.gta.ufrj.br/ensino/eel878/sockets
//             https://blog.taborkelly.net/programming/c/2016/01/09/sys-queue-example.html
//             https://man7.org/linux/man-pages/man3/list.3.html

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
#include <pthread.h>
#include <sys/queue.h>
#include <stdlib.h>


#define DATA_FILE_PATH "/var/tmp/aesdsocketdata"
#define INITIAL_BUFFER_SIZE 512

#define DEBUG
#ifdef DEBUG
    #define DEBUG_PRINT(...) printf(__VA_ARGS__)
#else
    #define DEBUG_PRINT(...) 
#endif


// Global variables to be closed in singal handler
int g_my_socket = -1;
int g_my_file_write = -1;
volatile int g_exit_flag = 0;
pthread_mutex_t g_write_mutex = PTHREAD_MUTEX_INITIALIZER;

// Structure for an entry within the singly linked thread list
struct thread_entry {
    pthread_t thread_id;
    int my_client;
    SLIST_ENTRY(thread_entry) next_slist_entry; 
};

SLIST_HEAD(thread_head, thread_entry);


// Helper function to close open resources before exiting
void cleanup() {
    if (g_my_file_write != -1) {
        close(g_my_file_write);
    }
    remove(DATA_FILE_PATH);
    pthread_mutex_destroy(&g_write_mutex);
    closelog();
}


// Signal handlerer for sigint, sigterm
void signal_handler(int signo) {
    if (g_my_socket != -1) {
        shutdown(g_my_socket, SHUT_RDWR); 
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

        DEBUG_PRINT("my client: %d\n",my_client);

        ssize_t bytes_received = recv(my_client, packet_buffer + packet_length, INITIAL_BUFFER_SIZE, 0);
        if (bytes_received <= 0) {
            perror("Call to recv() failed");
            break;
        }
        packet_length += bytes_received;

        char *newline = memchr(packet_buffer, '\n', packet_length);
        if (newline) {
        
            pthread_mutex_lock(&g_write_mutex); // Lock before the write
        
            if (write(g_my_file_write, packet_buffer, packet_length) != (ssize_t)packet_length) {
                perror("Call to write() failed");
                pthread_mutex_unlock(&g_write_mutex);
                break;
            }
            
            pthread_mutex_unlock(&g_write_mutex); // Unlock after the write

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
}


// Wrapper function for each thread to run to handle a connection
void *thread_connection_wrapper(void *arg) {
    struct thread_entry *my_entry = (struct thread_entry *)arg;
    handle_connection(my_entry->my_client, g_my_file_write);
    close(my_entry->my_client);
    free(my_entry);
    pthread_exit(NULL);
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


    struct thread_head head;
    SLIST_INIT(&head);


    // Infinite loop to repeatedly accept and handle clients
    while (!g_exit_flag) {
        socklen_t client_addr_len = sizeof(my_client_addr);
        
        struct thread_entry *current_entry = malloc(sizeof(struct thread_entry));
        if (!current_entry) {
            perror("Call to malloc() failed for linked list entry creation");
            break;
        }
        
        my_client = accept(g_my_socket, (struct sockaddr *)&my_client_addr, &client_addr_len);
        if (my_client == -1) {
            if (g_exit_flag) {
                break;
            }
            perror("Call to accept() failed");
            break;
        }
        current_entry->my_client = my_client;
        DEBUG_PRINT("Current entry client: %d\n",my_client);
        my_client = -1;

        syslog(LOG_INFO, "Accepted connection from %s", inet_ntoa(my_client_addr.sin_addr));
        printf("Accepted connection from %s\n", inet_ntoa(my_client_addr.sin_addr));

        SLIST_INSERT_HEAD(&head, current_entry, next_slist_entry);
        
        // Create a new thread in the linked list
        if (pthread_create(&current_entry->thread_id, NULL, thread_connection_wrapper, current_entry) != 0){
            perror("Call to pthread_create() failed");
            SLIST_REMOVE(&head, current_entry, thread_entry, next_slist_entry);
            free(current_entry);
            continue;
        }

        syslog(LOG_INFO, "Closed connection from %s", inet_ntoa(my_client_addr.sin_addr));
        printf("Closed connection from %s\n", inet_ntoa(my_client_addr.sin_addr));
        
        // TODO: figure out how to properly join any completed threads by traversing the list
        // Rewatch videos?
        
        // Join any completed threads by traversing the list
        /*
        struct thread_entry *current, *next;
        current = SLIST_FIRST(&head);
        while (current != NULL) {
            next = SLIST_NEXT(current, next_slist_entry);  
            pthread_join(current->thread_id, NULL);  
            SLIST_REMOVE(&head, current, thread_entry, next_slist_entry); 
            free(current);  
            current = next;  
        }
        */
        
    }

    syslog(LOG_INFO, "Caught signal, exiting");
    printf("Caught signal, exiting\n");
    
    // TODO: figure out how to properly join and free remaining threads
    /*
    struct thread_entry *current, *next;
    current = SLIST_FIRST(&head);
    while (current != NULL) {
        next = SLIST_NEXT(current, next_slist_entry);  
        pthread_join(current->thread_id, NULL);  
        SLIST_REMOVE(&head, current, thread_entry, next_slist_entry); 
        free(current);  
        current = next;  
    }
    */
    
    cleanup();
    
    return 0;
}

