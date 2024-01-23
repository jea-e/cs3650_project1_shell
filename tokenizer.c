#include "tokenizer.h"
#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char **tokenizer(const char *str) {
  int length = strlen(str);
  int total_tokens = 0;
  char **tokens = (char **)calloc(
      length + 1,
      sizeof(char *)); // in worst case, we can have 'length' number of tokens
  char temp_token[length +
                  1]; // hold characters that is part of a token to be added
  int token_index = 0;
  bool in_quotes = false;
  for (int i = 0; i < length; i++) {
    char current_char = str[i];
    switch (current_char) {
    case '(':
    case ')':
    case '<':
    case '>':
    case '|':
    case ';':
    case ' ':
    case '\t':
    case '\n':
    case '\0':
      if (in_quotes) {
        temp_token[token_index] = current_char;
        token_index++;
      } else {
        // If there is someting in the temp token, we need to add it
        // into the tokens list before doing anything else.
        // This is to handle case like 'abc;'
        if (token_index > 0) {
          // there are characters in the temp token
          // Add the temp token to the tokens list
          tokens[total_tokens] = (char *)calloc(token_index + 1, 1);
          // only copy token_index number of characters
          strncpy(tokens[total_tokens], temp_token, token_index);
          token_index = 0;
          total_tokens++;
        }
        if (current_char == ' ' || current_char == '\t' ||
            current_char == '\n' || current_char == '\0') {
          // for space characters, we don't add it to tokens list
          break;
        } else {
          // allocate 2 bytes;initialized with 0
          tokens[total_tokens] = (char *)calloc(2, 1);
          // add the character to tokens list
          tokens[total_tokens][0] = current_char;
          total_tokens++;
        }
      }
      break;
    case '"':
      in_quotes = !in_quotes; // flip the flag
      break;
    default:
      // add non-space non-special character to the token
      temp_token[token_index] = current_char;
      token_index++;
      break;
    }
  }
  // add last token if there is any
  if (token_index > 0) {
    // there are characters in the temp token
    // Add the temp token to the tokens list
    tokens[total_tokens] = (char *)calloc(token_index + 1, 1);
    // only copy token_index number of characters
    strncpy(tokens[total_tokens], temp_token, token_index);
    token_index = 0;
    total_tokens++;
  }
  // Mark the end of tokens list
  tokens[total_tokens] = NULL;
  return tokens;
}
