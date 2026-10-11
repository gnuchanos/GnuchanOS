"""Progress reporting for the stages of a build that take minutes.

The build renders thousands of icons, and one of its loops runs for most of the
time the script is alive. A silent script during that loop is indistinguishable
from a hung one, which is exactly the question this answers: a numbered heading
per stage, a percentage line while a stage is running, and the elapsed time when
it ends.

Nothing here decides anything. It is handed the same message sink the rest of
the library uses — ``print`` from the shipped script — so a caller that wants a
quiet build passes ``None`` and gets one.
"""

from __future__ import annotations

import time
from typing import Callable

#: Roughly how many progress lines one stage prints. Twenty is enough to see
#: movement on a terminal and few enough that a saved log stays readable.
STEPS_PER_STAGE = 20

#: A stage that finishes faster than this prints no percentage lines at all.
#: Without it, a stage that renders in a fraction of a second would still emit
#: its twenty evenly spaced lines and bury the output that matters.
MIN_TICK_SECONDS = 1.0

Logger = Callable[[str], None]


def clock(seconds: float) -> str:
    """A duration a person can read: seconds below a minute, else minutes."""
    if seconds < 60.0:
        return f"{seconds:.1f}s"
    minutes, rest = divmod(seconds, 60.0)
    return f"{int(minutes)}m{rest:04.1f}s"


class Progress:
    """Numbered stages with a running percentage inside the long ones.

    ``stages`` is the number of stages the caller is going to run, so each
    heading can say ``[3/6]``. Passing zero omits the total and prints ``[3]``,
    which is what a caller with an unknown number of steps wants.
    """

    def __init__(self, log: Logger, stages: int = 0) -> None:
        self._log = log
        self._stages = stages
        self._index = 0
        self._started = 0.0
        self._next = 0.0

    def stage(self, title: str) -> None:
        """Start a stage: announce it and begin timing it."""
        self._index += 1
        self._started = time.monotonic()
        self._next = 0.0
        counter = f"[{self._index}/{self._stages}]" if self._stages else f"[{self._index}]"
        self._log(f"{counter} {title}")

    def tick(self, done: int, total: int, detail: str = "") -> None:
        """Report progress inside a stage, at most ``STEPS_PER_STAGE`` times.

        The line is emitted when the count crosses a fixed fraction of the
        total rather than on a clock, so a stage that renders in two seconds
        prints nothing extra and the one that takes two minutes prints twenty
        evenly spaced lines.
        """
        if total <= 0 or done < self._next:
            return
        seconds = time.monotonic() - self._started
        if seconds < MIN_TICK_SECONDS:
            return
        step = max(1, total // STEPS_PER_STAGE)
        self._next = (done // step + 1) * step
        elapsed = clock(seconds)
        percent = 100.0 * done / total
        line = f"    {percent:5.1f}%  {done}/{total}  {elapsed}"
        if detail:
            line += f"  {detail}"
        self._log(line)

    def note(self, text: str) -> None:
        """One line of detail, for something that happened at this stage."""
        self._log(f"    {text}")

    def finish(self, detail: str = "") -> None:
        """End a stage with how long it took, and optionally what it produced."""
        elapsed = clock(time.monotonic() - self._started)
        line = f"    done in {elapsed}"
        if detail:
            line += f"  {detail}"
        self._log(line)
