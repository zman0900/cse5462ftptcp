#include <stdio.h>

#include "common.h"

int main(int argc, char *argv[]) {
    printf("Starting client\n");
    if (argc != 3) {
        printf("Usage: %s <address> <port>\n", argv[0]);
        return -1;
    }

    return 0;
}
