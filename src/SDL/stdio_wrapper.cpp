// Minimal stdio implementation for Saturn
// Provides only essential type definitions and exit() function

#include "stdio.h"
#include <string.h>

// Exit function (stub for Saturn)
void exit(int status) {
    (void)status;
    // On Saturn, we can't actually exit, just return
    // The game would typically reset or loop
}
