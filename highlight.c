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

#define VERSION "1.4.0"

#define COLOR_START "\033[1;31m"
#define COLOR_END   "\033[0m"

#define ANSI_CLEAR_LINE   "\033[2K"
#define ANSI_CURSOR_START "\r"


struct status {
    /* Total statistics */
    unsigned long long lines;
    unsigned long long bytes;

    /* Statistics for the current one-second interval */
    unsigned long long interval_lines;
    unsigned long long interval_bytes;

    /* Last completed one-second rates */
    double last_second_lines;
    double last_second_bytes;

    /* Timing */
    struct timespec start;
    struct timespec interval_start;

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
 * Format a byte value into a human-readable form.
 *
 * Uses decimal units:
 *
 *     B
 *     KB
 *     MB
 *     GB
 *     TB
 *     PB
 */
static void format_bytes(double bytes,
                         char *buffer,
                         size_t buffer_size)
{
    const char *units[] = {
        "B",
        "KB",
        "MB",
        "GB",
        "TB",
        "PB"
    };

    int unit = 0;

    while (bytes >= 1000.0 && unit < 5) {
        bytes /= 1000.0;
        unit++;
    }

    if (bytes >= 100.0) {
        snprintf(
            buffer,
            buffer_size,
            "%.0f %s",
            bytes,
            units[unit]
        );
    }
    else if (bytes >= 10.0) {
        snprintf(
            buffer,
            buffer_size,
            "%.1f %s",
            bytes,
            units[unit]
        );
    }
    else {
        snprintf(
            buffer,
            buffer_size,
            "%.2f %s",
            bytes,
            units[unit]
        );
    }
}


/*
 * Format a byte rate.
 */
static void format_rate(double bytes_per_second,
                        char *buffer,
                        size_t buffer_size)
{
    char value[64];

    format_bytes(
        bytes_per_second,
        value,
        sizeof(value)
    );

    snprintf(
        buffer,
        buffer_size,
        "%s/s",
        value
    );
}


/*
 * Initialise status tracking.
 */
static void status_init(struct status *status)
{
    memset(
        status,
        0,
        sizeof(*status)
    );

    /*
     * Status is always enabled.
     */
    status->enabled = 1;

    clock_gettime(
        CLOCK_MONOTONIC,
        &status->start
    );

    status->interval_start =
        status->start;
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
 * Update the one-second statistics.
 *
 * This is called after every input line.
 *
 * Once one second has elapsed, the current interval
 * becomes the "Last 1s" measurement.
 */
static void status_calculate_interval(struct status *status)
{
    struct timespec now;

    clock_gettime(
        CLOCK_MONOTONIC,
        &now
    );

    double interval =
        elapsed_seconds(
            &status->interval_start,
            &now
        );

    if (interval < 1.0)
        return;

    /*
     * Calculate the rate for the completed interval.
     */
    status->last_second_lines =
        (double)status->interval_lines /
        interval;

    status->last_second_bytes =
        (double)status->interval_bytes /
        interval;

    /*
     * Start a new interval.
     */
    status->interval_lines = 0;
    status->interval_bytes = 0;

    status->interval_start = now;
}


/*
 * Print the current status.
 */
static void status_update(struct status *status)
{
    if (!status->enabled)
        return;

    /*
     * Update the completed one-second interval.
     */
    status_calculate_interval(status);


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


    /*
     * Calculate average rates.
     */
    double average_lines =
        (double)status->lines /
        elapsed;


    double average_bytes =
        (double)status->bytes /
        elapsed;


    /*
     * Format byte values.
     */
    char total_bytes[128];
    char average_rate[128];
    char last_second_rate[128];


    format_bytes(
        (double)status->bytes,
        total_bytes,
        sizeof(total_bytes)
    );


    format_rate(
        average_bytes,
        average_rate,
        sizeof(average_rate)
    );


    format_rate(
        status->last_second_bytes,
        last_second_rate,
        sizeof(last_second_rate)
    );


    /*
     * Display status.
     */
    fprintf(
        stderr,

        ANSI_CURSOR_START
        ANSI_CLEAR_LINE

        "Lines: %llu  "
        "Bytes: %s  "
        "Avg: %.0f lines/s  %s  "
        "Last 1s: %.0f lines/s  %s",

        status->lines,
        total_bytes,

        average_lines,
        average_rate,

        status->last_second_lines,
        last_second_rate
    );


    fflush(stderr);
}


/*
 * Print the final status.
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


    /*
     * Include the final partial interval in the
     * Last 1s measurement.
     */
    double interval =
        elapsed_seconds(
            &status->interval_start,
            &now
        );


    if (interval > 0.0 &&
        (status->interval_lines > 0 ||
         status->interval_bytes > 0)) {

        status->last_second_lines =
            (double)status->interval_lines /
            interval;

        status->last_second_bytes =
            (double)status->interval_bytes /
            interval;
    }


    /*
     * Average rates.
     */
    double average_lines =
        (double)status->lines /
        elapsed;


    double average_bytes =
        (double)status->bytes /
        elapsed;


    /*
     * Format values.
     */
    char total_bytes[128];
    char average_rate[128];
    char last_second_rate[128];


    format_bytes(
        (double)status->bytes,
        total_bytes,
        sizeof(total_bytes)
    );


    format_rate(
        average_bytes,
        average_rate,
        sizeof(average_rate)
    );


    format_rate(
        status->last_second_bytes,
        last_second_rate,
        sizeof(last_second_rate)
    );


    /*
     * Print final status.
     */
    fprintf(
        stderr,

        ANSI_CURSOR_START
        ANSI_CLEAR_LINE

        "Lines: %llu  "
        "Bytes: %s  "
        "Avg: %.0f lines/s  %s  "
        "Last 1s: %.0f lines/s  %s\n",

        status->lines,
        total_bytes,

        average_lines,
        average_rate,

        status->last_second_lines,
        last_second_rate
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
         * Print text before the match.
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
 * Similar to:
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
         * Print whitespace unchanged.
         */
        while (*p != '\0' &&
               isspace((unsigned char)*p)) {

            putchar(*p);

            p++;
        }


        /*
         * End of line.
         */
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
         * Highlight selected field.
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
         * Print separator unchanged.
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
     * Process stdin line by line.
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
         * Update total counters.
         */
        status.lines++;

        status.bytes +=
            (unsigned long long)line_length;


        /*
         * Update current one-second counters.
         */
        status.interval_lines++;

        status.interval_bytes +=
            (unsigned long long)line_length;


        /*
         * Remove the previous status line before
         * printing the next input line.
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
         * Make sure the highlighted line is written
         * before the status line is displayed.
         */
        fflush(stdout);


        /*
         * Display updated status.
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
     * Print final statistics.
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
