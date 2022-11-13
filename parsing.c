#include <stdio.h>
#include <stdlib.h>

#include "mpc.h"

// TODO: check that code
#ifdef _WIN32
static char buffer[2048]

char* readline(char* prompt) {
  fputs(prompt, stdin);
  fgets(buffer, 2048, stdin);
  return buffer
}
#else
#include <editline/readline.h>
#endif

int main(int argc, char** argv) {
  puts("Lispy Version 0.0.0.0.1");
  puts("Press Ctrl+c to Exit");
  while(1) {
    char* input = readline("lispc> ");
    add_history(input);
    printf("no you are not %s\n", input);
    free(input);
  }

  return 0;
}

