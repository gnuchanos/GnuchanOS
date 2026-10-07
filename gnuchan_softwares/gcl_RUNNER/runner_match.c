/*
 * runner_match.c — the comparison, and the order it puts the list in.
 *
 * One query, one pass over the list, one array of matches sorted best first.
 * The pieces, in the order they run:
 *
 *   1. Fold the case, when the config says a capital letter does not matter —
 *      which is the default, because nobody remembers which of "Firefox" and
 *      "firefox" a program spells itself with.
 *
 *   2. Score the name against the query. A name that begins with the query
 *      beats one that merely contains it, a name that contains it beats one
 *      that only has the letters in order, and a shorter name beats a longer
 *      one at the same rank — so "Terminal" comes before "Terminal Emulator",
 *      which is what a person means when they type "term".
 *
 *   3. Sort by score. The sort is insertion sort, which on a few hundred items
 *      is a handful of comparisons each and has no allocation, no recursion and
 *      no worst case worth the name — all of which matters more here than the
 *      asymptotics, on the machine this is for.
 *
 * The scoring is the whole of the launcher's "intelligence", and it is
 * deliberately simple enough to be read: a person who finds a program in the
 * wrong place can look here and see exactly why.
 */
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "runner_match.h"

/* The ranks, highest first. A name that starts with the query is what a person
   typed the beginning of; one that contains it is what they typed the middle
   of; one that has the letters scattered is the weakest thing that still
   counts as a match. */
#define SCORE_PREFIX      1000
#define SCORE_SUBSTRING    600
#define SCORE_SEQUENCE     300
#define SCORE_SEQUENCE_SPREAD_PENALTY 40
#define SCORE_LENGTH_PENALTY            2

/* A match found on a word OTHER than the displayed name — the entry's
   GenericName/Keywords, or the program name in the command — is worth this
   much less than the same match on the name, so a program whose own name
   answers the query still lists above one that merely mentions it. It is small
   enough that a keyword match still beats a weaker name match. */
#define SCORE_ALTERNATE_PENALTY 50

void runner_match_init(RunnerMatches *matches) {
    matches->items = NULL;
    matches->count = 0;
    matches->capacity = 0;
}

void runner_match_free(RunnerMatches *matches) {
    free(matches->items);
    matches->items = NULL;
    matches->count = 0;
    matches->capacity = 0;
}

/* Fold one character to lower case, in the ASCII range. A name with a letter
   outside it — a Turkish dotless i, a Greek letter — is compared as it is,
   which is what a byte-wise launcher can honestly do; falling back to a locale
   here would make the match depend on the session's language in a way that is
   impossible to explain when it goes wrong. */
static char fold(char c, int case_sensitive) {
    if (case_sensitive) {
        return c;
    }
    return (char)tolower((unsigned char)c);
}

/* Whether two strings are equal, folded the same way the match is. */
static int name_matches_exactly(const char *name, const char *query,
                                int case_sensitive) {
    if (case_sensitive) {
        return strcmp(name, query) == 0;
    }
    return strcasecmp(name, query) == 0;
}

/* How well the name answers the query, or -1 when it does not.
 *
 * The three ranks are checked in order, and the first that holds is the one
 * that decides the score: a name is never scored as a subsequence when it
 * contains the query, because containing it is strictly more of a match. */
static int score_name(const char *name, const char *query,
                      int case_sensitive, int fuzzy) {
    if (query[0] == '\0') {
        /* Nothing typed matches everything, at the same score, so the list
           keeps the order it was read in. */
        return 0;
    }

    /* An exact name is the strongest thing there is: it is the program, not
       one whose name merely holds the letters. */
    if (name_matches_exactly(name, query, case_sensitive)) {
        return SCORE_PREFIX * 2;
    }

    /* Does the name begin with the query? A folded compare of the first
       characters, by length rather than by copying, because the name may be
       far longer than the query and this runs for every program. */
    size_t query_length = strlen(query);
    size_t name_length = strlen(name);
    if (name_length >= query_length) {
        int prefix = 1;
        for (size_t i = 0; i < query_length; i++) {
            if (fold(name[i], case_sensitive) !=
                fold(query[i], case_sensitive)) {
                prefix = 0;
                break;
            }
        }
        if (prefix) {
            /* A shorter name at the same rank is a better answer: "Terminal"
               before "Terminal Emulator". */
            return SCORE_PREFIX - (int)name_length * SCORE_LENGTH_PENALTY;
        }
    }

    /* Does the name hold the query, anywhere? */
    if (query_length <= name_length) {
        for (size_t start = 0; start + query_length <= name_length; start++) {
            int found = 1;
            for (size_t i = 0; i < query_length; i++) {
                if (fold(name[start + i], case_sensitive) !=
                    fold(query[i], case_sensitive)) {
                    found = 0;
                    break;
                }
            }
            if (found) {
                return SCORE_SUBSTRING - (int)name_length * SCORE_LENGTH_PENALTY;
            }
        }
    }

    if (!fuzzy) {
        return -1;
    }

    /* The letters in order, with gaps: "gimp" for "GNU Image Manipulation
       Program". The gap is what is counted, and each one costs a little, so a
       name whose letters are close together beats one whose letters are
       spread down a long title. */
    size_t name_at = 0;
    int gaps = 0;
    int last_hit = -1;
    for (size_t i = 0; i < query_length; i++) {
        char wanted = fold(query[i], case_sensitive);
        int found = 0;
        while (name_at < name_length) {
            if (fold(name[name_at], case_sensitive) == wanted) {
                if (last_hit >= 0) {
                    gaps += (int)(name_at - (size_t)last_hit) - 1;
                }
                last_hit = (int)name_at;
                name_at++;
                found = 1;
                break;
            }
            name_at++;
        }
        if (!found) {
            return -1;      /* a letter of the query is not in the name */
        }
    }
    return SCORE_SEQUENCE - gaps * SCORE_SEQUENCE_SPREAD_PENALTY;
}

/* Make room for one more match. The array grows by a step, the same way the
   program list does, because a query of one letter matches most of the list
   and one reallocation per match would be the whole list copied per letter.
   Returns 0 when there is room, -1 when there is not — out of memory is a
   reason to stop matching, not to write past the end. */
static int matches_reserve(RunnerMatches *matches) {
    if (matches->count < matches->capacity) {
        return 0;
    }
    int capacity = matches->capacity > 0 ? matches->capacity * 2 : 64;
    RunnerMatch *grown = realloc(matches->items,
                                 (size_t)capacity * sizeof(RunnerMatch));
    if (!grown) {
        return -1;
    }
    matches->items = grown;
    matches->capacity = capacity;
    return 0;
}

/* Put one match in its place, best first.
 *
 * An insertion into a sorted array, done as the matches are added rather than
 * with a sort after: the array is filled in one pass, and each item moves only
 * past the ones it beats. On a few hundred items that is a handful of
 * comparisons per match, and it needs no second buffer and no recursion —
 * which on the machine this is written for matters more than the asymptotics.
 */
static void insert_sorted(RunnerMatches *matches, int program_index,
                          int score) {
    if (matches->count >= matches->capacity) {
        return;
    }

    int at = matches->count;
    while (at > 0 && matches->items[at - 1].score < score) {
        matches->items[at] = matches->items[at - 1];
        at--;
    }
    matches->items[at].program_index = program_index;
    matches->items[at].score = score;
    matches->count++;
}

/* The program's own name, taken from the command it runs: the last component
   of command[0], so "/usr/bin/nemo" and "nemo" both give "nemo".
 *
 * It is searched because a program's DISPLAYED name is not always what a
 * person types. The clearest case is nemo, whose entry says Name=Files: a
 * person who knows the program as "nemo" types that, and a launcher that only
 * looked at "Files" found nothing and fell through to running the line as a
 * command — which worked, but showed nothing in the list, which is the fault
 * this fixes. The command's name is what the program calls itself in a shell. */
static void command_program_name(const RunnerProgram *program, char *out,
                                 unsigned int size) {
    out[0] = '\0';
    if (program->count <= 0 || program->command[0][0] == '\0') {
        return;
    }
    const char *full = program->command[0];
    const char *slash = strrchr(full, '/');
    const char *base = slash ? slash + 1 : full;
    snprintf(out, size, "%s", base);
}

void runner_match_query(RunnerMatches *matches, const RunnerProgramList *list,
                        const RunnerConfig *config, const char *query) {
    matches->count = 0;
    if (!matches || !list || !config) {
        return;
    }
    if (!query) {
        query = "";
    }

    for (int i = 0; i < list->count; i++) {
        const RunnerProgram *program = &list->items[i];

        int score = score_name(program->name, query,
                               config->case_sensitive, config->fuzzy);

        /* The other words the program can be found by, when the displayed
           name matched weakly or not at all: the keyword/generic field the
           entry carried, and the program name inside the command. A match on
           one of those is ranked a little below a match on the name, so a
           program whose NAME answers the query still comes first. */
        if (program->keywords[0]) {
            int extra = score_name(program->keywords, query,
                                   config->case_sensitive, config->fuzzy);
            if (extra > 0) {
                extra -= SCORE_ALTERNATE_PENALTY;
                if (extra > score) {
                    score = extra;
                }
            }
        }
        {
            char command_name[RUNNER_TEXT_LENGTH];
            command_program_name(program, command_name, sizeof(command_name));
            if (command_name[0]) {
                int extra = score_name(command_name, query,
                                       config->case_sensitive, config->fuzzy);
                if (extra > 0) {
                    extra -= SCORE_ALTERNATE_PENALTY;
                    if (extra > score) {
                        score = extra;
                    }
                }
            }
        }

        if (score < 0) {
            continue;
        }
        if (matches_reserve(matches) != 0) {
            return;         /* out of memory: the matches so far are still
                               a usable answer, and a crash is not */
        }
        insert_sorted(matches, i, score);
    }
}
