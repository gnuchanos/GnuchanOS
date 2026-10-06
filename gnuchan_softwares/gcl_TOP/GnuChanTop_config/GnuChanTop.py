# GnuChanTop - the settings the program reads, and nothing else.
#
# Every value here has a default built into the program, so this file is
# optional: remove it and GnuChanTop still runs, on the defaults. Edit a value
# and the program picks it up on the next start. The file is read as text, not
# executed - see top_config.c - so nothing here runs and nothing here can break
# the machine.
#
# The format is one `Key = value` per line, `#` starts a comment, and a line the
# program does not understand is skipped rather than being a reason to stop.

# Seconds between samples. Lower is more responsive and costs more CPU; a
# second is what btop uses and what this was built for.
UpdateTime = 1.0

# How many processes the list can hold on screen at once. The list is sorted
# and this limits how many rows are kept, so a machine with thousands of
# processes does not spend its time sorting them all every second.
ProcLimit = 200

# Draw the GPU half of the top band. When the machine has no GPU reading the
# half says so whatever this is set to; this is for a machine that has one and
# would rather not see it.
ShowGPU = true

# The column the process list is ordered by to begin with: cpu, mem, pid or
# name. The `s` key in the program cycles it while running, but does not write
# it back here, so this is where a permanent change is made.
Sort = "cpu"
