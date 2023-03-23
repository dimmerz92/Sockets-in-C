#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

/*-------------------------
| CONSTS
|-------------------------*/
#define MAX_BUFFER 256

/*-------------------------
| PRE-DECLARATIONS
| - main() dependencies
|-------------------------*/
void error(char *msg);

/*-------------------------
| MAIN()
|-------------------------*/
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
    char buffer[MAX_BUFFER];
    int sockfd, action;

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

    printf("Session started\n");

    while (1)
    {
        // send a message
        memset(buffer, 0, MAX_BUFFER);
        fgets(buffer, MAX_BUFFER, stdin);
        if (write(sockfd, buffer, strlen(buffer)) < 0)
        {
            close(sockfd);
            error("ERROR writing");
        }

        // receive a message
        memset(buffer, 0, MAX_BUFFER);
        if ((action = read(sockfd, buffer, MAX_BUFFER)) < 0)
        {
            close(sockfd);
            error("ERROR reading");
        }
        else if (action == 0)
        {
            printf("SERVER DISCONNECTED\n");
            break;
        }

        printf("%s\n", buffer);

        // disconnect gracefully
        if (strstr(buffer, "DISCONNECT: OK") || strstr(buffer, "CONNECT: ERROR"))
        {
            printf("SERVER DISCONNECTED\n");
            break;
        }
    }

    // close socket
    close(sockfd);

    return 0;
}

/*-------------------------
| FUNCTIONS
|-------------------------*/

// quick handling for errors
void error(char *msg)
{
    perror(msg);
    exit(1);
}