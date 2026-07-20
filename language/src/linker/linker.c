// ============================================================
// GCL Linker — Stub (v0.1)
// ============================================================

void linker_init(void) {}
void linker_link(const char *output, const char **objects, int count) {
    (void)output;
    (void)objects;
    (void)count;
    // v0.1: Passthrough — linking done by GCC directly
}
