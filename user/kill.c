#include <ulib.h>
#include <stdlib.h>

int main(int argc, char** argv)
{
    int i;
    printf("kill argc: %d\n", argc);
    if (argc < 2) {
        printf("usage: kill pid...\n");
        exit(1);
    }
    for (i = 1; i < argc; i++)
        kill(atoi(argv[i]));
    exit(0);
}