#pragma once

// A way to make a dynamic array of strings
typedef struct {
    char **items;
    int count;
    int capacity;
} stringArray;

/*
    Splits the provided string based on the provided delimeter

    params
        [char *] string - the string to be split
        [char *] delimeter - the delimeter used to split the string
        (out)[stringArray *] array - an empty stringArray struct

    returns
        [int] - 0 is success, 1 is an error occurred
*/
int split(char *string, char *delimeter, stringArray *array);