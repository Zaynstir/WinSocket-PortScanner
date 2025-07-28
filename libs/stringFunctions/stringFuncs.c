#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "stringFuncs.h"

int split(char *string, char *delimeter, stringArray *array) {
    int capacity = 5;
    int count = 0;

    char **items = malloc(array->capacity * sizeof(char *));
    if(!items) {
        return 1;
    }

    char *s = strdup(string);

    for(char *token = strtok(s, delimeter); token != NULL; token = strtok(NULL, delimeter)) {
        if(count == capacity) {
            capacity *= 2;
            items = realloc(items, capacity * sizeof(char *));
        }

        items[count++] = token;
    }

    array->count = count;
    array->capacity = capacity;
    array->items = items;

    return 0;
}