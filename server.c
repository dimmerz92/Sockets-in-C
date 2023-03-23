#include <stdio.h>
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
const int MAX_SESSIONS = 5;
const int MAX_DATA_STORAGE = 5;
const int MAX_BUFFER = 256;
const int MAX_ARG_ONE = 12;
const int MAX_ARG_TWO = 11;
const int MAX_ARG_THREE = 233;

/*-------------------------
| PRE-DECLARATIONS
| - main() dependencies
|-------------------------*/
void error(char *msg);
void *client_handler(void *cli_socket);

/*-------------------------
| STRUCTS
| - hold information about
|   sessions & client data
|-------------------------*/
// client data
typedef struct {
    char key[MAX_ARG_TWO];
    char value[MAX_ARG_THREE];
} client_data;

// current client sessions
typedef struct {
    char client_id[MAX_ARG_TWO];
    int allowance;
    client_data data[MAX_DATA_STORAGE];
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
    if (argc < 2)
    {
        error("ERROR insufficient arguments");
    }

    // init vars
    struct sockaddr_in serv_addr, cli_addr;
    socklen_t serv_len = sizeof(serv_addr);
    socklen_t cli_len = sizeof(cli_addr);
    int sockfd, new_sockfd;
    char buffer[MAX_BUFFER];
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
    if (listen(sockfd, MAX_SESSIONS) < 0)
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

        int *cli_sock = malloc(sizeof(int));
        *cli_sock = new_sockfd;

        // start a new thread
        if (pthread_create(&thread, NULL, client_handler, cli_sock) != 0)
        {
            close(new_sockfd);
        }
        pthread_detach(thread);
    }
    close(new_sockfd);
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

// gets the number of arguments supplied by client
int get_n_args(char *args)
{
    // error check
    if (strlen(args) > MAX_BUFFER - 1 || args == NULL)
    {
        return -1;
    }

    // init counter
    int word_count = 0;

    // count args
    for (int i = 0; i < MAX_BUFFER - 1; i++)
    {
        if (args[i] == ' ')
        {
            word_count++;
        }
        else if (args[i] == '\0')
        {
            word_count++;
            break;
        }
    }

    return word_count;
}

// gets the length of each argument supplied by client
int *get_arg_lengths(char *args)
{
    // error check
    int n_args = get_n_args(args);
    if (n_args < 0 || args == NULL)
    {
        return NULL;
    }

    // init vars
    int *arg_lengths = malloc(n_args * sizeof(int));
    int count = 0;

    // find arg lengths
    memset(arg_lengths, 0, n_args * sizeof(int));
    for (int i = 0; i < MAX_BUFFER; i++)
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
        if (strcmp(client_id, sessions[i].client_id) == 0 && sessions[i].allowance < MAX_DATA_STORAGE)
        {
            for (int j = 0; j < sessions[i].allowance; j++)
            {
                // if the key is in use, overwrite
                if (strcmp(sessions[i].data[j].key, key) == 0)
                {
                    strcpy(sessions[i].data[j].value, value);
                    return 0;
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

// retrieves client data
int get_data(char *client_id, char *key, char *dest)
{
    for (int i = 0; i < n_sessions; i++)
    {
        // if client exists and has data stored
        if (strcmp(client_id, sessions[i].client_id) == 0)
        {
            for (int j = 0; j < sessions[i].allowance; j++)
            {
                if (strcmp(key, sessions[i].data[j].key) == 0)
                {
                    strcpy(dest, sessions[i].data[j].value);
                    return 0;
                }
            }
        }
    }
    return -1;
}

// removes client data from data structure
int remove_data(char *client_id, char *key)
{
    for (int i = 0; i < n_sessions; i++)
    {
        // if client exists
        if (strcmp(client_id, sessions[i].client_id) == 0)
        {
            for (int j = 0; j < sessions[i].allowance; j++)
            {
                // if key exists, shift elements
                if (strcmp(key, sessions[i].data[j].key) == 0)
                {
                    for (int k = j; k < sessions[i].allowance; k++)
                    {
                        if (sessions[i].allowance - k == 1 || sessions[i].allowance == 1)
                        {
                            memset(&sessions[i].data[k], 0, sizeof(client_data));
                            sessions[i].allowance--;
                            return 0;
                        }
                        sessions[i].data[k] = sessions[i].data[k + 1];
                    }
                }
            }
        }
    }
    return -1;
}

// removes client session
void remove_session(char *client_id)
{
    for (int i = 0; i < n_sessions; i++)
    {
        if (strcmp(client_id, sessions[i].client_id) == 0)
        {
            for (int j = i; j < n_sessions - 1; j++)
            {
                sessions[j] = sessions[j + 1];
            }
            memset(&sessions[n_sessions - 1], 0, sizeof(client_session));
            n_sessions--;
            return;
        }
    }
}

// terminates session on errors
void terminate_session(char *client_id, int client_socket, int locked, int *mem)
{
    // free memory if not NULL
    if (mem != NULL)
    {
        free(mem);
    }

    // remove client session if id provided
    if (client_id != NULL && locked)
    {
        remove_session(client_id);
    }
    else if (client_id != NULL)
    {
        pthread_mutex_lock(&mutex);
        remove_session(client_id);
        pthread_mutex_unlock(&mutex);
    }

    // close the socket
    close(client_socket);
    pthread_mutex_unlock(&mutex);
}

// assigns client supplied arguments to a variable
int assign_arg(char *dest, char *src, int index, int size, int limit)
{
    if (size >= limit || src[0] == '\0')
    {
        return -1;
    }

    if (index == 0)
    {
        //memset(dest, 0, limit);
        strncpy(dest, src, size + 1);
    }
    else
    {
        //memset(dest, 0, limit);
        strncpy(dest, src + index + 1, size + 1);
    }
    dest[size] = '\0';

    return 0;
}

// handles client sessions
void *client_handler(void *cli_socket)
{
    // cast client socket back to int
    int client_socket = *(int *)cli_socket;
    free(cli_socket);

    // init vars
    char buffer[MAX_BUFFER];
    char arg_one[MAX_ARG_ONE];
    char arg_two[MAX_ARG_TWO];
    char arg_three[MAX_ARG_THREE];
    int n_args;
    int *l_args;
    int n, m, o;

    // initiate a connection
    memset(buffer, 0, MAX_BUFFER);
    if (read(client_socket, buffer, MAX_BUFFER) < 0)
    {
        close(client_socket);
        return NULL;
    }

    // handle newlines and carriage returns
    buffer[strcspn(buffer, "\n")] = '\0';
    buffer[strcspn(buffer, "\r")] = '\0';

    // get the number of arguments
    n_args = get_n_args(buffer);

    // if insufficient args, close connection
    if (n_args != 2)
    {
        close(client_socket);
        return NULL;
    }

    // allocate memory for argument lengths array
    l_args = malloc(n_args * sizeof(int));
    if (l_args == NULL)
    {
        close(client_socket);
        return NULL;
    }

    // parse buffer for arguments
    memcpy(l_args, get_arg_lengths(buffer), n_args * sizeof(int));
    if (l_args == NULL)
    {
        close(client_socket);
        return NULL;
    }

    n = assign_arg(arg_one, buffer, 0, l_args[0], MAX_ARG_ONE);
    m = assign_arg(arg_two, buffer, l_args[0], l_args[1], MAX_ARG_TWO);

    // if the first command is not CONNECT, close connection
    if (n < 0 || m < 0 || strcmp(arg_one, "CONNECT") != 0)
    {
        terminate_session(NULL, client_socket, 0, l_args);
        return NULL;
    }

    // lock thread to check for sessions
    pthread_mutex_lock(&mutex);

    // if client already has a session, close connection
    for (int i = 0; i < n_sessions; i++)
    {
        if (strcmp(sessions[i].client_id, arg_two) == 0)
        {
            send(client_socket, "CONNECT: ERROR - client_id in use", 33, 0);
            terminate_session(NULL, client_socket, 1, l_args);
            return NULL;
        }
    }

    // rejects CONNECT if too many sessions
    if (n_sessions == MAX_SESSIONS)
    {
        send(client_socket, "CONNECT: ERROR - too many sessions", 34, 0);
        terminate_session(NULL, client_socket, 1, l_args);
        return NULL;
    }

    // add client session
    client_session c_session;
    strcpy(c_session.client_id, arg_two);
    c_session.allowance = 0;
    sessions[n_sessions] = c_session;
    n_sessions++;

    // acknowledge successful connect
    if (send(client_socket, "CONNECT: OK", 11, 0) < 0)
    {
        terminate_session(c_session.client_id, client_socket, 1, l_args);
        return NULL;
    }

    // unlock the thread
    pthread_mutex_unlock(&mutex);

    // continue handling messages until client disconnects
    while (1)
    {
        // receive messages
        memset(buffer, 0, MAX_BUFFER);
        if (recv(client_socket, buffer, MAX_BUFFER, 0) < 0)
        {
            terminate_session(c_session.client_id, client_socket, 0, NULL);
            break;
        }

        // handle newlines and carriage returns
        buffer[strcspn(buffer, "\n")] = '\0';
        buffer[strcspn(buffer, "\r")] = '\0';

        // get the number of arguments
        n_args = get_n_args(buffer);

        // if insufficient args, close connection
        if (n_args < 2 && strcmp(buffer, "DISCONNECT") != 0)
        {
            terminate_session(c_session.client_id, client_socket, 0, NULL);
            break;
        }

        // reallocate memory for argument lengths array
        l_args = realloc(l_args, n_args * sizeof(int));
        if (l_args == NULL)
        {
            terminate_session(c_session.client_id, client_socket, 0, NULL);
            break;
        }

        // parse buffer for arguments
        memcpy(l_args, get_arg_lengths(buffer), n_args * sizeof(int));
        if (l_args == NULL)
        {
            terminate_session(c_session.client_id, client_socket, 0, NULL);
            break;
        }

        n = assign_arg(arg_one, buffer, 0, l_args[0], MAX_ARG_ONE);
        if (n_args >= 2)
        {
            m = assign_arg(arg_two, buffer, l_args[0], l_args[1], MAX_ARG_TWO);
            o = assign_arg(arg_three, buffer, l_args[0] + l_args[1] + 1, l_args[2], MAX_ARG_THREE);
        }

        // handle client commands
        pthread_mutex_lock(&mutex);
        if (strcmp(arg_one, "PUT") == 0)
        {
            // add the data
            if (n_args != 3 || add_data(c_session.client_id, arg_two, arg_three) < 0)
            {
                if(send(client_socket, "PUT: ERROR", 10, 0) < 0)
                {
                    terminate_session(c_session.client_id, client_socket, 1, NULL);
                    break;
                }
            }
            else if (send(client_socket, "PUT: OK", 7, 0) < 0)
            {
                terminate_session(c_session.client_id, client_socket, 1, NULL);
                break;
            }
        }
        else if (strcmp(arg_one, "GET") == 0)
        {
            // get the data
            char value[MAX_ARG_THREE];
            memset(value, 0, MAX_ARG_THREE);
            if (get_data(c_session.client_id, arg_two, value) < 0)
            {
                if(send(client_socket, "GET: ERROR", 10, 0) < 0)
                {
                    terminate_session(c_session.client_id, client_socket, 1, NULL);
                    break;
                }
            }
            
            else if (send(client_socket, value, sizeof(value), 0) < 0)
            {
                terminate_session(c_session.client_id, client_socket, 1, NULL);
                break;
            }
        }
        else if (strcmp(arg_one, "DELETE") == 0)
        {
            // delete data
            if (remove_data(c_session.client_id, arg_two) == 0)
            {
                if (send(client_socket, "DELETE: OK", 10, 0) < 0)
                {
                    terminate_session(c_session.client_id, client_socket, 1, NULL);
                    break;
                }
            }
            else if (send(client_socket, "DELETE: ERROR", 13, 0) < 0)
            {
                terminate_session(c_session.client_id, client_socket, 1, NULL);
                break;
            }
        }
        else if (strcmp(arg_one, "DISCONNECT") == 0)
        {
            // disconnect session
            remove_session(c_session.client_id);
            send(client_socket, "DISCONNECT: OK", 14, 0);
            close(client_socket);
            break;
        }
        else
        {
            // terminate session if unrecognised argument
            terminate_session(c_session.client_id, client_socket, 1, NULL);
            break;
        }
        pthread_mutex_unlock(&mutex);
    }

    pthread_mutex_unlock(&mutex);
    free(l_args);
    return NULL;
}