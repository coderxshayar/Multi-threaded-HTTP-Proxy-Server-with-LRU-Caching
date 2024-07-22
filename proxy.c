#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <netdb.h>
#include <sys/socket.h>
#include <signal.h>

#define PORT 8080
#define BUFFER_SIZE 20480
#define MAX_CONNECTIONS 10
int server_socket; // Make server_socket a global variable


const char *allowed_domains[] = {
    "httpbin.org",
    "example.com",
    "httpforever.com",
    "httpstatus.io",
    "hookbin.com",
    NULL
};
int is_allowed_domain(const char *hostname) {
    for (int i = 0; allowed_domains[i] != NULL; i++) {
        if (strcmp(hostname, allowed_domains[i]) == 0) {
            return 1;
        }
    }
    return 0;
}

typedef struct CacheEntry {
    char url[256];
    char response[BUFFER_SIZE];
    struct CacheEntry *prev;
    struct CacheEntry *next;
} CacheEntry;

CacheEntry *cache_head = NULL;
CacheEntry *cache_tail = NULL;
pthread_mutex_t cache_mutex = PTHREAD_MUTEX_INITIALIZER;
int cache_size = 0;

const int MAX_CACHE_SIZE = 5; // Adjust this value as needed

void move_to_head(CacheEntry *entry) {
    if (entry == cache_head) {
        return;
    }

    // Unlink the entry from its current position
    if (entry->prev) {
        entry->prev->next = entry->next;
    }
    if (entry->next) {
        entry->next->prev = entry->prev;
    }
    if (entry == cache_tail) {
        cache_tail = entry->prev;
    }

    // Move the entry to the head
    entry->next = cache_head;
    entry->prev = NULL;
    if (cache_head) {
        cache_head->prev = entry;
    }
    cache_head = entry;
    if (cache_tail == NULL) {
        cache_tail = entry;
    }
}

void add_to_cache(const char *url, const char *response) {
    pthread_mutex_lock(&cache_mutex);

    CacheEntry *new_entry = (CacheEntry *)malloc(sizeof(CacheEntry));
    strncpy(new_entry->url, url, sizeof(new_entry->url) - 1);
    new_entry->url[sizeof(new_entry->url) - 1] = '\0';
    strncpy(new_entry->response, response, sizeof(new_entry->response) - 1);
    new_entry->response[sizeof(new_entry->response) - 1] = '\0';
    new_entry->prev = NULL;
    new_entry->next = cache_head;

    if (cache_head) {
        cache_head->prev = new_entry;
    }
    cache_head = new_entry;
    if (cache_tail == NULL) {
        cache_tail = new_entry;
    }

    cache_size++;
    if (cache_size > MAX_CACHE_SIZE) {
        // Evict the least recently used entry
        CacheEntry *lru_entry = cache_tail;
        cache_tail = lru_entry->prev;
        if (cache_tail) {
            cache_tail->next = NULL;
        }
        free(lru_entry);
        cache_size--;
    }

    pthread_mutex_unlock(&cache_mutex);
}

CacheEntry *find_in_cache(const char *url) {
    pthread_mutex_lock(&cache_mutex);

    CacheEntry *current = cache_head;
    while (current != NULL) {
        if (strcmp(current->url, url) == 0) {
            move_to_head(current);
            pthread_mutex_unlock(&cache_mutex);
            return current;
        }
        current = current->next;
    }

    pthread_mutex_unlock(&cache_mutex);
    return NULL;
}

void *handle_client(void *client_socket_ptr) {
    int client_socket = *(int *)client_socket_ptr;
    free(client_socket_ptr);

    char buffer[BUFFER_SIZE];
    memset(buffer, 0, BUFFER_SIZE);

    recv(client_socket, buffer, BUFFER_SIZE - 1, 0);
    // printf("Received request:\n%s\n", buffer);

    char method[16], url[256], protocol[16];
    sscanf(buffer, "%s %s %s", method, url, protocol);

    if (strcmp(method, "GET") != 0) {
        const char *response = "HTTP/1.1 405 Method Not Allowed\r\n\r\n";
        send(client_socket, response, strlen(response), 0);
        close(client_socket);
        return NULL;
    }

    CacheEntry *cached_entry = find_in_cache(url);
    if (cached_entry) {
        printf("Cache hit for URL: %s serving from the cache\n", url);
        send(client_socket, cached_entry->response, strlen(cached_entry->response), 0);
        close(client_socket);
        return NULL;
    }

    printf("Cache miss for URL: %s\n", url);

    char hostname[256];
    int port = 80;
    char *path = strchr(url + 7, '/');  // Skip "http://"
    if (path) {
        size_t hostname_length = path - (url + 7);
        strncpy(hostname, url + 7, hostname_length);
        hostname[hostname_length] = '\0';
    } else {
        strcpy(hostname, url + 7);
        path = "/";
    }

    printf("Hostname: %s\n", hostname);
    printf("Path: %s\n", path);
      // Check if the domain is allowed
    if (!is_allowed_domain(hostname)) {
        const char *response = "HTTP/1.1 403 Forbidden\r\n\r\n";
        send(client_socket, response, strlen(response), 0);
        close(client_socket);
        return NULL;
    }

    struct hostent *remote_host = gethostbyname(hostname);
    if (!remote_host) {
        perror("Remote host lookup failed");
        close(client_socket);
        return NULL;
    }

    int remote_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (remote_socket < 0) {
        perror("Remote socket creation failed");
        close(client_socket);
        return NULL;
    }

    struct sockaddr_in remote_addr;
    remote_addr.sin_family = AF_INET;
    remote_addr.sin_port = htons(port);
    memcpy(&remote_addr.sin_addr.s_addr, remote_host->h_addr_list[0], remote_host->h_length);

    if (connect(remote_socket, (struct sockaddr *)&remote_addr, sizeof(remote_addr)) < 0) {
        perror("Remote connection failed");
        close(remote_socket);
        close(client_socket);
        return NULL;
    }

    char remote_request[BUFFER_SIZE];
    snprintf(remote_request, BUFFER_SIZE, "GET %s HTTP/1.1\r\nHost: %s\r\nConnection: close\r\n\r\n", path, hostname);
    send(remote_socket, remote_request, strlen(remote_request), 0);
    printf("Forwarded request:\n%s\n", remote_request);

    char remote_response[BUFFER_SIZE];
    int bytes_received;
    size_t total_bytes_received = 0; // To track total bytes received
    char full_response[BUFFER_SIZE] = {0}; // To store the full response
    while ((bytes_received = recv(remote_socket, remote_response, BUFFER_SIZE - 1, 0)) > 0) {
        remote_response[bytes_received] = '\0'; // Null-terminate the response
        printf("Received %d bytes from remote server\n", bytes_received);
        send(client_socket, remote_response, bytes_received, 0);
        if (total_bytes_received + bytes_received < BUFFER_SIZE - 1) {
            strncat(full_response, remote_response, bytes_received); // Append to full response
            total_bytes_received += bytes_received;
        } else {
            fprintf(stderr, "Buffer overflow detected, cannot cache full response\n");
            break;
        }
        memset(remote_response, 0, BUFFER_SIZE); // Clear buffer after sending response to client
    }

    if (bytes_received < 0) {
        perror("Error reading response from remote server");
    } else {
        printf("Caching response for URL: %s\n", url);
        add_to_cache(url, full_response); // Cache the full response
    }

    close(remote_socket);
    close(client_socket);

    return NULL;
}

void *print_cache(void *arg)
{
    while (1)
    {
        
        char input[10];
       
        printf("Enter 'cache' to print cache contents:\n");
        fgets(input, sizeof(input), stdin);
        if (strncmp(input, "cache", 5) == 0)
        { 
            pthread_mutex_lock(&cache_mutex);
            CacheEntry *current = cache_head;
            printf("\nCache contents:\n");
            while (current != NULL)
            {
                printf("URL: %s\nResponse: \n\n", current->url); // cache->response
                current = current->next;
            }
            pthread_mutex_unlock(&cache_mutex);
        }

        
    }
    return NULL;
}


void handle_signal(int signal) {
    if (signal == SIGINT) {
        printf("Received SIGINT, shutting down...\n");
        if (server_socket >= 0) {
            close(server_socket);
        }
        exit(EXIT_SUCCESS);
    }
}

int main() {
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    int opt = 1;

    // Set up signal handler
    if (signal(SIGINT, handle_signal) == SIG_ERR) {
        perror("signal");
        exit(EXIT_FAILURE);
    }
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Set the SO_REUSEADDR option
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt(SO_REUSEADDR) failed");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    if (listen(server_socket, MAX_CONNECTIONS) < 0) {
        perror("Listen failed");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    printf("Proxy server listening on port %d\n", PORT);

    pthread_t print_thread;
    if (pthread_create(&print_thread, NULL, print_cache, NULL) != 0) {
        perror("Failed to create print thread");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    while (1) {
        int client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_len);
        if (client_socket < 0) {
            perror("Accept failed");
            continue;
        }

        pthread_t thread;
        int *client_sock_ptr = malloc(sizeof(int));
        *client_sock_ptr = client_socket;

        if (pthread_create(&thread, NULL, handle_client, (void *)client_sock_ptr) < 0) {
            perror("Thread creation failed");
            close(client_socket);
            free(client_sock_ptr);
        }
    }

    close(server_socket);
    return 0;
}



