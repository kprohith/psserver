// psserver.c
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
#include <errno.h>
#include <signal.h>

/*
* Struct Definitions
*/

/* Client Struct
* -----------------------------------------------
* Structure to hold properties of each client
* id: unique ID of the client
* fd: file descriptor to the open socker
* fileRead: fdopen'd fd for reading
* fileWrite: fdopen'd fd for writing
* name: name of the client
* threadId: threadId of the thread running the client
* active: flag to indicate if the client is active
* sm: StringMap data structure to hold topics and subscribed clients
* guard: sempahore guard to lock data structures
*/
typedef struct Client {
    int id;
    int* fd;
    FILE* fileRead;
    FILE* fileWrite;
    char* name;
    pthread_t threadId;
    bool active;
    sem_t* guard;
    StringMap* sm;
    int* statistics;
} Client;

/* Args Struct
* -----------------------------------------------
* Structure to hold the arguments that are passed to each new thread
* client: pointer to the client struct for the thread
* clientCount: count of total clients
*/
typedef struct Args {
    Client* client;
    int clientCount;
} Args;

typedef struct SigArgs {
    sigset_t* set;
    int* statistics;
} SigArgs;

// Reference; https://stackoverflow.com/questions/3536153/c-dynamicall
// y-growing-array

/* ClientArray Struct
* -----------------------------------------------
* Structure to hold a dynamically sized array of Client*
* client: list of clients
* used: current utilized size of array
* size: size of array
* count: number of clients in array
*/
typedef struct ClientArray {
    Client** client;
    size_t used;
    size_t size;
    int count;
} ClientArray;

/*
 * Function Prototypes
 */
void* client_thread(void*);
int open_listen(const char* port, int connections);
void process_connections(int fdServer);
void init_client_array(ClientArray* a, size_t initialSize);
int insert_client_array(ClientArray* a, Client* element);
void remove_client(ClientArray* a, int index);
void delete_client(ClientArray* a, Client* element);
void print_client_array(ClientArray* a, StringMap* sm);
void free_client_array(ClientArray* a);
void init_lock(sem_t* l);
void take_lock(sem_t* l);
void release_lock(sem_t* l);
int is_valid_string(char* s);
void* sig_thread(void* arg);
int open_listen(const char* port, int connections);
void process_connections(int fdServer);
void* client_thread(void* arg);
void print_err();
void print_socket_err();

/* int main(int argc, char *argv[])
* -----------------------------------------------
* Initiates and runs the program
*
* argc: count of number of commandline arguments
* argv: the array of commandline arguments stored as strings
*
* Returns: 0 on successful termination
* Errors: programs exits with code 1 if the input is invalid
*/
int main(int argc, char* argv[]) {
    int fdServer, connections;
    char* portStr;
    if (argc < 2 || argc > 3) {
        print_err();
    }
    if (argv[1] && isdigit(argv[1][0])) {
        connections = atoi(argv[1]);
        if (connections < 0) {
            print_err();
        }
    } else {
        print_err();
    }
    if (argc == 3) {
        if (isdigit(argv[2][0])) {
            int portNum = atoi(argv[2]);
            if (portNum < 1024 || portNum > 65535) {
                if (portNum != 0) {
                    print_err();
                }
            }
            portStr = argv[2];
        } else {
            print_err();
        }
    } else {
        portStr = "0";
    }
    const char* port = portStr;
    fdServer = open_listen(port, connections);
    pthread_t thread;
    sigset_t set; // Reference: man page of pthread_sigmask
    int s;
    sigemptyset(&set); // Handle SIGHUP
    sigaddset(&set, SIGHUP);
    s = pthread_sigmask(SIG_BLOCK, &set, NULL);
    if (s == 0) {
        // SigArgs* sigArgs;
        // sigArgs->set = &set;
        // sigArgs->statistics = statistics;
        s = pthread_create(&thread, NULL, &sig_thread, (void*) &set);
        pthread_detach(thread);
    }
    process_connections(fdServer);
    return 0;
}

/* int open_listen(const char* port, int connections)
* -----------------------------------------------
* Listens on given port. Returns listening socket (or exits on failure)
*
* port: port to listen on
* connections: maximum limit on number of connected clients
*
* Returns: the fiel descriptor of the opened socket
* Errors: exit code 1 on failure to initialize socket
*                    2 on failure to open socket
                     4, on failure to listen successfully
* 
*/
int open_listen(const char* port, int connections) {
    struct addrinfo* ai = 0;
    struct addrinfo hints;
    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_INET;  // IPv4
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;        // listen on all IP addresses
    int err;
    if ((err = getaddrinfo(NULL, port, &hints, &ai))) {
        freeaddrinfo(ai);
        fprintf(stderr, "%s\n", gai_strerror(err));
        return 1;               // Could not determine address
    }
    // Create a socket and bind it to a port
    int listenfd = socket(AF_INET, SOCK_STREAM, 0); // 0=default protocol (TCP)
    // Allow address (port number) to be reused immediately
    int optVal = 1;
    if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR,
            &optVal, sizeof(int)) < 0) {
        perror("Error setting socket option");
        exit(1);
    }
    if (bind(listenfd, (struct sockaddr *)ai->ai_addr,
            sizeof(struct sockaddr)) < 0) {
        //  perror("Binding");
        fprintf(stderr, "psserver: unable to open socket for listening\n");
        exit(2);
    }
    struct sockaddr_in ad;
    memset(&ad, 0, sizeof(struct sockaddr_in));
    socklen_t len = sizeof(struct sockaddr_in);
    if (getsockname(listenfd, (struct sockaddr *)&ad, &len)) {
        perror("sockname");
        return 4;
    }
    fprintf(stderr, "%u\n", ntohs(ad.sin_port));
    fflush(stderr);
    if (listen(listenfd, 10) < 0) {  // Up to 10 connection requests can queue
        perror("Listen");
        return 4;
    }
    freeaddrinfo(ai);
    // Have listening socket - return it
    return listenfd;
}

/* void process_connections(int fdServer)
* -----------------------------------------------
* Processes incoming client connections and spawns a new thread for each client
*
* fdServer: file descriptor of the listening socket
*
* Errors: exits with code 1 on failure to accept a new connection
*/
void process_connections(int fdServer) {
    int fd;
    struct sockaddr_in fromAddr;
    socklen_t fromAddrSize;
    int clientCount = 0;
    int statistics[5] = {0};
    StringMap* sm = stringmap_init();
    sem_t l;
    init_lock(&l);
    while (1) { // Repeatedly accept connections
        fromAddrSize = sizeof(struct sockaddr_in);
        fd = accept(fdServer, (struct sockaddr *)&fromAddr, &fromAddrSize);
        if (fd < 0) {
            print_socket_err();
        }
        char hostname[NI_MAXHOST];
        int err = getnameinfo((struct sockaddr *)&fromAddr,
                            fromAddrSize, hostname, NI_MAXHOST, NULL, 0, 0);
        if (err) {
            print_socket_err();
        } 
        int fd2 = dup(fd);
        int fd1 = dup(fd);
        FILE* readClient = fdopen(fd1, "r");
        FILE* writeClient = fdopen(fd2, "w");
        ++clientCount;
        Args* args = malloc(sizeof(Args));
        Client* client = malloc(sizeof(Client));
        client->statistics = statistics;
        client->id = clientCount;
        client->fileRead = readClient;
        client->fileWrite = writeClient;
        client->active = true;
        client->sm = sm;
        client->guard = &l;
        args->clientCount = clientCount;
        args->client = client;
        pthread_create(&(client->threadId), NULL, client_thread, args);
        pthread_detach(client->threadId);
    }
    stringmap_free(sm);
}

/* void* client_thread(void* arg)
* -----------------------------------------------
* Function that is responsible for managing a client in a new thread
*
* arg: struct containing args that are to be passed to the function
*/
void* client_thread(void* arg) {
    Args* args = arg;
    Client* client = args->client;
    take_lock(client->guard);
    FILE* fileRead = client->fileRead;
    FILE* fileWrite = client->fileWrite;
    StringMap* sm = client->sm;
    int id = (int)client->id;
    free(arg);
    char* clientLine;
    fflush(fileWrite);
    while ((clientLine = read_line(fileRead))) {
        if (clientLine == NULL) {
            continue;
        }
        char** inputSplit = split_by_char(clientLine, ' ', 2);
        if (strcmp(inputSplit[0], "name") == 0) {
            if (inputSplit[1] && strlen(inputSplit[1]) != 0
                    && is_valid_string(inputSplit[1])) {
                if (client->name == NULL) {
                    client->name = strdup(inputSplit[1]);
                }
            } else {
                fprintf(fileWrite, ":invalid\n");
                fflush(fileWrite);
            }
        } else if (strcmp(inputSplit[0], "sub") == 0) {
            if (client->name == NULL) {
                continue;
            }
            void* item;
            if (!(item = stringmap_search(sm, inputSplit[1]))) {
                ClientArray* a = malloc(sizeof(ClientArray));
                init_client_array(a, 1);
                insert_client_array(a, client);
                stringmap_add(sm, inputSplit[1], a);
            } else {
                ClientArray* a = (ClientArray *) item;
                bool dupFlag = 0;
                for (int i = 0; i < a->count; i++) {
                    Client* c = a->client[i];
                    if (c) {
                        if ((c->id == client->id)) {
                            dupFlag = 1; // Already subbed
                            break;
                        }
                    } 
                }
                if (!dupFlag) {
                    stringmap_remove(sm, inputSplit[1]);
                    insert_client_array(a, client);
                    stringmap_add(sm, inputSplit[1], a);
                }
            }
        } else if (strcmp(inputSplit[0], "pub") == 0) {
            if (!inputSplit[1] || strlen(inputSplit[1]) == 0) {
                fprintf(fileWrite, ":invalid\n");
                fflush(fileWrite);
                continue;
            }
            if (client->name == NULL) {
                continue;
            }
            char** pubSplit = split_by_char(inputSplit[1], ' ', 2);
            if (!pubSplit[1] || strlen(pubSplit[1]) == 0) {
                fprintf(fileWrite, ":invalid\n");
                fflush(fileWrite);
                continue;
            }
            void* item;
            if (!(item = stringmap_search(sm, pubSplit[0]))) {
                // ERROR retrieving pubSplit[0]
            } else {
                if (!item) {
                    //    ERROR: Empty array
                } else {
                    ClientArray* a = (ClientArray *) item;
                    for (int i = 0; i < a->count; i++) {
                        Client* c = a->client[i];
                        if (client != NULL) {
                            fprintf(c->fileWrite, "%s:%s:%s\n", client->name,
                                    pubSplit[0], pubSplit[1]);
                            fflush(c->fileWrite);
                        }
                    }
                }
            }
            free(pubSplit);
        } else if (strcmp(inputSplit[0], "unsub") == 0) {
            if (client->name == NULL) {
                continue;
            }
            void* item;
            if (!(item = stringmap_search(sm, inputSplit[1]))) {
                //      ERROR retrieving inputSplit[1]
            } else {
                ClientArray* a = (ClientArray *) item;
                bool flag = 0;
                for (int i = 0; i < a->count; i++) {
                    Client* client = a->client[i];
                    if (client->id == id) {
                        delete_client(a, client);
                        flag = 1;
                        break;
                    }
                }
                if (flag) {
                    stringmap_remove(sm, inputSplit[1]);
                    stringmap_add(sm, inputSplit[1], a);
                }
            }
        } else {
            fprintf(fileWrite, ":invalid\n");
            fflush(fileWrite);
        }
        release_lock(client->guard);
        free(inputSplit);
        free(clientLine);
    }
    StringMapItem* smi = NULL;
    smi = stringmap_iterate(sm, smi);
    if (!smi) {
        stringmap_free(sm);
    }
    fclose(fileRead);
    fclose(fileWrite);
    return NULL;
}

/* void init_client_array(ClientArray* a, size_t initialSize)
* -----------------------------------------------
* Initializes a new ClientArray 
* 
* a: the ClientArray to be initialized
* initialSize: required initial size of array
*/
void init_client_array(ClientArray* a, size_t initialSize) {
    a->client = malloc(initialSize * sizeof(Client*));
    a->used = 0;
    a->size = initialSize;
    a->count = 0;
}

/* int insert_client_array(ClientArray* a, Client* element)
* -----------------------------------------------
* Inserts a client into ClientArray
* 
* a: the ClientArray to be initialized
* initialSize: required initial size of array
*
* Returns: 0, on insertion error
*          1, on successful insertion
*/
int insert_client_array(ClientArray* a, Client* element) {
    for (int i = 0; i < a->count; i++) {
        if (a->client[i] == element) {
            // ERROR: Already in array!
            return 0;
        }
        if (a->client[i]->id == element->id) {
            // ERROR: Already in array!
            return 0;
        }
    }
    if (a->used == a->size) {
        a->size *= 2;
        a->client = (Client**) realloc(a->client, a->size * sizeof(Client*));
    }
    a->client[a->used++] = element;
    a->count++;
    return 1;
}

/* void remove_client(ClientArray* a, int index)
* -----------------------------------------------
* Removes a client at a particular index
* 
* a: the ClientArray from which client is to be removed
* index: index of client that is to be removed
*/
void remove_client(ClientArray* a, int index) {
    if (index < 0 || index >= a->count) {
        return;
    }
    for (int i = index; i < a->count - 1; i++) {
        a->client[i] = a->client[i + 1];
    }
    a->count--;
}

/* void delete_client(ClientArray* a, Client* element)
* -----------------------------------------------
* Deletes a client from a ClientArray
* 
* a: the ClientArray from which element is to be removed
* element: the client that is to be removed
*/
void delete_client(ClientArray* a, Client* element) {
    for (int i = 0; i < a->count; i++) {
        if (a->client[i] == element) {
            remove_client(a, i);
            return;
        }
    }
}

/* void print_client_array(ClientArray* a, StringMap* sm)
* -----------------------------------------------
* Prints out the elements of Client Array as well as StringMap
* 
* a: the ClientArray to be printed
* sm: the StringMap to be printed
*/
void print_client_array(ClientArray* a, StringMap* sm) {
    printf("[");
    for (int i = 0; i < a->count; i++) {
        if (a->client[i] != NULL) {
            printf(" %d ", a->client[i]->id);
        }
    }
    printf("]\n");
    printf("-----\n");
    StringMapItem* smi = NULL;
    while ((smi = stringmap_iterate(sm, smi))) {
        printf("%s:%p\n", smi->key, smi->item);
    }
}

/* void free_client_array(ClientArray* a)
* -----------------------------------------------
* Frees memeory allocated to a ClientArray
* 
* a: the ClientArray to be freed
*/
void free_client_array(ClientArray* a) {
    free(a->client);
    a->client = NULL;
    a->used = a->size = 0;
}

/* void init_lock(sem_t* l)
* -----------------------------------------------
* Initializes a semphore lock
* 
*/
void init_lock(sem_t* l) {
    sem_init(l, 0, 1);
}

/* void take_lock(sem_t* l)
* -----------------------------------------------
* Assigns lock to the calling thread
* 
*/
void take_lock(sem_t* l) {
    sem_wait(l);
}

/* void release_lock(sem_t* l)
* -----------------------------------------------
* Releases lock from calling thread
* 
*/
void release_lock(sem_t* l) {
    sem_post(l);
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

/* void print_err() 
* -----------------------------------------------
* Prints the standard error message on invalid input
* Exits with code 1
*/
void print_err() {
    fprintf(stderr, "Usage: psserver connections [portnum]\n");
    exit(1);
}

/* void print_socket_err()
* -----------------------------------------------
* Prints the socket error message on invalid input
* Exits with code 2
*/
void print_socket_err() {
    fprintf(stderr, "psserver: unable to open socket for listening\n");
    exit(2);
}

/* void* sig_thread(void *arg)
* -----------------------------------------------
* Function that is passed to the dedicated signal handling thread
* Prints out client and subscription/publication statistics
* arg: struct of args passed to signal handling thread
* 
*/
void* sig_thread(void* arg) {
    // SigArgs* args;
    sigset_t* set = arg;
    int s, sig;
    // statistics[0] = connected clients, [1] completed clients, [2] pub ops
    // [3] sub ops, [4] unsub ops
    int statistics[5] = {0};
    for (;;) {
        s = sigwait(set, &sig);
        if (s != 0) {
            // ERROR!
        }
        printf("Connected clients:%d\n", statistics[0]);
        fflush(stdout);
        printf("Completed clients:%d\n", statistics[1]);
        fflush(stdout);
        printf("pub operations:%d\n", statistics[2]);
        fflush(stdout);
        printf("sub operations:%d\n", statistics[3]);
        fflush(stdout);
        printf("unsub operations:%d\n", statistics[4]);
        fflush(stdout);
    }
}