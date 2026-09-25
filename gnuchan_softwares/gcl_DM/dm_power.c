/*
 * dm_power.c — reboot and power off.
 *
 * A display manager runs as root, so it can ask the kernel directly. Going
 * through systemctl or logind would be the desktop's way, but a greeter is
 * deliberately below all of that: at the login screen there may be no init
 * system session to ask, and a machine that cannot be turned off from its
 * own login screen is a machine that has to be turned off with its button.
 *
 * The call is the same one `reboot` and `poweroff` make. If it is refused —
 * a container, or a kernel built without it — the greeter says so and returns
 * to the login screen rather than dying.
 */
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/reboot.h>

#include "dm_core.h"

/* Flush first: sync, sync, then the call, because the kernel's own flush on
   the way down is not guaranteed to reach a disk written to a second ago. */
static void sync_twice(void) {
    sync();
    sync();
}

void dm_power_reboot(void) {
    fprintf(stderr, "gnuchandm: rebooting\n");
    sync_twice();
    if (reboot(RB_AUTOBOOT) != 0) {
        fprintf(stderr, "gnuchandm: cannot reboot: %s\n", strerror(errno));
    }
}

void dm_power_shutdown(void) {
    fprintf(stderr, "gnuchandm: shutting down\n");
    sync_twice();
    if (reboot(RB_POWER_OFF) != 0) {
        fprintf(stderr, "gnuchandm: cannot power off: %s\n", strerror(errno));
    }
}
