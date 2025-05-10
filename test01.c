#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <netinet/in.h>
#include <sys/socket.h>

int main() {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in servaddr;
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(5050);
    servaddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    connect(sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr));

    fcntl(sockfd, F_SETFL, O_NONBLOCK);

    char buf[1024];
    while (1) {
        int n = recv(sockfd, buf, sizeof(buf) - 1, 0);
        if (n > 0) {
            buf[n] = '\0';
            printf("Received: %s", buf);
        }
        usleep(100000);
    }

    close(sockfd);
    return 0;
}
