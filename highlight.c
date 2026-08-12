#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <regex.h>
#include <unistd.h>
#include <getopt.h>
#include <time.h>
#include <errno.h>
#include <ctype.h>

#define VERSION "1.1.0"

#define COLOR_START "\033[1;31m"
#define COLOR_END   "\033[0m"
#define CLEAR_LINE  "\033[2K"
#define CARRIAGE_RETURN "\r"

#define INITIAL_BUFFER_SIZE 4096

struct status {
    unsigned long long lines;
    unsigned long long chars;

    unsigned long long last_lines;
    unsigned long long last_chars;

    struct timespec start;
    struct timespec last_update;

    int enabled;
};

struct options {
    int column_mode;
    int column;
    int case_insensitive;

    char *separator;
    char *regex_pattern;
};


/*
 * Return elapsed time in seconds between two timespec values.
 */
static double elapsed_seconds(const struct timespec *start,
                              const struct timespec *end)
{
    return (double)(end->tv_sec - start->tv_sec) +
           (double)(end->tv_nsec - start->tv_nsec) / 1000000000.0;
}


/*
 * Initialise the status display.
 *
 * Status is written to stderr and is only enabled when stderr
 * is connected to a terminal.
 */
static void status_init(struct status *status)
{
    memset(status, 0, sizeof(*status));

    status->enabled = isatty(STDERR_FILENO);

    clock_gettime(CLOCK_MONOTONIC, &status->start);
    status->last_update = status->start;
}


/*
 * Update the live status line once per second.
 */
static void status_update(struct status *status)
{
    if (!status->enabled)
        return;

    struct timespec now;

    clock_gettime(CLOCK_MONOTONIC, &now);

    double interval = elapsed_seconds(&status->last_update, &now);

    if (interval < 1.0)
        return;

    double lines_per_second =
        (double)(status->lines - status->last_lines) / interval;

    double chars_per_second =
        (double)(status->chars - status->last_chars) / interval;

    fprintf(stderr,
            CARRIAGE_RETURN
            CLEAR_LINE
            "Lines: %llu  "
            "Chars: %llu  "
            "Rate: %.0f lines/s  %.2f MB/s",
            status->lines,
            status->chars,
            lines_per_second,
            chars_per_second / (1024.0 * 1024.0));

    fflush(stderr);

    status->last_lines = status->lines;
    status->last_chars = status->chars;
    status->last_update = now;
}


/*
 * Print the final status line.
 */
static void status_finish(struct status *status)
{
    if (!status->enabled)
        return;

    struct timespec now;

    clock_gettime(CLOCK_MONOTONIC, &now);

    double elapsed = elapsed_seconds(&status->start, &now);

    if (elapsed <= 0.0)
        elapsed = 0.000001;

    double lines_per_second =
        (double)status->lines / elapsed;

    double chars_per_second =
        (double)status->chars / elapsed;

    fprintf(stderr,
            CARRIAGE_RETURN
            CLEAR_LINE
            "Lines: %llu  "
            "Chars: %llu  "
            "Rate: %.0f lines/s  %.2f MB/s\n",
            status->lines,
            status->chars,
            lines_per_second,
            chars_per_second / (1024.0 * 1024.0));

    fflush(stderr);
}


/*
 * Print usage information.
 */
static void print_usage(const char *program)
{
    fprintf(stderr,
        "Usage:\n"
        "  %s [options] <regex>\n"
        "  %s [options] -c <column>\n"
        "\n"
        "Highlight matching text from stdin.\n"
        "\n"
        "Options:\n"
        "  -c N        Highlight column N (1-based)\n"
        "  -F SEP      Field separator for column mode\n"
        "  -i          Case-insensitive regex matching\n"
        "  -h          Show this help\n"
        "  -v          Show version\n"
        "\n"
        "Column mode:\n"
        "  %s -c 9\n"
        "  %s -F : -c 3\n"
        "\n"
        "Regex mode:\n"
        "  cat logfile | %s 'error|warning'\n"
        "  cat logfile | %s -i error\n"
        "\n"
        "Examples:\n"
        "  echo 'hello error world' | %s error\n"
        "  cat access.log | %s -c 9\n"
        "  cat /etc/passwd | %s -F : -c 1\n",
        program,
        program,
        program,
        program,
        program,
        program,
        program,
        program,
        program);
}


/*
 * Print version.
 */
static void print_version(void)
{
    printf("highlight %s\n", VERSION);
}


/*
 * Print a highlighted section of a line.
 */
static void print_highlighted(const char *start, size_t length)
{
    fputs(COLOR_START, stdout);
    fwrite(start, 1, length, stdout);
    fputs(COLOR_END, stdout);
}


/*
 * Highlight all regex matches in a line.
 */
static void highlight_regex(const char *line, regex_t *regex)
{
    const char *cursor = line;
    regmatch_t match;

    while (*cursor != '\0') {

        int result = regexec(regex, cursor, 1, &match, 0);

        if (result != 0) {
            fputs(cursor, stdout);
            return;
        }

        /*
         * Protect against zero-length regex matches.
         */
        if (match.rm_so == match.rm_eo) {

            if (match.rm_so > 0) {
                fwrite(cursor, 1, match.rm_so, stdout);
            }

            if (cursor[match.rm_so] != '\0') {
                putchar(cursor[match.rm_so]);
                cursor += match.rm_so + 1;
            } else {
                break;
            }

            continue;
        }

        /*
         * Text before the match.
         */
        if (match.rm_so > 0) {
            fwrite(cursor, 1, match.rm_so, stdout);
        }

        /*
         * Highlight the match.
         */
        print_highlighted(
            cursor + match.rm_so,
            match.rm_eo - match.rm_so
        );

        cursor += match.rm_eo;
    }
}


/*
 * Highlight a single field in whitespace-separated mode.
 *
 * This behaves similarly to awk:
 *
 *     awk '{print $9}'
 *
 * Multiple whitespace characters are treated as one separator.
 */
static void highlight_column_whitespace(const char *line, int column)
{
    const char *p = line;
    int current_column = 0;

    while (*p != '\0') {

        /*
         * Skip whitespace.
         */
        while (*p != '\0' && isspace((unsigned char)*p)) {
            putchar(*p);
            p++;
        }

        if (*p == '\0')
            return;

        current_column++;

        const char *field_start = p;

        /*
         * Find the end of the field.
         */
        while (*p != '\0' && !isspace((unsigned char)*p)) {
            p++;
        }

        size_t field_length = (size_t)(p - field_start);

        if (current_column == column) {
            print_highlighted(field_start, field_length);
        } else {
            fwrite(field_start, 1, field_length, stdout);
        }
    }
}


/*
 * Highlight a column using a custom separator.
 *
 * Example:
 *
 *     -F : -c 3
 *
 * Input:
 *
 *     one:two:three:four
 *
 * Output:
 *
 *     one:two:<highlight>three</highlight>:four
 */
static void highlight_column_separator(const char *line,
                                       int column,
                                       const char *separator)
{
    size_t separator_length = strlen(separator);

    if (separator_length == 0) {
        highlight_column_whitespace(line, column);
        return;
    }

    const char *field_start = line;
    int current_column = 1;

    while (1) {

        const char *separator_position =
            strstr(field_start, separator);

        const char *field_end;

        if (separator_position != NULL) {
            field_end = separator_position;
        } else {
            field_end = field_start + strlen(field_start);
        }

        size_t field_length =
            (size_t)(field_end - field_start);

        if (current_column == column) {
            print_highlighted(field_start, field_length);
        } else {
            fwrite(field_start, 1, field_length, stdout);
        }

        if (separator_position == NULL) {
            break;
        }

        /*
         * Output the separator unchanged.
         */
        fwrite(separator_position, 1, separator_length, stdout);

        field_start =
            separator_position + separator_length;

        current_column++;
    }
}


/*
 * Highlight a column.
 */
static void highlight_column(const char *line,
                             int column,
                             const char *separator)
{
    if (column <= 0)
        return;

    if (separator == NULL) {
        highlight_column_whitespace(line, column);
    } else {
        highlight_column_separator(line, column, separator);
    }
}


/*
 * Parse a positive integer.
 */
static int parse_column(const char *value)
{
    char *end = NULL;

    errno = 0;

    long column = strtol(value, &end, 10);

    if (errno != 0 ||
        end == value ||
        *end != '\0' ||
        column <= 0) {

        fprintf(stderr,
                "Invalid column number: %s\n",
                value);

        return -1;
    }

    if (column > 2147483647L) {
        fprintf(stderr,
                "Column number is too large: %s\n",
                value);

        return -1;
    }

    return (int)column;
}


/*
 * Main program.
 */
int main(int argc, char *argv[])
{
    struct options options;

    memset(&options, 0, sizeof(options));

    int opt;

    while ((opt = getopt(argc, argv, "c:F:ihv")) != -1) {

        switch (opt) {

        case 'c':
            options.column = parse_column(optarg);

            if (options.column < 0) {
                return EXIT_FAILURE;
            }

            options.column_mode = 1;
            break;

        case 'F':
            options.separator = optarg;
            break;

        case 'i':
            options.case_insensitive = 1;
            break;

        case 'h':
            print_usage(argv[0]);
            return EXIT_SUCCESS;

        case 'v':
            print_version();
            return EXIT_SUCCESS;

        default:
            print_usage(argv[0]);
            return EXIT_FAILURE;
        }
    }


    /*
     * Column mode.
     */
    if (options.column_mode) {

        if (optind < argc) {
            fprintf(stderr,
                    "Error: regex argument cannot be used with -c\n\n");

            print_usage(argv[0]);
            return EXIT_FAILURE;
        }

    /*
     * Regex mode.
     */
    } else {

        if (optind >= argc) {
            fprintf(stderr,
                    "Error: regex is required\n\n");

            print_usage(argv[0]);
            return EXIT_FAILURE;
        }

        options.regex_pattern = argv[optind];
    }


    /*
     * Compile regex when required.
     */
    regex_t regex;

    if (!options.column_mode) {

        int regex_flags = REG_EXTENDED;

        if (options.case_insensitive) {
            regex_flags |= REG_ICASE;
        }

        int result =
            regcomp(
                &regex,
                options.regex_pattern,
                regex_flags
            );

        if (result != 0) {

            char error_message[256];

            regerror(
                result,
                &regex,
                error_message,
                sizeof(error_message)
            );

            fprintf(stderr,
                    "Regex error: %s\n",
                    error_message);

            return EXIT_FAILURE;
        }
    }


    /*
     * Allocate input buffer.
     *
     * getline() allows lines larger than the old fixed 64K
     * buffer and is available on modern Linux systems.
     */
    char *line = NULL;
    size_t line_capacity = 0;

    struct status status;

    status_init(&status);


    /*
     * Process stdin.
     */
    while (1) {

        ssize_t line_length =
            getline(&line, &line_capacity, stdin);

        if (line_length < 0) {
            break;
        }

        status.lines++;
        status.chars += (unsigned long long)line_length;

        /*
         * Highlight the requested content.
         */
        if (options.column_mode) {

            highlight_column(
                line,
                options.column,
                options.separator
            );

        } else {

            highlight_regex(
                line,
                &regex
            );
        }

        fflush(stdout);

        status_update(&status);
    }


    /*
     * Check for input errors.
     */
    if (ferror(stdin)) {

        perror("stdin");

        free(line);

        if (!options.column_mode) {
            regfree(&regex);
        }

        return EXIT_FAILURE;
    }


    /*
     * Final status.
     */
    status_finish(&status);


    /*
     * Cleanup.
     */
    free(line);

    if (!options.column_mode) {
        regfree(&regex);
    }

    return EXIT_SUCCESS;
}
