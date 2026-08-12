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

#define VERSION "1.3.0"

#define COLOR_START "\033[1;31m"
#define COLOR_END   "\033[0m"

#define ANSI_CLEAR_LINE   "\033[2K"
#define ANSI_CURSOR_START "\r"


struct status {
    unsigned long long lines;
    unsigned long long bytes;

    struct timespec start;

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
 * Calculate elapsed time in seconds.
 */
static double elapsed_seconds(const struct timespec *start,
                              const struct timespec *end)
{
    return (double)(end->tv_sec - start->tv_sec) +
           (double)(end->tv_nsec - start->tv_nsec) /
           1000000000.0;
}


/*
 * Format a byte rate into a human-readable form.
 *
 * Examples:
 *
 *     123        -> 123 B/s
 *     12345      -> 12.3 KB/s
 *     1234567    -> 1.23 MB/s
 *     1234567890 -> 1.23 GB/s
 */
static void format_rate(double bytes_per_second,
                        char *buffer,
                        size_t buffer_size)
{
    const char *units[] = {
        "B/s",
        "KB/s",
        "MB/s",
        "GB/s",
        "TB/s",
        "PB/s"
    };

    int unit = 0;


    while (bytes_per_second >= 1000.0 &&
           unit < 5) {

        bytes_per_second /= 1000.0;
        unit++;
    }


    if (bytes_per_second >= 100.0) {

        snprintf(
            buffer,
            buffer_size,
            "%.0f %s",
            bytes_per_second,
            units[unit]
        );

    } else if (bytes_per_second >= 10.0) {

        snprintf(
            buffer,
            buffer_size,
            "%.1f %s",
            bytes_per_second,
            units[unit]
        );

    } else {

        snprintf(
            buffer,
            buffer_size,
            "%.2f %s",
            bytes_per_second,
            units[unit]
        );
    }
}


/*
 * Initialise status tracking.
 *
 * Status is always enabled.
 */
static void status_init(struct status *status)
{
    memset(
        status,
        0,
        sizeof(*status)
    );

    status->enabled = 1;

    clock_gettime(
        CLOCK_MONOTONIC,
        &status->start
    );
}


/*
 * Clear the current status line.
 */
static void status_clear(struct status *status)
{
    if (!status->enabled)
        return;

    fprintf(
        stderr,
        ANSI_CURSOR_START
        ANSI_CLEAR_LINE
    );

    fflush(stderr);
}


/*
 * Print the current status.
 *
 * Called after every input line.
 */
static void status_update(struct status *status)
{
    if (!status->enabled)
        return;


    struct timespec now;

    clock_gettime(
        CLOCK_MONOTONIC,
        &now
    );


    double elapsed =
        elapsed_seconds(
            &status->start,
            &now
        );


    if (elapsed <= 0.0)
        elapsed = 0.000001;


    double lines_per_second =
        (double)status->lines /
        elapsed;


    double bytes_per_second =
        (double)status->bytes /
        elapsed;


    char rate_buffer[64];

    format_rate(
        bytes_per_second,
        rate_buffer,
        sizeof(rate_buffer)
    );


    fprintf(
        stderr,

        ANSI_CURSOR_START
        ANSI_CLEAR_LINE

        "Lines: %llu  "
        "Bytes: %llu  "
        "Rate: %.0f lines/s  "
        "%s",

        status->lines,
        status->bytes,
        lines_per_second,
        rate_buffer
    );


    fflush(stderr);
}


/*
 * Print the final status line.
 */
static void status_finish(struct status *status)
{
    if (!status->enabled)
        return;


    struct timespec now;

    clock_gettime(
        CLOCK_MONOTONIC,
        &now
    );


    double elapsed =
        elapsed_seconds(
            &status->start,
            &now
        );


    if (elapsed <= 0.0)
        elapsed = 0.000001;


    double lines_per_second =
        (double)status->lines /
        elapsed;


    double bytes_per_second =
        (double)status->bytes /
        elapsed;


    char rate_buffer[64];

    format_rate(
        bytes_per_second,
        rate_buffer,
        sizeof(rate_buffer)
    );


    fprintf(
        stderr,

        ANSI_CURSOR_START
        ANSI_CLEAR_LINE

        "Lines: %llu  "
        "Bytes: %llu  "
        "Rate: %.0f lines/s  "
        "%s\n",

        status->lines,
        status->bytes,
        lines_per_second,
        rate_buffer
    );


    fflush(stderr);
}


/*
 * Print usage information.
 */
static void print_usage(const char *program)
{
    fprintf(
        stderr,

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
        "  cat access.log | %s -c 9\n"
        "  cat /etc/passwd | %s -F : -c 1\n"
        "\n"

        "Regex mode:\n"
        "  cat logfile | %s 'error|warning'\n"
        "  cat logfile | %s -i error\n"
        "\n"

        "Examples:\n"
        "  echo 'hello error world' | %s error\n"
        "  echo 'one two three' | %s -c 2\n"
        "  echo 'one:two:three' | %s -F : -c 2\n",

        program,
        program,

        program,
        program,

        program,
        program,

        program,
        program,
        program
    );
}


/*
 * Print version.
 */
static void print_version(void)
{
    printf(
        "highlight %s\n",
        VERSION
    );
}


/*
 * Print highlighted text.
 */
static void print_highlighted(const char *start,
                             size_t length)
{
    fputs(
        COLOR_START,
        stdout
    );

    fwrite(
        start,
        1,
        length,
        stdout
    );

    fputs(
        COLOR_END,
        stdout
    );
}


/*
 * Highlight every regex match in a line.
 */
static void highlight_regex(const char *line,
                            regex_t *regex)
{
    const char *cursor = line;


    while (*cursor != '\0') {

        regmatch_t match;


        int result =
            regexec(
                regex,
                cursor,
                1,
                &match,
                0
            );


        /*
         * No more matches.
         */
        if (result != 0) {

            fputs(
                cursor,
                stdout
            );

            return;
        }


        /*
         * Protect against zero-length matches.
         */
        if (match.rm_so == match.rm_eo) {

            if (match.rm_so > 0) {

                fwrite(
                    cursor,
                    1,
                    match.rm_so,
                    stdout
                );
            }


            if (cursor[match.rm_so] != '\0') {

                putchar(
                    cursor[match.rm_so]
                );

                cursor +=
                    match.rm_so + 1;

            } else {

                break;
            }

            continue;
        }


        /*
         * Print text before match.
         */
        if (match.rm_so > 0) {

            fwrite(
                cursor,
                1,
                match.rm_so,
                stdout
            );
        }


        /*
         * Print highlighted match.
         */
        print_highlighted(
            cursor + match.rm_so,
            match.rm_eo - match.rm_so
        );


        /*
         * Continue after match.
         */
        cursor += match.rm_eo;
    }
}


/*
 * Highlight a column using whitespace separators.
 *
 * Similar to awk:
 *
 *     awk '{print $9}'
 */
static void highlight_column_whitespace(
    const char *line,
    int column)
{
    const char *p = line;

    int current_column = 0;


    while (*p != '\0') {

        /*
         * Output whitespace unchanged.
         */
        while (*p != '\0' &&
               isspace((unsigned char)*p)) {

            putchar(*p);

            p++;
        }


        if (*p == '\0')
            return;


        current_column++;

        const char *field_start = p;


        /*
         * Find end of field.
         */
        while (*p != '\0' &&
               !isspace((unsigned char)*p)) {

            p++;
        }


        size_t field_length =
            (size_t)(p - field_start);


        /*
         * Highlight selected column.
         */
        if (current_column == column) {

            print_highlighted(
                field_start,
                field_length
            );

        } else {

            fwrite(
                field_start,
                1,
                field_length,
                stdout
            );
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
 */
static void highlight_column_separator(
    const char *line,
    int column,
    const char *separator)
{
    size_t separator_length =
        strlen(separator);


    /*
     * Empty separator means whitespace mode.
     */
    if (separator_length == 0) {

        highlight_column_whitespace(
            line,
            column
        );

        return;
    }


    const char *field_start = line;

    int current_column = 1;


    while (1) {

        const char *separator_position =
            strstr(
                field_start,
                separator
            );


        const char *field_end;


        if (separator_position != NULL) {

            field_end =
                separator_position;

        } else {

            field_end =
                field_start +
                strlen(field_start);
        }


        size_t field_length =
            (size_t)(
                field_end -
                field_start
            );


        /*
         * Highlight selected column.
         */
        if (current_column == column) {

            print_highlighted(
                field_start,
                field_length
            );

        } else {

            fwrite(
                field_start,
                1,
                field_length,
                stdout
            );
        }


        /*
         * Last field.
         */
        if (separator_position == NULL)
            break;


        /*
         * Output separator unchanged.
         */
        fwrite(
            separator_position,
            1,
            separator_length,
            stdout
        );


        field_start =
            separator_position +
            separator_length;


        current_column++;
    }
}


/*
 * Highlight selected column.
 */
static void highlight_column(
    const char *line,
    int column,
    const char *separator)
{
    if (column <= 0)
        return;


    if (separator == NULL) {

        highlight_column_whitespace(
            line,
            column
        );

    } else {

        highlight_column_separator(
            line,
            column,
            separator
        );
    }
}


/*
 * Parse a positive column number.
 */
static int parse_column(const char *value)
{
    char *end = NULL;

    errno = 0;


    long column =
        strtol(
            value,
            &end,
            10
        );


    if (errno != 0 ||
        end == value ||
        *end != '\0' ||
        column <= 0) {

        fprintf(
            stderr,
            "Invalid column number: %s\n",
            value
        );

        return -1;
    }


    if (column > 2147483647L) {

        fprintf(
            stderr,
            "Column number is too large: %s\n",
            value
        );

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


    memset(
        &options,
        0,
        sizeof(options)
    );


    /*
     * Parse command-line options.
     */
    int opt;


    while ((opt = getopt(
                argc,
                argv,
                "c:F:ihv")) != -1) {

        switch (opt) {

        case 'c':

            options.column =
                parse_column(
                    optarg
                );


            if (options.column < 0)
                return EXIT_FAILURE;


            options.column_mode = 1;

            break;


        case 'F':

            options.separator =
                optarg;

            break;


        case 'i':

            options.case_insensitive =
                1;

            break;


        case 'h':

            print_usage(
                argv[0]
            );

            return EXIT_SUCCESS;


        case 'v':

            print_version();

            return EXIT_SUCCESS;


        default:

            print_usage(
                argv[0]
            );

            return EXIT_FAILURE;
        }
    }


    /*
     * Column mode.
     */
    if (options.column_mode) {

        if (optind < argc) {

            fprintf(
                stderr,
                "Error: regex cannot be used "
                "with -c\n\n"
            );


            print_usage(
                argv[0]
            );


            return EXIT_FAILURE;
        }


    /*
     * Regex mode.
     */
    } else {

        if (optind >= argc) {

            fprintf(
                stderr,
                "Error: regex is required\n\n"
            );


            print_usage(
                argv[0]
            );


            return EXIT_FAILURE;
        }


        options.regex_pattern =
            argv[optind];
    }


    /*
     * Compile regex.
     */
    regex_t regex;


    if (!options.column_mode) {

        int regex_flags =
            REG_EXTENDED;


        if (options.case_insensitive) {

            regex_flags |=
                REG_ICASE;
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


            fprintf(
                stderr,
                "Regex error: %s\n",
                error_message
            );


            return EXIT_FAILURE;
        }
    }


    /*
     * getline() dynamically grows this buffer.
     */
    char *line = NULL;

    size_t line_capacity = 0;


    /*
     * Initialise status.
     */
    struct status status;


    status_init(
        &status
    );


    /*
     * Process stdin.
     */
    while (1) {

        ssize_t line_length =
            getline(
                &line,
                &line_capacity,
                stdin
            );


        if (line_length < 0)
            break;


        /*
         * Update statistics.
         */
        status.lines++;

        status.bytes +=
            (unsigned long long)line_length;


        /*
         * Clear the previous status line.
         */
        status_clear(
            &status
        );


        /*
         * Highlight the line.
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


        /*
         * Ensure stdout is flushed before
         * drawing the status line.
         */
        fflush(stdout);


        /*
         * Display updated statistics.
         */
        status_update(
            &status
        );
    }


    /*
     * Check stdin for errors.
     */
    if (ferror(stdin)) {

        perror("stdin");


        free(line);


        if (!options.column_mode)
            regfree(&regex);


        return EXIT_FAILURE;
    }


    /*
     * Final status.
     */
    status_finish(
        &status
    );


    /*
     * Cleanup.
     */
    free(line);


    if (!options.column_mode)
        regfree(&regex);


    return EXIT_SUCCESS;
}
