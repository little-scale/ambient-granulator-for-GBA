#include "text_format.h"

static void terminate(TextBuffer *text)
{
    if (text->capacity > 0) {
        size_t index = text->length < text->capacity
            ? text->length : text->capacity - 1;
        text->data[index] = '\0';
    }
}

void text_init(TextBuffer *text, char *data, size_t capacity)
{
    text->data = data;
    text->capacity = capacity;
    text->length = 0;
    terminate(text);
}

void text_append_char(TextBuffer *text, char value)
{
    if (text->length + 1 < text->capacity)
        text->data[text->length] = value;
    ++text->length;
    terminate(text);
}

void text_append(TextBuffer *text, const char *value)
{
    while (*value != '\0')
        text_append_char(text, *value++);
}

void text_append_field(TextBuffer *text, const char *value,
                       unsigned int maximum, unsigned int width)
{
    unsigned int count = 0;
    while (count < maximum && value[count] != '\0') {
        text_append_char(text, value[count]);
        ++count;
    }
    while (count++ < width)
        text_append_char(text, ' ');
}

void text_append_uint(TextBuffer *text, unsigned int value,
                      unsigned int minimum_digits)
{
    char digits[10];
    unsigned int count = 0;

    do {
        digits[count++] = (char)('0' + value % 10u);
        value /= 10u;
    } while (value != 0u && count < sizeof(digits));
    while (count < minimum_digits && count < sizeof(digits))
        digits[count++] = '0';
    while (count > 0)
        text_append_char(text, digits[--count]);
}

void text_append_int(TextBuffer *text, int value, int always_sign,
                     unsigned int minimum_digits)
{
    unsigned int magnitude;
    if (value < 0) {
        text_append_char(text, '-');
        magnitude = (unsigned int)(-(value + 1)) + 1u;
    } else {
        if (always_sign)
            text_append_char(text, '+');
        magnitude = (unsigned int)value;
    }
    text_append_uint(text, magnitude, minimum_digits);
}
