#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

/*-------------------------
| CONSTS
|-------------------------*/
#define MAX_SESSIONS 5
#define MAX_BUFFER 256

/*-------------------------
| PRE-DECLARATIONS
| - main() dependencies
|-------------------------*/
void *client_handler(void *cli_socket);
void error(char *msg);
void strip_nl(char *buffer);
int ascii_buffer(char *buffer);
int get_session(char *client_id);
void remove_session(char *client_id);
int remove_data(char *client_id, char *key, int shift_items);

/*-------------------------
| STRUCTS
| - hold information about
|   sessions & client data
|-------------------------*/
// client data
typedef struct {
    char *key;
    char *value;
} client_data;

// current client sessions
typedef struct {
    char *client_id;
    int allowance;
    client_data *data;
} client_session;

client_session sessions[MAX_SESSIONS]; // array to hold session structs
int n_sessions = 0; // keeps track of number of sessions

/*-------------------------
| MULTI-THREADING
| - concurrency uses mutex
|   locks
|-------------------------*/
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

/*-------------------------
| MAIN()
|-------------------------*/
int main(int argc, char *argv[])
{
    // check arg length
    if (argc != 2)
    {
        error("ERROR insufficient arguments");
    }

    // initialise socket variables
    struct sockaddr_in serv_addr, cli_addr;
    socklen_t serv_len = sizeof(serv_addr);
    socklen_t cli_len = sizeof(cli_addr);
    int sockfd, new_sockfd, *cli_sock;
    pthread_t thread;

    // create socket
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        error("ERROR creating socket");
    }

    // add socket details
    memset(&serv_addr, 0, serv_len);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(atoi(argv[1]));
    serv_addr.sin_addr.s_addr = INADDR_ANY;

    // bind socket
    if (bind(sockfd, (struct sockaddr *)&serv_addr, serv_len) < 0)
    {
        error("ERROR binding socket");
    }

    // listen to socket
    if (listen(sockfd, MAX_SESSIONS) < 0)
    {
        error("ERROR listening to socket");
    }

    // keep accepting new connections
    while (1)
    {
        // accept incoming connection
        memset(&cli_addr, 0, cli_len);
        if ((new_sockfd = accept(sockfd, (struct sockaddr *)&cli_addr, &cli_len)) < 0)
        {
            continue;
        }

        // prepare client socket to pass in to new thread
        if ((cli_sock = malloc(sizeof(int))) == NULL)
        {
            close(new_sockfd);
            continue;
        }
        *cli_sock = new_sockfd;

        // start a new thread
        if (pthread_create(&thread, NULL, client_handler, cli_sock) != 0)
        {
            close(new_sockfd);
            free(cli_sock);
            continue;
        }
        pthread_detach(thread);
    }
    close(sockfd);
    return 0;
}

/*-------------------------
| FUNCTIONS
|-------------------------*/

// handles client sessions
void *client_handler(void *cli_socket)
{
    // initialise variables
    int client_socket = *(int *)cli_socket;
    //free(cli_socket);
    char buffer[MAX_BUFFER];
    char *command, *argument;
    int session, cmd_len, arg_len, arg, exists;

    // read first message, ensure CONNECT before starting new thread
    memset(buffer, 0, MAX_BUFFER);
    if (recv(client_socket, buffer, MAX_BUFFER - 1, 0) < 0)
    {
        close(client_socket);
        return NULL;
    }

    // replace \n & \r with \0
    strip_nl(buffer);

    // check message is ASCII only
    if (ascii_buffer(buffer) < 0)
    {
        close(client_socket);
        return NULL;
    }

    // check argument is CONNECT with space
    if (n_sessions == 5 || strncmp(buffer, "CONNECT ", 8) != 0)
    {
        close(client_socket);
        return NULL;
    }

    // check if client exists
    pthread_mutex_lock(&mutex);
    if (get_session(&buffer[strlen("CONNECT ")]) >= 0)
    {
        pthread_mutex_unlock(&mutex);
        send(client_socket, "CONNECT: ERROR", strlen("CONNECT: ERROR"), 0);
        close(client_socket);
        return NULL;
    }
    pthread_mutex_unlock(&mutex);

    // add client session
    client_session c_session;
    c_session.client_id = malloc((strlen(&buffer[strlen("CONNECT ")]) + 1) * sizeof(char));
    c_session.data = malloc(1);
    if (c_session.client_id == NULL || c_session.data == NULL)
    {
        send(client_socket, "CONNECT: ERROR", strlen("CONNECT: ERROR"), 0);
        close(client_socket);
        free(c_session.client_id);
        free(c_session.data);
        return NULL;
    }
    strcpy(c_session.client_id, &buffer[strlen("CONNECT ")]);
    c_session.allowance = 0;
    pthread_mutex_lock(&mutex);
    sessions[n_sessions] = c_session;
    n_sessions++;
    pthread_mutex_unlock(&mutex);

    // acknowledge connect
    if(send(client_socket, "CONNECT: OK", strlen("CONNECT: OK"), 0) < 0)
    {
        pthread_mutex_lock(&mutex);
        remove_session(c_session.client_id);
        pthread_mutex_unlock(&mutex);
        close(client_socket);
        return NULL;
    }

    // continue handling messages until DISCONNECT
    while (1)
    {
        // receive messages
        memset(buffer, 0, MAX_BUFFER);
        if (recv(client_socket, buffer, MAX_BUFFER - 1, 0) < 0)
        {
            break;
        }

        // replace \n & \r with \0
        strip_nl(buffer);

        // check message is ASCII only
        if (ascii_buffer(buffer) < 0)
        {
            break;
        }

        // get the command
        cmd_len = strcspn(buffer, " ") + 1;
        if ((command = malloc((cmd_len + 1) * sizeof(char))) == NULL)
        {
            break;
        }
        strncpy(command, buffer, cmd_len);
        command[cmd_len] = '\0';

        // disconnect if commanded, otherwise get argument
        if (strcmp(command, "DISCONNECT") == 0)
        {
            send(client_socket, "DISCONNECT: OK", strlen("DISCONNECT: OK"), 0);
            free(command);
            break;
        }
        else
        {
            arg_len = strlen(&buffer[cmd_len + 1]);
            if((argument = malloc((arg_len + 1) * sizeof(char))) == NULL)
            {
                free(command);
                break;
            }
            strncpy(argument, &buffer[cmd_len + 1], arg_len);
            argument[arg_len] = '\0';
        }

        // handle the command
        strcmp(command, "PUT ") == 0 ? (arg = 1) :
        strcmp(command, "GET ") == 0 ? (arg = 2) :
        strcmp(command, "DELETE ") == 0? (arg = 3) :
        (arg = -1);

        free(command);

        // error handling for switch case
        int switch_err = 0;

        switch(arg)
        {
            case 1:
                // PUT command: add data
                // acknowledge PUT command
                if (send(client_socket, "ACK", 3, 0) < 0)
                {
                    switch_err = 1;
                }

                if (!switch_err)
                {
                    // receive value
                    memset(buffer, 0, MAX_BUFFER);
                    if (recv(client_socket, buffer, MAX_BUFFER - 1, 0) < 0)
                    {
                        switch_err = 1;
                        goto put_error;
                    }
                    strip_nl(buffer);

                    // check message is ASCII only
                    if (ascii_buffer(buffer) < 0)
                    {
                        break;
                    }

                    // check if key exists
                    pthread_mutex_lock(&mutex);
                    session = get_session(c_session.client_id);
                    exists = 0;
                    for (int i = 0; i < sessions[session].allowance; i++)
                    {
                        // if key exists
                        if (strcmp(sessions[session].data[i].key, argument) == 0)
                        {
                            // change data
                            exists = 1;
                            sessions[session].data[i].key = realloc(sessions[session].data[i].key, (arg_len + 1) * sizeof(char));
                            sessions[session].data[i].value = realloc(sessions[session].data[i].value, (strlen(buffer) + 1) * sizeof(char));
                            if (sessions[session].data[i].key == NULL || sessions[session].data[i].value == NULL)
                            {
                                switch_err = 1;
                                goto put_error;
                            }
                            strcpy(sessions[session].data[i].key, argument);
                            strcpy(sessions[session].data[i].value, buffer);
                            break;
                        }
                    }
                }

                // if key doesn't exist
                if (!exists)
                {
                    // add data
                    client_data data;
                    data.key = malloc((arg_len + 1) * sizeof(char));
                    data.value = malloc((strlen(buffer) + 1) * sizeof(char));
                    sessions[session].data = realloc(sessions[session].data, (sessions[session].allowance + 1) * sizeof(client_data));
                    if (data.key == NULL || data.value == NULL || sessions[session].data == NULL)
                    {
                        switch_err = 1;
                        goto put_error;
                    }
                    strcpy(data.key, argument);
                    strcpy(data.value, buffer);
                    sessions[session].data[sessions[session].allowance] = data;
                    sessions[session].allowance++;
                }

                if (!switch_err)
                {
                    if (send(client_socket, "PUT: OK", strlen("PUT: OK"), 0) < 0)
                    {
                        switch_err = 1;
                    }
                }

                put_error:
                if (switch_err)
                {
                    switch_err = 0;
                    if (send(client_socket, "PUT: ERROR", strlen("PUT: ERROR"), 0) < 0)
                    {
                        switch_err = 1;
                    }
                }
                
                pthread_mutex_unlock(&mutex);
                break;

            case 2:
                // GET command
                pthread_mutex_lock(&mutex);
                session = get_session(c_session.client_id);
                exists = 0;

                // for all data items
                for (int i = 0; i < sessions[session].allowance; i++)
                {
                    // if key in data array
                    if (strcmp(sessions[session].data[i].key, argument) == 0)
                    {
                        exists = 1;
                        if (send(client_socket, sessions[session].data[i].value, strlen(sessions[session].data[i].value), 0) < 0)
                        {
                            switch_err = 1;
                        }
                        break;
                    }
                }

                if (switch_err || !exists)
                {
                    switch_err = 0;
                    if (send(client_socket, "GET: ERROR", strlen("GET: ERROR"), 0) < 0)
                    {
                        switch_err = 1;
                    }
                }

                pthread_mutex_unlock(&mutex);
                break;

            case 3:
                // DELETE command
                pthread_mutex_lock(&mutex);
                session = get_session(c_session.client_id);
                if (remove_data(c_session.client_id, argument, 1) < 0)
                {
                    switch_err = 1;
                }

                if (switch_err)
                {
                    switch_err = 0;
                    if (send(client_socket, "DELETE: ERROR", strlen("DELETE: ERROR"), 0) < 0)
                    {
                        switch_err = 1;
                    }
                }
                else
                {
                    if (send(client_socket, "DELETE: OK", strlen("DELETE: OK"), 0) < 0)
                    {
                        switch_err = 1;
                    }
                }

                pthread_mutex_unlock(&mutex);
                break;

            default:
                // ERROR
                switch_err = 1;
                break;
        }

        free(argument);

        // if error occured in switch
        if (switch_err)
        {
            break;
        }
    }

    pthread_mutex_lock(&mutex);
    remove_session(c_session.client_id);
    pthread_mutex_unlock(&mutex);
    close(client_socket);
    return NULL;
}

// quick handling for errors
void error(char *msg)
{
    fprintf(stderr, "%s\n", msg);
    exit(1);
}

// replaces newlines and carriage returns with null terminator
void strip_nl(char *buffer)
{
    buffer[strcspn(buffer, "\n")] = '\0';
    buffer[strcspn(buffer, "\r")] = '\0';
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

// gets the index of a client session in the sessions array
int get_session(char *client_id)
{
    for (int i = 0; i < n_sessions; i++)
    {
        if (strcmp(sessions[i].client_id, client_id) == 0)
        {
            return i;
        }
    }
    return -1;
}

// removes a client session from the sessions array
void remove_session(char *client_id)
{
    // get the session index
    int session = get_session(client_id);

    // for each data item in client data
    for (int i = 0; i < sessions[session].allowance; i++)
    {
        //remove data
        remove_data(client_id, sessions[session].data[i].key, 0);
    }
    // free memory
    free(sessions[session].client_id);
    free(sessions[session].data);
    sessions[session].allowance = 0;

    // if not the last session in array
    if (session < n_sessions - 1)
    {
        // for the remaining sessions
        for (int i = 0; i < n_sessions; i++)
        {
            // if last session in array
            if (i == n_sessions - 1)
            {
                // free duplicate
                for (int j = 0; j < sessions[i].allowance; j++)
                {
                    remove_data(sessions[i].client_id, sessions[i].data[j].key, 0);
                }
                sessions[i].client_id = NULL;
                sessions[i].data = NULL;
                sessions[i].allowance = 0;
                break;
            }
            // shift sessions to left
            sessions[i] = sessions[i + 1];
        }
    }
    // decrement n_sessions
    n_sessions--;
    return;
}

// removes client data based on a given key
int remove_data(char *client_id, char *key, int shift_items)
{
    // get the session index
    int session = get_session(client_id);

    // for each data item in stored data
    for (int i = 0; i < sessions[session].allowance; i++)
    {
        // if the key matches the argument
        if (strcmp(sessions[session].data[i].key, key) == 0)
        {
            // free the memory
            free(sessions[session].data[i].key);
            free(sessions[session].data[i].value);

            // if shifting items and not last item
            if (shift_items && i < sessions[session].allowance)
            {
                // for remaining items
                for (int j = i; j < sessions[session].allowance; j++)
                {
                    // if last item
                    if (j == sessions[session].allowance - 1)
                    {
                        // free duplicate
                        sessions[session].data[j].key = NULL;
                        sessions[session].data[j].value = NULL;
                        break;
                    }
                    // shift items to left
                    sessions[session].data[j] = sessions[session].data[j + 1];
                }
            }
            // decrement allowance
            sessions[session].allowance--;
            return 0;
        }
    }
    return -1;
}