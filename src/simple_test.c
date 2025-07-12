#include <stdio.h>
#include <sys/stat.h>

int main() {
    FILE *f = fopen("/tmp/test.opus", "rb");
    if (\!f) return 1;
    struct stat st;
    int fd = fileno(f);
    if (fstat(fd, &st) == 0) {
        printf("fstat size: %ld\n", st.st_size);
    }
    fseek(f, 0, SEEK_END);
    long pos = ftell(f);
    printf("ftell: %ld\n", pos);
    fclose(f);
    return 0;
}
