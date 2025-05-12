#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <netinet/ip.h>
#include <sys/select.h>

int count = 0, max_fd = 0;
int ids[2048];
char msgs[2048][100000];

fd_set rfds, wfds, afds;
char buf_read[100000];
char buf_write[120000];

void err(char  *msg)
{
    write(2, msg, strlen(msg));
    exit(1);
}

void appeal(int author, const char *msg)
{
    for (int fd = 0; fd <= max_fd; fd++)
        if (FD_ISSET(fd, &wfds) && fd != author)
            send(fd, msg, strlen(msg), 0);
}

void remove_client(int fd)
{
    sprintf(buf_write, "server: client %d just left\n", ids[fd]);
    appeal(fd, buf_write);
    memset(msgs[fd], 0, sizeof(msgs[fd]));
    FD_CLR(fd, &afds);
    close(fd);
}

void register_client(int fd)
{
    max_fd = fd > max_fd ? fd : max_fd;
    ids[fd] = count++;
    
    memset(msgs[fd], 0, sizeof(msgs[fd]));

    FD_SET(fd, &afds);
    sprintf(buf_write, "server: client %d just arrived\n", ids[fd]);
    appeal(fd, buf_write);
}

int main(int ac, char **av)
{
    if (ac != 2)
        err("Wrong number of arguments\n");

    FD_ZERO(&afds);

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
        err("Fatal error\n");
    max_fd = sockfd;

    FD_SET(sockfd, &afds);

    struct sockaddr_in servaddr;
    bzero(&servaddr, sizeof(servaddr));

    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    servaddr.sin_port = htons(atoi(av[1]));

    if (bind(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr)))
        err("Fatal error\n");
    if (listen(sockfd, SOMAXCONN))
        err("Fatal error\n");

    while (1)
    {
        rfds = wfds = afds;

        if (select(max_fd + 1, &rfds, &wfds, NULL, NULL) < 0)
            err("Fatal error\n");

        for (int fd = 0; fd <= max_fd; fd++)
        {
            if (!FD_ISSET(fd, &rfds))
                continue;

            if (fd == sockfd)
            {
                socklen_t addr_len = sizeof(servaddr);
                int client_fd = accept(sockfd, (struct sockaddr *)&servaddr, &addr_len);
                if (client_fd >= 0)
                {
                    register_client(client_fd);
                    break;
                }
            }
            else
            {
                int read_bytes = recv(fd, buf_read, sizeof(buf_read), 0);
                if (read_bytes <= 0)
                {
                    remove_client(fd);
                    break;
                }
                for (int i = 0, j = strlen(msgs[fd]); i < read_bytes; i++, j++)
                {
                    msgs[fd][j] = buf_read[i];
                    if (msgs[fd][j] == '\n')
                    {
                        msgs[fd][j] = '\0';
                        sprintf(buf_write, "client %d: %s\n", ids[fd], msgs[fd]);
                        appeal(fd, buf_write);
                        memset(msgs[fd], 0, sizeof(msgs[fd]));
                        j = -1;
                    }
                }
            }
        }
    }
    return 0;
}


/* man 7 ip */
