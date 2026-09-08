#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define INITIAL_CAPACITY 8
#define COMMAND_PROMPT "line-editor> "

/*
 * A dynamic array gives direct line-number access and keeps display, save,
 * and load simple. Its trade-off is O(n) pointer shifting for middle insert
 * and delete operations; a linked list would avoid the shift but would add
 * per-node link storage and still require O(n) traversal to find line n.
 */
typedef struct
{
    char **lines;
    int size;
    int capacity;
} Document;

typedef struct
{
    Document snapshot;
    int available;
} UndoState;

/* Document lifecycle and storage */
void initialize_document(Document *document);
void free_document(Document *document);
int copy_document(const Document *source, Document *destination);
int ensure_capacity(Document *document, int required_capacity);
int append_line(Document *document, const char *text);

/* Undo supports one most-recent modifying operation. */
void initialize_undo(UndoState *undo_state);
void clear_undo(UndoState *undo_state);
int save_undo_state(const Document *document, UndoState *undo_state);
int undo_last_action(Document *document, UndoState *undo_state);

/* Core and bonus features */
int insert_line(Document *document, UndoState *undo_state, int line_number,
                const char *text);
int delete_line(Document *document, UndoState *undo_state, int line_number);
int append_line_command(Document *document, UndoState *undo_state,
                        const char *text);
int replace_line(Document *document, UndoState *undo_state, int line_number,
                 const char *old_text, const char *new_text);
int replace_all(Document *document, UndoState *undo_state,
                const char *old_text, const char *new_text);
void display_document(const Document *document);
int save_document(const Document *document, const char *filename);
int load_document(Document *document, UndoState *undo_state, const char *filename);
void search_document(const Document *document, const char *query);
void count_document(const Document *document);
void show_statistics(const Document *document);

/* Input, parsing, and command processing */
int read_line(FILE *stream, char **result);
char *skip_spaces(char *text);
int parse_positive_integer(char **cursor, int *value);
int has_txt_extension(const char *filename);
void print_help(void);
int process_command(char *input, Document *document, UndoState *undo_state);

static int duplicate_string(const char *source, char **destination)
{
    size_t length = strlen(source);
    char *copy = malloc(length + 1);

    if (copy == NULL)
    {
        return 0;
    }

    memcpy(copy, source, length + 1);
    *destination = copy;
    return 1;
}

void initialize_document(Document *document)
{
    document->lines = NULL;
    document->size = 0;
    document->capacity = 0;
}

void free_document(Document *document)
{
    int index;

    for (index = 0; index < document->size; index++)
    {
        free(document->lines[index]);
    }

    free(document->lines);
    initialize_document(document);
}

int copy_document(const Document *source, Document *destination)
{
    int index;

    initialize_document(destination);
    destination->size = source->size;
    destination->capacity = source->capacity;

    if (source->capacity > 0)
    {
        destination->lines = malloc((size_t)source->capacity * sizeof(*destination->lines));
        if (destination->lines == NULL)
        {
            initialize_document(destination);
            return 0;
        }
    }

    for (index = 0; index < source->size; index++)
    {
        if (!duplicate_string(source->lines[index], &destination->lines[index]))
        {
            free_document(destination);
            return 0;
        }
    }

    return 1;
}

int ensure_capacity(Document *document, int required_capacity)
{
    char **larger_lines;
    int new_capacity;

    if (required_capacity <= document->capacity)
    {
        return 1;
    }

    new_capacity = document->capacity == 0 ? INITIAL_CAPACITY : document->capacity;
    while (new_capacity < required_capacity)
    {
        if (new_capacity > INT_MAX / 2)
        {
            new_capacity = required_capacity;
            break;
        }
        new_capacity *= 2;
    }

    larger_lines = realloc(document->lines,
                           (size_t)new_capacity * sizeof(*larger_lines));
    if (larger_lines == NULL)
    {
        return 0;
    }

    document->lines = larger_lines;
    document->capacity = new_capacity;
    return 1;
}

int append_line(Document *document, const char *text)
{
    char *copy;

    if (!ensure_capacity(document, document->size + 1))
    {
        return 0;
    }

    if (!duplicate_string(text, &copy))
    {
        return 0;
    }

    document->lines[document->size] = copy;
    document->size++;
    return 1;
}

void initialize_undo(UndoState *undo_state)
{
    initialize_document(&undo_state->snapshot);
    undo_state->available = 0;
}

void clear_undo(UndoState *undo_state)
{
    free_document(&undo_state->snapshot);
    undo_state->available = 0;
}

int save_undo_state(const Document *document, UndoState *undo_state)
{
    Document new_snapshot;

    if (!copy_document(document, &new_snapshot))
    {
        fprintf(stderr, "Error: unable to allocate undo snapshot.\n");
        return 0;
    }

    clear_undo(undo_state);
    undo_state->snapshot = new_snapshot;
    undo_state->available = 1;
    return 1;
}

int undo_last_action(Document *document, UndoState *undo_state)
{
    Document current_document;

    if (!undo_state->available)
    {
        return 0;
    }

    current_document = *document;
    *document = undo_state->snapshot;
    initialize_document(&undo_state->snapshot);
    undo_state->available = 0;
    free_document(&current_document);
    return 1;
}

int insert_line(Document *document, UndoState *undo_state, int line_number,
                const char *text)
{
    char *new_line;
    int index;

    if (line_number < 1 || line_number > document->size + 1)
    {
        fprintf(stderr, "Error: line number must be from 1 to %d.\n",
                document->size + 1);
        return 0;
    }

    if (!duplicate_string(text, &new_line))
    {
        fprintf(stderr, "Error: unable to allocate the new line.\n");
        return 0;
    }

    if (!save_undo_state(document, undo_state))
    {
        free(new_line);
        return 0;
    }

    if (!ensure_capacity(document, document->size + 1))
    {
        fprintf(stderr, "Error: unable to grow the document.\n");
        clear_undo(undo_state);
        free(new_line);
        return 0;
    }

    index = line_number - 1;
    for (int current = document->size; current > index; current--)
    {
        document->lines[current] = document->lines[current - 1];
    }

    document->lines[index] = new_line;
    document->size++;
    return 1;
}

int delete_line(Document *document, UndoState *undo_state, int line_number)
{
    int index;

    if (line_number < 1 || line_number > document->size)
    {
        fprintf(stderr, "Error: line number must be from 1 to %d.\n",
                document->size);
        return 0;
    }

    if (!save_undo_state(document, undo_state))
    {
        return 0;
    }

    index = line_number - 1;
    free(document->lines[index]);

    for (int current = index; current < document->size - 1; current++)
    {
        document->lines[current] = document->lines[current + 1];
    }

    document->size--;
    document->lines[document->size] = NULL;
    return 1;
}

int append_line_command(Document *document, UndoState *undo_state,
                        const char *text)
{
    if (text == NULL || *text == '\0')
    {
        fprintf(stderr, "Error: append text cannot be empty.\n");
        return 0;
    }

    if (!save_undo_state(document, undo_state))
    {
        return 0;
    }

    if (!append_line(document, text))
    {
        fprintf(stderr, "Error: unable to append the new line.\n");
        clear_undo(undo_state);
        return 0;
    }

    return 1;
}

static char *replace_first_occurrence(const char *source, const char *old_text,
                                      const char *new_text)
{
    const char *match;
    size_t source_length;
    size_t old_length;
    size_t new_length;
    size_t prefix_length;
    size_t result_length;
    char *result;

    if (old_text == NULL || *old_text == '\0')
    {
        return NULL;
    }

    old_length = strlen(old_text);
    new_length = strlen(new_text);
    source_length = strlen(source);
    match = strstr(source, old_text);
    if (match == NULL)
    {
        return NULL;
    }

    prefix_length = (size_t)(match - source);
    result_length = source_length + 1 + (new_length > old_length ? (new_length - old_length) : 0);
    result = malloc(result_length);
    if (result == NULL)
    {
        return NULL;
    }

    memcpy(result, source, prefix_length);
    memcpy(result + prefix_length, new_text, new_length);
    memcpy(result + prefix_length + new_length,
           match + old_length,
           source_length - prefix_length - old_length + 1);
    return result;
}

static char *replace_all_occurrences(const char *source, const char *old_text,
                                     const char *new_text, int *matches)
{
    const char *cursor;
    const char *match;
    size_t source_length;
    size_t old_length;
    size_t new_length;
    size_t total_length;
    char *result;
    char *write;
    int count;

    if (old_text == NULL || *old_text == '\0')
    {
        *matches = 0;
        return NULL;
    }

    source_length = strlen(source);
    old_length = strlen(old_text);
    new_length = strlen(new_text);
    cursor = source;
    count = 0;
    while ((match = strstr(cursor, old_text)) != NULL)
    {
        count++;
        cursor = match + old_length;
    }

    if (count == 0)
    {
        *matches = 0;
        return NULL;
    }

    total_length = source_length + 1;
    if (new_length > old_length)
    {
        total_length += (size_t)count * (new_length - old_length);
    }
    else if (old_length > new_length)
    {
        total_length -= (size_t)count * (old_length - new_length);
    }

    result = malloc(total_length);
    if (result == NULL)
    {
        *matches = -1;
        return NULL;
    }

    write = result;
    cursor = source;
    while (*cursor != '\0')
    {
        match = strstr(cursor, old_text);
        if (match == NULL)
        {
            size_t tail_length = strlen(cursor);
            memcpy(write, cursor, tail_length + 1);
            break;
        }

        {
            size_t prefix_length = (size_t)(match - cursor);
            memcpy(write, cursor, prefix_length);
            write += prefix_length;
            memcpy(write, new_text, new_length);
            write += new_length;
            cursor = match + old_length;
        }
    }

    *write = '\0';
    *matches = count;
    return result;
}

int replace_line(Document *document, UndoState *undo_state, int line_number,
                 const char *old_text, const char *new_text)
{
    char *updated_line;
    int index;

    if (line_number < 1 || line_number > document->size)
    {
        fprintf(stderr, "Error: line number must be from 1 to %d.\n",
                document->size);
        return 0;
    }

    if (old_text == NULL || *old_text == '\0')
    {
        fprintf(stderr, "Error: replacement text cannot be empty.\n");
        return 0;
    }

    if (!save_undo_state(document, undo_state))
    {
        return 0;
    }

    index = line_number - 1;
    updated_line = replace_first_occurrence(document->lines[index], old_text,
                                            new_text);
    if (updated_line == NULL)
    {
        fprintf(stderr, "Error: the text was not found on line %d.\n",
                line_number);
        clear_undo(undo_state);
        return 0;
    }

    free(document->lines[index]);
    document->lines[index] = updated_line;
    return 1;
}

int replace_all(Document *document, UndoState *undo_state,
                const char *old_text, const char *new_text)
{
    int total_matches = 0;
    int index;

    if (old_text == NULL || *old_text == '\0')
    {
        fprintf(stderr, "Error: replacement text cannot be empty.\n");
        return 0;
    }

    if (!save_undo_state(document, undo_state))
    {
        return 0;
    }

    for (index = 0; index < document->size; index++)
    {
        int matches_this_line = 0;
        char *updated_line = replace_all_occurrences(document->lines[index], old_text,
                                                     new_text, &matches_this_line);

        if (matches_this_line < 0)
        {
            fprintf(stderr, "Error: unable to allocate memory for replacement.\n");
            clear_undo(undo_state);
            return 0;
        }

        if (matches_this_line > 0)
        {
            total_matches += matches_this_line;
            free(document->lines[index]);
            document->lines[index] = updated_line;
        }
    }

    if (total_matches == 0)
    {
        printf("No matching text found.\n");
        clear_undo(undo_state);
        return 0;
    }

    return 1;
}

void display_document(const Document *document)
{
    if (document->size == 0)
    {
        printf("[empty document]\n");
        return;
    }

    for (int index = 0; index < document->size; index++)
    {
        printf("%d | %s\n", index + 1, document->lines[index]);
    }
}

int has_txt_extension(const char *filename)
{
    size_t length = strlen(filename);

    if (length < 4)
    {
        return 0;
    }

    return filename[length - 4] == '.' &&
           tolower((unsigned char)filename[length - 3]) == 't' &&
           tolower((unsigned char)filename[length - 2]) == 'x' &&
           tolower((unsigned char)filename[length - 1]) == 't';
}

int save_document(const Document *document, const char *filename)
{
    FILE *file;

    if (!has_txt_extension(filename))
    {
        fprintf(stderr, "Error: file name must end with .txt.\n");
        return 0;
    }

    file = fopen(filename, "w");
    if (file == NULL)
    {
        perror("Error opening file for writing");
        return 0;
    }

    for (int index = 0; index < document->size; index++)
    {
        if (fprintf(file, "%s\n", document->lines[index]) < 0)
        {
            perror("Error writing file");
            fclose(file);
            return 0;
        }
    }

    if (fclose(file) != 0)
    {
        perror("Error closing file");
        return 0;
    }

    return 1;
}

int read_line(FILE *stream, char **result)
{
    char *line = NULL;
    size_t length = 0;
    size_t capacity = 0;
    int character;

    while ((character = fgetc(stream)) != '\n' && character != EOF)
    {
        char *larger_line;

        if (length + 1 >= capacity)
        {
            size_t new_capacity = capacity == 0 ? 64 : capacity * 2;

            larger_line = realloc(line, new_capacity);
            if (larger_line == NULL)
            {
                free(line);
                return -1;
            }
            line = larger_line;
            capacity = new_capacity;
        }

        line[length++] = (char)character;
    }

    if (character == EOF && length == 0)
    {
        free(line);
        return 0;
    }

    if (line == NULL)
    {
        line = malloc(1);
        if (line == NULL)
        {
            return -1;
        }
    }

    line[length] = '\0';
    *result = line;
    return 1;
}

int load_document(Document *document, UndoState *undo_state, const char *filename)
{
    Document loaded_document;
    FILE *file;
    char *line = NULL;
    int read_result;

    if (!has_txt_extension(filename))
    {
        fprintf(stderr, "Error: file name must end with .txt.\n");
        return 0;
    }

    file = fopen(filename, "r");
    if (file == NULL)
    {
        perror("Error opening file for reading");
        return 0;
    }

    initialize_document(&loaded_document);
    while ((read_result = read_line(file, &line)) == 1)
    {
        if (!append_line(&loaded_document, line))
        {
            fprintf(stderr, "Error: unable to load the complete file.\n");
            free(line);
            fclose(file);
            free_document(&loaded_document);
            return 0;
        }
        free(line);
        line = NULL;
    }

    if (read_result < 0)
    {
        fprintf(stderr, "Error: unable to allocate memory while loading.\n");
        fclose(file);
        free_document(&loaded_document);
        return 0;
    }

    if (fclose(file) != 0)
    {
        perror("Error closing file");
        free_document(&loaded_document);
        return 0;
    }

    if (!save_undo_state(document, undo_state))
    {
        free_document(&loaded_document);
        return 0;
    }

    free_document(document);
    *document = loaded_document;
    return 1;
}

void search_document(const Document *document, const char *query)
{
    int matches = 0;

    if (query[0] == '\0')
    {
        fprintf(stderr, "Error: search text cannot be empty.\n");
        return;
    }

    for (int index = 0; index < document->size; index++)
    {
        if (strstr(document->lines[index], query) != NULL)
        {
            printf("%d | %s\n", index + 1, document->lines[index]);
            matches++;
        }
    }

    if (matches == 0)
    {
        printf("No matching lines found.\n");
    }
    else
    {
        printf("Matches: %d\n", matches);
    }
}

static int count_words(const char *text)
{
    int words = 0;
    int inside_word = 0;

    while (*text != '\0')
    {
        if (isspace((unsigned char)*text))
        {
            inside_word = 0;
        }
        else if (!inside_word)
        {
            words++;
            inside_word = 1;
        }
        text++;
    }

    return words;
}

void count_document(const Document *document)
{
    int total_words = 0;

    for (int index = 0; index < document->size; index++)
    {
        total_words += count_words(document->lines[index]);
    }

    printf("Total lines: %d\n", document->size);
    printf("Total words: %d\n", total_words);
}

void show_statistics(const Document *document)
{
    count_document(document);
}

char *skip_spaces(char *text)
{
    while (isspace((unsigned char)*text))
    {
        text++;
    }
    return text;
}

int parse_positive_integer(char **cursor, int *value)
{
    char *end;
    long parsed_value;

    *cursor = skip_spaces(*cursor);
    if (**cursor == '\0' || **cursor == '-')
    {
        return 0;
    }

    parsed_value = strtol(*cursor, &end, 10);
    if (end == *cursor || parsed_value < 1 || parsed_value > INT_MAX)
    {
        return 0;
    }

    *value = (int)parsed_value;
    *cursor = end;
    return 1;
}

void print_help(void)
{
    printf("Commands:\n");
    printf("  insert <line_number> <text>   Insert text at a 1-based line number.\n");
    printf("  delete <line_number>          Delete a line.\n");
    printf("  display                       Print all lines with numbers.\n");
    printf("  append <text>                 Append a new line at the end.\n");
    printf("  save <file.txt>               Save the document.\n");
    printf("  load <file.txt>               Replace the document with a file.\n");
    printf("  search <text>                 Print lines containing text.\n");
    printf("  replace <line> <old> <new>    Replace the first matching text on a line.\n");
    printf("  replaceall <old> <new>        Replace all matching text in the document.\n");
    printf("  undo                          Undo the most recent document-changing action.\n");
    printf("  count                         Show total lines and total words.\n");
    printf("  help                          Show this help.\n");
    printf("  exit / quit                   Exit the editor.\n");
}

static int command_matches(const char *command, const char *expected)
{
    return strcmp(command, expected) == 0;
}

int process_command(char *input, Document *document, UndoState *undo_state)
{
    char *command;
    char *arguments;
    char *separator;
    int line_number;

    command = skip_spaces(input);
    if (*command == '\0')
    {
        return 1;
    }

    separator = command;
    while (*separator != '\0' && !isspace((unsigned char)*separator))
    {
        separator++;
    }

    if (*separator != '\0')
    {
        *separator = '\0';
        arguments = skip_spaces(separator + 1);
    }
    else
    {
        arguments = separator;
    }

    if (command_matches(command, "insert"))
    {
        char *text = arguments;

        if (!parse_positive_integer(&text, &line_number))
        {
            fprintf(stderr, "Usage: insert <line> <text>\n");
            return 1;
        }
        text = skip_spaces(text);
        if (!insert_line(document, undo_state, line_number, text))
        {
            return 1;
        }
        printf("Inserted line %d.\n", line_number);
    }
    else if (command_matches(command, "delete"))
    {
        char *cursor = arguments;

        if (!parse_positive_integer(&cursor, &line_number) ||
            *skip_spaces(cursor) != '\0')
        {
            fprintf(stderr, "Usage: delete <line>\n");
            return 1;
        }
        if (delete_line(document, undo_state, line_number))
        {
            printf("Deleted line %d.\n", line_number);
        }
    }
    else if (command_matches(command, "append"))
    {
        if (*arguments == '\0')
        {
            fprintf(stderr, "Usage: append <text>\n");
        }
        else if (append_line_command(document, undo_state, arguments))
        {
            printf("Appended a new line.\n");
        }
    }
    else if (command_matches(command, "display"))
    {
        display_document(document);
    }
    else if (command_matches(command, "save"))
    {
        if (*arguments == '\0')
        {
            fprintf(stderr, "Usage: save <file.txt>\n");
        }
        else if (save_document(document, arguments))
        {
            printf("Saved %d lines to %s.\n", document->size, arguments);
        }
    }
    else if (command_matches(command, "load"))
    {
        if (*arguments == '\0')
        {
            fprintf(stderr, "Usage: load <file.txt>\n");
        }
        else if (load_document(document, undo_state, arguments))
        {
            printf("Loaded %d lines from %s.\n", document->size, arguments);
        }
    }
    else if (command_matches(command, "search"))
    {
        search_document(document, arguments);
    }
    else if (command_matches(command, "replace"))
    {
        char *cursor = arguments;
        char *old_text;
        char *new_text;

        if (!parse_positive_integer(&cursor, &line_number))
        {
            fprintf(stderr, "Usage: replace <line> <old_text> <new_text>\n");
            return 1;
        }

        cursor = skip_spaces(cursor);
        if (*cursor == '\0')
        {
            fprintf(stderr, "Usage: replace <line> <old_text> <new_text>\n");
            return 1;
        }

        old_text = cursor;
        while (*cursor != '\0' && !isspace((unsigned char)*cursor))
        {
            cursor++;
        }

        if (*cursor == '\0')
        {
            fprintf(stderr, "Usage: replace <line> <old_text> <new_text>\n");
            return 1;
        }

        *cursor = '\0';
        new_text = skip_spaces(cursor + 1);
        if (*new_text == '\0')
        {
            fprintf(stderr, "Usage: replace <line> <old_text> <new_text>\n");
            return 1;
        }

        if (replace_line(document, undo_state, line_number, old_text, new_text))
        {
            printf("Replaced text on line %d.\n", line_number);
        }
    }
    else if (command_matches(command, "replaceall"))
    {
        char *cursor = arguments;
        char *old_text;
        char *new_text;

        cursor = skip_spaces(cursor);
        if (*cursor == '\0')
        {
            fprintf(stderr, "Usage: replaceall <old_text> <new_text>\n");
            return 1;
        }

        old_text = cursor;
        while (*cursor != '\0' && !isspace((unsigned char)*cursor))
        {
            cursor++;
        }

        if (*cursor == '\0')
        {
            fprintf(stderr, "Usage: replaceall <old_text> <new_text>\n");
            return 1;
        }

        *cursor = '\0';
        new_text = skip_spaces(cursor + 1);
        if (*new_text == '\0')
        {
            fprintf(stderr, "Usage: replaceall <old_text> <new_text>\n");
            return 1;
        }

        if (replace_all(document, undo_state, old_text, new_text))
        {
            printf("Replaced all occurrences.\n");
        }
    }
    else if (command_matches(command, "count") || command_matches(command, "stats"))
    {
        count_document(document);
    }
    else if (command_matches(command, "undo"))
    {
        if (undo_last_action(document, undo_state))
        {
            printf("Last action undone.\n");
        }
        else
        {
            printf("Nothing to undo.\n");
        }
    }
    else if (command_matches(command, "help"))
    {
        print_help();
    }
    else if (command_matches(command, "quit") || command_matches(command, "exit"))
    {
        return 0;
    }
    else
    {
        fprintf(stderr, "Unknown command: %s (type help)\n", command);
    }

    return 1;
}

int main(int argc, char **argv)
{
    Document document;
    UndoState undo_state;
    char *input = NULL;
    int read_result;
    int running = 1;

    initialize_document(&document);
    initialize_undo(&undo_state);

    if (argc > 2)
    {
        fprintf(stderr, "Usage: %s [file.txt]\n", argv[0]);
        return EXIT_FAILURE;
    }

    if (argc == 2 && !load_document(&document, &undo_state, argv[1]))
    {
        clear_undo(&undo_state);
        free_document(&document);
        return EXIT_FAILURE;
    }
    clear_undo(&undo_state);

    printf("Simple Line Editor. Type help for commands.\n");
    while (running)
    {
        printf("%s", COMMAND_PROMPT);
        fflush(stdout);
        read_result = read_line(stdin, &input);

        if (read_result == 0)
        {
            putchar('\n');
            break;
        }
        if (read_result < 0)
        {
            fprintf(stderr, "Error: unable to read command.\n");
            break;
        }

        running = process_command(input, &document, &undo_state);
        free(input);
        input = NULL;
    }

    clear_undo(&undo_state);
    free_document(&document);
    return EXIT_SUCCESS;
}
