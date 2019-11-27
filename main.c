/* Sistemas Operativos, DEI/IST/ULisboa 2019-20 */
/* In order to simplify the access from the threads to the commands array,
   the commands will be inserted to the tail and removed from the head,
   when a command is removed, all the positions are shifted to the left,
   when the array is empty, the head has the command 'f'.
   The head and the tail are designed by the variables head and numberCommands.*/
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
    int uID,newSockfd;
};

struct threadArg *connections[MAX_THREADS];
struct sockaddr_un end_serv;

pthread_t tid[MAX_THREADS];

int sockfd,num_connects=0;

char* global_inputFile = NULL;
char* global_outputFile = NULL;
int numberThreads = 0;
int numBuckets = 0;

pthread_mutex_t commandsLock, semMut;
sem_t semprod, semcons;

tecnicofs* fs;

char inputCommands[MAX_COMMANDS][MAX_INPUT_SIZE];
int numberCommands = 0; // Tail of the commands array (first empty slot)
int head = 0;   // Head of the commands array
int kill = 0;   // 0 if still processing input, 1 otherwise

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

void insertCommand(char* data) {
    wait_sem(&semprod);
    mutex_lock(&semMut);

    /* keep numberCommands in boundaries */
    if (numberCommands == MAX_COMMANDS) numberCommands = 0;

    strcpy(inputCommands[numberCommands++], data);

    // keep numberCommands in boundarys
    if(numberCommands==MAX_COMMANDS)numberCommands=0;

    mutex_unlock(&semMut);
    post_sem(&semcons);
}

char* removeCommand() {
    wait_sem(&semcons);
    mutex_lock(&semMut);

    char *data = (char*) malloc(sizeof(char) * (strlen(inputCommands[0]) + 1));
    strcpy(data, inputCommands[head]);

    // keep head in boundarys
    if(++head==MAX_COMMANDS)head=0;
    // if there are no more commands to read, set the null command
    if(numberCommands==head)
        strcpy(inputCommands[head],"f");
    

    mutex_unlock(&semMut);
    post_sem(&semprod);

    return data;
}

void errorParse(int lineNumber) {
    fprintf(stderr, "Error: line %d invalid\n", lineNumber);
    exit(EXIT_FAILURE);
}

void * processInput() {
    FILE* inputFile;

    inputFile = fopen(global_inputFile, "r");
    if (!inputFile) {
        fprintf(stderr, "Error: Could not read %s\n", global_inputFile);
        exit(EXIT_FAILURE);
    }

    char line[MAX_INPUT_SIZE];
    int lineNumber = 0;

    while(fgets(line, sizeof(line)/sizeof(char), inputFile)) {
        char token;
        char name[MAX_INPUT_SIZE];
        char name1[MAX_INPUT_SIZE];
        char name2[MAX_INPUT_SIZE];

        lineNumber++;
        int rTokens = 0;
        int numTokens = sscanf(line, "%c %s", &token, name);

        /* perform minimal validation */
        if (numTokens < 1) {
            continue;
        }

        switch (token) {
            case 'c':
            case 'l':
            case 'd':
                if (numTokens != 2)
                    errorParse(lineNumber);

                insertCommand(line);

                break;
            case 'r':   /* special case for 'r' (3 tokens allowed) */
                rTokens = sscanf(name,"%s %s",name1,name2);
                if (numTokens + rTokens != 3)
                    errorParse(lineNumber);

                insertCommand(line);

                break;
            case '#':
                break;
            default: { /* error */
                errorParse(lineNumber);
            }
        }
    }

    /* process input ended, signal kill flag */
    mutex_lock(&commandsLock);
    kill = 1;
    mutex_unlock(&commandsLock);

    fclose(inputFile);

    return NULL;
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

void applyCommands(char* inputCommands) {
    mutex_lock(&commandsLock);
    char command[MAX_INPUT_SIZE];
    strcpy(command,inputCommands);
    char token = command[0];
    char name[MAX_INPUT_SIZE],name2[MAX_INPUT_SIZE];
    int iNumber;
    switch (token) {
        case 'c':
            sscanf(command, "%c %s", &token, name);
            
            iNumber = obtainNewInumber(fs);
            
            mutex_unlock(&commandsLock);

            create(fs, name, iNumber);

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

            iNumber = lookup(fs,name);
            if (!iNumber)
                printf("%s not found\n", name);
            else
                delete(fs, name);

            break;
        case 'r':
            sscanf(command,"%c %s %s", &token, name, name2);
            
            mutex_unlock(&commandsLock);

            // Verificate if booth file names are in use                   
            iNumber = lookup(fs, name);
            int exists = lookup(fs, name2);
            if (!iNumber)
                printf("%s not found\n", name);
            else if (exists)
                printf("%s already exists\n", name2);
            //Rename it
            else
                renameFile(fs, name, name2, iNumber);

            break;
        case 'f':
            //do nothing
            mutex_unlock(&commandsLock);

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

}

void* trata_cliente(void* sock){
    struct threadArg* tsockfd = (struct threadArg*)sock;
    int sockfd= tsockfd->newSockfd;
    int len;
    char buffer[100];
    while(read(sockfd,buffer,100) > 0){
        
        printf("Mensagem recebida:%s\n",buffer);
        applyCommands(buffer);
        printf("terminou\n");
        print_tecnicofs_tree(stdout, fs);
        if((write(sockfd,"Boa Noite",10))<0)
            perror("Erro no Write Server"); 
    }
}

int main(int argc, char* argv[]) {
    int novosockfd, dim_cli, dim_serv,i=0;
    struct sockaddr_un end_cli;

    parseArgs(argc, argv);
    
    mutex_init(&semMut);
    init_sem(&semprod, MAX_COMMANDS);
    init_sem(&semcons, 0);
    mutex_init(&commandsLock);

    FILE * outputFp = openOutputFile();
    fs = new_tecnicofs();


    mount(UNIXSTR_PATH);

    while(1){
        dim_cli = sizeof(end_cli);
        connections[num_connects]= (struct threadArg*)malloc(sizeof(int)*2);
        connections[num_connects]->newSockfd=accept(sockfd,(struct sockaddr *)&end_cli,&dim_cli);
        connections[num_connects]->uID=0;
        if (novosockfd<0)
            perror("Erro ao aceitar socket cliente");
        pthread_create(&tid[i++],NULL,trata_cliente,(void*)connections[num_connects++]);
    }

    
    close(novosockfd);

    print_tecnicofs_tree(outputFp, fs);
    fflush(outputFp);
    fclose(outputFp);

    mutex_destroy(&commandsLock);
    destroy_sem(&semcons);
    destroy_sem(&semprod);
    mutex_destroy(&semMut);

    free_tecnicofs(fs);

    exit(EXIT_SUCCESS);
}
