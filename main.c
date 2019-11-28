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
#define MAX_THREADS 100


struct threadArg{
    int newSockfd;
    uid_t uID;
};

struct threadArg *connections[MAX_THREADS];
struct sockaddr_un end_serv;

pthread_t tid[MAX_THREADS];

int sockfd,num_connects=0;

char* global_inputFile = NULL;
char* global_outputFile = NULL;
int numberThreads = 0;
int numBuckets = 1;
pthread_mutex_t commandsLock;
tecnicofs* fs;

static void displayUsage(const char* appName) {
    printf("Usage: %s input_filepath output_filepath threads_number buckets_number\n",
            appName);
    exit(EXIT_FAILURE);
}

static void parseArgs(long argc, char* const argv[]) {
    if (argc != 5) {
        fprintf(stderr, "Invalid format:\n");
        displayUsage(argv[0]);
    }

    global_inputFile = argv[1];
    global_outputFile = argv[2];

    numberThreads = atoi(argv[3]);
    if (!numberThreads) {
        fprintf(stderr, "Invalid number of threads.\n");
        displayUsage(argv[0]);
    }

    numBuckets = atoi(argv[4]);
    if(!numBuckets){
        fprintf(stderr,"Invalid number of buckets.\n");
        displayUsage(argv[0]);
    }
}


void errorParse(int lineNumber) {
    fprintf(stderr, "Error: line %d invalid\n", lineNumber);
    exit(EXIT_FAILURE);
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

void applyCommand(char* inputCommands,uid_t uID,int sockfd) {
    mutex_lock(&commandsLock);

    char command[MAX_INPUT_SIZE],name[MAX_INPUT_SIZE],name2[MAX_INPUT_SIZE], token;
    int iNumber,own,other;
    permission ownerPerm,othersPerm;

    strcpy(command,inputCommands);
    token = command[0];
    
    switch (token) {
        case 'c':
            sscanf(command, "%c %s %d%d", &token, name,&own,&other);
            
            ownerPerm=own;
            othersPerm=other;

            iNumber = obtainNewInumber(uID,ownerPerm,othersPerm);
            printf("%d\n",iNumber);
            mutex_unlock(&commandsLock);

            create(fs, name, iNumber,sockfd);
            
            break;
        case 'l':
            sscanf(command, "%c %s", &token, name);
            mutex_unlock(&commandsLock);

            int searchResult = lookup(fs, name);
            if (!searchResult)
                printf("%s not found\n", name);
            else
                printf("%s found with inumber %d\n", name, searchResult);
            
            break;
        case 'd':
            sscanf(command, "%c %s", &token, name);

            mutex_unlock(&commandsLock);

            delete(fs, name,uID,sockfd);

            break;
        case 'r':
            sscanf(command,"%c %s %s", &token, name, name2);
            
            mutex_unlock(&commandsLock);

            renameFile(fs, name, name2, sockfd);

            break;
        default: { /* error */
            mutex_unlock(&commandsLock);

            fprintf(stderr, "Error: commands to apply\n");
            exit(EXIT_FAILURE);
        }
    }
}



int mount(char* address){
    int dim_serv;
    if((sockfd = socket(AF_UNIX,SOCK_STREAM,0))<0)
        perror("Erro ao criar socket servidor");
    
    unlink(address);

    bzero((char*)&end_serv,sizeof(end_serv));

    end_serv.sun_family = AF_UNIX;
    strcpy(end_serv.sun_path,address);
    dim_serv = sizeof(end_serv.sun_family)+ strlen(end_serv.sun_path);

    if((bind(sockfd, (struct sockaddr *)&end_serv, dim_serv))<0)
        perror("Erro no Bind Servidor");
    
    listen(sockfd,5);
    return 0;
}

void* trata_cliente(void* sock){
    struct threadArg* tsockfd = (struct threadArg*)sock;
    int sockfd= tsockfd->newSockfd;
    uid_t uID= tsockfd->uID;
    char buffer[100];
    while(read(sockfd,buffer,100) > 0){
        printf("%s\n",buffer);
        applyCommand(buffer,uID,sockfd);
    }
    return NULL;
}

int main(int argc, char* argv[]) {
    struct ucred user;
    int dim_cli,i=0,len=sizeof(struct ucred);
    struct sockaddr_un end_cli;
    
    parseArgs(argc, argv);

    FILE * outputFp = openOutputFile();
    fs = new_tecnicofs();

    mutex_init(&commandsLock);
    inode_table_init();
    mount(UNIXSTR_PATH);

    while(1){
        dim_cli = sizeof(end_cli);
        connections[num_connects]= (struct threadArg*)malloc(sizeof(int)+sizeof(uid_t));
        connections[num_connects]->newSockfd=accept(sockfd,(struct sockaddr *)&end_cli,(socklen_t*)&dim_cli);
        if (connections[num_connects]->newSockfd<0)
            perror("Erro ao aceitar socket cliente");

        if(getsockopt(connections[num_connects]->newSockfd,SOL_SOCKET,SO_PEERCRED,&user,(socklen_t*)&len)<0)
            perror("Erro ao obter userID do cliente");
        connections[num_connects]->uID=user.uid;

        pthread_create(&tid[i++],NULL,trata_cliente,(void*)connections[num_connects++]);
    }

    

    print_tecnicofs_tree(outputFp, fs);
    fflush(outputFp);
    fclose(outputFp);
    mutex_destroy(&commandsLock);

    free_tecnicofs(fs);

    exit(EXIT_SUCCESS);
}
