/* Sistemas Operativos, DEI/IST/ULisboa 2019-20 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <semaphore.h>
#include "fs.h"
#include "constants.h"
#include "lib/timer.h"
#include "sync.h"

char* global_inputFile = NULL;
char* global_outputFile = NULL;
int numberThreads = 0;
int numBuckets = 0;
pthread_mutex_t commandsLock,semMut;
sem_t semprod, semcons;
tecnicofs* fs;

char inputCommands[MAX_COMMANDS][MAX_INPUT_SIZE];
int numberCommands = 0;
int headQueue = 0;
int kill=0;

void shift_left(char array[MAX_COMMANDS][MAX_INPUT_SIZE],int size){
    for(int i=0;i<size;++i){
        strcpy(array[i],array[i+1]);
    }
}

static void displayUsage (const char* appName){
    printf("Usage: %s input_filepath output_filepath threads_number buckets_number\n", appName);
    exit(EXIT_FAILURE);
}

static void parseArgs (long argc, char* const argv[]){
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
    strcpy(inputCommands[numberCommands++], data);
    mutex_unlock(&semMut);
    post_sem(&semcons);
}

void removeCommand(char* data) {
    wait_sem(&semcons);
    mutex_lock(&semMut);
    strcpy(data,inputCommands[0]);
    numberCommands--;
    shift_left(inputCommands, numberCommands);
    mutex_unlock(&semMut);
    post_sem(&semprod);
    return ;
}

void errorParse(int lineNumber){
    fprintf(stderr, "Error: line %d invalid\n", lineNumber);
    exit(EXIT_FAILURE);
}

void * processInput(){
    FILE* inputFile;
    inputFile = fopen(global_inputFile, "r");
    if(!inputFile){
        fprintf(stderr, "Error: Could not read %s\n", global_inputFile);
        exit(EXIT_FAILURE);
    }
    char line[MAX_INPUT_SIZE];
    int lineNumber = 0;
    while(fgets(line, sizeof(line)/sizeof(char), inputFile)){
        char token;
        char name[MAX_INPUT_SIZE];
        lineNumber++;

        int numTokens = sscanf(line, "%c %s", &token, name);

        /* perform minimal validation */
        if (numTokens < 1) {
            continue;
        }
        switch (token) {
            case 'c':
            case 'l':
            case 'd':
                if(numTokens != 2)
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
    mutex_lock(&commandsLock);
    kill=1;
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

void * applyCommands(){
    while(1){
        mutex_lock(&commandsLock);
        if(kill!=1){
            char* command=NULL;
            removeCommand(command);
            if (command == NULL){
                mutex_unlock(&commandsLock);
                continue;
            }
            char token;
            char name[MAX_INPUT_SIZE];
            sscanf(command, "%c %s", &token, name);

            int iNumber;
            switch (token) {
                case 'c':
                    iNumber = obtainNewInumber(fs);
                    mutex_unlock(&commandsLock);
                    create(fs, name, iNumber);
                    break;
                case 'l':
                    mutex_unlock(&commandsLock);
                    int searchResult = lookup(fs, name);
                    if(!searchResult)
                        printf("%s not found\n", name);
                    else
                        printf("%s found with inumber %d\n", 
                                name, searchResult);
                    break;
                case 'd':
                    mutex_unlock(&commandsLock);
                    delete(fs, name);
                    break;
                default: { /* error */
                    mutex_unlock(&commandsLock);
                    fprintf(stderr, "Error: commands to apply\n");
                    exit(EXIT_FAILURE);
                }
            }
        }else{
            mutex_unlock(&commandsLock);
            return NULL;
        }
    }
}

void runThreads(FILE* timeFp){
    TIMER_T startTime, stopTime;
    #if defined (RWLOCK) || defined (MUTEX)
        pthread_t writer;
        pthread_t* workers =(pthread_t*) malloc(numberThreads * sizeof(pthread_t));
    #endif

    TIMER_READ(startTime);
    #if defined (RWLOCK) || defined (MUTEX)
        if(pthread_create(&writer,NULL,processInput,NULL)!=0){
            perror("Can't create thread");
            exit(EXIT_FAILURE);
        }
        for(int i = 0; i < numberThreads; i++){
            int err = pthread_create(&workers[i], NULL, applyCommands, NULL);
            if (err != 0){
                perror("Can't create thread");
                exit(EXIT_FAILURE);
            }
        }
        if(pthread_join(writer, NULL)) {
            perror("Can't join thread");
        }
        for(int i = 0; i < numberThreads; i++) {
            if(pthread_join(workers[i], NULL)) {
                perror("Can't join thread");
            }
        }
    #else
        processInput();
        applyCommands();
    #endif
    TIMER_READ(stopTime);
    fprintf(timeFp, "TecnicoFS completed in %.4f seconds.\n",
            TIMER_DIFF_SECONDS(startTime, stopTime));
    #if defined (RWLOCK) || defined (MUTEX)
        free(workers);
    #endif
}

int main(int argc, char* argv[]) {
    parseArgs(argc, argv);
    mutex_init(&semMut);
    sem_init(&semprod,0,MAX_COMMANDS);
    sem_init(&semcons,0,0);
    mutex_init(&commandsLock);
    FILE * outputFp = openOutputFile();
    fs = new_tecnicofs();

    runThreads(stdout);
    print_tecnicofs_tree(outputFp, fs);
    fflush(outputFp);
    fclose(outputFp);

    mutex_destroy(&commandsLock);
    free_tecnicofs(fs);
    exit(EXIT_SUCCESS);
}
