#include "../src/undo.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures = 0, checks = 0;
#define CHECK(cond, msg) do { \
    checks++; \
    if (!(cond)) { failures++; printf("FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); } \
} while (0)

/* type s into b at the cursor, recording each char */
static void type(struct undo *u, struct buffer *b, const char *s) {
    for (; *s; s++) {
        buffer_insert_char(b, *s);
        undo_record_insert(u, b, buffer_cursor(b) - 1, 1);
    }
}

static int text_is(struct buffer *b, const char *want) {
    char *t = buffer_text(b);
    int ok = strcmp(t, want) == 0;
    free(t);
    return ok;
}

int main(void) {
    struct buffer *b = buffer_new();
    struct undo *u = undo_new();

    /* a typing run is one undo step */
    type(u, b, "hello world");
    CHECK(undo_undo(u, b) == 0, "undo a typing run");
    CHECK(text_is(b, ""), "run fully undone");
    CHECK(undo_redo(u, b) == 0, "redo it");
    CHECK(text_is(b, "hello world"), "redo restores text");

    /* cursor motion breaks the run into a new step */
    undo_break(u);
    type(u, b, "!");
    undo_undo(u, b);
    CHECK(text_is(b, "hello world"), "second run undone alone");
    undo_undo(u, b);
    CHECK(text_is(b, ""), "first run still undoable");
    undo_redo(u, b);
    undo_redo(u, b);
    CHECK(text_is(b, "hello world!"), "both redone in order");

    /* deletes: record before deleting; a backspace run merges */
    buffer_move_to(b, buffer_len(b));
    for (int i = 0; i < 6; i++) {
        undo_record_delete(u, b, buffer_cursor(b) - 1, 1);
        buffer_backspace(b);
    }
    CHECK(text_is(b, "hello "), "six backspaces");
    undo_undo(u, b);
    CHECK(text_is(b, "hello world!"), "backspace run undone as one step");
    undo_undo(u, b);   /* the "!" insert */
    undo_undo(u, b);   /* the typing run */
    CHECK(text_is(b, ""), "everything undone");
    CHECK(undo_undo(u, b) != 0, "undo at the bottom says so");

    /* new edits after undo drop the redo future */
    undo_redo(u, b);
    undo_redo(u, b);   /* "hello world" + "!" */
    undo_undo(u, b);   /* remove "!" */
    undo_break(u);
    type(u, b, "?");
    CHECK(undo_redo(u, b) != 0, "redo future dropped by new edit");
    undo_undo(u, b);
    CHECK(text_is(b, "hello world"), "new edit undone, earlier state intact");

    /* forward delete records the removed text */
    undo_break(u);
    buffer_move_to(b, 0);
    undo_record_delete(u, b, 0, 5);
    buffer_delete(b, 5);
    CHECK(text_is(b, " world"), "forward delete");
    undo_undo(u, b);
    CHECK(text_is(b, "hello world"), "forward delete restored");

    undo_free(u);
    buffer_free(b);
    printf("%d checks passed\n", checks);
    if (failures) { printf("%d FAILURES\n", failures); return 1; }
    return 0;
}
