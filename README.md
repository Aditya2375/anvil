# anvil

A terminal IDE in C with no dependencies beyond libc: multiple buffers,
a file tree, syntax highlighting, project-wide search, and an integrated
build/test runner - everything a small systems project needs, in one
binary you can read end to end.

**[Live demo](https://aditya2375.github.io/anvil/)** - the real editor recorded in a terminal under Xvfb (a browser tab has no pty to run it in).

## Status: early but editing

- **Gap-buffer text core** (`src/buffer.c`): O(1) amortized insert and
  delete at the cursor, line queries, file load/save. Verified by unit
  tests and a 20-trial, 100k-operation differential fuzz test against a
  reference model.
- **Full-screen editor** (`src/main.c`): raw-terminal UI with scrolling,
  arrows/Home/End/PgUp/PgDn, insert/delete/backspace/enter, Ctrl-S save,
  Ctrl-Q quit, and a status bar. Verified by a pseudo-terminal test that
  types into a real anvil process and checks the file on disk.

On the bench, in order:

1. File tree and buffer switching.
2. Syntax highlighting (C first).
3. Project search and the build/test runner.
4. Undo/redo.

## Building and testing

```sh
make          # builds build/anvil
make test     # unit tests + differential fuzz
```

No dependencies beyond a C compiler.

## Layout

```text
src/    buffer.c (text core), UI to come
tests/  unit tests and the differential fuzzer
docs/   design notes
```
