#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define INPUT_BUFFER_SIZE 1024

typedef struct
{
    char **lines;
    int size;
    int capacity;
} Document;

typedef struct
{
    char **lines;
    int size;
    int capacity;
    int available;
} UndoState;

/* Document and undo-state helpers */
void initializeDocument(Document *document);
void freeDocument(Document *document);
void initializeUndoState(UndoState *undoState);
void clearUndoState(UndoState *undoState);
void freeUndoState(UndoState *undoState);
int duplicateString(const char *source, char **destination);
int copyDocument(const Document *source, Document *destination);
int saveUndoState(const Document *document, UndoState *undoState);
int undoLastAction(Document *document, UndoState *undoState);

/* The four requested features */
void insertLine(Document *document, UndoState *undoState);
void deleteLine(Document *document, UndoState *undoState);
void undo(Document *document, UndoState *undoState);
void showLineAndWordCount(const Document *document);

/* Input and allocation helpers */
int readInteger(const char *prompt, int *value);
char *readText(const char *prompt);
int ensureCapacity(Document *document, int requiredCapacity);
int countWords(const char *text);
void displayDocument(const Document *document);
int saveDocument(const Document *document, const char *filename);
int loadDocument(Document *document, UndoState *undoState, const char *filename);
void searchDocument(const Document *document, const char *text);
int appendDocument(Document *document, UndoState *undoState, const char *text);
int insertLineAt(Document *document, UndoState *undoState, int lineNumber, const char *text);
int deleteLineByNumber(Document *document, UndoState *undoState, int lineNumber);
int replaceLine(Document *document, UndoState *undoState, int lineNumber, const char *oldText, const char *newText);
int replaceAll(Document *document, UndoState *undoState, const char *oldText, const char *newText);
void printHelp(void);
int processCommand(char *input, Document *document, UndoState *undoState);

void initializeDocument(Document *document)
{
    document->lines = NULL;
    document->size = 0;
    document->capacity = 0;
}

void freeDocument(Document *document)
{
    int index;

    for (index = 0; index < document->size; index++)
    {
        free(document->lines[index]);
    }

    free(document->lines);
    initializeDocument(document);
}

void initializeUndoState(UndoState *undoState)
{
    undoState->lines = NULL;
    undoState->size = 0;
    undoState->capacity = 0;
    undoState->available = 0;
}

void clearUndoState(UndoState *undoState)
{
    int index;

    for (index = 0; index < undoState->size; index++)
    {
        free(undoState->lines[index]);
    }

    free(undoState->lines);
    initializeUndoState(undoState);
}

void freeUndoState(UndoState *undoState)
{
    clearUndoState(undoState);
}

int duplicateString(const char *source, char **destination)
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

int copyDocument(const Document *source, Document *destination)
{
    int index;

    initializeDocument(destination);
    destination->size = source->size;
    destination->capacity = source->capacity;

    if (source->capacity > 0)
    {
        destination->lines = malloc((size_t)source->capacity * sizeof(*destination->lines));
        if (destination->lines == NULL)
        {
            initializeDocument(destination);
            return 0;
        }
    }

    for (index = 0; index < source->size; index++)
    {
        if (!duplicateString(source->lines[index], &destination->lines[index]))
        {
            freeDocument(destination);
            return 0;
        }
    }

    return 1;
}

int saveUndoState(const Document *document, UndoState *undoState)
{
    Document snapshot;

    if (!copyDocument(document, &snapshot))
    {
        fprintf(stderr, "Error: could not save undo state.\n");
        return 0;
    }

    clearUndoState(undoState);
    undoState->lines = snapshot.lines;
    undoState->size = snapshot.size;
    undoState->capacity = snapshot.capacity;
    undoState->available = 1;
    return 1;
}

int undoLastAction(Document *document, UndoState *undoState)
{
    Document currentDocument;
    char **savedLines;
    int savedSize;
    int savedCapacity;

    if (!undoState->available)
    {
        return 0;
    }

    currentDocument = *document;
    savedLines = undoState->lines;
    savedSize = undoState->size;
    savedCapacity = undoState->capacity;

    document->lines = savedLines;
    document->size = savedSize;
    document->capacity = savedCapacity;

    undoState->lines = NULL;
    undoState->size = 0;
    undoState->capacity = 0;
    undoState->available = 0;

    freeDocument(&currentDocument);
    return 1;
}

int readInteger(const char *prompt, int *value)
{
    char input[INPUT_BUFFER_SIZE];
    char extra;

    printf("%s", prompt);
    if (fgets(input, sizeof(input), stdin) == NULL)
    {
        return 0;
    }

    if (sscanf(input, " %d %c", value, &extra) != 1)
    {
        printf("Invalid number.\n");
        return 0;
    }

    return 1;
}

char *readText(const char *prompt)
{
    char *text = NULL;
    size_t length = 0;
    size_t capacity = 0;
    int character;

    printf("%s", prompt);

    while ((character = fgetc(stdin)) != '\n' && character != EOF)
    {
        char *largerText;

        if (length + 1 >= capacity)
        {
            size_t newCapacity = capacity == 0 ? 16 : capacity * 2;
            largerText = realloc(text, newCapacity);
            if (largerText == NULL)
            {
                free(text);
                return NULL;
            }
            text = largerText;
            capacity = newCapacity;
        }

        text[length++] = (char)character;
    }

    if (text == NULL)
    {
        text = malloc(1);
        if (text == NULL)
        {
            return NULL;
        }
    }

    text[length] = '\0';
    return text;
}

int ensureCapacity(Document *document, int requiredCapacity)
{
    char **largerLines;
    int newCapacity;

    if (document->capacity >= requiredCapacity)
    {
        return 1;
    }

    newCapacity = document->capacity == 0 ? 4 : document->capacity * 2;
    while (newCapacity < requiredCapacity)
    {
        newCapacity *= 2;
    }

    largerLines = realloc(document->lines, (size_t)newCapacity * sizeof(*largerLines));
    if (largerLines == NULL)
    {
        return 0;
    }

    document->lines = largerLines;
    document->capacity = newCapacity;
    return 1;
}

void insertLine(Document *document, UndoState *undoState)
{
    int lineNumber;
    int index;
    char *newLine;

    if (!readInteger("Enter line number (1 to size + 1): ", &lineNumber))
    {
        return;
    }

    if (lineNumber < 1 || lineNumber > document->size + 1)
    {
        printf("Invalid line number.\n");
        return;
    }

    newLine = readText("Enter line text: ");
    if (newLine == NULL)
    {
        fprintf(stderr, "Error: could not allocate memory for the new line.\n");
        return;
    }

    if (!saveUndoState(document, undoState))
    {
        free(newLine);
        return;
    }

    if (!ensureCapacity(document, document->size + 1))
    {
        fprintf(stderr, "Error: could not increase document capacity.\n");
        clearUndoState(undoState);
        free(newLine);
        return;
    }

    index = lineNumber - 1;
    for (int current = document->size; current > index; current--)
    {
        document->lines[current] = document->lines[current - 1];
    }

    document->lines[index] = newLine;
    document->size++;
    printf("Line inserted.\n");
}

void deleteLine(Document *document, UndoState *undoState)
{
    int lineNumber;
    int index;
    int current;

    if (document->size == 0)
    {
        printf("The document is empty.\n");
        return;
    }

    if (!readInteger("Enter line number to delete: ", &lineNumber))
    {
        return;
    }

    if (lineNumber < 1 || lineNumber > document->size)
    {
        printf("Invalid line number.\n");
        return;
    }

    if (!saveUndoState(document, undoState))
    {
        return;
    }

    index = lineNumber - 1;
    free(document->lines[index]);

    for (current = index; current < document->size - 1; current++)
    {
        document->lines[current] = document->lines[current + 1];
    }

    document->size--;
    document->lines[document->size] = NULL;
    printf("Line deleted.\n");
}

void undo(Document *document, UndoState *undoState)
{
    if (!undoLastAction(document, undoState))
    {
        printf("Nothing to undo\n");
        return;
    }

    printf("Last action undone.\n");
}

int countWords(const char *text)
{
    int words = 0;
    int insideWord = 0;

    while (*text != '\0')
    {
        if (isspace((unsigned char)*text))
        {
            insideWord = 0;
        }
        else if (!insideWord)
        {
            words++;
            insideWord = 1;
        }
        text++;
    }

    return words;
}

void showLineAndWordCount(const Document *document)
{
    int totalWords = 0;
    int index;

    for (index = 0; index < document->size; index++)
    {
        totalWords += countWords(document->lines[index]);
    }

    printf("Total lines: %d\n", document->size);
    printf("Total words: %d\n", totalWords);
}

static char *trimWhitespace(char *text)
{
    char *end;

    while (*text != '\0' && isspace((unsigned char)*text))
    {
        text++;
    }

    end = text + strlen(text);
    while (end > text && isspace((unsigned char)*(end - 1)))
    {
        end--;
    }
    *end = '\0';

    return text;
}

static char *readCommandLine(FILE *stream)
{
    char *line = NULL;
    size_t length = 0;
    size_t capacity = 0;
    int character;

    while ((character = fgetc(stream)) != '\n' && character != EOF)
    {
        char *largerLine;

        if (length + 1 >= capacity)
        {
            size_t newCapacity = capacity == 0 ? 64 : capacity * 2;
            largerLine = realloc(line, newCapacity);
            if (largerLine == NULL)
            {
                free(line);
                return NULL;
            }
            line = largerLine;
            capacity = newCapacity;
        }

        line[length++] = (char)character;
    }

    if (character == EOF && length == 0)
    {
        free(line);
        return NULL;
    }

    if (line == NULL)
    {
        line = malloc(1);
        if (line == NULL)
        {
            return NULL;
        }
    }

    line[length] = '\0';
    return line;
}

static char *readFileLine(FILE *stream)
{
    char *line = NULL;
    size_t length = 0;
    size_t capacity = 0;
    int character;

    while ((character = fgetc(stream)) != '\n' && character != EOF)
    {
        char *largerLine;

        if (length + 1 >= capacity)
        {
            size_t newCapacity = capacity == 0 ? 64 : capacity * 2;
            largerLine = realloc(line, newCapacity);
            if (largerLine == NULL)
            {
                free(line);
                return NULL;
            }
            line = largerLine;
            capacity = newCapacity;
        }

        line[length++] = (char)character;
    }

    if (character == EOF && length == 0)
    {
        free(line);
        return NULL;
    }

    if (line == NULL)
    {
        line = malloc(1);
        if (line == NULL)
        {
            return NULL;
        }
    }

    line[length] = '\0';
    return line;
}

static char *parseNextToken(char **cursor)
{
    char *token;

    if (cursor == NULL || *cursor == NULL)
    {
        return NULL;
    }

    *cursor = trimWhitespace(*cursor);
    if (**cursor == '\0')
    {
        return NULL;
    }

    if (**cursor == '"')
    {
        token = ++(*cursor);
        while (**cursor != '\0' && **cursor != '"')
        {
            (*cursor)++;
        }

        if (**cursor == '"')
        {
            **cursor = '\0';
            (*cursor)++;
        }
        return token;
    }

    token = *cursor;
    while (**cursor != '\0' && !isspace((unsigned char)**cursor))
    {
        (*cursor)++;
    }

    if (**cursor != '\0')
    {
        **cursor = '\0';
        (*cursor)++;
    }

    return token;
}

static char *replaceSubstring(const char *source, const char *oldText, const char *newText, int replaceAll, int *matchCount)
{
    const char *cursor;
    const char *firstMatch;
    char *result;
    char *write;
    size_t sourceLength;
    size_t oldLength;
    size_t newLength;
    size_t totalLength;
    size_t prefixLength;
    int matches;

    if (source == NULL || oldText == NULL || *oldText == '\0')
    {
        if (matchCount != NULL)
        {
            *matchCount = 0;
        }
        return NULL;
    }

    sourceLength = strlen(source);
    oldLength = strlen(oldText);
    newLength = strlen(newText);
    cursor = source;
    matches = 0;
    while ((firstMatch = strstr(cursor, oldText)) != NULL)
    {
        matches++;
        cursor = firstMatch + oldLength;
    }

    if (matches == 0)
    {
        if (matchCount != NULL)
        {
            *matchCount = 0;
        }
        return NULL;
    }

    if (!replaceAll)
    {
        firstMatch = strstr(source, oldText);
        if (firstMatch == NULL)
        {
            if (matchCount != NULL)
            {
                *matchCount = 0;
            }
            return NULL;
        }

        prefixLength = (size_t)(firstMatch - source);
        totalLength = sourceLength - oldLength + newLength + 1;
        result = malloc(totalLength);
        if (result == NULL)
        {
            if (matchCount != NULL)
            {
                *matchCount = -1;
            }
            return NULL;
        }

        memcpy(result, source, prefixLength);
        memcpy(result + prefixLength, newText, newLength);
        memcpy(result + prefixLength + newLength,
               firstMatch + oldLength,
               sourceLength - prefixLength - oldLength + 1);

        if (matchCount != NULL)
        {
            *matchCount = 1;
        }
        return result;
    }

    if (newLength >= oldLength)
    {
        totalLength = sourceLength + (size_t)matches * (newLength - oldLength) + 1;
    }
    else
    {
        totalLength = sourceLength - (size_t)matches * (oldLength - newLength) + 1;
    }

    result = malloc(totalLength);
    if (result == NULL)
    {
        if (matchCount != NULL)
        {
            *matchCount = -1;
        }
        return NULL;
    }

    write = result;
    cursor = source;
    while (*cursor != '\0')
    {
        const char *match = strstr(cursor, oldText);

        if (match == NULL)
        {
            size_t tailLength = strlen(cursor);
            memcpy(write, cursor, tailLength + 1);
            break;
        }

        prefixLength = (size_t)(match - cursor);
        memcpy(write, cursor, prefixLength);
        write += prefixLength;
        memcpy(write, newText, newLength);
        write += newLength;
        cursor = match + oldLength;
    }

    *write = '\0';
    if (matchCount != NULL)
    {
        *matchCount = matches;
    }
    return result;
}

int appendDocument(Document *document, UndoState *undoState, const char *text)
{
    char *newLine;

    if (text == NULL || *text == '\0')
    {
        fprintf(stderr, "Error: append text cannot be empty.\n");
        return 0;
    }

    if (!saveUndoState(document, undoState))
    {
        return 0;
    }

    if (!ensureCapacity(document, document->size + 1))
    {
        fprintf(stderr, "Error: could not increase document capacity.\n");
        clearUndoState(undoState);
        return 0;
    }

    if (!duplicateString(text, &newLine))
    {
        fprintf(stderr, "Error: could not allocate memory for the appended line.\n");
        clearUndoState(undoState);
        return 0;
    }

    document->lines[document->size] = newLine;
    document->size++;
    return 1;
}

int insertLineAt(Document *document, UndoState *undoState, int lineNumber, const char *text)
{
    int index;
    char *newLine;

    if (lineNumber < 1 || lineNumber > document->size + 1)
    {
        printf("Invalid line number.\n");
        return 0;
    }

    if (text == NULL || *text == '\0')
    {
        printf("Text cannot be empty.\n");
        return 0;
    }

    if (!saveUndoState(document, undoState))
    {
        return 0;
    }

    if (!ensureCapacity(document, document->size + 1))
    {
        fprintf(stderr, "Error: could not increase document capacity.\n");
        clearUndoState(undoState);
        return 0;
    }

    if (!duplicateString(text, &newLine))
    {
        fprintf(stderr, "Error: could not allocate memory for the new line.\n");
        clearUndoState(undoState);
        return 0;
    }

    index = lineNumber - 1;
    for (int current = document->size; current > index; current--)
    {
        document->lines[current] = document->lines[current - 1];
    }

    document->lines[index] = newLine;
    document->size++;
    return 1;
}

int deleteLineByNumber(Document *document, UndoState *undoState, int lineNumber)
{
    int index;

    if (document->size == 0)
    {
        printf("The document is empty.\n");
        return 0;
    }

    if (lineNumber < 1 || lineNumber > document->size)
    {
        printf("Invalid line number.\n");
        return 0;
    }

    if (!saveUndoState(document, undoState))
    {
        return 0;
    }

    index = lineNumber - 1;
    free(document->lines[index]);

    for (int current = index; current < document->size - 1; current++)
    {
        document->lines[current] = document->lines[current + 1];
    }

    document->size--;
    document->lines[document->size] = NULL;
    return 1;
}

void displayDocument(const Document *document)
{
    if (document == NULL || document->size == 0)
    {
        printf("Document is empty.\n");
        return;
    }

    for (int index = 0; index < document->size; index++)
    {
        printf("%d: %s\n", index + 1, document->lines[index]);
    }
}

int saveDocument(const Document *document, const char *filename)
{
    FILE *file;

    if (filename == NULL || *filename == '\0')
    {
        fprintf(stderr, "Error: filename is empty.\n");
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

int loadDocument(Document *document, UndoState *undoState, const char *filename)
{
    Document loadedDocument;
    FILE *file;
    char *line;

    if (filename == NULL || *filename == '\0')
    {
        fprintf(stderr, "Error: filename is empty.\n");
        return 0;
    }

    file = fopen(filename, "r");
    if (file == NULL)
    {
        perror("Error opening file for reading");
        return 0;
    }

    initializeDocument(&loadedDocument);
    while ((line = readFileLine(file)) != NULL)
    {
        if (!ensureCapacity(&loadedDocument, loadedDocument.size + 1))
        {
            fprintf(stderr, "Error: not enough memory to load the file.\n");
            free(line);
            fclose(file);
            freeDocument(&loadedDocument);
            return 0;
        }

        loadedDocument.lines[loadedDocument.size] = line;
        loadedDocument.size++;
    }

    if (fclose(file) != 0)
    {
        perror("Error closing file");
        freeDocument(&loadedDocument);
        return 0;
    }

    if (!saveUndoState(document, undoState))
    {
        freeDocument(&loadedDocument);
        return 0;
    }

    freeDocument(document);
    *document = loadedDocument;
    return 1;
}

void searchDocument(const Document *document, const char *text)
{
    int found = 0;

    if (text == NULL || *text == '\0')
    {
        fprintf(stderr, "Error: search text cannot be empty.\n");
        return;
    }

    for (int index = 0; index < document->size; index++)
    {
        if (strstr(document->lines[index], text) != NULL)
        {
            printf("Found at line %d: %s\n", index + 1, document->lines[index]);
            found = 1;
        }
    }

    if (!found)
    {
        printf("Text not found.\n");
    }
}

int replaceLine(Document *document, UndoState *undoState, int lineNumber, const char *oldText, const char *newText)
{
    char *replacement;
    int updated;
    int lineIndex;

    if (lineNumber < 1 || lineNumber > document->size)
    {
        printf("Invalid line number.\n");
        return 0;
    }

    if (oldText == NULL || *oldText == '\0')
    {
        fprintf(stderr, "Error: replacement text cannot be empty.\n");
        return 0;
    }

    replacement = replaceSubstring(document->lines[lineNumber - 1], oldText, newText, 0, &updated);
    if (updated == 0 || replacement == NULL)
    {
        printf("Text not found.\n");
        return 0;
    }

    if (!saveUndoState(document, undoState))
    {
        free(replacement);
        return 0;
    }

    lineIndex = lineNumber - 1;
    free(document->lines[lineIndex]);
    document->lines[lineIndex] = replacement;
    return 1;
}

int replaceAll(Document *document, UndoState *undoState, const char *oldText, const char *newText)
{
    char **replacementLines = NULL;
    int totalMatches = 0;
    int hasChanges = 0;

    if (oldText == NULL || *oldText == '\0')
    {
        fprintf(stderr, "Error: replacement text cannot be empty.\n");
        return 0;
    }

    if (document->size > 0)
    {
        replacementLines = calloc((size_t)document->size, sizeof(*replacementLines));
        if (replacementLines == NULL)
        {
            fprintf(stderr, "Error: could not allocate replacement buffer.\n");
            return 0;
        }
    }

    for (int index = 0; index < document->size; index++)
    {
        int matches = 0;
        char *replacement = replaceSubstring(document->lines[index], oldText, newText, 1, &matches);

        if (matches < 0)
        {
            fprintf(stderr, "Error: could not allocate memory for replacement.\n");
            for (int cleanupIndex = 0; cleanupIndex < index; cleanupIndex++)
            {
                free(replacementLines[cleanupIndex]);
            }
            free(replacementLines);
            return 0;
        }

        if (matches > 0)
        {
            replacementLines[index] = replacement;
            totalMatches += matches;
            hasChanges = 1;
        }
    }

    if (!hasChanges)
    {
        printf("Text not found.\n");
        free(replacementLines);
        return 0;
    }

    if (!saveUndoState(document, undoState))
    {
        for (int index = 0; index < document->size; index++)
        {
            free(replacementLines[index]);
        }
        free(replacementLines);
        return 0;
    }

    for (int index = 0; index < document->size; index++)
    {
        if (replacementLines[index] != NULL)
        {
            free(document->lines[index]);
            document->lines[index] = replacementLines[index];
        }
    }

    free(replacementLines);
    return 1;
}

void printHelp(void)
{
    printf("Commands:\n");
    printf("  append <text>                Add a new line at the end.\n");
    printf("  insert <line_number> <text>  Insert text at a 1-based line number.\n");
    printf("  delete <line_number>         Delete a line.\n");
    printf("  display                     Show all lines with numbers.\n");
    printf("  save <filename>             Save the document to a text file.\n");
    printf("  load <filename>             Load the document from a text file.\n");
    printf("  search <text>               Search every line for matching text.\n");
    printf("  replace <line> <old> <new>  Replace text on one line.\n");
    printf("  replaceall <old> <new>      Replace all matching text in the document.\n");
    printf("  undo                        Undo the most recent document-changing operation.\n");
    printf("  count                       Show total lines and total words.\n");
    printf("  help                        Show this help text.\n");
    printf("  exit / quit                 Exit the editor.\n");
}

int processCommand(char *input, Document *document, UndoState *undoState)
{
    char *command;
    char *arguments;
    char *cursor;
    char *oldText;
    char *newText;
    long lineNumber;
    char *end;

    if (input == NULL)
    {
        return 1;
    }

    command = trimWhitespace(input);
    if (*command == '\0')
    {
        return 1;
    }

    arguments = command;
    while (*arguments != '\0' && !isspace((unsigned char)*arguments))
    {
        arguments++;
    }

    if (*arguments != '\0')
    {
        *arguments = '\0';
        arguments = trimWhitespace(arguments + 1);
    }
    else
    {
        arguments = "";
    }

    if (strcmp(command, "append") == 0)
    {
        if (*arguments == '\0')
        {
            fprintf(stderr, "Usage: append <text>\n");
        }
        else if (appendDocument(document, undoState, arguments))
        {
            printf("Line appended.\n");
        }
    }
    else if (strcmp(command, "insert") == 0)
    {
        cursor = trimWhitespace(arguments);
        if (*cursor == '\0')
        {
            fprintf(stderr, "Usage: insert <line_number> <text>\n");
            return 1;
        }

        lineNumber = strtol(cursor, &end, 10);
        if (end == cursor || lineNumber < 1 || lineNumber > INT_MAX)
        {
            fprintf(stderr, "Usage: insert <line_number> <text>\n");
            return 1;
        }

        cursor = trimWhitespace(end);
        if (*cursor == '\0')
        {
            fprintf(stderr, "Usage: insert <line_number> <text>\n");
            return 1;
        }

        if (insertLineAt(document, undoState, (int)lineNumber, cursor))
        {
            printf("Line inserted.\n");
        }
    }
    else if (strcmp(command, "delete") == 0)
    {
        cursor = trimWhitespace(arguments);
        if (*cursor == '\0')
        {
            fprintf(stderr, "Usage: delete <line_number>\n");
            return 1;
        }

        lineNumber = strtol(cursor, &end, 10);
        if (end == cursor || lineNumber < 1 || lineNumber > INT_MAX)
        {
            fprintf(stderr, "Usage: delete <line_number>\n");
            return 1;
        }

        if (deleteLineByNumber(document, undoState, (int)lineNumber))
        {
            printf("Line deleted.\n");
        }
    }
    else if (strcmp(command, "display") == 0)
    {
        displayDocument(document);
    }
    else if (strcmp(command, "save") == 0)
    {
        if (*arguments == '\0')
        {
            fprintf(stderr, "Usage: save <filename>\n");
        }
        else if (saveDocument(document, arguments))
        {
            printf("Document saved.\n");
        }
    }
    else if (strcmp(command, "load") == 0)
    {
        if (*arguments == '\0')
        {
            fprintf(stderr, "Usage: load <filename>\n");
        }
        else if (loadDocument(document, undoState, arguments))
        {
            printf("Document loaded.\n");
        }
    }
    else if (strcmp(command, "search") == 0)
    {
        if (*arguments == '\0')
        {
            fprintf(stderr, "Usage: search <text>\n");
        }
        else
        {
            searchDocument(document, arguments);
        }
    }
    else if (strcmp(command, "replace") == 0)
    {
        cursor = trimWhitespace(arguments);
        if (*cursor == '\0')
        {
            fprintf(stderr, "Usage: replace <line_number> <old_text> <new_text>\n");
            return 1;
        }

        lineNumber = strtol(cursor, &end, 10);
        if (end == cursor || lineNumber < 1 || lineNumber > INT_MAX)
        {
            fprintf(stderr, "Usage: replace <line_number> <old_text> <new_text>\n");
            return 1;
        }

        cursor = trimWhitespace(end);
        oldText = parseNextToken(&cursor);
        newText = parseNextToken(&cursor);

        if (oldText == NULL || newText == NULL)
        {
            fprintf(stderr, "Usage: replace <line_number> <old_text> <new_text>\n");
            return 1;
        }

        if (replaceLine(document, undoState, (int)lineNumber, oldText, newText))
        {
            printf("Line replaced.\n");
        }
    }
    else if (strcmp(command, "replaceall") == 0)
    {
        cursor = trimWhitespace(arguments);
        oldText = parseNextToken(&cursor);
        newText = parseNextToken(&cursor);

        if (oldText == NULL || newText == NULL)
        {
            fprintf(stderr, "Usage: replaceall <old_text> <new_text>\n");
            return 1;
        }

        if (replaceAll(document, undoState, oldText, newText))
        {
            printf("All matching text replaced.\n");
        }
    }
    else if (strcmp(command, "undo") == 0)
    {
        undo(document, undoState);
    }
    else if (strcmp(command, "count") == 0)
    {
        showLineAndWordCount(document);
    }
    else if (strcmp(command, "help") == 0)
    {
        printHelp();
    }
    else if (strcmp(command, "exit") == 0 || strcmp(command, "quit") == 0)
    {
        return 0;
    }
    else
    {
        printf("Unknown command. Type help for available commands.\n");
    }

    return 1;
}

int main(int argc, char *argv[])
{
    Document document;
    UndoState undoState;
    char *input = NULL;
    int running = 1;

    initializeDocument(&document);
    initializeUndoState(&undoState);

    if (argc > 2)
    {
        fprintf(stderr, "Usage: %s [filename]\n", argv[0]);
        freeDocument(&document);
        freeUndoState(&undoState);
        return EXIT_FAILURE;
    }

    if (argc == 2)
    {
        if (!loadDocument(&document, &undoState, argv[1]))
        {
            freeDocument(&document);
            freeUndoState(&undoState);
            return EXIT_FAILURE;
        }
        clearUndoState(&undoState);
    }

    printf("Line Editor\nType 'help' for a list of commands.\n");
    while (running)
    {
        printf("editor> ");
        fflush(stdout);
        input = readCommandLine(stdin);
        if (input == NULL)
        {
            printf("\n");
            break;
        }

        running = processCommand(input, &document, &undoState);
        free(input);
        input = NULL;
    }

    freeDocument(&document);
    freeUndoState(&undoState);
    return EXIT_SUCCESS;
}
