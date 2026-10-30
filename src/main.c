/* anvil - a terminal IDE. Today: a real full-screen editor on the gap
 * buffer core (open, edit, navigate, save, quit). The rest of the IDE
 * (file tree, highlighting, search, build runner) lands on top of this. */
#include "buffer.h"

#include <ctype.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <sys/ioctl.h>

static struct termios saved_termios;
static int raw_active = 0;

/* write(2) to the terminal, tolerating short writes */
static void out(const char *s, size_t n) {
    while (n) {
        ssize_t w = write(STDOUT_FILENO, s, n);
        if (w <= 0) return;
        s += w;
        n -= (size_t)w;
    }
}
#define outs(lit) out(lit, sizeof(lit) - 1)

static void raw_off(void) {
    if (raw_active) {
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &saved_termios);
        raw_active = 0;
        /* leave the alternate screen, show the cursor */
        outs("\033[?1049l\033[?25h");
    }
}

static int raw_on(void) {
    if (tcgetattr(STDIN_FILENO, &saved_termios) != 0) return -1;
    struct termios raw = saved_termios;
    raw.c_iflag &= (tcflag_t)~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_oflag &= (tcflag_t)~(OPOST);
    raw.c_cflag |= CS8;
    raw.c_lflag &= (tcflag_t)~(ECHO | ICANON | IEXTEN | ISIG);
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) != 0) return -1;
    raw_active = 1;
    atexit(raw_off);
    outs("\033[?1049h\033[?25l");  /* alt screen, hide cursor */
    return 0;
}

struct editor {
    struct buffer *buf;
    const char    *path;
    int rows, cols;
    size_t rowoff, coloff;
    char status[128];
};

static void term_size(struct editor *e) {
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_row && ws.ws_col) {
        e->rows = ws.ws_row;
        e->cols = ws.ws_col;
    } else {
        e->rows = 24;       /* size unknown: assume a small terminal */
        e->cols = 80;
    }
}

/* cursor line/column within the buffer */
static size_t cur_line(struct editor *e) {
    return buffer_pos_line(e->buf, buffer_cursor(e->buf));
}
static size_t cur_col(struct editor *e) {
    size_t pos = buffer_cursor(e->buf);
    return pos - buffer_line_start(e->buf, cur_line(e));
}

static void scroll(struct editor *e) {
    size_t line = cur_line(e), col = cur_col(e);
    int view = e->rows - 1;             /* last row is the status bar */
    if ((long)line < (long)e->rowoff) e->rowoff = line;
    if ((long)line >= (long)(e->rowoff + view)) e->rowoff = line - view + 1;
    if ((long)col < (long)e->coloff) e->coloff = col;
    if ((long)col >= (long)(e->coloff + e->cols)) e->coloff = col - e->cols + 1;
}

static void draw(struct editor *e) {
    scroll(e);
    outs("\033[H");  /* home */
    int view = e->rows - 1;
    size_t nlines = buffer_line_count(e->buf);
    for (int r = 0; r < view; r++) {
        size_t line = e->rowoff + r;
        outs("\033[K");    /* clear row */
        if (line < nlines) {
            size_t start = buffer_line_start(e->buf, line);
            size_t len = buffer_line_len(e->buf, line);
            if (e->coloff < len) {
                size_t show = len - e->coloff;
                if ((int)show > e->cols) show = (size_t)e->cols;
                /* bytes may straddle the gap; emit one at a time */
                for (size_t i = 0; i < show; i++) {
                    char c = buffer_at(e->buf, start + e->coloff + i);
                    if (isprint((unsigned char)c) || c == '\t')
                        out(&c, 1);
                }
            }
        } else {
            outs("~");
        }
        outs("\r\n");
    }
    /* status bar */
    int n = snprintf(e->status, sizeof e->status,
                     "\033[7m %s%s - line %zu/%zu, col %zu \033[m",
                     e->path, e->buf->dirty ? " [+]" : "",
                     cur_line(e) + 1, nlines, cur_col(e) + 1);
    out(e->status, (size_t)n);
    /* place the hardware cursor */
    n = snprintf(e->status, sizeof e->status, "\033[%zu;%zuH",
                 cur_line(e) - e->rowoff + 1, cur_col(e) - e->coloff + 1);
    out(e->status, (size_t)n);
}

enum { KEY_ESCAPE = 1000, KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT,
       KEY_HOME, KEY_END, KEY_PGUP, KEY_PGDN, KEY_DEL };

static int read_key(void) {
    char c;
    if (read(STDIN_FILENO, &c, 1) != 1) return -1;
    if (c != '\033') return (unsigned char)c;
    char seq[3];
    if (read(STDIN_FILENO, &seq[0], 1) != 1) return KEY_ESCAPE;
    if (read(STDIN_FILENO, &seq[1], 1) != 1) return KEY_ESCAPE;
    if (seq[0] == '[') {
        if (seq[1] >= '0' && seq[1] <= '9') {
            if (read(STDIN_FILENO, &seq[2], 1) != 1) return KEY_ESCAPE;
            if (seq[2] == '~') {
                switch (seq[1]) {
                case '1': case '7': return KEY_HOME;
                case '4': case '8': return KEY_END;
                case '3': return KEY_DEL;
                case '5': return KEY_PGUP;
                case '6': return KEY_PGDN;
                }
            }
        } else {
            switch (seq[1]) {
            case 'A': return KEY_UP;
            case 'B': return KEY_DOWN;
            case 'C': return KEY_RIGHT;
            case 'D': return KEY_LEFT;
            case 'H': return KEY_HOME;
            case 'F': return KEY_END;
            }
        }
    }
    return KEY_ESCAPE;
}

static void move_to(struct editor *e, size_t line, size_t col) {
    size_t nlines = buffer_line_count(e->buf);
    if (line >= nlines) line = nlines - 1;
    size_t llen = buffer_line_len(e->buf, line);
    if (col > llen) col = llen;
    buffer_move_to(e->buf, buffer_line_start(e->buf, line) + col);
}

static void process_key(struct editor *e, int k, int *quit) {
    size_t line = cur_line(e), col = cur_col(e);
    switch (k) {
    case KEY_UP:    if (line) move_to(e, line - 1, col); break;
    case KEY_DOWN:  move_to(e, line + 1, col); break;
    case KEY_LEFT:
        if (col) move_to(e, line, col - 1);
        else if (line) move_to(e, line - 1, buffer_line_len(e->buf, line - 1));
        break;
    case KEY_RIGHT:
        if (col < buffer_line_len(e->buf, line)) move_to(e, line, col + 1);
        else move_to(e, line + 1, 0);
        break;
    case KEY_HOME:  move_to(e, line, 0); break;
    case KEY_END:   move_to(e, line, (size_t)-1); break;
    case KEY_PGUP:  move_to(e, line > (size_t)e->rows ? line - e->rows : 0, col); break;
    case KEY_PGDN:  move_to(e, line + e->rows, col); break;
    case KEY_DEL:   buffer_delete(e->buf, 1); break;
    case 127: case 8: buffer_backspace(e->buf); break;
    case '\r':      buffer_insert_char(e->buf, '\n'); break;
    case 19: {      /* ctrl-s */
        char err[128] = {0};
        if (e->path && buffer_save(e->buf, e->path, err, sizeof err) != 0)
            snprintf(e->status, sizeof e->status, "save failed: %s", err);
        break;
    }
    case 17: *quit = 1; break;  /* ctrl-q */
    default:
        if (isprint(k) || k == '\t') buffer_insert_char(e->buf, (char)k);
        break;
    }
}

int main(int argc, char **argv) {
    if (argc > 2) {
        fprintf(stderr, "usage: %s [file]\n", argv[0]);
        return 2;
    }
    struct editor e = {0};
    e.buf = buffer_new();
    e.path = argc == 2 ? argv[1] : "untitled";
    if (argc == 2) {
        char err[128];
        if (buffer_load(e.buf, e.path, err, sizeof err) != 0) {
            /* a missing file starts empty; other errors are real */
            if (access(e.path, F_OK) == 0) {
                fprintf(stderr, "anvil: %s\n", err);
                return 1;
            }
        }
    }
    if (raw_on() != 0) {
        fprintf(stderr, "anvil: needs a terminal\n");
        return 1;
    }
    int quit = 0;
    while (!quit) {
        term_size(&e);
        draw(&e);
        int k = read_key();
        if (k < 0) break;
        process_key(&e, k, &quit);
    }
    raw_off();
    buffer_free(e.buf);
    return 0;
}
