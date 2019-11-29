/* Sistemas Operativos, DEI/IST/ULisboa 2019-20 */

#ifndef CONSTANTS_H
#define CONSTANTS_H
#define MAX_INPUT_SIZE 100
#define DELAY 5000

#if defined (RWLOCK) || defined (MUTEX)
    #define MAX_COMMANDS 10
#else
    #define MAX_COMMANDS 1500
#endif

#endif /* CONSTANTS_H */
