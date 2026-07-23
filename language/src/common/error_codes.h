#ifndef GCL_ERROR_CODES_H
#define GCL_ERROR_CODES_H

/* ==========================================================
 * GCL Error Codes
 * ==========================================================
 *
 * Format: EXXX
 *   E0XX — Lexer errors
 *   E1XX — Parser / Directive errors
 *   E2XX — Preprocessor errors
 *   E3XX — Codegen errors
 *   E4XX — Semantic / Type errors
 *
 * ========================================================== */

/* --- Lexer (E0XX) --- */
#define ERR_UNEXPECTED_CHAR      "E001"  /* Unexpected character in input */
#define ERR_UNTERMINATED_STRING  "E002"  /* Unterminated string literal */
#define ERR_UNTERMINATED_COMMENT "E003"  /* Unterminated block comment */

/* --- Parser / Directives (E1XX) --- */
#define ERR_EXPECTED_TOKEN       "E100"  /* Expected a specific token (generic) */
#define ERR_EXPECTED_IDENT       "E101"  /* Expected identifier after directive */
#define ERR_EXPECTED_STRING      "E102"  /* Expected string literal */
#define ERR_EXPECTED_NUMBER      "E103"  /* Expected numeric literal */
#define ERR_EXPECTED_BRACE_OPEN  "E104"  /* Expected '{' */
#define ERR_EXPECTED_BRACE_CLOSE "E105"  /* Expected '}' */
#define ERR_EXPECTED_LPAREN      "E106"  /* Expected '(' */
#define ERR_EXPECTED_RPAREN      "E107"  /* Expected ')' */
#define ERR_UNKNOWN_DIRECTIVE    "E108"  /* Unknown preprocessor directive */
#define ERR_INVALID_EXTERN_C     "E109"  /* Invalid extern "c" syntax — only C ABI supported */
#define ERR_BAD_DEFINE_SYNTAX    "E110"  /* Bad #define syntax */
#define ERR_BAD_IF_CONDITION     "E111"  /* Invalid #if/#elif condition */
#define ERR_UNEXPECTED_ELIF      "E112"  /* #elif without matching #if */
#define ERR_UNEXPECTED_ELSE      "E113"  /* #else without matching #if */
#define ERR_UNEXPECTED_ENDIF     "E114"  /* #endif without matching #if */
#define ERR_NESTED_IF_DEPTH      "E115"  /* #if nesting too deep (>1024) */

/* --- Preprocessor (E2XX) --- */
#define ERR_INCLUDE_NOT_FOUND    "E201"  /* #include file not found */
#define ERR_LIB_NOT_FOUND        "E202"  /* #lib file not found */
#define ERR_EXTERN_FILE_NOT_FOUND "E203" /* #extern file not found */
#define ERR_RECURSIVE_INCLUDE    "E204"  /* Recursive #include detected */
#define ERR_CONDITIONAL_EVAL     "E205"  /* Error evaluating #if condition */
#define ERR_UNDEFINED_DEFINE     "E206"  /* #undef on undefined name */

/* --- Codegen (E3XX) --- */
#define ERR_CODEGEN_UNSUPPORTED  "E301"  /* Unsupported construct in codegen */
#define ERR_INVALID_DEBUG_ARG    "E302"  /* Invalid argument in #debug */
#define ERR_EXPORT_FAILED        "E303"  /* Export / compilation failed */
#define ERR_INVALID_PLATFORM     "E304"  /* Unknown platform name */

/* --- Semantic / Type (E4XX) --- */
#define ERR_UNDEFINED_IDENTIFIER "E401"  /* Undefined identifier */
#define ERR_TYPE_MISMATCH        "E402"  /* Type mismatch */
#define ERR_REDEFINED_SYMBOL     "E403"  /* Symbol redefinition */

#endif /* GCL_ERROR_CODES_H */
