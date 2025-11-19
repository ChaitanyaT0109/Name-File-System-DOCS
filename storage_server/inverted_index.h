/*
 * storage_server/inverted_index.h
 * Inverted Index for content search - maps words to files
 */

#ifndef INVERTED_INDEX_H
#define INVERTED_INDEX_H

#include "protocol.h"

#define MAX_WORD_LEN 100
#define MAX_FILES_PER_WORD 50
#define INDEX_SIZE 2003  // Prime number for hash table

// File list for a word
typedef struct {
    char filenames[MAX_FILES_PER_WORD][MAX_FILENAME_LEN];
    int file_count;
} FileList;

// Index entry (word -> file list)
typedef struct IndexNode {
    char word[MAX_WORD_LEN];
    FileList files;
    struct IndexNode* next;
} IndexNode;

// Inverted index (hash table)
typedef struct {
    IndexNode* table[INDEX_SIZE];
} InvertedIndex;

// Initialize index
void index_init(InvertedIndex* index);

// Add a word-file mapping
int index_add_word(InvertedIndex* index, const char* word, const char* filename);

// Remove all entries for a file
int index_remove_file(InvertedIndex* index, const char* filename);

// Search for files containing a word
int index_search(InvertedIndex* index, const char* word, char result[MAX_CONTENT_LEN]);

// Index an entire file (parse and add all words)
int index_file(InvertedIndex* index, const char* filename, const char* content);

// Destroy index
void index_destroy(InvertedIndex* index);

#endif // INVERTED_INDEX_H
