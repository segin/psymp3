#include <stdio.h>
#include <sys/stat.h>

int main() {
    FILE *f = fopen("/tmp/test.opus", "rb");
    if (\!f) {
        printf("Failed to open file\n");
        return 1;
    }
    
    struct stat st;
    int fd = fileno(f);
    if (fstat(fd, &st) == 0) {
        printf("fstat size: %ld (0x%lx)\n", st.st_size, st.st_size);
    }
    
    fseek(f, 0, SEEK_END);
    long pos = ftell(f);
    printf("ftell after SEEK_END: %ld (0x%lx)\n", pos, pos);
    
    fclose(f);
    return 0;
}
EOF < /dev/null
