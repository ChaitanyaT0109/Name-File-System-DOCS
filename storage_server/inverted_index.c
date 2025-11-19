/*
 * storage_server/inverted_index.c
 * Inverted Index implementation for content search
 */

#include "inverted_index.h"
#include "logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// Hash function for words
static unsigned long hash_word(const char* word) {
    unsigned long hash = 5381;
    int c;
    while ((c = *word++)) {
        hash = ((hash << 5) + hash) + tolower(c);
    }
    return hash % INDEX_SIZE;
}

// Convert word to lowercase
static void to_lowercase(char* dest, const char* src) {
    int i = 0;
    while (src[i] && i < MAX_WORD_LEN - 1) {
        dest[i] = tolower(src[i]);
        i++;
    }
    dest[i] = '\0';
}

// Check if character is word separator
static int is_separator(char c) {
    return !isalnum(c);
}

void index_init(InvertedIndex* index) {
    for (int i = 0; i < INDEX_SIZE; i++) {
        index->table[i] = NULL;
    }
    log_info("Inverted index initialized with %d buckets", INDEX_SIZE);
}

int index_add_word(InvertedIndex* index, const char* word, const char* filename) {
    if (!word || !filename || strlen(word) == 0) {
        return -1;
    }
    
    // Normalize word to lowercase
    char normalized_word[MAX_WORD_LEN];
    to_lowercase(normalized_word, word);
    
    // Hash the word
    unsigned long hash = hash_word(normalized_word);
    
    // Search for existing entry
    IndexNode* current = index->table[hash];
    IndexNode* prev = NULL;
    
    while (current) {
        if (strcmp(current->word, normalized_word) == 0) {
            // Word exists, check if file already in list
            for (int i = 0; i < current->files.file_count; i++) {
                if (strcmp(current->files.filenames[i], filename) == 0) {
                    return 0;  // Already indexed
                }
            }
            
            // Add file to list
            if (current->files.file_count < MAX_FILES_PER_WORD) {
                strncpy(current->files.filenames[current->files.file_count], 
                       filename, MAX_FILENAME_LEN - 1);
                current->files.file_count++;
                return 0;
            } else {
                log_warning("Max files reached for word '%s'", normalized_word);
                return -1;
            }
        }
        prev = current;
        current = current->next;
    }
    
    // Word doesn't exist, create new entry
    IndexNode* new_node = (IndexNode*)malloc(sizeof(IndexNode));
    if (!new_node) {
        log_error("Failed to allocate memory for index node");
        return -1;
    }
    
    strncpy(new_node->word, normalized_word, MAX_WORD_LEN - 1);
    new_node->word[MAX_WORD_LEN - 1] = '\0';
    new_node->files.file_count = 1;
    strncpy(new_node->files.filenames[0], filename, MAX_FILENAME_LEN - 1);
    new_node->next = NULL;
    
    // Insert at head of chain
    if (prev == NULL) {
        new_node->next = index->table[hash];
        index->table[hash] = new_node;
    } else {
        prev->next = new_node;
    }
    
    return 0;
}

int index_remove_file(InvertedIndex* index, const char* filename) {
    int removed = 0;
    
    for (int i = 0; i < INDEX_SIZE; i++) {
        IndexNode* current = index->table[i];
        IndexNode* prev = NULL;
        
        while (current) {
            // Remove filename from this word's file list
            int found = -1;
            for (int j = 0; j < current->files.file_count; j++) {
                if (strcmp(current->files.filenames[j], filename) == 0) {
                    found = j;
                    break;
                }
            }
            
            if (found != -1) {
                // Shift remaining files
                for (int j = found; j < current->files.file_count - 1; j++) {
                    strcpy(current->files.filenames[j], current->files.filenames[j + 1]);
                }
                current->files.file_count--;
                removed++;
                
                // If no files left for this word, remove the node
                if (current->files.file_count == 0) {
                    IndexNode* to_delete = current;
                    if (prev == NULL) {
                        index->table[i] = current->next;
                    } else {
                        prev->next = current->next;
                    }
                    current = current->next;
                    free(to_delete);
                    continue;
                }
            }
            
            prev = current;
            current = current->next;
        }
    }
    
    log_info("Removed %d word entries for file '%s'", removed, filename);
    return removed;
}

int index_search(InvertedIndex* index, const char* word, char result[MAX_CONTENT_LEN]) {
    // Normalize word
    char normalized_word[MAX_WORD_LEN];
    to_lowercase(normalized_word, word);
    
    // Hash and search
    unsigned long hash = hash_word(normalized_word);
    IndexNode* current = index->table[hash];
    
    while (current) {
        if (strcmp(current->word, normalized_word) == 0) {
            // Found the word, build result string
            result[0] = '\0';
            for (int i = 0; i < current->files.file_count; i++) {
                if (i > 0) {
                    strcat(result, "\n");
                }
                strcat(result, current->files.filenames[i]);
            }
            log_info("Search for '%s': found in %d files", word, current->files.file_count);
            return current->files.file_count;
        }
        current = current->next;
    }
    
    // Not found
    strcpy(result, "No files found containing this word");
    log_info("Search for '%s': no matches", word);
    return 0;
}

int index_file(InvertedIndex* index, const char* filename, const char* content) {
    if (!content || strlen(content) == 0) {
        return 0;
    }
    
    // First remove existing entries for this file
    index_remove_file(index, filename);
    
    // Parse content and extract words
    char buffer[MAX_CONTENT_LEN];
    strncpy(buffer, content, MAX_CONTENT_LEN - 1);
    buffer[MAX_CONTENT_LEN - 1] = '\0';
    
    char word[MAX_WORD_LEN];
    int word_idx = 0;
    int words_indexed = 0;
    
    for (int i = 0; buffer[i] != '\0'; i++) {
        if (is_separator(buffer[i])) {
            if (word_idx > 0) {
                word[word_idx] = '\0';
                // Index the word (only if it's at least 2 characters)
                if (strlen(word) >= 2) {
                    index_add_word(index, word, filename);
                    words_indexed++;
                }
                word_idx = 0;
            }
        } else {
            if (word_idx < MAX_WORD_LEN - 1) {
                word[word_idx++] = buffer[i];
            }
        }
    }
    
    // Handle last word
    if (word_idx > 0) {
        word[word_idx] = '\0';
        if (strlen(word) >= 2) {
            index_add_word(index, word, filename);
            words_indexed++;
        }
    }
    
    log_info("Indexed %d words from file '%s'", words_indexed, filename);
    return words_indexed;
}

void index_destroy(InvertedIndex* index) {
    for (int i = 0; i < INDEX_SIZE; i++) {
        IndexNode* current = index->table[i];
        while (current) {
            IndexNode* next = current->next;
            free(current);
            current = next;
        }
        index->table[i] = NULL;
    }
    log_info("Inverted index destroyed");
}
