#include <arpa/inet.h>
#include <fcntl.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/queue.h>

#define MAX_N_CONNECTIONS 10

#define SOCKET_PROXY_SERVERS_PORT 8888
#define SOCKET_PROXY_CLIENTS_PORT 8887

sem_t semaphore_proxy_servers;
sem_t semaphore_proxy_clients;

int connections_servers[MAX_N_CONNECTIONS];
int connections_servers_count = 0;

int connections_clients[MAX_N_CONNECTIONS];
int connections_clients_count = 0;

typedef struct {
    int id;
    int complete;
} requisition;

void init_requisition(requisition *r, int id) {
    r->id = id;
    r->complete = 0;
}

typedef struct {
  requisition **array;
  size_t used;
  size_t size;
} requisition_array;

void init_array(requisition_array *a, size_t initial_size) {
  a->array = malloc(initial_size * sizeof(requisition*));
  a->used = 0;
  a->size = initial_size;
}

void insert_array(requisition_array *a, requisition *element) {
  if (a->used == a->size) {
    a->size *= 2;
    a->array = realloc(a->array, a->size * sizeof(requisition*));
  }
  a->array[a->used++] = element;
}

void free_array(requisition_array *a) {
  free(a->array);
  a->array = NULL;
  a->used = a->size = 0;
}

int get_requisition_id(char *message) {
    char message_aux[2000];
    memset(message_aux, '\0', sizeof(message_aux));
    strcpy(message_aux, message);

    return atoi(strtok(message_aux, "|"));
}

int get_socket_fd(char *message) {
    char message_aux[2000];
    memset(message_aux, '\0', sizeof(message_aux));
    strcpy(message_aux, message);
    strtok(message_aux, "|");

    return atoi(strtok(NULL, "|"));
}

void get_payload(char *message, char *payload) {
    char message_aux[2000];
    memset(message_aux, '\0', sizeof(message_aux));
    strcpy(message_aux, message);

    strtok(message_aux, "|");
    strtok(NULL, "|");

    strcpy(payload, strtok(NULL, "\n"));
}

requisition_array requisitions;
int requisitions_counter;

void *proxy_servers_handler(void *);

void *proxy_clients_handler(void *);

void *exec_bind_clients(void *);

void *exec_bind_servers(void *);

void make_message(int requisition_id, int socket_fd, char *payload, char *message) {
    char str_aux[2000];

    memset(str_aux, '\0', sizeof(str_aux));
    snprintf(str_aux, 10, "%d", requisition_id);
    strcat(message, str_aux);

    strcat(message, "|");

    memset(str_aux, '\0', sizeof(str_aux));
    snprintf(str_aux, 10, "%d", socket_fd);
    strcat(message, str_aux);
    
    strcat(message, "|");
    strcat(message, payload);
}

void clients_connection_handler(void *socket_fd, sem_t *semaphore) {
    int sock = *(int *) socket_fd;
    int read_size;
    char message[2000], final_message[2000];

    while ((read_size = recv(sock, message, 2000, 0)) > 0) {
        sem_wait(semaphore);

        requisitions_counter++;
        requisition req;
        init_requisition(&req, requisitions_counter);
        
        insert_array(&requisitions, &req);

        memset(final_message, '\0', sizeof(final_message));

        make_message(requisitions_counter, sock, message, final_message);

        printf("%s\n", message);
        for (int i = 0; i < connections_servers_count; ++i) {
            int sock_server = connections_servers[i];

            printf("%d\n", sock_server);

            write(sock_server, final_message, strlen(final_message));
        }

        memset(message, '\0', sizeof(message));
        memset(final_message, '\0', sizeof(final_message));

        sem_post(semaphore);
    }

    if (read_size == 0) {
        puts("client disconnected\n");
        fflush(stdout);
    } else if (read_size == -1) {
        perror("recv falhou\n");
    }

    free(socket_fd);
}

void servers_connection_handler(void *socket_fd, sem_t *semaphore) {
    int sock = *(int *) socket_fd;
    int read_size;
    char message[2000], final_msg[2000];

    while ((read_size = recv(sock, message, 2000, 0)) > 0) {
        sem_wait(semaphore);

        requisition *req = requisitions.array[get_requisition_id(message) - 1];

        if (!req->complete) {
            printf("%s\n", "Primeira resposta da requisicao");
            printf("%s\n", message);
            int sock_client = get_socket_fd(message);

            printf("%d\n", sock_client);

            get_payload(message, final_msg);

            write(sock_client, final_msg, strlen(final_msg));

            req->complete = 1;
        } else {
            printf("%s\n", "Requisicao ja respondida");
        }

        memset(message, '\0', sizeof(message));

        sem_post(semaphore);
    }

    if (read_size == 0) {
        puts("client disconnected\n");
        fflush(stdout);
    } else if (read_size == -1) {
        perror("recv falhou\n");
    }

    free(socket_fd);
}

void *new_connection_servers(void *socket_fd) {
    connections_servers[connections_servers_count] = *(int *) socket_fd;

    connections_servers_count++;

    return NULL;
}

void *new_connection_clients(void *socket_fd) {
    connections_clients[connections_clients_count] = *(int *) socket_fd;

    connections_clients_count++;

    return NULL;
}

int bind_socket(sem_t *semaphore, void *connection_handler(void *),
                int socket_port, void *on_new_connection(void *)) {
    int socket_fd, new_sock, *sock;
    struct sockaddr_in server, client;
    int client_add_len = sizeof(server);
    int fd[2];
    sem_init(semaphore, 0, 1);

    if ((socket_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed\n");
        exit(EXIT_FAILURE);
    }
    puts("socket created\n");

    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons(socket_port);

    if (bind(socket_fd, (struct sockaddr *) &server, sizeof(server)) < 0) {
        perror("bind failed\n");
        exit(EXIT_FAILURE);
    }
    puts("bind success\n");

    listen(socket_fd, 3);
    pipe(fd);
    puts("waiting messages . . .\n");
    while ((new_sock = accept(socket_fd, (struct sockaddr *) &client,
                              (socklen_t *) &client_add_len))) {
        if (new_sock == -1) {
            perror("socket connection error\n");
            exit(EXIT_FAILURE);
        }

        pthread_t sniffer_thread;
        sock = malloc(1);
        *sock = new_sock;

        on_new_connection((void *) sock);

        if (pthread_create(&sniffer_thread, NULL, connection_handler,
                           (void *) sock) > 0) {
            perror("not possible to create a thread\n");
            exit(EXIT_FAILURE);
        }

        puts("connection established\n");
    }

    return socket_fd;
}

void *proxy_servers_handler(void *socket_fd) {
    servers_connection_handler(socket_fd, &semaphore_proxy_servers);

    return NULL;
}

void *proxy_clients_handler(void *socket_fd) {
    clients_connection_handler(socket_fd, &semaphore_proxy_clients);

    return NULL;
}

void *exec_bind_clients(void *p) {
    bind_socket(&semaphore_proxy_clients, proxy_clients_handler,
                SOCKET_PROXY_CLIENTS_PORT, new_connection_clients);

    return NULL;
}

void *exec_bind_servers(void *p) {
    bind_socket(&semaphore_proxy_servers, proxy_servers_handler,
                SOCKET_PROXY_SERVERS_PORT, new_connection_servers);

    return NULL;
}

int main() {
    init_array(&requisitions, 2000);

    pthread_t sniffer_thread1;
    if (pthread_create(&sniffer_thread1, NULL, exec_bind_clients, NULL) > 0) {
        perror("not possible to create a thread\n");
        exit(EXIT_FAILURE);
    }

    pthread_t sniffer_thread2;
    if (pthread_create(&sniffer_thread2, NULL, exec_bind_servers, NULL) > 0) {
        perror("not possible to create a thread\n");
        exit(EXIT_FAILURE);
    }

    pthread_join(sniffer_thread1, NULL);
    pthread_join(sniffer_thread2, NULL);

    exit(EXIT_SUCCESS);
}