#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>

typedef struct s_client
{
    int     id;
    char    *msg;
    size_t  msg_size;
}   t_client;

t_client    clients[1024];
fd_set      read_set, write_set, current;
int         maxfd = 0, gid = 0;
char        send_buffer[120000], recv_buffer[120000];

void    err(char  *msg)
{
    if (msg)
        write(2, msg, strlen(msg));
    else
        write(2, "Fatal error", 11);
    write(2, "\n", 1);
    exit(1);
}

void    send_to_all(int except)
{
    for (int fd = 0; fd <= maxfd; fd++)
    {
        if  (FD_ISSET(fd, &write_set) && fd != except)
            if (send(fd, send_buffer, strlen(send_buffer), 0) == -1)
                err(NULL);
    }
}

int     main(int ac, char **av)
{
    if (ac != 2)
        err("Wrong number of arguments");

    struct sockaddr_in  serveraddr;
    socklen_t           len;
    int serverfd = socket(AF_INET, SOCK_STREAM, 0);
    if (serverfd == -1) err(NULL);
    maxfd = serverfd;

    FD_ZERO(&current);
    FD_SET(serverfd, &current);
    bzero(clients, sizeof(clients));
    bzero(&serveraddr, sizeof(serveraddr));

    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK); // Исправлено на 127.0.0.1
    serveraddr.sin_port = htons(atoi(av[1]));

    if (bind(serverfd, (const struct sockaddr *)&serveraddr, sizeof(serveraddr)) == -1 || listen(serverfd, 100) == -1)
        err(NULL);

    while (1)
    {
        read_set = write_set = current;
        if (select(maxfd + 1, &read_set, &write_set, 0, 0) == -1) continue;

        for (int fd = 0; fd <= maxfd; fd++)
        {
            if (FD_ISSET(fd, &read_set))
            {
                if (fd == serverfd)
                {
                    int clientfd = accept(serverfd, (struct sockaddr *)&serveraddr, &len);
                    if (clientfd == -1) continue;
                    if (clientfd > maxfd) maxfd = clientfd;
                    clients[clientfd].id = gid++;
                    clients[clientfd].msg = calloc(1, 1); // Динамический буфер
                    clients[clientfd].msg_size = 0;
                    FD_SET(clientfd, &current);
                    sprintf(send_buffer, "server: client %d just arrived\n", clients[clientfd].id);
                    send_to_all(clientfd);
                    break;
                }
                else
                {
                    int ret = recv(fd, recv_buffer, sizeof(recv_buffer), 0);
                    if (ret <= 0)
                    {
                        sprintf(send_buffer, "server: client %d just left\n", clients[fd].id);
                        send_to_all(fd);
                        FD_CLR(fd, &current);
                        close(fd);
                        free(clients[fd].msg); // Освобождаем память
                        break;
                    }
                    else
                    {
                        clients[fd].msg = realloc(clients[fd].msg, clients[fd].msg_size + ret + 1);
                        memcpy(clients[fd].msg + clients[fd].msg_size, recv_buffer, ret);
                        clients[fd].msg_size += ret;
                        clients[fd].msg[clients[fd].msg_size] = '\0';

                        char *line = strtok(clients[fd].msg, "\n");
                        while (line)
                        {
                            sprintf(send_buffer, "client %d: %s\n", clients[fd].id, line);
                            send_to_all(fd);
                            line = strtok(NULL, "\n");
                        }
                        free(clients[fd].msg);
                        clients[fd].msg = calloc(1, 1);
                        clients[fd].msg_size = 0;
                        break;
                    }
                }
            }
        }
    }
    return (0);
}
