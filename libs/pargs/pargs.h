#pragma once

// list of parg flags
typedef enum {
    NO_VALUE = 0x01,
    REQUIRED = 0x02
} pargFlags;

// A key:value pair for arguments
typedef struct {
    char *key;
    char *value;
} kvPair;

/*
    The parameters for a single argument

    [char*] name - the name of the argument
    [char*] shorthand - the shorthand/abbreviation of the name of the argument
    [char*] desc - the description of the argument
    [pargFlags] flags
*/ 
typedef struct {
    char *name;
    char *shorthand;
    char *desc;
    pargFlags flags;
} pargOption;

/*
    A simple array data structure
*/
typedef struct {
    kvPair *items;
    int count;
} pargArray;

/*
    Frees the pargArray pointer struct

    params
        [pargArray*] pargs
*/
void freePargArray(pargArray *pargs);

/*
    Counts the number of options in the provided array

    Loops through each index until it hits a NULL value. This is done so that I don't have to pass the array size.

    params
        [pargOption*] options

    returns
        [int]
*/
int countPargOptions(pargOption *options);

/*
    Looks for an pargOption that matches the provided flag and returns it to the option

    params
        [char*] flag - 
        [pargOption*] options - array of pargOptions
        OUT [pargOption**] option - empty pointer to pargOption. Will store the pargOption that matches the supplied flag

    returns
        [int]
            - 0 if successful
            - 1 if failed to find a match
*/
int findOption(char *flag, pargOption *options, pargOption **option);

/*
    This will parse the supplied cmdline arguments and parse it to match the supplied pargOptions

    params
        [int] argc
        [char**] argv 
        [pargOption*] options - an array of options supplied by the user
*/
pargArray* parseArgs(int argc, char *argv[], pargOption *options);

/*
    prints every KeyValue pair in the supplied array

    params
        [pargArray*] pargs
*/
void printPargs(pargArray *pargs);

/*
    Will provide a string that represents the supplied argument

    params
        [char*] name - the name of the flag
        [pargArray*] pargs - the array of parsed arguments

    returns
        [char*] - value that was supplied for the argument
            will return NULL if name doesn't exist
*/
char* getValue(char* name, pargArray *pargs);