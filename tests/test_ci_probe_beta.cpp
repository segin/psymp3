// Temporary CI probe (branch-only, never merged): second deliberate failure
// so the run must report MULTIPLE failures, not just the first.
#include <cstdio>
int main() {
    printf("CI probe beta: also failing deliberately\n");
    return 1;
}
