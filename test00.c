#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <errno.h>
#include <sys/select.h>

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        exit(1);
    }

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("Socket creation failed");
        exit(1);
    }

    if (fcntl(sock, F_SETFL, O_NONBLOCK) < 0) {
        perror("fcntl failed");
        close(sock);
        exit(1);
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(atoi(argv[1]));
    inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr);

    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        if (errno != EINPROGRESS) {
            perror("Connect failed");
            close(sock);
            exit(1);
        }
    }

    fd_set fdset;
    struct timeval tv = {5, 0};
    FD_ZERO(&fdset);
    FD_SET(sock, &fdset);
    if (select(sock + 1, NULL, &fdset, NULL, &tv) <= 0) {
        fprintf(stderr, "Connection timeout or error\n");
        close(sock);
        exit(1);
    }

    printf("Connected to server. Type messages (Ctrl+D to quit):\n");

    char buf[1024];
    while (1) {
        FD_ZERO(&fdset);
        FD_SET(STDIN_FILENO, &fdset);
        FD_SET(sock, &fdset);
        if (select(sock + 1, &fdset, NULL, NULL, NULL) < 0) {
            perror("Select failed");
            break;
        }

        if (FD_ISSET(sock, &fdset)) {
            int n = recv(sock, buf, sizeof(buf) - 1, 0);
            if (n < 0 && errno != EWOULDBLOCK) {
                perror("Recv failed");
                break;
            } else if (n == 0) {
                printf("Server disconnected\n");
                break;
            } else if (n > 0) {
                buf[n] = '\0';
                printf("Received: %s", buf);
            }
        }

        if (FD_ISSET(STDIN_FILENO, &fdset)) {
            if (fgets(buf, sizeof(buf), stdin) == NULL) {
                break;
            }
            int len = strlen(buf);
            if (send(sock, buf, len, 0) < 0 && errno != EWOULDBLOCK) {
                perror("Send failed");
                break;
            }
        }
    }

    close(sock);
    return 0;
}