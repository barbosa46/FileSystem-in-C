/* Sistemas Operativos, DEI/IST/ULisboa 2019-20 */
#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <semaphore.h>
#include <sys/un.h>
#include <sys/types.h>
#include <sys/socket.h>
#include "fs.h"
#include "constants.h"
#include "lib/timer.h"
#include "sync.h"

#define UNIXSTR_PATH "/tmp/socket.unix.stream"


struct threadArg {
    int newSockfd;
    uid_t uID;
};

struct threadArg *connections[MAX_CLIENTS];
struct sockaddr_un end_serv;

pthread_t tid[MAX_CLIENTS];

int sockfd, num_connects = 0;
int numberThreads = 0;

char* socket_name = NULL;
char* global_outputFile = NULL;
int numBuckets = 1;

pthread_mutex_t commandsLock;

tecnicofs* fs;

static void displayUsage(const char* appName) {
    printf("Usage: %s input_filepath output_filepath threads_number buckets_number\n",
            appName);
    exit(EXIT_FAILURE);
}

static void parseArgs(long argc, char* const argv[]) {
    if (argc != 4) {
        fprintf(stderr, "Invalid format:\n");
        displayUsage(argv[0]);
    }

    socket_name = argv[1];
    global_outputFile = argv[2];

    numBuckets = atoi(argv[3]);
    if(!numBuckets){
        fprintf(stderr,"Invalid number of buckets.\n");
        displayUsage(argv[0]);
    }
}

FILE * openOutputFile() {
    FILE *fp;

    fp = fopen(global_outputFile, "w");
    if (fp == NULL) {
        perror("Error opening output file");
        exit(EXIT_FAILURE);
    }

    return fp;
}

void applyCommand(char* inputCommands, uid_t uID, openedFile** filetable, int sockfd) {
    mutex_lock(&commandsLock);

    char command[MAX_INPUT_SIZE], name[MAX_INPUT_SIZE], new_name[MAX_INPUT_SIZE], token;
    int iNumber, own, other, mode;
    permission ownerPerm, othersPerm;

    strcpy(command,inputCommands);
    token = command[0];

    switch (token) {
        case 'c':
            sscanf(command, "%c %s %d%d", &token, name, &own, &other);

            ownerPerm = own;
            othersPerm = other;

            iNumber = obtainNewInumber(uID, ownerPerm, othersPerm);
            mutex_unlock(&commandsLock);

            create(fs, name, iNumber,sockfd);

            break;
        case 'd':
            sscanf(command, "%c %s", &token, name);

            mutex_unlock(&commandsLock);

            delete(fs, name, uID, sockfd);

            break;
        case 'r':
            sscanf(command,"%c %s %s", &token, name, new_name);

            mutex_unlock(&commandsLock);

            renameFile(fs, name, new_name, uID, sockfd);

            break;
        case 'o':
            sscanf(command,"%c %s %d", &token, name, &mode);

            mutex_unlock(&commandsLock);

            openFile(fs, name, (permission) mode, uID, filetable, sockfd);
        default: { /* error */
            mutex_unlock(&commandsLock);

            fprintf(stderr, "Error: commands to apply\n");
            exit(EXIT_FAILURE);
        }
    }
}

openedFile** init_client_filetable(int index) {
    int i;

    for (i = 0; i < 5; i++) opened[index][i] = NULL;

    return opened[index];
}

void destroy_client_filetable(int index) {
    int i;

    for (i = 0; i < 5; i++) opened[index][i] = NULL;
}

int mount(char* address){
    int dim_serv;
    if((sockfd = socket(AF_UNIX,SOCK_STREAM, 0)) <0)
        perror("Error creating server socket");

    unlink(address);

    bzero((char*)&end_serv, sizeof(end_serv));

    end_serv.sun_family = AF_UNIX;
    strcpy(end_serv.sun_path, address);
    dim_serv = sizeof(end_serv.sun_family) + strlen(end_serv.sun_path);

    if ((bind(sockfd, (struct sockaddr *)&end_serv, dim_serv)) < 0)
        perror("Error binding server");

    listen(sockfd, 5);

    return 0;
}

void* handle_client(void* sock) {
    char buffer[100];

    struct threadArg* tsockfd = (struct threadArg*) sock;
    int sockfd = tsockfd->newSockfd;
    uid_t uID = tsockfd->uID;

    openedFile **client_opened = init_client_filetable(num_connects - 1);

    while (read(sockfd, buffer, 100) > 0)
        applyCommand(buffer, uID, client_opened, sockfd);

    destroy_client_filetable(num_connects - 1);

    return NULL;
}

int main(int argc, char* argv[]) {
    struct ucred user;
    int dim_cli, i = 0, len = sizeof(struct ucred);
    struct sockaddr_un end_cli;

    parseArgs(argc, argv);

    FILE * outputFp = openOutputFile();
    fs = new_tecnicofs();
    inode_table_init();

    mutex_init(&commandsLock);

    mount(socket_name);

    while (1) {
        dim_cli = sizeof(end_cli);
        connections[num_connects] = (struct threadArg*) malloc(sizeof(int) + sizeof(uid_t));
        connections[num_connects]->newSockfd = accept(sockfd, (struct sockaddr *)&end_cli, (socklen_t*)&dim_cli);
        if (connections[num_connects]->newSockfd < 0)
            perror("Error accepting client connection");

        if(getsockopt(connections[num_connects]->newSockfd, SOL_SOCKET, SO_PEERCRED, &user, (socklen_t*)&len) < 0)
            perror("Error obtaining client id");
        connections[num_connects]->uID = user.uid;

        pthread_create(&tid[i++], NULL, handle_client, (void*)connections[num_connects++]);
    }

    print_tecnicofs_tree(outputFp, fs);
    fflush(outputFp);
    fclose(outputFp);
    mutex_destroy(&commandsLock);

    inode_table_destroy();
    free_tecnicofs(fs);

    exit(EXIT_SUCCESS);
}