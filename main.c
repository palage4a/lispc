#include <stdio.h>

static char buffer[2048];

int main(int argc, char** argv) {
  while(1) {
    fputs("lispy> ", stdout);
    fgets(buffer, 2048, stdin);
    printf("no you are %s", buffer);
  }

  return 0;
}

