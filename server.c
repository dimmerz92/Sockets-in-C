#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

// pre declared functions
void error(char *msg);
void client_handler(int client_socket);

// client data
typedef struct {
    char key[11];
    char value[256];
} client_data;

// current client sessions
typedef struct {
    char client_id[11];
    int allowance;
    client_data data[5];
} client_session;

client_session sessions[5];
int n_sessions = 0;

// multithread handling
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

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
    pthread_t thread;

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

    // keep accepting ad infinitum
    while (1)
    {
        // accept incoming connections
        memset(&cli_addr, 0, sizeof(cli_addr));
        if ((new_sockfd = accept(sockfd, (struct sockaddr *)&cli_addr, &cli_len)) < 0)
        {
            continue;
        }

        // start a new thread
        if(pthread_create(&thread, NULL, client_handler, new_sockfd) != 0)
        {
            close(new_sockfd);
        }
    }
    close(new_sockfd);
    close(sockfd);

    return 0;
}

void error(char *msg)
{
    perror(msg);
    exit(1);
}

int get_n_args(char *args)
{
    // error check
    if (strlen(args) > 255 || args[0] == '\n')
    {
        return -1;
    }

    // init counter
    int word_count = 0;

    // count args
    for (int i = 0; i < 255; i++)
    {
        if (args[i] == ' ')
        {
            word_count++;
        }
        else if (args[i] == '\n')
        {
            word_count++;
            break;
        }
    }

    return word_count;
}

int *get_arg_lengths(char *args)
{
    // error check
    int n_args = get_n_args(args);
    if (n_args < 0 || args == 0)
    {
        return -1;
    }

    // init vars
    int arg_lengths[n_args];
    int count = 0;

    // find arg lengths
    memset(arg_lengths, 0, sizeof(arg_lengths));
    for (int i = 0; i < 255; i++)
    {
        if (args[i] == '\0')
        {
            break;
        }
        if (args[i] == ' ')
        {
            count++;
            continue;
        }
            arg_lengths[count]++;
    }

    return arg_lengths;
}

// adds client data to data structure
int add_data(char *client_id, char *key, char *value)
{
    for (int i = 0; i < n_sessions; i++)
    {
        // if client exists and has allowance
        if (strcmp(client_id, sessions[i].client_id) == 0 && sessions[i].allowance < 5)
        {
            for (int j = 0; j < sessions[i].allowance; j++)
            {
                // if the key is in use, error
                if (strcmp(sessions[i].data[j].key, key) == 0)
                {
                    return -1;
                }
            }
            // add key value pair
            strcpy(sessions[i].data[sessions[i].allowance].key, key);
            strcpy(sessions[i].data[sessions[i].allowance].value, value);
            sessions[i].allowance++;
            return 0;
        }
    }
    return -1;
}

// removes client data from data structure
int remove_data(char *client_id, char *key)
{
    for (int i = 0; i < n_sessions; i++)
    {
        // if client exists and has data stored
        if (strcmp(client_id, sessions[i].client_id) == 0 && sessions[i].allowance > 0)
        {
            int key_exists = 0;
            for (int j = 0; j < sessions[i].allowance; j++)
            {
                // if key exists, shift elements
                if (key_exists)
                {
                    sessions[i].data[j - 1] = sessions[i].data[j];
                    if (j == sessions[i].allowance - 1)
                    {
                        memset(&sessions[i].data[j], 0, sizeof(client_data));
                        sessions[i].allowance--;
                        return 0;
                    }
                }
                // check key exists
                if (strcmp(key, sessions[i].data[j].key) == 0)
                {
                    key_exists = 1;
                }
            }
        }
    }
    return -1;
}