#include <stdio.h>
#include <netdb.h>
#include <ctype.h>
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
int ascii_buffer(char *buffer);

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

    while (1)
    {
        // send a message
        memset(buffer, 0, MAX_BUFFER);
        fgets(buffer, MAX_BUFFER, stdin);
        if (ascii_buffer(buffer) < 0)
        {
            close(sockfd);
            error("ERROR input contains non-ASCII characters");
        }
        
        // check ascii chars
        if (write(sockfd, buffer, strlen(buffer)) < 0)
        {
            close(sockfd);
            error("ERROR writing");
        }

        // receive a message
        memset(buffer, 0, MAX_BUFFER - 1);
        if ((action = read(sockfd, buffer, MAX_BUFFER)) < 0)
        {
            close(sockfd);
            error("ERROR reading");
        }

        // check ascii chars
        if (ascii_buffer(buffer) < 0)
        {
            close(sockfd);
            error("ERROR input contains non-ASCII characters");
        }

        if (action == 0)
        {
            // server force disconnected
            break;
        }
        else if (strcmp(buffer, "ACK") == 0)
        {
            // send the value
            memset(buffer, 0, MAX_BUFFER);
            fgets(buffer, MAX_BUFFER, stdin);

            // check ascii chars
            if (ascii_buffer(buffer) < 0)
            {
                close(sockfd);
                error("ERROR input contains non-ASCII characters");
            }

            if (write(sockfd, buffer, strlen(buffer)) < 0)
            {
                close(sockfd);
                error("ERROR writing");
            }

            // receive a message
            memset(buffer, 0, MAX_BUFFER - 1);
            if ((action = read(sockfd, buffer, MAX_BUFFER)) < 0)
            {
                close(sockfd);
                error("ERROR reading");
            }

            // check ascii chars
            if (ascii_buffer(buffer) < 0)
            {
                close(sockfd);
                error("ERROR input contains non-ASCII characters");
            }
        }

        printf("%s\n", buffer);

        // disconnect gracefully
        if (strstr(buffer, "DISCONNECT: OK") || strstr(buffer, "CONNECT: ERROR"))
        {
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
    fprintf(stderr, "%s\n", msg);
    exit(1);
}

// checks buffer contains only ASCII characters
int ascii_buffer(char *buffer)
{
    for (int i = 0; i < strlen(buffer); i++)
    {
        if (!isascii(buffer[i]))
        {
            return -1;
        }
    }
    return 0;
}