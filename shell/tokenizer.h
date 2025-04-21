#ifndef TOKENIZER_H_
#define TOKENIZER_H_

#include <stdlib.h>

/* A struct that represents a list of words. */
struct tokens;

/* Turn a string into a list of words. */
struct tokens *tokenize(const char *line);

/* How many words are there? */
size_t tokens_get_length(struct tokens *tokens);

/* Get me the Nth word (zero-indexed) */
char *tokens_get_token(struct tokens *tokens, size_t n);

/* Free the memory */
void tokens_destroy(struct tokens *tokens);

/* change token to another */
void tokens_set_token(struct tokens *tokens, size_t old_index, char *new_token);

/* set the length of the tokens */
void tokens_set_length(struct tokens *tokens, size_t new_length);

#endif
