#include <stdio.h>
#include <stdlib.h>

void runIdle() {
    TracePrintf(0, "Running Idle...\n");
    while (1) {
        Pause();
    }
}

int main(int argc, char *argv[]) {
    runIdle();
    return 0;
}
