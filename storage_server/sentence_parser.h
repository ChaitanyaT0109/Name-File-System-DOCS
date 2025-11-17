/*
 * storage_server/sentence_parser.h
 * 
 * Utility functions for parsing files into sentences
 * Sentences are delimited by . ! ? characters (EVERY occurrence)
 */

#ifndef SENTENCE_PARSER_H
#define SENTENCE_PARSER_H

#include <stddef.h>

#define MAX_SENTENCES 10000
#define MAX_WORDS_PER_SENTENCE 1000

// Parse file content into sentences (splits on . ! ?)
// Returns number of sentences, fills sentences array
// Each sentence is null-terminated
int parse_sentences(const char* content, char*** sentences_out);

// Free sentences array
void free_sentences(char** sentences, int count);

// Parse sentence into words
// Returns number of words, fills words array
int parse_words(const char* sentence, char*** words_out);

// Free words array
void free_words(char** words, int count);

// Insert word at index in sentence (modifies sentence)
// Returns new sentence (caller must free)
char* insert_word(const char* sentence, int word_idx, const char* word);

// Rebuild file from sentences array
// Returns new content (caller must free)
char* rebuild_file(char** sentences, int count);

// Check if string contains sentence delimiter
int has_delimiter(const char* str);

// Split string by delimiters into multiple parts
// Returns number of parts
int split_by_delimiters(const char* str, char*** parts_out);

#endif
