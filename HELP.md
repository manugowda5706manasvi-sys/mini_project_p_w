# Line Editor User Manual

## Starting the editor

Build the program:

```text
gcc -Wall -Wextra -std=c11 main.c -o line_editor
```

Run an empty editor:

```text
./line_editor
```

Load a file immediately:

```text
./line_editor journal.txt
```

Only `.txt` files are accepted. File names with spaces are not supported by the command syntax.

## Commands

### `insert <line> <text>`

Insert text at a 1-based line number. Valid positions are `1` through `size + 1`.

```text
line-editor> insert 1 First line
Inserted line 1.
line-editor> insert 2 Second line
Inserted line 2.
line-editor> insert 2 Inserted in the middle
Inserted line 2.
```

The line text is everything after the line number. An empty line can be inserted with `insert 1`.

### `delete <line>`

Delete the requested 1-based line. The line memory is freed and later lines move up.

```text
line-editor> delete 2
Deleted line 2.
```

Deleting from an empty document, using zero or a negative number, or using a number larger than the document size produces an error without changing the document.

### `display`

Print every line with consecutive 1-based numbers.

```text
line-editor> display
1 | First line
2 | Last line
```

An empty document prints `[empty document]`.

### `save <file.txt>`

Write the current document to the specified text file. Each document line becomes one file line.

```text
line-editor> save journal.txt
Saved 2 lines to journal.txt.
```

### `load <file.txt>`

Replace the current document with the contents of a text file.

```text
line-editor> load journal.txt
Loaded 2 lines from journal.txt.
```

Loading is a modifying operation and can be undone once with `undo`.

### `search <text>`

Find a substring in every line and print matching lines and their numbers. Search is case-sensitive.

```text
line-editor> search line
1 | First line
2 | Last line
Matches: 2
```

### `stats`

Print total lines, words, and characters. A word is a sequence of non-whitespace characters. Line-ending characters are not counted as document characters.

```text
line-editor> stats
Lines: 2
Words: 4
Characters: 20
```

### `undo`

Restore the document state immediately before the most recent insert, delete, or load. Only one undo level is stored.

```text
line-editor> undo
Last action undone.
line-editor> undo
Nothing to undo.
```

Saving, displaying, searching, and showing statistics do not clear the available undo state because they do not modify the document.

### `help`

Print the command list.

### `quit` or `exit`

Release all allocated memory and leave the editor.

```text
line-editor> quit
```

## Invalid input examples

```text
line-editor> delete abc
Usage: delete <line>
line-editor> insert -2 Text
Usage: insert <line> <text>
line-editor> save notes.doc
Error: file name must end with .txt.
```

The command loop continues after these errors.
