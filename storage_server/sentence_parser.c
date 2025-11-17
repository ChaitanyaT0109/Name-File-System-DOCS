/*
 * storage_server/sentence_parser.c
 * 
 * Implementation of sentence parsing utilities
 * CRITICAL: Every . ! ? creates a new sentence (even in "e.g.")
 */

#include "sentence_parser.h"
#include "../common/logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static int is_delimiter(char c) {
    return (c == '.' || c == '!' || c == '?');
}

int parse_sentences(const char* content, char*** sentences_out) {
    if (!content || !sentences_out) return 0;
    
    int count = 0;
    int capacity = 100;
    char** sentences = malloc(capacity * sizeof(char*));
    
    const char* start = content;
    const char* ptr = content;
    
    while (*ptr) {
        if (is_delimiter(*ptr)) {
            // Found delimiter, extract sentence including delimiter
            int len = ptr - start + 1;
            char* sentence = malloc(len + 1);
            memcpy(sentence, start, len);
            sentence[len] = '\0';
            
            // Expand array if needed
            if (count >= capacity) {
                capacity *= 2;
                sentences = realloc(sentences, capacity * sizeof(char*));
            }
            
            sentences[count++] = sentence;
            start = ptr + 1;
        }
        ptr++;
    }
    
    // Add remaining text as last sentence (if any)
    if (*start != '\0') {
        int len = ptr - start;
        char* sentence = malloc(len + 1);
        memcpy(sentence, start, len);
        sentence[len] = '\0';
        
        if (count >= capacity) {
            capacity++;
            sentences = realloc(sentences, capacity * sizeof(char*));
        }
        
        sentences[count++] = sentence;
    }
    
    *sentences_out = sentences;
    log_info("Parsed %d sentences from content", count);
    return count;
}

void free_sentences(char** sentences, int count) {
    if (!sentences) return;
    for (int i = 0; i < count; i++) {
        if (sentences[i]) free(sentences[i]);
    }
    free(sentences);
}

int parse_words(const char* sentence, char*** words_out) {
    if (!sentence || !words_out) return 0;
    
    int count = 0;
    int capacity = 50;
    char** words = malloc(capacity * sizeof(char*));
    
    const char* ptr = sentence;
    char word_buf[1024];
    int word_len = 0;
    
    while (*ptr) {
        if (isspace(*ptr)) {
            // End of word
            if (word_len > 0) {
                word_buf[word_len] = '\0';
                
                if (count >= capacity) {
                    capacity *= 2;
                    words = realloc(words, capacity * sizeof(char*));
                }
                
                words[count] = malloc(word_len + 1);
                strcpy(words[count], word_buf);
                count++;
                word_len = 0;
            }
        } else {
            word_buf[word_len++] = *ptr;
        }
        ptr++;
    }
    
    // Add last word if any
    if (word_len > 0) {
        word_buf[word_len] = '\0';
        
        if (count >= capacity) {
            capacity++;
            words = realloc(words, capacity * sizeof(char*));
        }
        
        words[count] = malloc(word_len + 1);
        strcpy(words[count], word_buf);
        count++;
    }
    
    *words_out = words;
    return count;
}

void free_words(char** words, int count) {
    if (!words) return;
    for (int i = 0; i < count; i++) {
        if (words[i]) free(words[i]);
    }
    free(words);
}

char* insert_word(const char* sentence, int word_idx, const char* word) {
    char** words;
    int word_count = parse_words(sentence, &words);
    
    if (word_idx < 0 || word_idx > word_count) {
        log_error("Invalid word index %d (sentence has %d words)", word_idx, word_count);
        free_words(words, word_count);
        return NULL;
    }
    
    // Calculate new sentence length
    size_t new_len = strlen(word) + 1;  // +1 for space
    for (int i = 0; i < word_count; i++) {
        new_len += strlen(words[i]) + 1;  // +1 for space
    }
    new_len += 10;  // Safety margin
    
    char* new_sentence = malloc(new_len);
    new_sentence[0] = '\0';
    
    // Build new sentence
    for (int i = 0; i < word_idx; i++) {
        strcat(new_sentence, words[i]);
        strcat(new_sentence, " ");
    }
    
    strcat(new_sentence, word);
    strcat(new_sentence, " ");
    
    for (int i = word_idx; i < word_count; i++) {
        strcat(new_sentence, words[i]);
        if (i < word_count - 1) strcat(new_sentence, " ");
    }
    
    free_words(words, word_count);
    
    // Remove trailing space if not ending with delimiter
    int len = strlen(new_sentence);
    if (len > 0 && new_sentence[len-1] == ' ' && !is_delimiter(new_sentence[len-2])) {
        new_sentence[len-1] = '\0';
    }
    
    return new_sentence;
}

char* rebuild_file(char** sentences, int count) {
    if (!sentences || count == 0) return strdup("");
    
    // Calculate total length
    size_t total_len = 0;
    for (int i = 0; i < count; i++) {
        total_len += strlen(sentences[i]);
    }
    total_len += 1;  // Null terminator
    
    char* content = malloc(total_len);
    content[0] = '\0';
    
    for (int i = 0; i < count; i++) {
        strcat(content, sentences[i]);
    }
    
    return content;
}

int has_delimiter(const char* str) {
    while (*str) {
        if (is_delimiter(*str)) return 1;
        str++;
    }
    return 0;
}

int split_by_delimiters(const char* str, char*** parts_out) {
    if (!str || !parts_out) return 0;
    
    int count = 0;
    int capacity = 10;
    char** parts = malloc(capacity * sizeof(char*));
    
    const char* start = str;
    const char* ptr = str;
    
    while (*ptr) {
        if (is_delimiter(*ptr)) {
            // Include delimiter in part
            int len = ptr - start + 1;
            char* part = malloc(len + 1);
            memcpy(part, start, len);
            part[len] = '\0';
            
            if (count >= capacity) {
                capacity *= 2;
                parts = realloc(parts, capacity * sizeof(char*));
            }
            
            parts[count++] = part;
            start = ptr + 1;
        }
        ptr++;
    }
    
    // Add remaining text
    if (*start != '\0') {
        int len = ptr - start;
        char* part = malloc(len + 1);
        memcpy(part, start, len);
        part[len] = '\0';
        
        if (count >= capacity) {
            capacity++;
            parts = realloc(parts, capacity * sizeof(char*));
        }
        
        parts[count++] = part;
    }
    
    *parts_out = parts;
    return count;
}
