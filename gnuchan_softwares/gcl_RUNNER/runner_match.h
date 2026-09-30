/*
 * runner_match.h — narrowing the list to what was typed.
 *
 * A launcher is one thing above all: it finds a program from a few letters.
 * Everything else it does is presentation. So this is the part that decides
 * what a query finds, and it is kept on its own because it is the one piece
 * that can be got wrong in a way the user feels — "I typed the name and it was
 * not there" — and the piece worth being able to read on its own.
 *
 * Two ways to match, and both are on by default:
 *
 *   Substring   the query appears in the name, in order, with no gaps. This is
 *               what finds "term" in "Terminal": the letters are there, next to
 *               each other.
 *
 *   Subsequence the query's letters appear in the name in order but NOT next to
 *               each other. This is what finds "gimp" in "GNU Image
 *               Manipulation Program", and "ffx" in "Firefox". Typing initials
 *               is how most people use a launcher.
 *
 * A subsequence match is weaker than a substring one and is scored lower, so a
 * program whose name contains the query exactly comes first and the initials
 * come after. The score is what orders the list, and it is computed here so
 * the drawing code never has to think about why one row is above another.
 */
#ifndef GNUCHANRUNNER_MATCH_H
#define GNUCHANRUNNER_MATCH_H

#include "runner_apps.h"
#include "runner_config.h"

/* One match: which program, and how well it matched. */
typedef struct RunnerMatch {
    int program_index;   /* into the RunnerProgramList the match came from */
    int score;           /* higher is a better match; never negative       */
} RunnerMatch;

/* The matches a query found, best first.
 *
 * The array is allocated by the call and owned by it until the next call or
 * the free: the launcher queries again on every key press, and one buffer
 * reused for all of them is what keeps a fast typist from allocating on each
 * letter. `count` is how many of the array are filled. */
typedef struct RunnerMatches {
    RunnerMatch *items;
    int count;
    int capacity;
} RunnerMatches;

void runner_match_init(RunnerMatches *matches);
void runner_match_free(RunnerMatches *matches);

/* Fill `matches` with every program the query finds, best first.
 *
 * An empty query matches everything, with every score equal, and the order is
 * the list's own — which is the order the directory happened to be read in and
 * is as good as any: a launcher with nothing typed is not a list a person
 * reads, it is one they type into.
 *
 * The comparison is done here and nowhere else, so case folding, the fuzzy
 * switch and the scoring are one piece of code with one meaning. */
void runner_match_query(RunnerMatches *matches, const RunnerProgramList *list,
                        const RunnerConfig *config, const char *query);

#endif /* GNUCHANRUNNER_MATCH_H */
