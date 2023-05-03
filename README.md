# Assessment 4 - Client-Server with TLS Programs Assessment Submission README

## Author
**Name:**       Andrew Weymes\
**Student ID:** 220277169\
**Unit:**       COSC540 - Computer Networks and Information Security\
**Due Date:**   30/04/2023 (Trimester 1, 2023)

## Dependencies
- Bash shell
- GCC compiler
- C headers: stdio.h, ctype.h, stdlib.h, string.h, pthread.h, openssl/evp.h, openssl/bio.h, openssl/ssl.h

## Introduction
This is a submission for COSC540 - Computer Networks and Information Security at The University of New England (UNE). The submission contains two programs, the server and client, and two shell scripts to compile and run both programs. The server allows concurrent users to CONNECT with a client ID and subsequently add, delete, or manipulate data with the PUT, GET, and DELETE commands. This is done using elliptic curve cryptography from the OpenSSL library.

## Instructions
### Running the server
To run the server, use the following command:
```
bash startServer.sh 'port'
```
Where 'port' is the port number you want the server to run on.

### Running the client
To run the client, use the following command:
```
bash startClient.sh 'address' 'port'
```
Where 'address' is the server address (localhost for this assignment) and 'port' is the port number of the server.

### Client commands
**CONNECT 'client_id'** - The server will expect the first message to be CONNECT with a 'client_id' as a unique string chosen by the user

**PUT 'key'** - To store a 'key' 'value' pair, the client should pass the argument PUT with the 'key' that the 'value' should be attributed. The server will then await a second message with the 'value' to be stored.

**GET 'key'** - To return the value of a 'key', the client should pass the argument GET with the 'key' for the 'value' desired.

**DELETE 'key'** - To delete a stored 'key', the client should pass the argument DELETE with the 'key' for the 'key' 'value' pair to be deleted.

**DISCONNECT** - To disconnect from a session, the client should pass the argument DISCONNECT. The disconnect will delete all client data stored and remove the session.

## Further information & constraints
- Arguments in parenthesis are expected to be unique strings chosen by the client.
- On unexpected errors or incorrect client arguments, the server will force disconnect and delete all user data and remove the session.
- The maximum message a client can send is 256 characters including a null terminator.
    - This is inclusive of both command and argument.
