/* Randomized differential test: the gap buffer against a naive reference
 * model (a plain array with memmove). Any mismatch in length, content,
 * or cursor position fails. */
#include "../src/buffer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct ref {
    char  *data;
    size_t len, cap, cur;
};

static void ref_reserve(struct ref *r, size_t n) {
    if (r->len + n <= r->cap) return;
    while (r->len + n > r->cap) r->cap = r->cap ? r->cap * 2 : 256;
    r->data = realloc(r->data, r->cap);
    if (!r->data) { perror("realloc"); exit(1); }
}

static void ref_insert(struct ref *r, char c) {
    ref_reserve(r, 1);
    memmove(r->data + r->cur + 1, r->data + r->cur, r->len - r->cur);
    r->data[r->cur++] = c;
    r->len++;
}

static void ref_delete(struct ref *r, size_t n) {
    if (r->cur + n > r->len) n = r->len - r->cur;
    memmove(r->data + r->cur, r->data + r->cur + n, r->len - r->cur - n);
    r->len -= n;
}

static void ref_backspace(struct ref *r) {
    if (!r->cur) return;
    memmove(r->data + r->cur - 1, r->data + r->cur, r->len - r->cur);
    r->cur--;
    r->len--;
}

int main(void) {
    unsigned rng = 12345;
    for (int trial = 0; trial < 20; trial++) {
        struct buffer *b = buffer_new();
        struct ref r = {0};
        for (int op = 0; op < 5000; op++) {
            rng = rng * 1103515245 + 12345;
            unsigned what = (rng >> 16) % 100;
            if (what < 55) {                    /* insert printable or \n */
                char c = (rng >> 20) % 10 == 0 ? '\n' : (char)(' ' + (rng >> 24) % 95);
                buffer_insert_char(b, c);
                ref_insert(&r, c);
            } else if (what < 70) {             /* delete forward 1-8 */
                size_t n = 1 + (rng >> 20) % 8;
                buffer_delete(b, n);
                ref_delete(&r, n);
            } else if (what < 80) {             /* backspace */
                buffer_backspace(b);
                ref_backspace(&r);
            } else {                            /* jump somewhere */
                size_t pos = r.len ? (rng >> 20) % (r.len + 1) : 0;
                buffer_move_to(b, pos);
                r.cur = pos;
            }
            if (buffer_len(b) != r.len || buffer_cursor(b) != r.cur) {
                printf("MISMATCH trial %d op %d: len %zu vs %zu, cur %zu vs %zu\n",
                       trial, op, buffer_len(b), r.len, buffer_cursor(b), r.cur);
                return 1;
            }
        }
        char *t = buffer_text(b);
        if (r.len && memcmp(t, r.data, r.len) != 0) {
            printf("CONTENT MISMATCH trial %d\n", trial);
            return 1;
        }
        /* spot-check line queries against the reference content */
        size_t lines = 1;
        for (size_t i = 0; i < r.len; i++)
            if (r.data[i] == '\n') lines++;
        if (r.len && r.data[r.len - 1] == '\n') lines--;
        if (buffer_line_count(b) != lines) {
            printf("LINE COUNT MISMATCH trial %d: %zu vs %zu\n",
                   trial, buffer_line_count(b), lines);
            return 1;
        }
        free(t);
        free(r.data);
        buffer_free(b);
    }
    printf("fuzz: 20 trials x 5000 ops, no divergence\n");
    return 0;
}
