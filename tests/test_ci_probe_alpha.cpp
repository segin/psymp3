// Temporary CI probe (branch-only, never merged): fails deliberately so the
// failure-collection behavior of the workflow can be observed end to end.
#include <cstdio>
int main() {
    printf("CI probe alpha: failing deliberately\n");
    return 1;
}
