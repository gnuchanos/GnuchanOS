/* Version: derleyici ve dil versiyon bilgisi */
#include "gcl.h"
#include "version.h"

void version_print_short(void) {
    printf("gcl %s\n", GCL_VERSION_STR);
}

void version_print_full(void) {
    printf("Gnuchan Language Compiler\n");
    printf("Version:     %s\n", GCL_VERSION_STR);
    printf("Stage:       %s\n", GCL_STAGE);
    printf("Language:    %s\n", GCL_LANG_NAME);
    printf("Standard:    %s\n", GCL_LANG_STANDARD);
    printf("Base:        GNU99 + GNU Extensions\n");
    printf("Built:       %s %s\n", __DATE__, __TIME__);
}

int version_check_lang(const char *req) {
    /* req format: "0.1" veya "0.1.0" */
    int maj = 0, min = 0, pat = 0;
    sscanf(req, "%d.%d.%d", &maj, &min, &pat);
    if (maj > GCL_MAJOR) return -1;  /* req karsilanamaz */
    if (maj < GCL_MAJOR) return 1;   /* fazlasiyla karsilanir */
    if (min > GCL_MINOR) return -1;
    if (min < GCL_MINOR) return 1;
    if (pat > GCL_PATCH) return -1;
    return 0; /* tam eslesme */
}
