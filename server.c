#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

void error(char *msg)
{
    perror(msg);
    exit(1);
}

int main(int argc, char *argv[])
{
    // check arg length
    if (argc < 2)
    {
        error("ERROR insufficient arguments");
    }

    // init vars
    struct sockaddr_in serv_addr, cli_addr;
    socklen_t serv_len = sizeof(serv_addr);
    socklen_t cli_len = sizeof(cli_addr);
    int sockfd, new_sockfd;
    char buffer[256];

    // make socket
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        error("ERROR creating socket");
    }

    // socket details
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(atoi(argv[1]));
    serv_addr.sin_addr.s_addr = INADDR_ANY;

    // bind socket
    if (bind(sockfd, (struct sockaddr *)&serv_addr, serv_len) < 0)
    {
        error("ERROR binding socket");
    }

    // listen to socket
    if (listen(sockfd, 5) < 0)
    {
        error("ERROR listening to socket");
    }

    // accept incoming connections
    if ((new_sockfd = accept(sockfd, (struct sockaddr *)&cli_addr, &cli_len)) < 0)
    {
        error("ERROR accepting socket");
    }

    // read message
    memset(buffer, 0, 256);
    if (read(new_sockfd, buffer, 256) < 0)
    {
        error("ERROR reading message");
    }

    printf("%s", buffer);

    // write message
    if (write(new_sockfd, "I got your message", 18) < 0)
    {
        error("ERROR writing message");
    }

    return 0;
}