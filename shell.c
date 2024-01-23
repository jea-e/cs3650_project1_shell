#include "tokenizer.h"
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

const int MAX_LINE_SIZE = 1024;

void freeTokens(char **tokens) {
  if (tokens) {
    // Free tokens memory
    for (int i = 0; tokens[i] != NULL; i++) {
      free(tokens[i]);
    }
    free(tokens);
  }
}

int getTokenListLen(char **tokens) {
  int n = 0;
  while (tokens[n] != NULL) {
    n++;
  }
  return n;
}

// create a new NULL-terminated list of tokens consits of [tokens[s] ..,
// tokens[e - 1], NULL] Note that the item with index 'e' is not copied.
// where 's' is the starting index and 'count' is number of tokens to
// copy.
char **copyTokens(char **tokens, int s, int count) {
  int len = count + 1; // to include space for NULL
  char **new_list = (char **)calloc(len, sizeof(char *));
  for (int i = 0; i < count; i++) {
    new_list[i] = (char *)calloc(strlen(tokens[s + i]), sizeof(char));
    strcpy(new_list[i], tokens[s + i]);
  }
  new_list[count] = NULL; // last item has to be NULL
  return new_list;
}

// for debugging
void printTokens(char **tokens, int start, int end) {
  for (int i = start; i <= end; i++) {
    if (tokens[i])
      printf("%s ", tokens[i]);
    else
      printf("NULL");
  }
  printf("\n");
}

void doHelp() {
  printf("cd : change the working directory to the path specified\nsource : "
         "executes the script specified in the argument\nprev : prints the "
         "previous command line and executes it again\nhelp : explains all the "
         "built-in commands\n");
}

void doCd(char **tokens) {
  if (tokens[1] == NULL) {
    const char *home_directory = getenv("HOME");
    if (chdir(home_directory) == 0) {
      // printf("directory changed to %s\n", home_directory);
      return;
    }
  } else {
    if (chdir(tokens[1]) == 0) {
      // printf("directory changed to %s\n", tokens[1]);
    } else {
      printf("directory not found: %s\n", tokens[1]);
      return;
    }
  }
}

void executeSimpleCommand(char **tokens, int in_fd, int out_fd) {
  // fork child process
  pid_t pid = fork();

  if (pid == -1) {
    perror("fork");
  } else if (pid == 0) {
    // This code is executed by the child process
    if (in_fd != STDIN_FILENO) {
      if (dup2(in_fd, STDIN_FILENO) < 0) {
        perror("dup2");
        exit(0);
      }
      close(in_fd);
    }
    if (out_fd != STDOUT_FILENO) {
      if (dup2(out_fd, STDOUT_FILENO) < 0) {
        perror("dup2");
        exit(0);
      }
      close(out_fd);
    }
    execvp(tokens[0], tokens);
    // will be here ONLY execvp failed
    if (errno == 2) { // File not found
      printf("%s: command not found\n", tokens[0]);
    }
    exit(0);
  } else { // parent process
    int status;
    wait(&status); // wait child to finish
  }
}

void processRedirectCommand(char **tokens) {
  // handle redirect if needed
  int n = getTokenListLen(tokens);
  char **simple_command = NULL;
  int in_fd = STDIN_FILENO;
  int out_fd = STDOUT_FILENO;

  if (n > 2) {
    // echo "1 2 3" > num.txt
    // check redirect output
    if (strcmp(tokens[n - 2], ">") == 0) {
      int fd = open(tokens[n - 1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
      if (fd < 0) {
        // unable to open output file
        perror("open");
      }
      out_fd = fd;
      // last two tokens are no use anymore
      simple_command = copyTokens(tokens, 0, n - 2);
    } else if (strcmp(tokens[n - 2], "<") == 0) {
      int fd = open(tokens[n - 1], O_RDONLY);
      if (fd < 0) {
        perror("open");
      }
      in_fd = fd;
      // last two tokens are no use anymore
      simple_command = copyTokens(tokens, 0, n - 2);
    }
  }
  if (simple_command == NULL) {
    simple_command = copyTokens(tokens, 0, n);
  }

  // Execute the simple command
  executeSimpleCommand(simple_command, in_fd, out_fd);
  freeTokens(simple_command);
}

void processPipeCommand(char **tokens) {
  int n = getTokenListLen(tokens);
  // command_a | command_b | command_c
  // running commands in order and create pipe
  // to connect the output of previous command to the next
  int pipe_at = -1;
  for (int i = 0; i < n; i++) {
    if (strcmp(tokens[i], "|") == 0) {
      pipe_at = i;
      break;
    }
  }

  if (pipe_at == -1) {
    processRedirectCommand(tokens);
    return;
  }

  // split the tokens into two parts:
  // before_pipe and after_pipe.
  // Note that the after_pipe command could have pipe as well
  char **command1 = copyTokens(tokens, 0, pipe_at - 0);
  int next_index = pipe_at + 1;
  char **command2 = copyTokens(tokens, next_index, n - next_index);

  int pipefds[2];
  if (pipe(pipefds) == -1) {
    perror("Pipe creation failed");
    exit(1);
  }

  pid_t child1, child2;
  if ((child1 = fork()) == -1) {
    perror("Fork 1 failed");
    exit(1);
  }
  if (child1 == 0) {
    close(pipefds[0]);
    dup2(pipefds[1],
         STDOUT_FILENO);
    processRedirectCommand(command1);

    exit(1);
  }

  if ((child2 = fork()) == -1) {
    perror("Fork 2 failed");
    exit(1);
  }

  if (child2 == 0) {
    close(pipefds[1]);
    dup2(pipefds[0],
         STDIN_FILENO);
    processRedirectCommand(command2);

    exit(1);
  }

  close(pipefds[0]);
  close(pipefds[1]);

  waitpid(child1, NULL, 0);
  waitpid(child2, NULL, 0);

  freeTokens(command1);
  freeTokens(command2);
}

void processSequence(char **tokens) {
  // handle redirect if needed
  int token_len = getTokenListLen(tokens);
  int seq_start = -1;
  for (int i = 0; i < token_len; i++) {
    if (strcmp(tokens[i], ";") == 0) {
      // terminate a sequence; need execute it
      char **sequence = copyTokens(tokens, seq_start, i - seq_start);
      processPipeCommand(sequence);
      freeTokens(sequence);
      seq_start = -1; // mark for sequence being processed
    } else {
      if (seq_start < 0) {
        seq_start = i;
      }
    }
  } // end for loop
  if (seq_start >= 0) {
    char **sequence = copyTokens(tokens, seq_start, token_len - seq_start);
    processPipeCommand(sequence);
    freeTokens(sequence);
  }
}

void doPrev(char **tokens) {
  if (tokens) {
    processSequence(tokens);
  }
}

void doSource(char **tokens) {
  if (tokens[1] == NULL) {
    printf("failed to open source file %s\n", tokens[1]);
    return;
  }

  // Open and read source file
  FILE *file = fopen(tokens[1], "r");
  if (file == NULL) {
    printf("please enter a source file");
    return;
  }

  char input[MAX_LINE_SIZE];

  // Read and execute each line of source file
  while (fgets(input, sizeof(input), file)) {
    char **lineTokens = tokenizer(input);
    if (lineTokens[0] == NULL) {
      freeTokens(lineTokens);
      continue;
    }
    processSequence(lineTokens);
    freeTokens(lineTokens);
  }
  fclose(file);
}

int main(int argc, char **argv) {
  char input[MAX_LINE_SIZE]; // max 1024 characters in one line
  bool run = true;

  char *sequence[MAX_LINE_SIZE];
  char **prevCommand = NULL;

  printf("Welcome to mini-shell.\n");

  while (true) {
    printf("shell $ ");
    fflush(stdout);
    if (fgets(input, sizeof(input), stdin)) {
      char **tokens = tokenizer(input);
      int token_len = getTokenListLen(tokens);

      // if user inputs newline only
      if (tokens[0] == NULL) {
        freeTokens(tokens);
        continue;
      }

      // Copy tokens to prevCommand when it is not 'prev'
      if (strcmp(tokens[0], "prev") != 0) {
        freeTokens(prevCommand);
        prevCommand = copyTokens(tokens, 0, token_len);
      }

      if (strcmp(tokens[0], "exit") == 0) {
        // Free tokens memory
        freeTokens(tokens);
        freeTokens(prevCommand);
        printf("Bye bye.\n");
        return 0;
      } else if (strcmp(tokens[0], "help") == 0) {
        doHelp();
        freeTokens(tokens);
        continue;
      } else if (strcmp(tokens[0], "cd") == 0) {
        doCd(tokens);
        freeTokens(tokens);
        continue;
      } else if (strcmp(tokens[0], "source") == 0) {
        doSource(tokens);
        freeTokens(tokens);
        continue;
      } else if (strcmp(tokens[0], "prev") == 0) {
        doPrev(prevCommand);
        freeTokens(tokens);
        continue;
      }

      processSequence(tokens);
      // Free tokens memory
      freeTokens(tokens);
    }
    if (feof(stdin)) {
      printf("\n");
      break;
    }
  } // end while
  freeTokens(prevCommand);

  printf("Bye bye.\n");
  return 0;
}
