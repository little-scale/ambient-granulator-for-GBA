#ifndef AMBGRANULAR_TEXT_FORMAT_H
#define AMBGRANULAR_TEXT_FORMAT_H

#include <stddef.h>

typedef struct {
    char *data;
    size_t capacity;
    size_t length;
} TextBuffer;

void text_init(TextBuffer *text, char *data, size_t capacity);
void text_append_char(TextBuffer *text, char value);
void text_append(TextBuffer *text, const char *value);
void text_append_field(TextBuffer *text, const char *value,
                       unsigned int maximum, unsigned int width);
void text_append_uint(TextBuffer *text, unsigned int value,
                      unsigned int minimum_digits);
void text_append_int(TextBuffer *text, int value, int always_sign,
                     unsigned int minimum_digits);

#endif
