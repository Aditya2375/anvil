# anvil

A terminal IDE in C with no dependencies beyond libc: multiple buffers,
a file tree, syntax highlighting, project-wide search, and an integrated
build/test runner - everything a small systems project needs, in one
binary you can read end to end.

## Status: early

The text core exists and is heavily tested; the interface is being built
on top of it. What works today:

- **Gap-buffer text core** (`src/buffer.c`): O(1) amortized insert and
  delete at the cursor, line queries, file load/save. Verified by unit
  tests and a 20-trial, 100k-operation differential fuzz test against a
  reference model.

On the bench, in order:

1. Raw-terminal UI: buffer view, cursor motion, insert/delete, save/quit.
2. File tree and buffer switching.
3. Syntax highlighting (C first).
4. Project search and the build/test runner.

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
