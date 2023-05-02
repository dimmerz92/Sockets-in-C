#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <openssl/evp.h>
#include <openssl/bio.h>
#include <openssl/ssl.h>


/*-------------------------
| CONSTS
|-------------------------*/
#define MAX_SESSIONS 5
#define MAX_BUFFER 256

/*-------------------------
| PRE-DECLARATIONS
| - main() dependencies
|-------------------------*/
void *client_handler(void *ssl);
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
        fprintf(stderr, "Error insufficient arguments\n");
        return -1;
    }

    // Generate EC keys
    EVP_PKEY_CTX *key_ctx = EVP_PKEY_CTX_new_id(EVP_PKEY_EC, NULL);
    if (key_ctx == NULL)
    {
        fprintf(stderr, "Error generating key context\n");
        exit(1);
    }

    if (EVP_PKEY_keygen_init(key_ctx) <= 0)
    {
        fprintf(stderr, "Error generating keygen\n");
        exit(1);
    }

    if (EVP_PKEY_CTX_set_ec_paramgen_curve_nid(key_ctx, NID_X9_62_prime256v1) <= 0)
    {
        fprintf(stderr, "Error generating elliptic curve\n");
        exit(1);
    }

    EVP_PKEY *keys;
    if (EVP_PKEY_keygen(key_ctx, &keys) <= 0)
    {
        fprintf(stderr, "Error generating keys\n");
        exit(1);
    }

    EVP_PKEY_CTX_free(key_ctx);

    // Initialise OpenSSL SSL context object
    SSL_CTX *ctx = SSL_CTX_new(TLS_server_method());
    if (ctx == NULL)
    {
        fprintf(stderr, "Error generating SSL context\n");
        exit(1);
    }

    // Generate and self-sign certificate
    X509 *cert = X509_new();
    if (cert == NULL)
    {
        fprintf(stderr, "Error generating certificate\n");
        exit(1);
    }
    X509_set_version(cert, 2);
    ASN1_INTEGER_set(X509_get_serialNumber(cert), 1);
    X509_gmtime_adj(X509_get_notBefore(cert), 0);
    X509_gmtime_adj(X509_get_notAfter(cert), (long) 60 * 60 * 24 * 365); // valid for 1 year (in seconds)
    X509_set_pubkey(cert, keys);
    X509_sign(cert, keys, EVP_sha256());

    // Add cert and keys to SSL context
    if (SSL_CTX_use_certificate(ctx, cert) != 1)
    {
        fprintf(stderr, "Error loading certificate\n");
        exit(1);
    }

    if (SSL_CTX_use_PrivateKey(ctx, keys) != 1)
    {
        fprintf(stderr, "Error loading keys\n");
        exit(1);
    }

    if (SSL_CTX_check_private_key(ctx) != 1)
    {
        fprintf(stderr, "Error checking keys");
        exit(1);
    }

    // Initialise OpenSSL listen socket
    BIO *bio = BIO_new_accept(argv[1]);
    if (bio == NULL)
    {
        fprintf(stderr, "Error initalising BIO socket\n");
        return -1;
    }

    // Bind the socket
    if (BIO_do_accept(bio) <= 0)
        {
            fprintf(stderr, "Error binding socket\n");
            return -1;
        }

    // Keep accepting new connections
    while (1)
    {
        // Accept incoming connections
        if (BIO_do_accept(bio) <= 0)
        {
            fprintf(stderr, "Error accepting connection\n");
            continue;
        }

        // Get the client BIO
        BIO *client_bio = BIO_pop(bio);
        if (client_bio == NULL)
        {
            fprintf(stderr, "Error getting client BIO\n");
            continue;
        }

        // Initialise SSL
        SSL *ssl = SSL_new(ctx);
        if (ssl == NULL)
        {
            fprintf(stderr, "Error initialising ssl\n");
            BIO_free(client_bio);
            continue;
        }

        // Wrap BIO in SSL
        SSL_set_bio(ssl, client_bio, client_bio);
        if (SSL_accept(ssl) <= 0)
        {
            fprintf(stderr, "Error applying SSL\n");
            BIO_free(client_bio);
            //SSL_free(ssl);
            continue;
        }

        pthread_t thread;
        if (pthread_create(&thread, NULL, client_handler, (void *) ssl) != 0)
        {
            fprintf(stderr, "Error producing thread\n");
            SSL_free(ssl);
        } else {
            pthread_detach(thread);
        }
    }
    return 0;
}

/*-------------------------
| FUNCTIONS
|-------------------------*/

// handles client sessions
void *client_handler(void *ssl)
{
    // Initalise variables
    SSL *client = (SSL *) ssl;
    char buffer[MAX_BUFFER], *command, *argument;
    int session, cmd_len, arg_len, arg, exists;

    // read first message, ensure CONNECT before starting new thread
    memset(buffer, 0, MAX_BUFFER);
    if (SSL_read(client, buffer, MAX_BUFFER - 1) <= 0)
    {
        fprintf(stderr, "Error reading CONNECT\n");
        SSL_free(client);
        return NULL;
    }

    // replace \n & \r with \0
    strip_nl(buffer);

    // check message is ASCII only
    if (ascii_buffer(buffer) < 0)
    {
        SSL_free(client);
        return NULL;
    }

    // check argument is CONNECT with space
    if (n_sessions == 5 || strncmp(buffer, "CONNECT ", 8) != 0)
    {
        SSL_free(client);
        return NULL;
    }

    // check if client exists
    pthread_mutex_lock(&mutex);
    if (get_session(&buffer[strlen("CONNECT ")]) >= 0)
    {
        pthread_mutex_unlock(&mutex);
        SSL_write(client, "CONNECT: ERROR", strlen("CONNECT: ERROR"));
        SSL_free(client);
        return NULL;
    }
    pthread_mutex_unlock(&mutex);

    // add client session
    client_session c_session;
    c_session.client_id = malloc((strlen(&buffer[strlen("CONNECT ")]) + 1) * sizeof(char));
    c_session.data = malloc(1);
    if (c_session.client_id == NULL || c_session.data == NULL)
    {
        SSL_write(client, "CONNECT: ERROR", strlen("CONNECT: ERROR"));
        SSL_free(client);
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
    if(SSL_write(client, "CONNECT: OK", strlen("CONNECT: OK")) <= 0)
    {
        pthread_mutex_lock(&mutex);
        remove_session(c_session.client_id);
        pthread_mutex_unlock(&mutex);
        SSL_free(client);
        return NULL;
    }

    // continue handling messages until DISCONNECT
    while (1)
    {
        // receive messages
        memset(buffer, 0, MAX_BUFFER);
        if (SSL_read(client, buffer, MAX_BUFFER - 1) <= 0)
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
            SSL_write(client, "DISCONNECT: OK", strlen("DISCONNECT: OK"));
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
                if (SSL_write(client, "ACK", 3) <= 0)
                {
                    switch_err = 1;
                }

                if (!switch_err)
                {
                    // receive value
                    memset(buffer, 0, MAX_BUFFER);
                    if (SSL_read(client, buffer, MAX_BUFFER - 1) <= 0)
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
                    if (SSL_write(client, "PUT: OK", strlen("PUT: OK")) <= 0)
                    {
                        switch_err = 1;
                    }
                }

                put_error:
                if (switch_err)
                {
                    switch_err = 0;
                    if (SSL_write(client, "PUT: ERROR", strlen("PUT: ERROR")) <= 0)
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
                        if (SSL_write(client, sessions[session].data[i].value, strlen(sessions[session].data[i].value)) <= 0)
                        {
                            switch_err = 1;
                        }
                        break;
                    }
                }

                if (switch_err || !exists)
                {
                    switch_err = 0;
                    if (SSL_write(client, "GET: ERROR", strlen("GET: ERROR")) <= 0)
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
                    if (SSL_write(client, "DELETE: ERROR", strlen("DELETE: ERROR")) <= 0)
                    {
                        switch_err = 1;
                    }
                }
                else
                {
                    if (SSL_write(client, "DELETE: OK", strlen("DELETE: OK")) <= 0)
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
    SSL_free(client);
    return NULL;
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