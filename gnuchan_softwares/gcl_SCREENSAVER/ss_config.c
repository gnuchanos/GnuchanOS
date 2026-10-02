/*
 * ss_config.c — the walk that gives the settings script its meaning.
 *
 * ss_parser.c reads the file as a name and a value, or a name and a call. This
 * is the other half: it knows which call a screen saver understands and what
 * each of its arguments means.
 *
 * The one call it understands, whole:
 *
 *     gcl_SS.Effect(
 *         Name="pipe",               which effect: "pipe" or "3dwall"
 *         PrimaryColor="#d400ff",    the effect's colour
 *         BackgroundColor="#27022b", what it draws over
 *         IdleSeconds=300,           how long the desk is quiet first
 *         Inhibit=True,              take the ScreenSaver D-Bus name
 *     )
 *
 * A name that is not one of these is not a mistake worth refusing the file
 * for: a script is written while it is being lived with, and a value the saver
 * does not use is a value it does not use. That is different from a statement
 * that cannot be READ — an unclosed bracket, a missing quote — which the
 * parser refuses, and this module turns that refusal into the message the
 * program prints.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ss_config.h"
#include "ss_parser.h"

/* --- the saver a machine with no settings file gets ----------------------- */

void ss_config_defaults(SsConfig *config) {
    memset(config, 0, sizeof(*config));

    config->effect = SS_EFFECT_PIPE;
    /* The desktop's own purple and its dark background, as names; they are
       resolved to pixels by whoever has a display. */
    snprintf(config->primary, sizeof(config->primary), "#d400ff");
    snprintf(config->background, sizeof(config->background), "#27022b");
    config->idle_seconds = 300;
    config->inhibit = 1;
    config->error[0] = '\0';
}

/* --- the effect's name ---------------------------------------------------- */

SsEffectKind ss_effect_of(const char *written) {
    if (!written) {
        return SS_EFFECT_PIPE;
    }
    if (strcmp(written, "3dwall") == 0 || strcmp(written, "3DWall") == 0 ||
        strcmp(written, "wall") == 0) {
        return SS_EFFECT_3DWALL;
    }
    /* "pipe" and anything unknown are the pipe: a name that matches nothing is
       reported by the walker, and the pipe is the honest default. */
    return SS_EFFECT_PIPE;
}

const char *ss_effect_name(SsEffectKind effect) {
    return effect == SS_EFFECT_3DWALL ? "3dwall" : "pipe";
}

/* --- reading one call ----------------------------------------------------- */

/* If the call passed an argument called `name`, copy its text into out. Returns
   1 when it did. A caller that already has a value keeps it when this returns
   0, so a script that names one colour and not another gets the desktop it
   expects for the one it left out. */
static int argument_text(const SsStatement *statement, const char *name,
                         char *out, unsigned int size) {
    const SsValue *value = ss_argument(statement, name);
    if (!value) {
        return 0;
    }
    ss_value_text(value, out, size);
    return out[0] != '\0';
}

/* gcl_SS.Effect(...). The only call this program has. */
static void read_effect(SsConfig *config, const SsStatement *statement) {
    char text[SS_TEXT_LENGTH];

    if (argument_text(statement, "Name", text, sizeof(text))) {
        SsEffectKind effect = ss_effect_of(text);
        /* A name that is not "pipe" and not "3dwall" is a mistake the person
           who wrote it wants to hear about, rather than a silent pipe. */
        if (strcmp(text, "pipe") != 0 &&
            strcmp(text, "3dwall") != 0 &&
            strcmp(text, "3DWall") != 0 &&
            strcmp(text, "wall") != 0) {
            fprintf(stderr,
                    "gnuchanss: config: effect '%s' is not known; the ones "
                    "there are \"pipe\" and \"3dwall\", so the pipe is used\n",
                    text);
        }
        config->effect = effect;
    }

    /* The two colours are read under their own names, and each of the obvious
       spellings works: PrimaryColor or PrimaryColour, and so on. A script
       written by a person spells it one way or the other. */
    if (argument_text(statement, "PrimaryColor", text, sizeof(text)) ||
        argument_text(statement, "PrimaryColour", text, sizeof(text))) {
        snprintf(config->primary, sizeof(config->primary), "%s", text);
    }
    if (argument_text(statement, "BackgroundColor", text, sizeof(text)) ||
        argument_text(statement, "BackgroundColour", text, sizeof(text))) {
        snprintf(config->background, sizeof(config->background), "%s", text);
    }

    config->idle_seconds = ss_value_number(
        ss_argument(statement, "IdleSeconds"), config->idle_seconds);
    config->inhibit = ss_value_bool(
        ss_argument(statement, "Inhibit"), config->inhibit);
}

/* --- the public entry points ---------------------------------------------- */

char *ss_config_path(char *buffer, unsigned int size) {
    const char *xdg = getenv("XDG_CONFIG_HOME");
    if (xdg && xdg[0]) {
        snprintf(buffer, size, "%s/GnuChanSS/GnuChanSS.py", xdg);
        return buffer;
    }
    const char *home = getenv("HOME");
    if (home && home[0]) {
        snprintf(buffer, size, "%s/.config/GnuChanSS/GnuChanSS.py", home);
        return buffer;
    }
    /* No home and no XDG directory: there is nowhere to look, and the caller
       reads that as "no script" rather than as an error. */
    buffer[0] = '\0';
    return buffer;
}

int ss_config_load(SsConfig *config, const char *path) {
    /* The defaults are laid down first, and the walk only ever changes what
       the file names. That is what makes a settings file able to name one
       colour and get the rest of the saver it expects. */
    ss_config_defaults(config);

    if (!path || !path[0]) {
        snprintf(config->error, sizeof(config->error),
                 "no settings file was found");
        return -1;
    }

    SsStatement *statements = NULL;
    int count = 0;
    if (ss_parse_file(path, &statements, &count) != 0) {
        snprintf(config->error, sizeof(config->error), "%s",
                 ss_parse_last_error());
        return -1;
    }

    for (int i = 0; i < count; i++) {
        if (statements[i].kind == SS_STMT_CALL &&
            strcmp(statements[i].target, "gcl_SS.Effect") == 0) {
            read_effect(config, &statements[i]);
        }
        /* Every other statement is a name or a call this saver does not act
           on. It is left alone: a script is lived in, and not every name in it
           is meant for here. */
    }
    ss_parse_free(statements, count);

    /* One setting with a sensible floor, clamped here rather than at the use:
       a saver that runs the instant the desk is quiet is one nobody can use,
       and the file may say so by mistake. */
    if (config->idle_seconds < 1) {
        config->idle_seconds = 1;
    }
    return 0;
}

int ss_config_load_default(SsConfig *config) {
    char path[SS_TEXT_LENGTH];
    ss_config_path(path, sizeof(path));
    if (!path[0]) {
        ss_config_defaults(config);
        return -1;
    }
    if (ss_config_load(config, path) != 0) {
        /* No file, or a file that could not be read: the defaults are what the
           caller wanted anyway, and the reason is in config->error. */
        return -1;
    }
    return 0;
}
