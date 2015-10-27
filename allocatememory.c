#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

size_t log2mine(size_t n)
/* returns ceil(log2(n)), so 1ul<<log2(n) is the smallest power of 2,
   that is not less than n. */
{
    size_t r = ((n & (n-1)) == 0 ? 0 : 1); // add 1 if n is not power of 2

    while (n > 1)
    {
        n >>= 1;
        r++;
    }

    return r;
}

int main(int argc, char **argv) {
    size_t i = 0;
    size_t j = 0;
    for(i = 0; i < 1000; i++) {
        j += log2mine(i);
    }
    return j;
}
