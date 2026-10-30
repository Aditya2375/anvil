#include "undo.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

enum op_kind { OP_INSERT, OP_DELETE };

struct entry {
    enum op_kind kind;
    size_t pos;
    char  *text;
    size_t len;
};

struct undo {
    struct entry *entries;
    size_t count, cap;
    size_t applied;         /* entries below this are live; above are redoable */
    int    merge_kind;      /* enum op_kind of the open run, -1 when closed */
    size_t merge_end;       /* where the open insert run ends */
};

struct undo *undo_new(void) {
    struct undo *u = calloc(1, sizeof *u);
    if (!u) { perror("calloc"); exit(1); }
    u->merge_kind = -1;
    return u;
}

void undo_free(struct undo *u) {
    if (!u) return;
    for (size_t i = 0; i < u->count; i++) free(u->entries[i].text);
    free(u->entries);
    free(u);
}

void undo_break(struct undo *u) { u->merge_kind = -1; }

/* Recording a new op drops everything redoable above the waterline. */
static void truncate_redo(struct undo *u) {
    for (size_t i = u->applied; i < u->count; i++) free(u->entries[i].text);
    u->count = u->applied;
}

static struct entry *push(struct undo *u, enum op_kind kind, size_t pos) {
    if (u->count == u->cap) {
        u->cap = u->cap ? u->cap * 2 : 64;
        u->entries = realloc(u->entries, u->cap * sizeof *u->entries);
        if (!u->entries) { perror("realloc"); exit(1); }
    }
    struct entry *e = &u->entries[u->count++];
    e->kind = kind;
    e->pos = pos;
    e->text = NULL;
    e->len = 0;
    u->applied = u->count;
    return e;
}

static char *copy_range(struct buffer *b, size_t pos, size_t n) {
    char *s = malloc(n + 1);
    if (!s) { perror("malloc"); exit(1); }
    for (size_t i = 0; i < n; i++) s[i] = buffer_at(b, pos + i);
    s[n] = '\0';
    return s;
}

void undo_record_insert(struct undo *u, struct buffer *b, size_t pos, size_t n) {
    /* call after the insert happened; text is read back from the buffer */
    truncate_redo(u);
    if (u->merge_kind == OP_INSERT && pos == u->merge_end && n == 1) {
        struct entry *e = &u->entries[u->count - 1];
        e->text = realloc(e->text, e->len + 2);
        if (!e->text) { perror("realloc"); exit(1); }
        e->text[e->len] = buffer_at(b, pos);
        e->text[++e->len] = '\0';
        u->merge_end = pos + 1;
        return;
    }
    struct entry *e = push(u, OP_INSERT, pos);
    e->text = copy_range(b, pos, n);
    e->len = n;
    u->merge_kind = OP_INSERT;
    u->merge_end = pos + n;
}

void undo_record_delete(struct undo *u, struct buffer *b, size_t pos, size_t n) {
    /* call before the delete happens, while the text still exists */
    truncate_redo(u);
    /* backspace runs merge: deleting backwards from the same end */
    if (u->merge_kind == OP_DELETE && n == 1 && u->count &&
        pos + 1 == u->entries[u->count - 1].pos) {
        struct entry *e = &u->entries[u->count - 1];
        e->text = realloc(e->text, e->len + 2);
        if (!e->text) { perror("realloc"); exit(1); }
        memmove(e->text + 1, e->text, e->len + 1);
        e->text[0] = buffer_at(b, pos);
        e->len++;
        e->pos = pos;
        return;
    }
    struct entry *e = push(u, OP_DELETE, pos);
    e->text = copy_range(b, pos, n);
    e->len = n;
    u->merge_kind = OP_DELETE;
    u->merge_end = pos;
}

static int apply_inverse(struct buffer *b, struct entry *e) {
    buffer_move_to(b, e->pos);
    if (e->kind == OP_INSERT) {
        buffer_delete(b, e->len);
    } else {
        buffer_insert(b, e->text, e->len);
    }
    return 0;
}

int undo_undo(struct undo *u, struct buffer *b) {
    if (u->applied == 0) return -1;
    struct entry *e = &u->entries[u->applied - 1];
    apply_inverse(b, e);
    u->applied--;
    u->merge_kind = -1;
    return 0;
}

int undo_redo(struct undo *u, struct buffer *b) {
    if (u->applied >= u->count) return -1;
    struct entry *e = &u->entries[u->applied];
    buffer_move_to(b, e->pos);
    if (e->kind == OP_INSERT) {
        buffer_insert(b, e->text, e->len);
    } else {
        buffer_delete(b, e->len);
    }
    u->applied++;
    u->merge_kind = -1;
    return 0;
}
