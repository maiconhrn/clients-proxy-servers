#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>

#define SOCKET_CLIENT_PROXY_PORT 8887

enum operation {
    INSERT,
    SEARCH
};

void make_message(enum operation op, char *payload, char *message) {
    snprintf(message, 10, "%d", op);
    strcat(message, "|");
    strcat(message, payload);
}

int main() {
    int socket_fd;
    struct sockaddr_in server;
    char book[2000], server_reply[2000], message[2000];
    char acao[2000], clean_buffer;
    char field[2000];
    char *campo;

    if ((socket_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed\n");
        exit(EXIT_FAILURE);
    }
    puts("socket created\n");

    server.sin_addr.s_addr = inet_addr("127.0.0.1");
    server.sin_family = AF_INET;
    server.sin_port = htons(SOCKET_CLIENT_PROXY_PORT);

    if (connect(socket_fd, (struct sockaddr *) &server, sizeof(server)) < 0) {
        perror("connection failed");
        exit(EXIT_FAILURE);
    }
    puts("connection success");

    while (1) {
        printf("Entre com a requisicao (cadastrar/buscar): ");
        scanf("%s", acao);
        clean_buffer = getchar();

        if (strcmp(acao, "cadastrar") == 0) {
            memset(acao, '\0', sizeof(acao));
            memset(server_reply, '\0', sizeof(server_reply));
            memset(book, '\0', sizeof(book));

            printf("%s", "Entre com o nome do livro: ");
            fgets(field, 1000, stdin);
            field[strlen(field) - 1] = '\0';
            strcat(book, field);
            memset(field, '\0', sizeof(field));
            printf("Entre com o autor: ");
            fgets(field, 1000, stdin);
            field[strlen(field) - 1] = '\0';
            strcat(book, "|");
            strcat(book, field);
            memset(field, '\0', sizeof(field));
            printf("Entre com o ano de publicacao: ");
            fgets(field, 1000, stdin);
            field[strlen(field) - 1] = '\0';
            strcat(book, "|");
            strcat(book, field);
            memset(field, '\0', sizeof(field));

            memset(message, '\0', sizeof(message));
            make_message(INSERT, book, message);
            if (send(socket_fd, message, strlen(message), 0) < 0) {
                perror("Failed to send request\n");
                exit(EXIT_FAILURE);
            }

            if (recv(socket_fd, server_reply, 2000, 0) < 0) {
                puts("recv falhou\n");
                break;
            }

            memset(book, '\0', sizeof(book));
            puts(server_reply);
            memset(server_reply, '\0', sizeof(server_reply));
        } else if (strcmp(acao, "buscar") == 0) {
            memset(acao, '\0', sizeof(acao));
            memset(server_reply, '\0', sizeof(server_reply));

            printf("%s", "Entre com o nome do livro a ser buscado: ");
            fgets(book, 1000, stdin);
            book[strlen(book) - 1] = '\0';

            memset(message, '\0', sizeof(message));
            make_message(SEARCH, book, message);
            if (send(socket_fd, message, strlen(message), 0) < 0) {
                perror("Failed to send request\n");
                exit(EXIT_FAILURE);
            }

            if (recv(socket_fd, server_reply, 2000, 0) < 0) {
                perror("recv failed\n");
                exit(EXIT_FAILURE);
            }

            memset(book, '\0', sizeof(book));

            if (strcmp(server_reply, "Nenhum cadastro encontrado\n") == 0) {  
                printf("%s", server_reply);
            } else {
                printf("\n");
                campo = strtok(server_reply, "|");
                printf("Titulo: %s\n", campo);
                campo = strtok(NULL, "|");
                printf("Autor: %s\n", campo);
                campo = strtok(NULL, "|");
                printf("Ano de publicacao: %s\n", campo);
            }
            campo = NULL;
            memset(server_reply, '\0', sizeof(server_reply));
        } else {
            memset(server_reply, '\0', sizeof(server_reply));
            printf("Comando nao identificado\n");
        }
    }

    close(socket_fd);

    return 0;
}