#include <stdio.h>
#include "kernel.h"
int main() {
    int i;
    fflush(stdout);
    while (1){ 
        fflush(stdout);
        Pause();
    }
    return 0;
}