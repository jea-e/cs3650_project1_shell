#include "tokenizer.h"
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
  char input[1000] = {0};
  char **tokens = NULL;
  // get a line from stdin
  while (fgets(input, sizeof(input), stdin)) {
    tokens = tokenizer(input);
    // print tokens
    for (int i = 0; tokens[i] != NULL; i++) {
      char *token = tokens[i];
      printf("%s\n", token);
    }
    // free all memory
    for (int i = 0; tokens[i] != NULL; i++) {
      char *token = tokens[i];
      // free the memory of each token
      free(token);
    }
    // free the memory of the tokens list
    free(tokens);
  }
  return 0;
}
