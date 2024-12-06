#include <stdlib.h>
#include <string.h>

int atoi(const char *nptr) {
    return (int)strtol(nptr, (char **)NULL, 10);
}