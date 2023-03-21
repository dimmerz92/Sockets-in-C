#include <netdb.h>
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
    if (argc < 3)
    {
        error("ERROR insufficient arguments");
    }

    // init vars
    struct sockaddr_in serv_addr;
    struct hostent *serv;
    socklen_t serv_len = sizeof(serv_addr);
    int sockfd;
    char buffer[256];

    // make socket
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        error("ERROR creating socket");
    }

    // resolve hostname
    if ((serv = gethostbyname(argv[1])) == NULL)
    {
        error("ERROR no such host");
    }

    // socket details
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(atoi(argv[2]));
    bcopy((char *)serv->h_addr, (char*)&serv_addr.sin_addr.s_addr, serv->h_length);
    
    // connect to server
    if (connect(sockfd, (struct sockaddr *)&serv_addr, serv_len) < 0)
    {
        error("ERROR connecting");
    }

    // send a message
    memset(buffer, 0, 256);
    printf("Enter a message: ");
    fgets(buffer, 255, stdin);
    if (write(sockfd, buffer, strlen(buffer)) < 0)
    {
        error("ERROR writing");
    }

    // receive a message
    memset(buffer, 0, 255);
    if (read(sockfd, buffer, 255) < 0)
    {
        error("ERROR reading");
    }

    printf("%s\n", buffer);

    // close socket
    close(sockfd);

    return 0;
}