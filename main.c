#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include <ctype.h>
#include <pthread.h>
#include <sys/time.h>
#include "fs.h"

#define MAX_COMMANDS 150000
#define MAX_INPUT_SIZE 100

#if defined (RWLOCK) || defined (MUTEX)
    pthread_mutex_t commandMut;
#endif

int numberThreads = 0;
tecnicofs* fs;

char inputCommands[MAX_COMMANDS][MAX_INPUT_SIZE];
int numberCommands = 0;
int headQueue = 0;
int numBuckets= 0;
char fileLocation[MAX_INPUT_SIZE];

/*_________________________Multithreading_Aux_Functions_________________________*/

void initCommandLock(){
    #if defined (RWLOCK) || defined (MUTEX)
    	if(pthread_mutex_init(&commandMut, NULL) != 0){
            fprintf(stderr, "Error: Could not init mutex.\n");
            exit(EXIT_FAILURE);
        }
	#endif
}

void lockCommand() {
    #if defined (RWLOCK) || defined (MUTEX)
        if(pthread_mutex_lock(&commandMut) != 0) {
            fprintf(stderr, "Error: Could not lock mutex\n");
            exit(EXIT_FAILURE);
        }
    #endif
}

void unlockCommand() {
    #if defined (RWLOCK) || defined (MUTEX)
        if(pthread_mutex_unlock(&commandMut) != 0) {
            fprintf(stderr, "Error: Could not unlock mutex\n");
            exit(EXIT_FAILURE);
        }
    #endif
}

void destroyCommandLock(){
    #if defined (RWLOCK) || defined (MUTEX)
        if(pthread_mutex_destroy(&commandMut)!= 0) {
            fprintf(stderr, "Error: Could not destroy mutex\n");
            exit(EXIT_FAILURE);
        }
	#endif
}

/*_________________________End_Of_Aux_Functions_________________________*/

static void displayUsage (const char* appName){
    printf("Usage: %s\n", appName);
    exit(EXIT_FAILURE);
}

static void parseArgs (long argc, char* const argv[]){
    if (argc != 5) {
        fprintf(stderr, "Invalid format:\n");
        displayUsage(argv[0]);
    }
    strcpy(fileLocation,argv[1]);

    numberThreads= atoi(argv[3]);
    if(numberThreads<=0){
        fprintf(stderr,"Invalid number of threads.\n");
        displayUsage(argv[0]);
    }

    numBuckets= atoi(argv[4]);
    if(numBuckets<=0){
        fprintf(stderr,"Invalid number of buckets.\n");
        displayUsage(argv[0]);
    }
}

int insertCommand(char* data) {
    if(numberCommands != MAX_COMMANDS) {
        strcpy(inputCommands[numberCommands++], data);
        return 1;
    }
    return 0;
}

char* removeCommand() {
    if((numberCommands > 0)){
        numberCommands--;
        return inputCommands[headQueue++];
    }
    return NULL;
}

void errorParse(){
    fprintf(stderr, "Error: command invalid\n");
    //exit(EXIT_FAILURE);
}

void processInput(){
    FILE *file = fopen(fileLocation,"r");
    char line[MAX_INPUT_SIZE];

    while (fgets(line, sizeof(line)/sizeof(char), file)) {
        char token;
        char name[MAX_INPUT_SIZE];

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
                    errorParse();
                if(insertCommand(line))
                    break;
                return;
            case '#':
                break;
            default: { /* error */
                errorParse();
            }
        }
    }
    fclose(file);
}

void * applyCommands(){
    while(1) {
        lockCommand();
        if(numberCommands > 0) {

            const char* command = removeCommand();

            if (command == NULL) {
                unlockCommand();
                continue;
            }

            char token;
            char name[MAX_INPUT_SIZE];
            int numTokens = sscanf(command, "%c %s", &token, name);

            if (numTokens != 2) {
                fprintf(stderr, "Error: invalid command in Queue\n");
                exit(EXIT_FAILURE);
            }

            int searchResult;
            int iNumber;
            switch (token) {
                case 'c':
                    iNumber = obtainNewInumber(fs);

                    unlockCommand();

                    create(fs, name, iNumber);

                    break;
                case 'l':
                    unlockCommand();

                    searchResult = lookup(fs, name);
                    if(!searchResult)
                        printf("%s not found\n", name);
                    else
                        printf("%s found with inumber %d\n", name, searchResult);

                    break;
                case 'd':
                    unlockCommand();

                    delete(fs, name);

                    break;
                default: { /* error */
                    unlockCommand();

                    fprintf(stderr, "Error: command to apply\n");
                    exit(EXIT_FAILURE);
                }
            }
        } else {
            unlockCommand();
            return NULL;
        }
    }
}

void runThreads(int numberThreads){
    struct timeval start, finish;
    double elapsed;
    int i = 0;
    pthread_t threads[numberThreads];

    if(gettimeofday(&start, NULL) != 0){
        fprintf(stderr, "Error: gettimeofday failed.\n");
        exit(EXIT_FAILURE);
    }

    for(i = 0;i < numberThreads; ++i){
        if (pthread_create(&threads[i], 0, applyCommands, NULL) != 0){
            fprintf(stderr, "Error: Could not create thread\n");
            exit(EXIT_FAILURE);
        }
    }
    for (i = 0; i < numberThreads; ++i) {
        if (pthread_join(threads[i], NULL) != 0) {
            fprintf(stderr, "Error: Could not join threads\n");
        }
    }

    if(gettimeofday(&finish, NULL) != 0){
        fprintf(stderr, "Error: gettimeofday failed.\n");
        exit(EXIT_FAILURE);
    }

    elapsed = (double)(finish.tv_sec) + (double)(finish.tv_usec / 1000000.0) - \
            (double)(start.tv_sec) + (double)(start.tv_usec / 1000000.0);
    printf("TecnicoFS completed in %.4f seconds.\n",elapsed);
}

int main(int argc, char* argv[]) {
    FILE *out;

    initCommandLock();
    parseArgs(argc, argv);
    if((out = fopen(argv[2],"w")) == NULL){
        fprintf(stderr,"ERROR: Couldn't open file.\n");
        exit(EXIT_FAILURE);
    }
    fs = new_tecnicofs();
    processInput();

    runThreads(numberThreads);

    print_tecnicofs(out, fs);
    fclose(out);
    destroyCommandLock();
    free_tecnicofs(fs);

    exit(EXIT_SUCCESS);
}
