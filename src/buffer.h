#ifndef ANVIL_BUFFER_H
#define ANVIL_BUFFER_H

#include <stddef.h>

/* The text buffer: a gap buffer over bytes. The gap sits at the cursor,
 * so insertions and deletions at the cursor are O(1) amortized; moving
 * the cursor far away copies the bytes in between. Lines are computed
 * on demand by scanning - the buffer does not keep a line index. */
struct buffer {
    char  *data;    /* backing store, size cap */
    size_t gap;     /* gap start = cursor position */
    size_t gap_end; /* gap end; text length = cap - (gap_end - gap) */
    size_t cap;
    int    dirty;   /* unsaved changes */
};

struct buffer *buffer_new(void);
void           buffer_free(struct buffer *b);

size_t  buffer_len(const struct buffer *b);
char    buffer_at(const struct buffer *b, size_t pos);   /* pos < len */
size_t  buffer_cursor(const struct buffer *b);
void    buffer_move_to(struct buffer *b, size_t pos);
void    buffer_move_delta(struct buffer *b, long delta);
void    buffer_insert(struct buffer *b, const char *s, size_t n);
void    buffer_insert_char(struct buffer *b, char c);
void    buffer_delete(struct buffer *b, size_t n);       /* forward */
void    buffer_backspace(struct buffer *b);

/* Line-oriented access over the byte array. Lines are 0-based; the last
 * line may lack a trailing newline. */
size_t  buffer_line_count(const struct buffer *b);
size_t  buffer_line_start(const struct buffer *b, size_t line);
size_t  buffer_line_len(const struct buffer *b, size_t line);  /* sans \n */
size_t  buffer_pos_line(const struct buffer *b, size_t pos);   /* line of pos */

/* Whole-buffer access (copies across the gap). Caller frees. */
char   *buffer_text(const struct buffer *b);

/* File IO: load replaces contents (dirty=0), save writes through. */
int     buffer_load(struct buffer *b, const char *path, char *err, int errcap);
int     buffer_save(struct buffer *b, const char *path, char *err, int errcap);

#endif
