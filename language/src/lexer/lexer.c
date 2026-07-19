/* Lexer: kaynak kodu token akisina cevirir */
#include "gcl.h"
#include "lexer.h"

const char *token_name(TokenType t) {
    switch (t) {
    case T_EOF: return "EOF"; case T_ERROR: return "ERROR";
    case T_INT: return "int"; case T_IDENT: return "IDENTIFIER";
    case T_NUMBER: return "NUMBER"; case T_CHAR: return "CHAR";
    case T_SIZEOF: return "sizeof"; case T_SEMICOLON: return ";";
    case T_ASSIGN: return "="; case T_ADD_ASSIGN: return "+=";
    case T_SUB_ASSIGN: return "-="; case T_MUL_ASSIGN: return "*=";
    case T_DIV_ASSIGN: return "/="; case T_MOD_ASSIGN: return "%=";
    case T_AND_ASSIGN: return "&="; case T_OR_ASSIGN: return "|=";
    case T_XOR_ASSIGN: return "^="; case T_LSHIFT_ASSIGN: return "<<=";
    case T_RSHIFT_ASSIGN: return ">>=";
    case T_PLUS: return "+"; case T_MINUS: return "-";
    case T_STAR: return "*"; case T_SLASH: return "/";
    case T_PERCENT: return "%"; case T_AMPERSAND: return "&";
    case T_PIPE: return "|"; case T_CARET: return "^";
    case T_TILDE: return "~"; case T_LSHIFT: return "<<";
    case T_RSHIFT: return ">>";
    case T_EQ: return "=="; case T_NE: return "!=";
    case T_LT: return "<"; case T_GT: return ">";
    case T_LE: return "<="; case T_GE: return ">=";
    case T_AND: return "&&"; case T_OR: return "||";
    case T_NOT: return "!"; case T_INC: return "++";
    case T_DEC: return "--";
    case T_QUESTION: return "?"; case T_COLON: return ":";
    case T_LPAREN: return "("; case T_RPAREN: return ")";
    case T_LBRACE: return "{"; case T_RBRACE: return "}";
    case T_LBRACKET: return "["; case T_RBRACKET: return "]";
    case T_COMMA: return ","; case T_DOT: return ".";
    }
    return "?";
}

static void skip_ws(Lexer *l) {
    while (*l->pos) {
        char c = *l->pos;
        if (c == ' ' || c == '\t' || c == '\r') { l->pos++; l->col++; }
        else if (c == '\n') { l->pos++; l->line++; l->col = 1; }
        else if (c == '/' && *(l->pos+1) == '/') {
            l->pos += 2; l->col += 2;
            while (*l->pos && *l->pos != '\n') { l->pos++; l->col++; }
        } else break;
    }
}

static Token make_tok(Lexer *l, TokenType t) {
    Token tk = { t, "", 0, { l->filename, l->line, l->col } };
    return tk;
}

static Token make_tok_str(Lexer *l, TokenType t, const char *s) {
    Token tk = make_tok(l, t);
    strncpy(tk.text, s, 255);
    return tk;
}

Token lexer_next(Lexer *l) {
    skip_ws(l);
    if (!*l->pos) return make_tok(l, T_EOF);

    char c = *l->pos;

    /* char literal */
    if (c == '\'') {
        l->pos++; l->col++;
        int v = 0;
        if (*l->pos == '\\') {
            l->pos++; l->col++;
            switch(*l->pos) {
            case 'n': v=10; break; case 't': v=9; break;
            case 'r': v=13; break; case '0': v=0; break;
            case '\\': v=92; break; case '\'': v=39; break;
            default: v=*l->pos;
            }
            l->pos++; l->col++;
        } else { v = *l->pos; l->pos++; l->col++; }
        if (*l->pos == '\'') { l->pos++; l->col++; }
        Token tk = make_tok(l, T_CHAR);
        tk.ival = v; return tk;
    }

    /* number: dec, hex, oct, bin */
    if (isdigit(c) || (c=='0' && (l->pos[1]=='x'||l->pos[1]=='X'||
        l->pos[1]=='b'||l->pos[1]=='B'))) {
        Token tk = make_tok(l, T_NUMBER);
        if (c=='0' && (l->pos[1]=='x'||l->pos[1]=='X')) {
            l->pos+=2; l->col+=2; char b[64]; int i=0;
            while (*l->pos && isxdigit(*l->pos)&&i<63) {b[i++]=*l->pos++;l->col++;}
            b[i]=0; tk.ival=strtol(b,NULL,16);
        } else if (c=='0' && (l->pos[1]=='b'||l->pos[1]=='B')) {
            l->pos+=2; l->col+=2; long v=0;
            while (*l->pos=='0'||*l->pos=='1'){v=v*2+(*l->pos-'0');l->pos++;l->col++;}
            tk.ival=v;
        } else if (c=='0' && l->pos[1]>='0' && l->pos[1]<='7') {
            l->pos++; l->col++; long v=0;
            while (*l->pos>='0'&&*l->pos<='7'){v=v*8+(*l->pos-'0');l->pos++;l->col++;}
            tk.ival=v;
        } else { long v=0; while(*l->pos&&isdigit(*l->pos)){v=v*10+(*l->pos-'0');l->pos++;l->col++;} tk.ival=v; }
        return tk;
    }

    /* identifier / keyword */
    if (isalpha(c) || c == '_') {
        int i = 0;
        while (*l->pos && (isalnum(*l->pos)||*l->pos=='_') && i<255) { l->text[i++] = *l->pos++; l->col++; }
        l->text[i]=0; Token tk = make_tok(l, T_IDENT);
        strncpy(tk.text,l->text,255);
        if (strcmp(l->text,"int")==0) return make_tok(l, T_INT);
        if (strcmp(l->text,"sizeof")==0) return make_tok(l, T_SIZEOF);
        return tk;
    }

    /* 3-char ops */
    if (l->pos[0]=='<' && l->pos[1]=='<' && l->pos[2]=='=') { l->pos+=3; l->col+=3; return make_tok_str(l,T_LSHIFT_ASSIGN,"<<="); }
    if (l->pos[0]=='>' && l->pos[1]=='>' && l->pos[2]=='=') { l->pos+=3; l->col+=3; return make_tok_str(l,T_RSHIFT_ASSIGN,">>="); }

    /* 2-char ops */
    if (c=='+'&&l->pos[1]=='+'){l->pos+=2;l->col+=2;return make_tok_str(l,T_INC,"++");}
    if (c=='-'&&l->pos[1]=='-'){l->pos+=2;l->col+=2;return make_tok_str(l,T_DEC,"--");}
    if (c=='+'&&l->pos[1]=='='){l->pos+=2;l->col+=2;return make_tok_str(l,T_ADD_ASSIGN,"+=");}
    if (c=='-'&&l->pos[1]=='='){l->pos+=2;l->col+=2;return make_tok_str(l,T_SUB_ASSIGN,"-=");}
    if (c=='*'&&l->pos[1]=='='){l->pos+=2;l->col+=2;return make_tok_str(l,T_MUL_ASSIGN,"*=");}
    if (c=='/'&&l->pos[1]=='='){l->pos+=2;l->col+=2;return make_tok_str(l,T_DIV_ASSIGN,"/=");}
    if (c=='%'&&l->pos[1]=='='){l->pos+=2;l->col+=2;return make_tok_str(l,T_MOD_ASSIGN,"%=");}
    if (c=='&'&&l->pos[1]=='='){l->pos+=2;l->col+=2;return make_tok_str(l,T_AND_ASSIGN,"&=");}
    if (c=='|'&&l->pos[1]=='='){l->pos+=2;l->col+=2;return make_tok_str(l,T_OR_ASSIGN,"|=");}
    if (c=='^'&&l->pos[1]=='='){l->pos+=2;l->col+=2;return make_tok_str(l,T_XOR_ASSIGN,"^=");}
    if (c=='<'&&l->pos[1]=='<'){l->pos+=2;l->col+=2;return make_tok_str(l,T_LSHIFT,"<<");}
    if (c=='>'&&l->pos[1]=='>'){l->pos+=2;l->col+=2;return make_tok_str(l,T_RSHIFT,">>");}
    if (c=='='&&l->pos[1]=='='){l->pos+=2;l->col+=2;return make_tok_str(l,T_EQ,"==");}
    if (c=='!'&&l->pos[1]=='='){l->pos+=2;l->col+=2;return make_tok_str(l,T_NE,"!=");}
    if (c=='<'&&l->pos[1]=='='){l->pos+=2;l->col+=2;return make_tok_str(l,T_LE,"<=");}
    if (c=='>'&&l->pos[1]=='='){l->pos+=2;l->col+=2;return make_tok_str(l,T_GE,">=");}
    if (c=='&'&&l->pos[1]=='&'){l->pos+=2;l->col+=2;return make_tok_str(l,T_AND,"&&");}
    if (c=='|'&&l->pos[1]=='|'){l->pos+=2;l->col+=2;return make_tok_str(l,T_OR,"||");}
    if (c=='-'&&l->pos[1]=='>'){l->pos+=2;l->col+=2;return make_tok_str(l,T_DOT,"->");}

    /* 1-char ops */
    l->pos++; l->col++;
    switch (c) {
    case ';': return make_tok_str(l,T_SEMICOLON,";");
    case '=': return make_tok_str(l,T_ASSIGN,"=");
    case '+': return make_tok_str(l,T_PLUS,"+");
    case '-': return make_tok_str(l,T_MINUS,"-");
    case '*': return make_tok_str(l,T_STAR,"*");
    case '/': return make_tok_str(l,T_SLASH,"/");
    case '%': return make_tok_str(l,T_PERCENT,"%");
    case '&': return make_tok_str(l,T_AMPERSAND,"&");
    case '|': return make_tok_str(l,T_PIPE,"|");
    case '^': return make_tok_str(l,T_CARET,"^");
    case '~': return make_tok_str(l,T_TILDE,"~");
    case '<': return make_tok_str(l,T_LT,"<");
    case '>': return make_tok_str(l,T_GT,">");
    case '!': return make_tok_str(l,T_NOT,"!");
    case '(': return make_tok_str(l,T_LPAREN,"(");
    case ')': return make_tok_str(l,T_RPAREN,")");
    case '{': return make_tok_str(l,T_LBRACE,"{");
    case '}': return make_tok_str(l,T_RBRACE,"}");
    case '[': return make_tok_str(l,T_LBRACKET,"[");
    case ']': return make_tok_str(l,T_RBRACKET,"]");
    case '?': return make_tok_str(l,T_QUESTION,"?");
    case ':': return make_tok_str(l,T_COLON,":");
    case ',': return make_tok_str(l,T_COMMA,",");
    }
    return make_tok_str(l, T_ERROR, (char[]){c,0});
}

void lexer_init(Lexer *l, const char *src, const char *fname, DebugFlags d) {
    l->pos=l->start=src; l->filename=fname?fname:"<stdin>";
    l->line=1; l->col=1; l->debug=d; l->cur=lexer_next(l);
}
Token lexer_peek(Lexer *l) { return l->cur; }
Token lexer_advance(Lexer *l) { l->prev=l->cur; l->cur=lexer_next(l); return l->prev; }
int lexer_match(Lexer *l, TokenType t) {
    if (l->cur.type==t) { lexer_advance(l); return 1; }
    return 0;
}

void lexer_dump(Lexer *l) {
    printf("── TOKENS ──\n"); int n=0;
    while (l->cur.type!=T_EOF) {
        Token t=lexer_advance(l);
        printf("  %s",token_name(t.type));
        if(t.type==T_NUMBER||t.type==T_CHAR) printf("(%d)",t.ival);
        else if(t.type==T_IDENT) printf("(%s)",t.text);
        printf("  [%d:%d]\n",t.loc.line,t.loc.col); n++;
    }
    printf("  EOF\n── %d tokens ──\n",n);
}
