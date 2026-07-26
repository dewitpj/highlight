#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <regex.h>

#define COLOR_START "\033[1;31m"
#define COLOR_END   "\033[0m"

#define MAX_LINE 65536

int main(int argc, char *argv[])
{
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <regex>\n", argv[0]);
        return 1;
    }

    regex_t regex;
    int rc = regcomp(&regex, argv[1], REG_EXTENDED);

    if (rc != 0) {
        char errbuf[256];
        regerror(rc, &regex, errbuf, sizeof(errbuf));
        fprintf(stderr, "Regex error: %s\n", errbuf);
        return 1;
    }

    char line[MAX_LINE];

    while (fgets(line, sizeof(line), stdin)) {

        char *cursor = line;
        regmatch_t match;

        while (regexec(&regex, cursor, 1, &match, 0) == 0) {

            /* Print text before the match */
            fwrite(cursor, 1, match.rm_so, stdout);

            /* Prevent infinite loop on zero-length matches */
            if (match.rm_so == match.rm_eo) {
                putchar(*cursor);
                if (*cursor == '\0')
                    break;
                cursor++;
                continue;
            }

            /* Highlight the match */
            printf(COLOR_START);
            fwrite(cursor + match.rm_so,
                   1,
                   match.rm_eo - match.rm_so,
                   stdout);
            printf(COLOR_END);

            cursor += match.rm_eo;
        }

        fputs(cursor, stdout);
    }

    regfree(&regex);
    return 0;
}
