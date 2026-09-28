/*
 * runner_desktop.h — reading one .desktop file.
 *
 * A program installed on a freedesktop machine announces itself in a file of
 * key = value lines under a [Desktop Entry] heading. This reads that one
 * file into a RunnerProgram: the name to show, the comment under it, and the
 * command to run.
 *
 * It is kept apart from the scan that finds the files because they are two
 * jobs — walking directories, and reading what is in them — and only this one
 * needs to know the format. The format itself is small: a heading, then
 * names and values, with '#' starting a comment, and a key written twice
 * meaning the last one.
 *
 * Two entries are deliberately not read. An entry whose Type is not
 * Application is not a program — a Link or a Directory is a shortcut a file
 * manager draws, not something to start. An entry marked NoDisplay or Hidden
 * is installed but is not to be shown, which is exactly what those keys are
 * for: a launcher that listed them would be listing the things the machine
 * already said to leave out.
 */
#ifndef GNUCHANRUNNER_DESKTOP_H
#define GNUCHANRUNNER_DESKTOP_H

#include "runner_apps.h"

/* Read the .desktop file at path into program. `show_no_display` and
   `show_hidden` are the config's answer to whether the two hidden keys are
   respected. Returns 0 when the file held a program, -1 when it could not be
   read or did not describe one — which is the usual case for the many files
   in the directory that are not programs. */
int runner_desktop_read(const char *path, RunnerProgram *program,
                        int show_no_display, int show_hidden);

#endif /* GNUCHANRUNNER_DESKTOP_H */
