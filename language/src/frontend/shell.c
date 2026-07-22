#include "shell.h"
#include "lexer.h"
#include "parser.h"
#include "parse_directive.h"
#include "colors.h"
#include "codegen.h"
#include "preprocessor.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int shell_run(CodegenOpts *opts) {
    printf(CLR_PURPLE "gcl v0.2" CLR_RESET " — interactive shell\n\n");
    char line[1024];
    while (1) {
        printf(CLR_PURPLE "gcl> " CLR_RESET); fflush(stdout);
        if (!fgets(line, sizeof(line), stdin)) break;
        size_t len = strlen(line);
        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) line[--len] = '\0';
        if (len == 0) continue;
        if (strcmp(line, "exit") == 0 || strcmp(line, "quit") == 0) break;

        Lexer sl;
        lexer_init(&sl, line, "<shell>");
        Parser *sp = parser_new(&sl);
        AstNode *spg = parser_parse(sp);
        spg->left = preprocess_inline(spg);

        for (AstNode *sn = spg->left; sn; sn = sn->next)
            if (sn->kind == NODE_DEBUG) {
                opts->mode = MODE_EXEC;
                opts->output = stdout;
                codegen_emit(spg, opts);
                break;
            }

        ast_free(spg);
        free(sp);
    }
    printf("bye.\n");
    return 0;
}
