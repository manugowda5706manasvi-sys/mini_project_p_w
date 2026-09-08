# Paper Design Outline

## 1. Data structures

```text
Document
+----------------+
| lines ------+  |----> [char *] [char *] [char *] ...
| size        |  |
| capacity    |  |
+----------------+

Each char * points to a separately allocated, null-terminated line.

UndoState
+----------------------+
| snapshot: Document   |
| available: 0 or 1    |
+----------------------+
```

The pointer array is contiguous. The text buffers are separate heap blocks. The document grows by `realloc()` when `size == capacity`.

## 2. Function signatures

```c
void initialize_document(Document *document);
void free_document(Document *document);
int copy_document(const Document *source, Document *destination);
int ensure_capacity(Document *document, int required_capacity);
int append_line(Document *document, const char *text);

int insert_line(Document *document, UndoState *undo_state,
                int line_number, const char *text);
int delete_line(Document *document, UndoState *undo_state, int line_number);
void display_document(const Document *document);
int save_document(const Document *document, const char *filename);
int load_document(Document *document, UndoState *undo_state,
                  const char *filename);
void search_document(const Document *document, const char *query);
void show_statistics(const Document *document);

int save_undo_state(const Document *document, UndoState *undo_state);
int undo_last_action(Document *document, UndoState *undo_state);
int process_command(char *input, Document *document, UndoState *undo_state);
```

## 3. Insert algorithm

```text
INSERT(line_number, text)
1. Check that 1 <= line_number <= size + 1.
2. Allocate and copy the new text.
3. Save a deep-copy undo snapshot.
4. Grow the pointer array if capacity is full.
5. Convert the user number to index = line_number - 1.
6. For current = size down to index + 1:
       lines[current] = lines[current - 1]
7. Set lines[index] to the new string.
8. Increase size.
```

Before insertion at line 2:

```text
index:       0             1             2
lines:    ["one"]       ["two"]       ["three"]
                         ^ insert here
```

After shifting and placing the new pointer:

```text
index:       0             1             2             3
lines:    ["one"]    ["new line"]    ["two"]       ["three"]
```

## 4. Delete algorithm

```text
DELETE(line_number)
1. Check that 1 <= line_number <= size.
2. Save a deep-copy undo snapshot.
3. Convert index = line_number - 1.
4. Free lines[index].
5. For current = index to size - 2:
       lines[current] = lines[current + 1]
6. Decrease size.
7. Set the unused final pointer to NULL.
```

Before deleting line 2:

```text
index:       0             1             2
lines:    ["one"]       ["two"]       ["three"]
```

After freeing and shifting:

```text
index:       0             1
lines:    ["one"]       ["three"]
size = 2
```

## 5. Save and load algorithm

```text
SAVE
1. Validate the .txt suffix.
2. Open the file for writing.
3. Write every string followed by a newline.
4. Close the file and report errors.

LOAD
1. Validate the .txt suffix and open the file.
2. Build a separate temporary Document line by line.
3. If any allocation or read error occurs, free the temporary document.
4. Save an undo snapshot of the current document.
5. Free the current document and move the temporary document into its place.
```

## 6. Undo ownership rule

A modifying operation must call `save_undo_state()` immediately before its first document mutation. `insert_line`, `delete_line`, and `load_document` do this internally. The snapshot is a deep copy, so every string has independent storage. Restoring swaps the snapshot into the document, frees the replaced document, and marks the undo slot unavailable.

## 7. Complexity

- Display: `O(n)` lines.
- Search: `O(n * average_line_length)` for substring search.
- Count: `O(total characters)`.
- Insert/delete after validation: `O(n)` pointer shifts.
- Save/load: `O(total file characters)`.
- Undo snapshot: `O(total document characters)` time and memory.
