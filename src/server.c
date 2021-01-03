#include <arpa/inet.h>
#include <fcntl.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define FILE_NAME_BOOKS "build/memoria_compartilhada.txt"
#define SOCKET_SERVER_PROXY_PORT 8888

enum operation {
    INSERT,
    SEARCH
};

sem_t semaphore_server;

void make_message(char *requisition_id, char *socket_fd, char *payload, char *message) {
    strcpy(message, requisition_id);
    strcat(message, "|");
    strcat(message, socket_fd);
    strcat(message, "|");
    strcat(message, payload);
}

void get_requisition_id(char *message, char *requisition_id) {
    char message_aux[2000];
    memset(message_aux, '\0', sizeof(message_aux));
    strcpy(message_aux, message);

    strcpy(requisition_id, strtok(message_aux, "|"));
}

void get_socket_fd(char *message, char *socket_fd) {
    char message_aux[2000];
    memset(message_aux, '\0', sizeof(message_aux));
    strcpy(message_aux, message);

    strtok(message_aux, "|");
    strcpy(socket_fd, strtok(NULL, "|"));
}

enum operation get_operation(char *message) {
    char message_aux[2000];
    memset(message_aux, '\0', sizeof(message_aux));
    strcpy(message_aux, message);

    strtok(message_aux, "|");
    strtok(NULL, "|");
    return (enum operation) atoi(strtok(NULL, "|"));
}

void get_payload(char *message, char *payload) {
    char message_aux[2000];
    memset(message_aux, '\0', sizeof(message_aux));
    strcpy(message_aux, message);

    strtok(message_aux, "|");
    strtok(NULL, "|");
    strtok(NULL, "|");

    strcpy(payload, strtok(NULL, "\n"));
}

int main() {
    int socket_fd;
    struct sockaddr_in server;

    if ((socket_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed\n");
        exit(EXIT_FAILURE);
    }
    puts("socket created\n");

    server.sin_addr.s_addr = inet_addr("127.0.0.1");
    server.sin_family = AF_INET;
    server.sin_port = htons(SOCKET_SERVER_PROXY_PORT);

    if (connect(socket_fd, (struct sockaddr *) &server, sizeof(server)) < 0) {
        perror("connection failed");
        exit(EXIT_FAILURE);
    }
    puts("connection success");

    sem_init(&semaphore_server, 0, 1);

    int read_size;
    int found = 0;
    char *insercao_ok = "Novo livro registrado\n";
    char *requsicao_desconhecida = "Comando desconhecido\n";
    char *nenhum_cadastro = "Nenhum cadastro encontrado\n";
    char message[2000], message_socket_fd[2000], message_requisition_id[2000], save_buffer[2000], new_reg[2000],
            book_name[2000], *final_message;
    char *field;
    char *myfifo = "/tmp/myfifo";
    FILE *fp;
    int fd1, fd2;
    pid_t childpid;

    mkfifo(myfifo, 0666);

    while ((read_size = recv(socket_fd, message, 2000, 0)) > 0) {
        sem_wait(&semaphore_server);

        get_requisition_id(message, message_requisition_id);
        get_socket_fd(message, message_socket_fd);
        enum operation op = get_operation(message);

        if (op == INSERT) {
            if ((childpid = fork()) == -1) {
                perror("fork error\n");
                exit(EXIT_FAILURE);
            }

            if (childpid > 0) {
                printf("in parent\n");

                fd1 = open(myfifo, O_WRONLY);
                write(fd1, message, strlen(message));
                close(fd1);
            }

            if (childpid == 0) {
                memset(message, '\0', sizeof(message));

                printf("in child\n");
                fd2 = open(myfifo, O_RDONLY);
                read(fd2, message, 2000);
                close(fd2);

                memset(new_reg, '\0', sizeof(new_reg));

                get_requisition_id(message, message_requisition_id);
                get_socket_fd(message, message_socket_fd);
                get_payload(message, new_reg);

                size_t size_register = strlen(new_reg);

                fp = fopen(FILE_NAME_BOOKS, "a+");
                fwrite(&size_register, sizeof(int), 1, fp);
                fwrite(new_reg, strlen(new_reg), 1, fp);
                fclose(fp);


                memset(new_reg, '\0', sizeof(new_reg));
                memset(message, '\0', sizeof(message));

                make_message(message_requisition_id, message_socket_fd, insercao_ok, message);
                write(socket_fd, message, strlen(message));

                exit(EXIT_SUCCESS);
            }

            memset(message, '\0', sizeof(message));
        } else if (op == SEARCH) {
            int reg_size;
            char read_buffer[2000];

            fp = fopen(FILE_NAME_BOOKS, "r");
            rewind(fp);
            found = 0;

            get_requisition_id(message, message_requisition_id);
            get_socket_fd(message, message_socket_fd);
            get_payload(message, book_name);

            while (fread(&reg_size, sizeof(int), 1, fp) && !found) {
                fread(read_buffer, reg_size, 1, fp);
                strcpy(save_buffer, read_buffer);

                field = strtok(save_buffer, "|");
                if (strcmp(book_name, field) == 0) {
                    printf("%s\n", read_buffer);
                    found = 1;

                    memset(message, '\0', sizeof(message));

                    make_message(message_requisition_id, message_socket_fd, read_buffer, message);

                    write(socket_fd, message, strlen(message));
                }

                memset(read_buffer, '\0', sizeof(read_buffer));
                memset(save_buffer, '\0', sizeof(save_buffer));
            }

            if (!found) {
                memset(message, '\0', sizeof(message));

                make_message(message_requisition_id, message_socket_fd, nenhum_cadastro, message);

                write(socket_fd, message, strlen(message));
            }

            memset(message, '\0', sizeof(message));

            fclose(fp);
        } else {
            memset(message, '\0', sizeof(message));
            strcat(message, message_socket_fd);
            strcat(message, "|");
            strcat(message, requsicao_desconhecida);
            write(socket_fd, message, strlen(message));
        }
        sem_post(&semaphore_server);
    }

    if (read_size == 0) {
        puts("client disconnected\n");
        //remove from connections list
        fflush(stdout);
    } else if (read_size == -1) {
        perror("recv falhou\n");
    }

    exit(EXIT_SUCCESS);
}
