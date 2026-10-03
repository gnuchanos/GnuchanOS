/*
 * main.cpp — start the browser on the machine it actually runs on.
 *
 * This is a 2007 laptop: an Intel 965GM whose OpenGL is desktop 2.1 and GLES
 * 2.0. Chromium — which Qt WebEngine embeds — runs only on GLES 3.0, so on
 * this GPU its compositor can never make a context and always falls back to
 * the CPU. Measured here: with the blocklist overridden and GLX named, the
 * engine still reports "Failed to initialize Skia for SharedContextState".
 * Asking it to try the GPU on every load only wastes startup and fills the
 * log; the engine is therefore pinned to software rendering, and the heavy
 * background machinery is trimmed for a two-core, low-memory machine.
 *
 * The switches are set BEFORE the QApplication is built: Chromium inside Qt
 * WebEngine reads them when it starts, and a switch set after that point is a
 * switch nothing reads. Qt reads the same switches from the environment, so
 * this is the one place they can be set for both a person running the binary
 * and a test that runs it.
 *
 *     GnuChanBrowser              the optimised software profile below
 *     GnuChanBrowser --software   accepted for compatibility; the same thing
 */
#include "browser_theme.h"
#include "browser_window.h"

#include <QApplication>
#include <QByteArray>

int main(int argc, char **argv) {
    /* The engine wants this for a session that runs the setuid-free binary as
       a normal user without a sandbox helper installed. */
    qputenv("QTWEBENGINE_DISABLE_SANDBOX", QByteArrayLiteral("1"));

    QByteArray flags;

    /* No GPU process at all - and that is the point, not a side effect.

       The wrong turn was to keep a GPU process alive and point it at ANGLE on
       SwiftShader, hoping for software WebGL. It ran, and it cost more than
       it gave: this machine's software GL context kept being lost (thousands
       of "CreateSharedImage failed"), so the process never settled and the
       browser burned about 180% of the CPU while sitting on an empty page.
       That is worse than the feature was worth.

       `--disable-gpu` starts no GPU process, so there is nothing left to spin.
       The pages are composited by the processor directly, the browser is quiet
       when idle, and this machine's real GPU — which Chromium cannot use
       anyway, since it needs GLES 3.0 and this chip gives 2.0 — is never in
       the path and cannot hang. The cost is WebGL: with no GPU process it is
       simply off. On a 2007 laptop that is the right trade, and it is the one
       that keeps the fans off. */
    flags +=
        "--disable-gpu "
        "--disable-gpu-compositing "
        "--disable-gpu-rasterization "
        "--disable-accelerated-2d-canvas "
        "--disable-accelerated-video-decode "
        "--disable-accelerated-video-encode ";

    /* Fit the machine: two cores and little memory.

         --renderer-process-limit=1    one renderer instead of one per site,
                                       the single biggest memory saving on a
                                       small laptop
         --num-raster-threads=1        one raster thread, matching two cores,
                                       rather than spawning past them
         --disable-dev-shm-usage       /dev/shm is often small; keeping the
                                       work in memory avoids running it dry */
    flags +=
        "--renderer-process-limit=1 "
        "--num-raster-threads=1 "
        "--disable-dev-shm-usage ";

    /* Built for a processor, since that is all this machine has to draw with.

         --enable-low-end-device-mode  Chromium's own profile for a weak
                                       machine: it trims buffers, stops
                                       animation and other work that exists to
                                       look smooth on hardware that can afford
                                       it, and is exactly what a software-only
                                       browser on two cores wants
         --js-flags=--max-old-space-size=128
                                       cap the page heap at 128 MB. A large
                                       heap is a promise of garbage collection
                                       later, and there is only 4 GB here
         --disk-cache-size=52428800    a 50 MB disk cache instead of the
                                       default; less written, less read back
         --disable-features=...        the background machinery that has no
                                       place in a light browser: the back/
                                       forward page cache, translate, the
                                       optimisation-hints fetch, and the media
                                       router */
    flags +=
        "--enable-low-end-device-mode "
        "--js-flags=--max-old-space-size=128 "
        "--disk-cache-size=52428800 "
        "--disable-features=BackForwardCache,Translate,OptimizationHints,"
        "MediaRouter ";

    /* Fewer programs, and no rescue timers. The network service normally runs
       as its own process; on a machine this small it is worth folding into the
       browser process - one fewer program to schedule and to keep in memory.
       The hang and GPU watchdogs are timers that exist to recover a machine
       whose hardware stalls; this one has no GPU in the path, and on a slow
       processor a watchdog that fires on ordinary slowness is worse than no
       watchdog, because "the page is unresponsive" is just what a heavy page
       looks like here. */
    flags +=
        "--enable-features=NetworkServiceInProcess "
        "--disable-hang-monitor "
        "--disable-gpu-watchdog "
        "--disable-crash-reporter "
        "--no-pings "
        "--disable-prompt-on-repost ";

    /* Less happening behind the window, which is CPU the page never gets.
       None of these change what a page shows; they stop background network,
       update checks, and metrics. */
    flags +=
        "--disable-background-networking "
        "--disable-component-update "
        "--disable-domain-reliability "
        "--disable-client-side-phishing-detection "
        "--disable-sync "
        "--disable-default-apps "
        "--disable-extensions "
        "--disable-translate "
        "--no-first-run "
        "--no-default-browser-check "
        "--metrics-recording-only "
        "--disable-breakpad";

    qputenv("QTWEBENGINE_CHROMIUM_FLAGS", flags);

    QApplication application(argc, argv);
    application.setApplicationName(QStringLiteral("GnuChanBrowser"));
    /* One style sheet for the whole window — the GnuchanOS violet — so every
       widget in the browser is painted from the same palette. */
    application.setStyleSheet(browser_style_sheet());

    BrowserWindow window;
    window.show();

    return application.exec();
}
