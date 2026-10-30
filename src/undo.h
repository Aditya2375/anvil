#ifndef ANVIL_UNDO_H
#define ANVIL_UNDO_H

#include "buffer.h"

/* Undo/redo over a buffer: an operation log. Typing runs merge into one
 * step; any cursor motion or a switch between inserting and deleting
 * starts a new step. The log owns copies of the text it records. */
struct undo;

struct undo *undo_new(void);
void         undo_free(struct undo *u);

void undo_record_insert(struct undo *u, struct buffer *b, size_t pos, size_t n);
void undo_record_delete(struct undo *u, struct buffer *b, size_t pos, size_t n);
void undo_break(struct undo *u);    /* call on cursor motion: next op is a new step */

int  undo_undo(struct undo *u, struct buffer *b);   /* 0 = something undone */
int  undo_redo(struct undo *u, struct buffer *b);   /* 0 = something redone */

#endif
