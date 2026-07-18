#include "token.h"
#include <string.h>

TokenType token_keyword(const char *s, size_t len) {
    if (!s || len == 0) return TOKEN_IDENTIFIER;
    switch (len) {
    case 2:
        if (memcmp(s, "if", 2) == 0) return TOKEN_KW_IF;
        break;
    case 3:
        if (memcmp(s, "int", 3) == 0) return TOKEN_KW_INT;
        if (memcmp(s, "for", 3) == 0) return TOKEN_KW_FOR;
        break;
    case 4:
        if (memcmp(s, "char", 4) == 0) return TOKEN_KW_CHAR;
        if (memcmp(s, "long", 4) == 0) return TOKEN_KW_LONG;
        if (memcmp(s, "void", 4) == 0) return TOKEN_KW_VOID;
        if (memcmp(s, "else", 4) == 0) return TOKEN_KW_ELSE;
        if (memcmp(s, "bool", 4) == 0) return TOKEN_KW_BOOL;
        if (memcmp(s, "true", 4) == 0) return TOKEN_KW_TRUE;
        if (memcmp(s, "int8", 4) == 0) return TOKEN_KW_INT8;
        break;
    case 5:
        if (memcmp(s, "short", 5) == 0) return TOKEN_KW_SHORT;
        if (memcmp(s, "float", 5) == 0) return TOKEN_KW_FLOAT;
        if (memcmp(s, "while", 5) == 0) return TOKEN_KW_WHILE;
        if (memcmp(s, "false", 5) == 0) return TOKEN_KW_FALSE;
        if (memcmp(s, "int16", 5) == 0) return TOKEN_KW_INT16;
        if (memcmp(s, "int32", 5) == 0) return TOKEN_KW_INT32;
        if (memcmp(s, "int64", 5) == 0) return TOKEN_KW_INT64;
        if (memcmp(s, "uint8", 5) == 0) return TOKEN_KW_UINT8;
        break;
    case 6:
        if (memcmp(s, "double", 6) == 0) return TOKEN_KW_DOUBLE;
        if (memcmp(s, "return", 6) == 0) return TOKEN_KW_RETURN;
        if (memcmp(s, "int128", 6) == 0) return TOKEN_KW_INT128;
        if (memcmp(s, "size_t", 6) == 0) return TOKEN_KW_SIZE_T;
        if (memcmp(s, "uint16", 6) == 0) return TOKEN_KW_UINT16;
        if (memcmp(s, "uint32", 6) == 0) return TOKEN_KW_UINT32;
        if (memcmp(s, "uint64", 6) == 0) return TOKEN_KW_UINT64;
        break;
    case 7:
        if (memcmp(s, "intptr_t", 7) == 0) return TOKEN_KW_INTPTR_T;
        if (memcmp(s, "ssize_t", 7) == 0) return TOKEN_KW_SSIZE_T;
        break;
    case 8:
        if (memcmp(s, "unsigned", 8) == 0)  return TOKEN_KW_UNSIGNED;
        if (memcmp(s, "uintptr_t", 8) == 0)  return TOKEN_KW_UINTPTR_T;
        break;
    case 9:
        /* uint8_t is too long for keyword, use uint8 (len=5) instead */
        break;
    }
    return TOKEN_IDENTIFIER;
}

const char *token_name(TokenType t) {
    switch (t) {
    case TOKEN_EOF: return "EOF";
    case TOKEN_IDENTIFIER: return "IDENTIFIER";
    case TOKEN_NUMBER_INT: return "NUMBER_INT";
    case TOKEN_NUMBER_FLOAT: return "NUMBER_FLOAT";
    case TOKEN_STRING: return "STRING";
    case TOKEN_CHAR_LIT: return "CHAR_LIT";
    case TOKEN_KW_CHAR: return "char";
    case TOKEN_KW_SHORT: return "short";
    case TOKEN_KW_INT: return "int";
    case TOKEN_KW_LONG: return "long";
    case TOKEN_KW_FLOAT: return "float";
    case TOKEN_KW_DOUBLE: return "double";
    case TOKEN_KW_VOID: return "void";
    case TOKEN_KW_IF: return "if";
    case TOKEN_KW_ELSE: return "else";
    case TOKEN_KW_WHILE: return "while";
    case TOKEN_KW_RETURN: return "return";
    case TOKEN_KW_FOR: return "for";
    case TOKEN_PREPROC_INCLUDE: return "#include";
    case TOKEN_PREPROC_LIB: return "#lib";
    case TOKEN_PREPROC_EXTERN: return "#extern";
    /* new type keywords */
    case TOKEN_KW_INT8: return "int8";
    case TOKEN_KW_INT16: return "int16";
    case TOKEN_KW_INT32: return "int32";
    case TOKEN_KW_INT64: return "int64";
    case TOKEN_KW_INT128: return "int128";
    case TOKEN_KW_UNSIGNED: return "unsigned";
    case TOKEN_KW_UINT8: return "uint8";
    case TOKEN_KW_UINT16: return "uint16";
    case TOKEN_KW_UINT32: return "uint32";
    case TOKEN_KW_UINT64: return "uint64";
    case TOKEN_KW_BOOL: return "bool";
    case TOKEN_KW_SIZE_T: return "size_t";
    case TOKEN_KW_SSIZE_T: return "ssize_t";
    case TOKEN_KW_INTPTR_T: return "intptr_t";
    case TOKEN_KW_UINTPTR_T: return "uintptr_t";
    case TOKEN_KW_TRUE: return "true";
    case TOKEN_KW_FALSE: return "false";
    /* Operators & punctuation */
    case TOKEN_PLUS:    return "+";
    case TOKEN_MINUS:   return "-";
    case TOKEN_STAR:    return "*";
    case TOKEN_SLASH:   return "/";
    case TOKEN_PERCENT: return "%";
    case TOKEN_EQ:      return "=";
    case TOKEN_EQEQ:    return "==";
    case TOKEN_BANG:    return "!";
    case TOKEN_BANGEQ:  return "!=";
    case TOKEN_LT:      return "<";
    case TOKEN_GT:      return ">";
    case TOKEN_LE:      return "<=";
    case TOKEN_GE:      return ">=";
    case TOKEN_AND:     return "&";
    case TOKEN_ANDAND:  return "&&";
    case TOKEN_OR:      return "|";
    case TOKEN_OROR:    return "||";
    case TOKEN_PIPE:    return "|";
    case TOKEN_CARET:   return "^";
    case TOKEN_TILDE:   return "~";
    case TOKEN_LSHIFT:  return "<<";
    case TOKEN_RSHIFT:  return ">>";
    case TOKEN_PLUSPLUS:   return "++";
    case TOKEN_MINUSMINUS: return "--";
    case TOKEN_PLUSEQ:     return "+=";
    case TOKEN_MINUSEQ:    return "-=";
    case TOKEN_STAREQ:     return "*=";
    case TOKEN_SLASHEQ:    return "/=";
    case TOKEN_PERCENTEQ:  return "%=";
    case TOKEN_ANDEQ:      return "&=";
    case TOKEN_PIPEEQ:     return "|=";
    case TOKEN_CARETEQ:    return "^=";
    case TOKEN_ARROW:    return "->";
    case TOKEN_DOT:      return ".";
    case TOKEN_COMMA:    return ",";
    case TOKEN_SEMI:     return ";";
    case TOKEN_COLON:    return ":";
    case TOKEN_QUESTION: return "?";
    case TOKEN_LPAREN:   return "(";
    case TOKEN_RPAREN:   return ")";
    case TOKEN_LBRACKET: return "[";
    case TOKEN_RBRACKET: return "]";
    case TOKEN_LBRACE:   return "{";
    case TOKEN_RBRACE:   return "}";
    case TOKEN_AT:       return "@";
    case TOKEN_AMPERSAND: return "&";
    default: return "?";
    }
}
