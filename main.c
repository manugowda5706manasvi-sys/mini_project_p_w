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
void display_document(const Document *document);
int save_document(const Document *document, const char *filename);
int load_document(Document *document, UndoState *undo_state, const char *filename);
void search_document(const Document *document, const char *query);
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

void show_statistics(const Document *document)
{
    int words = 0;
    size_t characters = 0;

    for (int index = 0; index < document->size; index++)
    {
        words += count_words(document->lines[index]);
        characters += strlen(document->lines[index]);
    }

    printf("Lines: %d\n", document->size);
    printf("Words: %d\n", words);
    printf("Characters: %zu\n", characters);
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
    printf("  insert <line> <text>  Insert text at a 1-based line number.\n");
    printf("  delete <line>         Delete a line.\n");
    printf("  display               Print all lines with numbers.\n");
    printf("  save <file.txt>       Save the document.\n");
    printf("  load <file.txt>       Replace the document with a file.\n");
    printf("  search <text>         Print lines containing text.\n");
    printf("  stats                 Print line, word, and character counts.\n");
    printf("  undo                  Undo the most recent insert, delete, or load.\n");
    printf("  help                  Show this help.\n");
    printf("  quit                  Exit the editor.\n");
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
    else if (command_matches(command, "stats"))
    {
        show_statistics(document);
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
