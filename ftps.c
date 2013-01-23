#include <errno.h>
#include <netdb.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "common.h"

#define CONNECTION_QUEUE 5

void sigchld_handler(int s) {
    while(waitpid(-1, NULL, WNOHANG) > 0);
    printf("Child died\n");
}

int main(int argc, char *argv[]) {
    int sockfd, connfd;
    struct addrinfo hints, *servinfo, *p;
    int status, yes=1;
    struct sigaction sa;
    struct sockaddr_storage clientaddr;
    socklen_t ca_size;
    char addrString[INET6_ADDRSTRLEN];

    printf("Starting server...\n");
    if (argc != 2) {
        printf("Usage: %s <port>\n", argv[0]);
        return -1;
    }

    // Setup structures
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    if ((status = getaddrinfo(NULL, argv[1], &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
        return 1;
    }

    //Bind to first address
    for (p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            perror("socket");
            continue;
        }
        // Let other sockets bind port unless active listening socket
        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
            perror("setsockopt");
            exit(1);
        }
        // Bind socket
        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            perror("bind");
            close(sockfd);
            continue;
        }
        // Socket bound
        break;
    }

    // Make sure socket is bound
    if (p == NULL)  {
        fprintf(stderr, "Failed to bind to an address.\n");
        return 1;
    }

    // Clean up
    freeaddrinfo(servinfo);

    // Start listening
    if (listen(sockfd, CONNECTION_QUEUE) == -1) {
        perror("listen");
        exit(1);
    }
    printf("Listening for connections...\n");

    // Zombie hunting
    sa.sa_handler = sigchld_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("sigaction");
        exit(1);
    }

    // Listen forever
    while(1) {
        ca_size = sizeof clientaddr;
        connfd = accept(sockfd, (struct sockaddr *)&clientaddr, &ca_size);
        if (connfd == -1) {
            perror("accept");
            continue;
        }
        getInAddrString(clientaddr.ss_family, (struct sockaddr *)&clientaddr, 
            addrString, sizeof addrString);
        printf("New connection from %s\n", addrString);

        int pid;
        if ((pid = fork()) < 0) {
            perror("fork");
            exit(1);
        } else if (pid == 0) {
            // Child process
            printf("Hello from child\n");
            sleep(5);
            exit(0);
        } else {
            // Parent process
            printf("Started child %d\n", pid);
            // Close connection in parent
            close(connfd);
        }
    }

    return 0;
}
