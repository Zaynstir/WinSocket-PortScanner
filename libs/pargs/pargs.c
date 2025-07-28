#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "pargs.h"

void freePargArray(pargArray *pargs) {
    free(pargs->items);
    free(pargs);
}

int countPargOptions(pargOption *options) {
    int i = 0;
    while(options[i++].name) {}
    return i;
}

int findOption(char *flag, pargOption *options, pargOption **option) {
    // Keep track of whether flag is a full name or shorthand
    int isFullName = (strncmp("--", flag, 2) == 0);
    char *truncatedFlag = isFullName ? flag + 2 : flag + 1;

    // getting count of pargOptions so that we don't need to pass it as a function
    int optionCount = countPargOptions(options);

    // storing the length of string to prevent constant calls
    size_t flagLength = strlen(truncatedFlag);
    
    // find pargOption that matches the supplied flag
    for(int i = 0; i < optionCount; i++) {
        *option = (options+i);
        if(isFullName) {
            // if flag.len and name.len aren't equal, then we can assume this isn't the flag 
            if(flagLength != strlen((*option)->name)) continue;
            if(strncmp((*option)->name, truncatedFlag, flagLength) == 0) return 0;
        } else {
            // if flag.len and shorthand.len aren't equal, then we can assume this isn't the flag 
            if(flagLength != strlen((*option)->shorthand)) continue;
            if(strncmp((*option)->shorthand, truncatedFlag, flagLength) == 0) return 0;
        }
    }
    return 1;
}

pargArray* parseArgs(int argc, char *argv[], pargOption *options) {
    // initialize pargArray and properties
    pargArray *pargs = malloc(sizeof(pargArray));
    if(pargs == NULL) {
        printf("Err: Failed to allocate pargArray");
        exit(EXIT_FAILURE);
    }

    pargs->count = 0;
    pargs->items = malloc(argc * sizeof(kvPair));
    if(pargs->items == NULL){
        printf("Err: Failed to allocation pargs->items");
        free(pargs);
        exit(EXIT_FAILURE);
    }

    // loop through each supplied argument and parse it accordingly
    for(int i = 1; i < argc; i++) {
        char *arg = argv[i];
        
        int len = strlen(arg);
        
        // if invalid argument through error
        if(arg[0] != '-' || len == 1) {
            freePargArray(pargs);
            printf("Err: '%s' is not a valid flag\n", arg);
            exit(EXIT_FAILURE);
        }

        // find argOption, otherwise we can assume user supplied invalid argument
        pargOption *option;
        if(findOption(arg, options, &option)) {
            free(options);
            freePargArray(pargs);
            printf("Err: '%s' is not an available flag\n", arg);
            exit(EXIT_FAILURE);
        }

        // variable reference for ease
        kvPair *pair = &(pargs->items[pargs->count++]);
        pair->key = option->name;
        
        // if pargOption doesn't require value, then set to null
        if(option->flags & NO_VALUE) {
            pair->value = NULL;
        } 
        // set the next argument as the value for the kvPair and increment i so that we can read the next argument.
        else {
            if(i + 1 >= argc) {
                free(options);
                freePargArray(pargs);
                printf("Err: Expected value after: %s", arg);
                exit(EXIT_FAILURE);
            }
            pair->value = argv[++i];
        }
    }

    return pargs;
}

void printPargs(pargArray *pargs) {
    for(int i = 0; i < pargs->count; i++) {
        kvPair pair = pargs->items[i];
        printf("Flag: '%s', Val: '%s'\n", pair.key, pair.value);
    }
}

char* getValue(char* name, pargArray *pargs) {
    for(int i = 0; i < pargs->count; i++) {
        if(strncmp(pargs->items[i].key, name, strlen(pargs->items[i].key)) == 0) {
            return pargs->items[i].value;
        }
    }
    return NULL;
}