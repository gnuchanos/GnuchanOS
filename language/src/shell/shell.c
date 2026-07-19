/* Shell: interactive GCL REPL */
#include "gcl.h"
#include "lexer.h"
#include "ast.h"
#include "parser.h"
#include "codegen.h"
#include "semantic.h"
#include "version.h"
#include <string.h>

void shell_run(void) {
    printf("GCL Interactive Shell v%s\n", GCL_VERSION_STR);
    printf("Type 'exit' or Ctrl+C to quit\n\n");

    char line[4096];
    int total_len = 0;
    char *buf = 0;
    size_t buf_cap = 0;

    while (1) {
        printf("gcl> ");
        fflush(stdout);

        if (!fgets(line, sizeof(line), stdin)) {
            printf("\n");
            break;
        }

        /* exit / quit */
        if (strcmp(line, "exit\n") == 0 || strcmp(line, "quit\n") == 0)
            break;

        /* multi-line support: devam ediyor mu? */
        size_t llen = strlen(line);
        int has_semi = (llen > 0 && line[llen-2] == ';');

        /* buffer'a ekle */
        if (!buf) {
            buf = malloc(65536);
            buf_cap = 65536;
            total_len = 0;
        }
        size_t remain = buf_cap - total_len - 1;
        if (llen >= remain) break; /* cok uzun satir */
        memcpy(buf + total_len, line, llen);
        total_len += llen;
        buf[total_len] = 0;

        if (!has_semi) continue; /* henuz complete degil */

        /* lex */
        Lexer lx;
        lexer_init(&lx, buf, "<stdin>", DBG_NONE);

        /* parse */
        Parser ps;
        parser_init(&ps, &lx, buf, DBG_NONE);
        Node *ast = parser_parse(&ps);
        if (ps.errors > 0) {
            fprintf(stderr, "%d error(s)\n", ps.errors);
            total_len = 0;
            continue;
        }

        /* semantic */
        SemState sem;
        sem_init(&sem);
        sem_enter_scope(&sem);
        if (sem_analyze(&sem, ast, buf) > 0) {
            fprintf(stderr, "semantic error\n");
            total_len = 0;
            continue;
        }

        /* codegen -> stdout */
        codegen_generate(stdout, ast);
        printf("\n");

        total_len = 0; /* reset buffer */
    }

    free(buf);
}
