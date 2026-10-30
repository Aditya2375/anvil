#include "buffer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define INITIAL_CAP 4096

struct buffer *buffer_new(void) {
    struct buffer *b = calloc(1, sizeof *b);
    if (!b) { perror("calloc"); exit(1); }
    b->data = malloc(INITIAL_CAP);
    if (!b->data) { perror("malloc"); exit(1); }
    b->cap = INITIAL_CAP;
    b->gap = 0;
    b->gap_end = INITIAL_CAP;
    return b;
}

void buffer_free(struct buffer *b) {
    if (!b) return;
    free(b->data);
    free(b);
}

size_t buffer_len(const struct buffer *b) {
    return b->cap - (b->gap_end - b->gap);
}

size_t buffer_cursor(const struct buffer *b) { return b->gap; }

/* Map a logical position to its physical index, skipping the gap. */
static size_t phys(const struct buffer *b, size_t pos) {
    return pos < b->gap ? pos : pos + (b->gap_end - b->gap);
}

char buffer_at(const struct buffer *b, size_t pos) {
    return pos < buffer_len(b) ? b->data[phys(b, pos)] : '\0';
}

void buffer_move_to(struct buffer *b, size_t pos) {
    size_t len = buffer_len(b);
    if (pos > len) pos = len;
    if (pos < b->gap) {
        memmove(b->data + b->gap_end - (b->gap - pos),
                b->data + pos, b->gap - pos);
        b->gap_end -= b->gap - pos;
        b->gap = pos;
    } else if (pos > b->gap) {
        memmove(b->data + b->gap, b->data + b->gap_end, pos - b->gap);
        b->gap_end += pos - b->gap;
        b->gap = pos;
    }
}

void buffer_move_delta(struct buffer *b, long delta) {
    long pos = (long)b->gap + delta;
    if (pos < 0) pos = 0;
    buffer_move_to(b, (size_t)pos);
}

static void buffer_reserve(struct buffer *b, size_t extra) {
    if (b->gap_end - b->gap >= extra) return;
    size_t len = buffer_len(b);
    size_t ncap = b->cap * 2;
    while (ncap < len + extra) ncap *= 2;
    char *nd = malloc(ncap);
    if (!nd) { perror("malloc"); exit(1); }
    memcpy(nd, b->data, b->gap);
    size_t tail = b->cap - b->gap_end;
    memcpy(nd + ncap - tail, b->data + b->gap_end, tail);
    free(b->data);
    b->data = nd;
    b->cap = ncap;
    b->gap_end = ncap - tail;
}

void buffer_insert(struct buffer *b, const char *s, size_t n) {
    buffer_reserve(b, n);
    memcpy(b->data + b->gap, s, n);
    b->gap += n;
    b->dirty = 1;
}

void buffer_insert_char(struct buffer *b, char c) {
    buffer_insert(b, &c, 1);
}

void buffer_delete(struct buffer *b, size_t n) {
    size_t len = buffer_len(b);
    if (b->gap + n > len) n = len - b->gap;
    b->gap_end += n;
    if (n) b->dirty = 1;
}

void buffer_backspace(struct buffer *b) {
    if (b->gap > 0) { b->gap--; b->dirty = 1; }
}

char *buffer_text(const struct buffer *b) {
    size_t len = buffer_len(b);
    char *out = malloc(len + 1);
    if (!out) { perror("malloc"); exit(1); }
    memcpy(out, b->data, b->gap);
    memcpy(out + b->gap, b->data + b->gap_end, b->cap - b->gap_end);
    out[len] = '\0';
    return out;
}

size_t buffer_line_count(const struct buffer *b) {
    /* Editor convention: a trailing newline terminates the last line, it
     * does not start a phantom one. The empty buffer still has one line. */
    size_t newlines = 0, len = buffer_len(b);
    for (size_t i = 0; i < len; i++)
        if (buffer_at(b, i) == '\n') newlines++;
    if (len == 0) return 1;
    return buffer_at(b, len - 1) == '\n' ? newlines : newlines + 1;
}

size_t buffer_line_start(const struct buffer *b, size_t line) {
    size_t n = 0, len = buffer_len(b);
    if (line == 0) return 0;
    for (size_t i = 0; i < len; i++)
        if (buffer_at(b, i) == '\n' && ++n == line) return i + 1;
    return len;
}

size_t buffer_line_len(const struct buffer *b, size_t line) {
    size_t start = buffer_line_start(b, line);
    size_t i = start, len = buffer_len(b);
    while (i < len && buffer_at(b, i) != '\n') i++;
    return i - start;
}

size_t buffer_pos_line(const struct buffer *b, size_t pos) {
    size_t n = 0;
    for (size_t i = 0; i < pos && i < buffer_len(b); i++)
        if (buffer_at(b, i) == '\n') n++;
    return n;
}

int buffer_load(struct buffer *b, const char *path, char *err, int errcap) {
    FILE *f = fopen(path, "rb");
    if (!f) { snprintf(err, errcap, "cannot open %s", path); return -1; }
    b->gap = 0;
    b->gap_end = b->cap;
    char chunk[8192];
    size_t n;
    while ((n = fread(chunk, 1, sizeof chunk, f)) > 0)
        buffer_insert(b, chunk, n);
    fclose(f);
    buffer_move_to(b, 0);
    b->dirty = 0;
    return 0;
}

int buffer_save(struct buffer *b, const char *path, char *err, int errcap) {
    FILE *f = fopen(path, "wb");
    if (!f) { snprintf(err, errcap, "cannot write %s", path); return -1; }
    size_t ok1 = fwrite(b->data, 1, b->gap, f);
    size_t ok2 = fwrite(b->data + b->gap_end, 1, b->cap - b->gap_end, f);
    if (fclose(f) != 0 || ok1 != b->gap || ok2 != b->cap - b->gap_end) {
        snprintf(err, errcap, "write to %s failed", path);
        return -1;
    }
    b->dirty = 0;
    return 0;
}
