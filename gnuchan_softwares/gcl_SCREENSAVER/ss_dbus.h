/*
 * ss_dbus.h — the org.freedesktop.ScreenSaver name, so a video can pause us.
 *
 * Firefox, Chromium, mpv and VLC follow the freedesktop screen-saver protocol:
 * while a video is playing they call Inhibit on the session's ScreenSaver name
 * and expect the running screen saver to honour it. A saver that does NOT hold
 * the name cannot be inhibited — the applications call into nothing and the
 * saver, if it ran anyway, would drop a pipe show over the middle of a film.
 *
 * So this is the small piece that takes that name and answers the calls. It is
 * optional on purpose: a machine without dbus (or built without it) still shows
 * a screen saver, it just cannot be paused by a video, and everything here
 * degrades to "not inhibited".
 *
 * The name is announced on the session bus, which is where the applications
 * look. Only the calls a screen saver has to answer are implemented; the rest
 * answer a harmless default.
 */
#ifndef GNUCHANSS_DBUS_H
#define GNUCHANSS_DBUS_H

/* Take the ScreenSaver name. Returns 1 when the name was claimed and callers
   can be answered, 0 when D-Bus is not available or the name is already held
   by another saver. Either way the caller carries on. */
int ss_dbus_claim(void);

/* How many inhibitors are held right now: a video playing means it is more
   than zero, and the saver then does not draw. */
int ss_dbus_inhibit_count(void);

/* Let the bus deliver any calls that are waiting. Called between frames, so a
   video that starts inhibiting mid-show is noticed within a frame. */
void ss_dbus_pump(void);

/* Release the name. Called on the way out, so a second saver can take it. */
void ss_dbus_release(void);

#endif /* GNUCHANSS_DBUS_H */
