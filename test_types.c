#include <stdio.h>
#include <sys/types.h>
#include <limits.h>
int main() { 
    printf("sizeof(off_t) = %zu\n", sizeof(off_t));
    printf("sizeof(long) = %zu\n", sizeof(long)); 
    printf("LONG_MAX = %ld\n", LONG_MAX);
    return 0; 
}
