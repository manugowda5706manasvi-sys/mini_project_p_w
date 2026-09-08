#include <ctype.h>
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
    char **currentLines;
    int currentSize;
    int currentCapacity;

    if (!undoState->available)
    {
        return 0;
    }

    currentLines = document->lines;
    currentSize = document->size;
    currentCapacity = document->capacity;

    document->lines = undoState->lines;
    document->size = undoState->size;
    document->capacity = undoState->capacity;

    undoState->lines = currentLines;
    undoState->size = currentSize;
    undoState->capacity = currentCapacity;
    undoState->available = 0;

    clearUndoState(undoState);
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
