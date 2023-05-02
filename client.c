#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <openssl/evp.h>
#include <openssl/bio.h>
#include <openssl/ssl.h>

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
        fprintf(stderr, "Error insufficient arguments\n");
        exit(1);
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
    SSL_CTX *ctx = SSL_CTX_new(TLS_client_method());
    if (ctx == NULL)
    {
        fprintf(stderr, "Error generating SSL context\n");
        exit(1);
    }

    SSL_CTX_set_verify(ctx, SSL_VERIFY_NONE, NULL);

    // Initalise variables
    int action;
    char buffer[MAX_BUFFER], host_port[100];

    strcpy(host_port, argv[1]);
    strcat(host_port, ":");
    strcat(host_port, argv[2]);

    // Initialise OpenSSL socket
    BIO *bio = BIO_new_ssl_connect(ctx);
    if (bio == NULL)
    {
        fprintf(stderr, "Error initalising BIO socket\n");
        exit(1);
    }

    // Initialise SSL
    SSL *ssl;
    if (BIO_get_ssl(bio, &ssl) <= 0)
    {
        fprintf(stderr, "Error initialising ssl\n");
        exit(1);
    }

    SSL_set_mode(ssl, SSL_MODE_AUTO_RETRY);
    if(BIO_set_conn_hostname(bio, host_port) <= 0)
    {
        fprintf(stderr, "Error adding hostname\n");
        exit(1);
    }
    
    // Connect to server
    if (BIO_do_connect(bio) <= 0)
    {
        fprintf(stderr, "Error connecting to server\n");
        BIO_free(bio);
        exit(1);
    }

    // Perform TLS handshake
    if (BIO_do_handshake(bio) <= 0)
    {
        fprintf(stderr, "Error on TLS handshake\n");
        BIO_free(bio);
        exit(1);
    }

    while (1)
    {
        // Client input
        memset(buffer, 0, MAX_BUFFER);
        fgets(buffer, MAX_BUFFER, stdin);
        if (ascii_buffer(buffer) < 0)
        {
            fprintf(stderr, "Error input contains non-ASCII characters\n");
            BIO_free(bio);
            exit(1);
        }
        
        // check ascii chars
        if (BIO_write(bio, buffer, strlen(buffer)) <= 0)
        {
            fprintf(stderr, "Error writing to server\n");
            BIO_free(bio);
            exit(1);
        }

        // receive a message
        memset(buffer, 0, MAX_BUFFER);
        if ((action = BIO_read(bio, buffer, MAX_BUFFER)) <= 0)
        {
            fprintf(stderr, "Error reading from server\n");
            BIO_free(bio);
            exit(1);
        }

        // check ascii chars
        if (ascii_buffer(buffer) < 0)
        {
            fprintf(stderr, "Error input contains non-ASCII characters\n");
            BIO_free(bio);
            exit(1);
        }

        else if (strcmp(buffer, "ACK") == 0)
        {
            // send the value
            memset(buffer, 0, MAX_BUFFER);
            fgets(buffer, MAX_BUFFER, stdin);

            // check ascii chars
            if (ascii_buffer(buffer) < 0)
            {
                fprintf(stderr, "Error input contains non-ASCII characters\n");
                BIO_free(bio);
                exit(1);
            }

            if (BIO_write(bio, buffer, strlen(buffer)) <= 0)
            {
                fprintf(stderr, "Error writing to server\n");
                BIO_free(bio);
                exit(1);
            }

            // receive a message
            memset(buffer, 0, MAX_BUFFER - 1);
            if ((action = BIO_read(bio, buffer, MAX_BUFFER)) < 0)
            {
                fprintf(stderr, "Error reading from server\n");
                BIO_free(bio);
                exit(1);
            }

            // check ascii chars
            if (ascii_buffer(buffer) < 0)
            {
                fprintf(stderr, "Error input contains non-ASCII characters\n");
                BIO_free(bio);
                exit(1);
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
    BIO_free(bio);

    return 0;
}

/*-------------------------
| FUNCTIONS
|-------------------------*/

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