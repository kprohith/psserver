// psclient.c
// Author: Rohith Kotia Palakirti

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <string.h>
#include <pthread.h>
#include <stringmap.h>
#include <stdbool.h>
#include <csse2310a3.h>
#include <csse2310a4.h>
#include <semaphore.h>

/*
 * Function Prototypes
 */
void* stdin_thread(void* arg);
int is_valid_string(char* s);

/* int main(int argc, char *argv[])
* -----------------------------------------------
* Initiates and runs psclient
*
* argc: count of number of commandline arguments
* argv: the array of commandline arguments stored as strings
*
* Returns: exit code of the program
* Errors: program exits with code 1 if the input is invalid
*                            code 2 is topic or name is invalid
*                            code 3 if connection to port fails
*                            code 4 if server is terminated
*/
int main(int argc, char* argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Usage: psclient portnum name [topic] ...\n");
        exit(1);
    }
    const char* portNum = argv[1];
    char* name = argv[2];
    char* topics[argc - 3];
    for (int i = 3; i < argc; i++) {
        topics[i - 3] = argv[i];
    }
    if (!is_valid_string(name)) {
        fprintf(stderr, "psclient: invalid name\n");
        exit(2);
    }
    for (int i = 0; i < argc - 3; i++) {
        if (!(is_valid_string(topics[i])) && strlen(topics[i]) != 0) {
            fprintf(stderr, "psclient: invalid topic\n");
            exit(2);
        }
    }
    struct addrinfo* ai = 0;
    struct addrinfo hints;
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_INET;  // IPv4  for generic could use AF_UNSPEC
    hints.ai_socktype = SOCK_STREAM;
    int err;
    if ((err = getaddrinfo("localhost", portNum, &hints, &ai))) {
        freeaddrinfo(ai);
        fprintf(stderr, "psclient: unable to connect to port %s\n", portNum);
        exit(3);
    }
    int fd = socket(AF_INET, SOCK_STREAM, 0);   // 0 == use default protocol
    if (connect(fd, (struct sockaddr *)ai->ai_addr, sizeof(struct sockaddr))) {
        fprintf(stderr, "psclient: unable to connect to port %s\n", portNum);
        exit(3);
    }
    int fd2 = dup(fd);
    FILE* to = fdopen(fd, "w");
    FILE* from = fdopen(fd2, "r");
    fprintf(to, "name %s\n", name);
    fflush(to);
    for (int i = 0; i < argc - 3; i++) {
        fprintf(to, "sub %s\n", topics[i]);
        fflush(to);
    }
    pthread_t threadId;
    pthread_create(&threadId, NULL, stdin_thread, to);
    pthread_detach(threadId);
    char* readLine;
    while ((readLine = read_line(from))) {
        printf("%s\n", readLine);
        fflush(stdout);
    }
    fprintf(stderr, "psclient: server connection terminated\n");
    free(readLine);
    exit(4);
}

/* void* stdin_thread(void* arg)
* -----------------------------------------------
* Function that is responsible for reading input from stdin of client,
* then redirecting the input to the server, runs in a dedicated async thread
*
* arg: struct containing args that are to be passed to the function
*/
void* stdin_thread(void* arg) {
    FILE* to = (FILE *) arg;
    char* stdinLine;
    while ((stdinLine = read_line(stdin))) {
        fprintf(to, "%s\n", stdinLine);
        fflush(to);
    }
    exit(0);
}

/* int is_valid_string(char* s)
* -----------------------------------------------
* Checks if the passed string is valid or not
*
* s: string to be checked
* 
* Returns: 0, if invalid
*          1, if valid
*/
int is_valid_string(char* s) {
    if (strchr(s, ' ') || strchr(s, ':') || strchr(s, '\n')) {
        return 0;
    } else {
        return 1;
    }
}
