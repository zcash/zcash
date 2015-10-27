#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

// Allocate 10GB
#define MAX_MEMORY (10L*1024*1024*1024)
// ... in 100MB steps.
#define STEP (100*1024*1024)

int main(int argc, char **argv) {
    unsigned long long allocated = 0;
    unsigned char *x;
    int i;
    while (allocated < MAX_MEMORY) {
        x = malloc(STEP);
        if (x == NULL) {
            printf("null result!\n");
        }
        for (i = 0; i < STEP; i++) {
            x[i] = i;
        }
        allocated += STEP;
        sleep(1);
        printf("Allocated %llu\n", allocated);
    }

    while(1) {
        printf("Waiting to die...\n");
        sleep(1);
    }
}
