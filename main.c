/*
 * CLLE - C Line Editor
 * Portable C99 implementation.
 *
 * Core features:
 *   insert, delete, display
 * Bonus features:
 *   search, replace, stats
 * File features:
 *   load on startup, save/saveas
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAX_LINE_LENGTH 1024
#define INITIAL_CAPACITY 16
#define MAX_COMMAND_LENGTH 2048

typedef struct {
    char **lines;
    size_t count;
    size_t capacity;
} Document;

/* ---------- Utility ---------- */

static void die(const char *message) {
    fprintf(stderr, "Error: %s\n", message);
    exit(EXIT_FAILURE);
}

static char *duplicate_string(const char *src) {
    size_t len = strlen(src);
    char *copy = (char *)malloc(len + 1);
    if (!copy) return NULL;
    memcpy(copy, src, len + 1);
    return copy;
}

static void trim_newline(char *text) {
    size_t len = strlen(text);
    while (len > 0 && (text[len - 1] == '\n' || text[len - 1] == '\r')) {
        text[--len] = '\0';
    }
}

static char *trim_spaces(char *text) {
    while (isspace((unsigned char)*text)) text++;

    char *end = text + strlen(text);
    while (end > text && isspace((unsigned char)end[-1])) {
        *--end = '\0';
    }
    return text;
}

/* ---------- Document management ---------- */

static void init_document(Document *doc) {
    doc->count = 0;
    doc->capacity = INITIAL_CAPACITY;
    doc->lines = (char **)malloc(doc->capacity * sizeof(char *));
    if (!doc->lines) die("Unable to allocate document.");
}

static void free_document(Document *doc) {
    size_t i;
    for (i = 0; i < doc->count; i++) {
        free(doc->lines[i]);
    }
    free(doc->lines);
    doc->lines = NULL;
    doc->count = 0;
    doc->capacity = 0;
}

static int ensure_capacity(Document *doc) {
    if (doc->count < doc->capacity) return 1;

    size_t new_capacity = doc->capacity * 2;
    char **new_lines = (char **)realloc(
        doc->lines, new_capacity * sizeof(char *)
    );

    if (!new_lines) return 0;

    doc->lines = new_lines;
    doc->capacity = new_capacity;
    return 1;
}

/*
 * Insert text at 1-based position.
 * Valid positions are 1 through count + 1.
 */
static int insert_line(Document *doc, size_t line_number, const char *text) {
    if (line_number < 1 || line_number > doc->count + 1) return 0;
    if (!ensure_capacity(doc)) return 0;

    size_t index = line_number - 1;
    size_t i;

    for (i = doc->count; i > index; i--) {
        doc->lines[i] = doc->lines[i - 1];
    }

    doc->lines[index] = duplicate_string(text);
    if (!doc->lines[index]) {
        for (i = index; i < doc->count; i++) {
            doc->lines[i] = doc->lines[i + 1];
        }
        return 0;
    }

    doc->count++;
    return 1;
}

static int delete_line(Document *doc, size_t line_number) {
    if (line_number < 1 || line_number > doc->count) return 0;

    size_t index = line_number - 1;
    size_t i;

    free(doc->lines[index]);

    for (i = index; i + 1 < doc->count; i++) {
        doc->lines[i] = doc->lines[i + 1];
    }

    doc->count--;
    return 1;
}

static void display_document(const Document *doc) {
    size_t i;

    if (doc->count == 0) {
        printf("[Document is empty]\n");
        return;
    }

    for (i = 0; i < doc->count; i++) {
        printf("%zu | %s\n", i + 1, doc->lines[i]);
    }
}

/* ---------- File handling ---------- */

static int save_document(const Document *doc, const char *filename) {
    FILE *file = fopen(filename, "w");
    if (!file) return 0;

    size_t i;
    for (i = 0; i < doc->count; i++) {
        fprintf(file, "%s\n", doc->lines[i]);
    }

    fclose(file);
    return 1;
}

static int load_document(Document *doc, const char *filename) {
    FILE *file = fopen(filename, "r");
    char buffer[MAX_LINE_LENGTH];

    if (!file) return 0;

    while (fgets(buffer, sizeof(buffer), file)) {
        trim_newline(buffer);

        /* Lines longer than the buffer are safely truncated. */
        if (!insert_line(doc, doc->count + 1, buffer)) {
            fclose(file);
            return 0;
        }
    }

    fclose(file);
    return 1;
}

/* ---------- Bonus features ---------- */

static void search_document(const Document *doc, const char *query) {
    size_t i;
    int found = 0;

    if (*query == '\0') {
        printf("Usage: search <word or phrase>\n");
        return;
    }

    for (i = 0; i < doc->count; i++) {
        if (strstr(doc->lines[i], query) != NULL) {
            printf("Found on line %zu: %s\n", i + 1, doc->lines[i]);
            found = 1;
        }
    }

    if (!found) printf("No match found.\n");
}

static int replace_first_in_line(char *line, size_t size,
                                 const char *old_text, const char *new_text) {
    char *match = strstr(line, old_text);
    if (!match) return 0;

    size_t prefix_len = (size_t)(match - line);
    size_t old_len = strlen(old_text);
    size_t new_len = strlen(new_text);
    size_t suffix_len = strlen(match + old_len);

    if (prefix_len + new_len + suffix_len + 1 > size) return -1;

    memmove(line + prefix_len + new_len,
            match + old_len,
            suffix_len + 1);

    memcpy(line + prefix_len, new_text, new_len);
    return 1;
}

static void replace_document(Document *doc,
                              const char *old_text,
                              const char *new_text,
                              size_t line_number) {
    size_t i;
    int replacements = 0;

    if (*old_text == '\0') {
        printf("Old text cannot be empty.\n");
        return;
    }

    if (line_number == 0) {
        for (i = 0; i < doc->count; i++) {
            char *copy = duplicate_string(doc->lines[i]);
            if (!copy) {
                printf("Memory error.\n");
                return;
            }

            while (strstr(copy, old_text)) {
                size_t needed = strlen(copy) - strlen(old_text)
                              + strlen(new_text) + 1;
                char *expanded = (char *)malloc(needed);
                if (!expanded) {
                    free(copy);
                    printf("Memory error.\n");
                    return;
                }

                char *match = strstr(copy, old_text);
                size_t prefix = (size_t)(match - copy);
                memcpy(expanded, copy, prefix);
                strcpy(expanded + prefix, new_text);
                strcat(expanded, match + strlen(old_text));

                free(copy);
                copy = expanded;
                replacements++;
            }

            free(doc->lines[i]);
            doc->lines[i] = copy;
        }
    } else {
        if (line_number > doc->count) {
            printf("Invalid line number.\n");
            return;
        }

        char *line = doc->lines[line_number - 1];

        while (strstr(line, old_text)) {
            size_t old_len = strlen(old_text);
            size_t new_len = strlen(new_text);
            size_t current_len = strlen(line);

            if (new_len <= old_len) {
                replace_first_in_line(line, MAX_LINE_LENGTH,
                                       old_text, new_text);
            } else {
                size_t new_size = current_len - old_len + new_len + 1;
                char *new_line = (char *)malloc(new_size);
                if (!new_line) {
                    printf("Memory error.\n");
                    return;
                }

                char *match = strstr(line, old_text);
                size_t prefix = (size_t)(match - line);
                memcpy(new_line, line, prefix);
                memcpy(new_line + prefix, new_text, new_len);
                strcpy(new_line + prefix + new_len, match + old_len);

                free(line);
                doc->lines[line_number - 1] = new_line;
                line = new_line;
            }

            replacements++;
        }
    }

    printf("Replacements made: %d\n", replacements);
}

static void show_stats(const Document *doc) {
    size_t i;
    size_t words = 0;
    size_t characters = 0;

    for (i = 0; i < doc->count; i++) {
        const char *p = doc->lines[i];
        int in_word = 0;

        characters += strlen(p);

        while (*p) {
            if (isspace((unsigned char)*p)) {
                in_word = 0;
            } else if (!in_word) {
                words++;
                in_word = 1;
            }
            p++;
        }
    }

    printf("Lines      : %zu\n", doc->count);
    printf("Words      : %zu\n", words);
    printf("Characters : %zu\n", characters);
}

/* ---------- User interface ---------- */

static void print_help(void) {
    printf(
        "\nCLLE - C Line Editor\n"
        "Commands:\n"
        "  insert <n> <text>       Insert text at line n\n"
        "  delete <n>              Delete line n\n"
        "  display                 Show all lines\n"
        "  save <file>             Save document to file\n"
        "  load <file>             Load a file (replaces current document)\n"
        "  search <text>           Search for text\n"
        "  replace <old> <new>     Replace text across the document\n"
        "  replace <n> <old> <new> Replace text on one line\n"
        "  stats                   Show line/word/character counts\n"
        "  help                    Show this help\n"
        "  quit                    Exit editor\n\n"
        "Examples:\n"
        "  insert 1 Hello world\n"
        "  insert 2 C is fast.\n"
        "  delete 1\n"
        "  search C\n"
        "  replace C C language\n"
        "  replace 1 Hello Hi\n"
        "  save notes.txt\n"
        "  quit\n\n"
    );
}

static int parse_positive_number(const char *text, size_t *number) {
    char *end;
    unsigned long value;

    if (!text || *text == '\0') return 0;

    value = strtoul(text, &end, 10);

    if (*end != '\0' || value == 0) return 0;

    *number = (size_t)value;
    return 1;
}

static void command_loop(Document *doc) {
    char command[MAX_COMMAND_LENGTH];

    while (1) {
        printf("clle> ");
        fflush(stdout);

        if (!fgets(command, sizeof(command), stdin)) break;
        trim_newline(command);

        char *input = trim_spaces(command);
        if (*input == '\0') continue;

        char *verb = strtok(input, " ");
        if (!verb) continue;

        if (strcmp(verb, "insert") == 0) {
            char *number_text = strtok(NULL, " ");
            char *text = strtok(NULL, "");

            size_t number;

            if (!number_text || !text ||
                !parse_positive_number(number_text, &number)) {
                printf("Usage: insert <line_number> <text>\n");
                continue;
            }

            text = trim_spaces(text);

            if (insert_line(doc, number, text))
                printf("Line inserted.\n");
            else
                printf("Invalid line number or memory error.\n");
        }
        else if (strcmp(verb, "delete") == 0) {
            char *number_text = strtok(NULL, " ");
            size_t number;

            if (!number_text ||
                !parse_positive_number(number_text, &number)) {
                printf("Usage: delete <line_number>\n");
                continue;
            }

            if (delete_line(doc, number))
                printf("Line deleted.\n");
            else
                printf("Invalid line number.\n");
        }
        else if (strcmp(verb, "display") == 0) {
            display_document(doc);
        }
        else if (strcmp(verb, "save") == 0) {
            char *filename = strtok(NULL, " ");

            if (!filename) {
                printf("Usage: save <filename>\n");
                continue;
            }

            if (save_document(doc, filename))
                printf("Saved to %s\n", filename);
            else
                printf("Could not save file.\n");
        }
        else if (strcmp(verb, "load") == 0) {
            char *filename = strtok(NULL, " ");

            if (!filename) {
                printf("Usage: load <filename>\n");
                continue;
            }

            Document temp;
            init_document(&temp);

            if (!load_document(&temp, filename)) {
                printf("Could not load file.\n");
                free_document(&temp);
            } else {
                free_document(doc);
                *doc = temp;
                printf("Loaded %s\n", filename);
            }
        }
        else if (strcmp(verb, "search") == 0) {
            char *query = strtok(NULL, "");
            if (!query) {
                printf("Usage: search <text>\n");
            } else {
                search_document(doc, trim_spaces(query));
            }
        }
        else if (strcmp(verb, "replace") == 0) {
            char *first = strtok(NULL, " ");
            char *second = strtok(NULL, " ");
            char *third = strtok(NULL, "");

            if (!first || !second) {
                printf("Usage: replace <old> <new>\n");
                printf("   or: replace <line> <old> <new>\n");
                continue;
            }

            size_t line_number = 0;

            if (third && parse_positive_number(first, &line_number)) {
                replace_document(doc, second, trim_spaces(third), line_number);
            } else {
                char *rest = third ? third : "";
                size_t new_len = strlen(second) + 1 + strlen(rest) + 1;
                char *new_text = (char *)malloc(new_len);

                if (!new_text) {
                    printf("Memory error.\n");
                    continue;
                }

                if (*rest)
                    snprintf(new_text, new_len, "%s %s", second, trim_spaces(rest));
                else
                    strcpy(new_text, second);

                replace_document(doc, first, new_text, 0);
                free(new_text);
            }
        }
        else if (strcmp(verb, "stats") == 0) {
            show_stats(doc);
        }
        else if (strcmp(verb, "help") == 0) {
            print_help();
        }
        else if (strcmp(verb, "quit") == 0 || strcmp(verb, "exit") == 0) {
            break;
        }
        else {
            printf("Unknown command. Type 'help'.\n");
        }
    }
}

int main(int argc, char *argv[]) {
    Document doc;
    init_document(&doc);

    printf("=================================\n");
    printf("        CLLE - Line Editor       \n");
    printf("=================================\n");

    /*
     * If a filename is supplied, load it automatically.
     * Example: ./clle notes.txt
     */
    if (argc >= 2) {
        if (load_document(&doc, argv[1])) {
            printf("Loaded: %s\n", argv[1]);
        } else {
            printf("Could not load '%s'. Starting empty.\n", argv[1]);
        }
    }

    printf("Type 'help' for commands.\n\n");
    command_loop(&doc);

    free_document(&doc);
    printf("Goodbye.\n");

    return 0;
}
