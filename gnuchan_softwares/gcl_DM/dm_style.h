/*
 * dm_style.h — the one place every colour and size lives.
 *
 * The greeter has no theming yet and does not need any: this struct is the
 * whole of its appearance, so a future settings file only has to fill it in.
 * Keeping the palette in one place is also what keeps the greeter and the
 * window manager looking like one system — the accent here is the same
 * #c77dff the WM draws its focused border in.
 */
#ifndef GNUCHANDM_STYLE_H
#define GNUCHANDM_STYLE_H

#include <X11/Xlib.h>

typedef struct DmStyle {
    /* Purple, dark to bright. */
    unsigned long background;    /* #1a0b2e - the screen behind the panel   */
    unsigned long panel;         /* #32143f - the login panel              */
    unsigned long panel_edge;    /* #7b2cbf - the line around the panel    */
    unsigned long field;         /* #241033 - an input field, unfocused    */
    unsigned long field_focus;   /* #3a1a52 - the field being typed in     */
    unsigned long text;          /* #e0c3fc - normal text                  */
    unsigned long text_muted;    /* #9d7bba - labels and hints             */
    unsigned long accent;        /* #c77dff - the sign-in button, focus    */
    unsigned long accent_dim;    /* #7b2cbf - the button when it is idle   */
    unsigned long danger;        /* #ff5d8f - an error message             */

    XFontStruct *font_label;     /* field labels and buttons               */
    XFontStruct *font_field;     /* what the user types                    */
    XFontStruct *font_title;     /* the host name above the panel          */

    int margin;                  /* space inside the panel and the window  */
    int gap;                     /* space between two stacked controls     */
    int panel_width;             /* the login panel's width                */
    int field_height;            /* the height of one input field          */
    int button_height;           /* the height of one button               */
} DmStyle;

/* Resolve every colour and load a font. Returns 0 on success. A font that
   cannot be loaded is not fatal: a missing font falls back to whatever the
   X server offers, because a greeter that refuses to draw is worse than one
   drawn in a font the user did not choose. */
int  dm_style_load(DmStyle *style, Display *display, int screen);

void dm_style_free(DmStyle *style, Display *display);

#endif /* GNUCHANDM_STYLE_H */
