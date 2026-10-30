#include "../src/buffer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures = 0, checks = 0;
#define CHECK(cond, msg) do { \
    checks++; \
    if (!(cond)) { failures++; printf("FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); } \
} while (0)

int main(void) {
    struct buffer *b = buffer_new();
    CHECK(buffer_len(b) == 0, "empty at birth");
    CHECK(buffer_line_count(b) == 1, "empty buffer has one line");

    const char *hello = "hello world";
    buffer_insert(b, hello, strlen(hello));
    CHECK(buffer_len(b) == 11, "insert grows length");
    CHECK(buffer_cursor(b) == 11, "cursor after insert");
    CHECK(b->dirty, "insert marks dirty");

    buffer_move_to(b, 5);
    buffer_insert_char(b, ',');
    char *t = buffer_text(b);
    CHECK(strcmp(t, "hello, world") == 0, "insert mid-buffer");
    free(t);

    buffer_move_to(b, 0);
    buffer_delete(b, 7);
    t = buffer_text(b);
    CHECK(strcmp(t, "world") == 0, "forward delete");
    free(t);

    buffer_move_to(b, 5);
    buffer_backspace(b);
    buffer_backspace(b);
    t = buffer_text(b);
    CHECK(strcmp(t, "wor") == 0, "backspace");
    free(t);
    buffer_backspace(b);
    buffer_backspace(b);
    buffer_backspace(b);
    buffer_backspace(b);   /* at start: no-op, must not underflow */
    CHECK(buffer_len(b) == 0, "backspace to empty");

    /* growth past the initial capacity */
    for (int i = 0; i < 10000; i++)
        buffer_insert_char(b, 'a' + (char)(i % 26));
    CHECK(buffer_len(b) == 10000, "grows to 10k chars");
    buffer_move_to(b, 0);
    CHECK(buffer_at(b, 0) == 'a' && buffer_at(b, 25) == 'z', "content intact after growth");
    buffer_move_to(b, 5000);
    CHECK(buffer_at(b, 5000) == 'a' + 5000 % 26, "random access after moves");
    buffer_free(b);

    /* lines */
    b = buffer_new();
    const char *src = "one\ntwo\n\nthree\n";
    buffer_insert(b, src, strlen(src));
    CHECK(buffer_line_count(b) == 4, "line count with trailing newline");
    CHECK(buffer_line_start(b, 2) == 8, "empty line start");
    CHECK(buffer_line_len(b, 0) == 3, "line 0 len");
    CHECK(buffer_line_len(b, 2) == 0, "line 2 empty");
    CHECK(buffer_pos_line(b, 8) == 2, "position to line");
    buffer_free(b);

    /* file round trip */
    b = buffer_new();
    const char *big = NULL;
    {
        size_t cap = 100000;
        char *tmp = malloc(cap);
        size_t w = 0;
        for (int i = 0; i < 2000; i++)
            w += (size_t)snprintf(tmp + w, cap - w, "line %d of the test file\n", i);
        buffer_insert(b, tmp, w);
        free(tmp);
        big = buffer_text(b);
    }
    char err[128];
    CHECK(buffer_save(b, "/tmp/anvil_test.txt", err, sizeof err) == 0, "save works");
    CHECK(!b->dirty, "save clears dirty");
    struct buffer *b2 = buffer_new();
    CHECK(buffer_load(b2, "/tmp/anvil_test.txt", err, sizeof err) == 0, "load works");
    char *t2 = buffer_text(b2);
    CHECK(strcmp(t2, big) == 0, "round trip preserves every byte");
    CHECK(buffer_line_count(b2) == 2000, "loaded line count");
    CHECK(buffer_cursor(b2) == 0, "load leaves cursor at start");
    free(t2);
    free((void *)big);
    CHECK(buffer_load(b2, "/tmp/does_not_exist_xyz", err, sizeof err) != 0,
          "load failure reported");
    buffer_free(b);
    buffer_free(b2);

    printf("%d checks passed\n", checks);
    if (failures) { printf("%d FAILURES\n", failures); return 1; }
    return 0;
}
