/* ================================================================
   parser.mly — Menhir parser for GCL-SH (Python-like syntax)
   COLON-free: blocks start with { directly, no : required.
   ================================================================ */

%{
open Ast
let mk_pos (p : Lexing.position) : pos = {
  file = p.pos_fname;
  line = p.pos_lnum;
  col = p.pos_cnum - p.pos_bol + 1
}
let mk_expr n p = { e_node = n; e_typ = None; e_pos = mk_pos p }
let mk_stmt n p = { s_node = n; s_pos = mk_pos p }
let mk_stmt_from_pos n p = { s_node = n; s_pos = p }
let mk_decl n p = { d_node = n; d_typ = None; d_pos = mk_pos p }
%}

/* ---- Token declarations ---- */
%token EOF
%token LPAREN RPAREN LBRACE RBRACE LBRACK RBRACK
%token SEMI COMMA DOT COLON ARROW DARROW
%token PLUS MINUS STAR SLASH PERCENT
%token POW FLOORDIV
%token EQEQ NEQ LT GT LE GE
%token AND OR NOT
%token IS IN
%token BITAND BITOR BITXOR BITNOT LSHIFT RSHIFT
%token ASSIGN ADD_ASSIGN SUB_ASSIGN MUL_ASSIGN DIV_ASSIGN MOD_ASSIGN
%token POW_ASSIGN FLOORDIV_ASSIGN
%token BITAND_ASSIGN BITOR_ASSIGN BITXOR_ASSIGN LSHIFT_ASSIGN RSHIFT_ASSIGN
%token INC DEC WALRUS
%token IF ELIF ELSE WHILE FOR RETURN BREAK CONTINUE
%token DEF CLASS STRUCT ENUM PASS
%token TRY EXCEPT FINALLY RAISE WITH
%token YIELD LAMBDA
%token MATCH CASE
%token GLOBAL NONLOCAL DEL ASSERT
%token IMPORT FROM AS
%token SELF SUPER
%token TRUE FALSE NULL NONE
%token <int> INT
%token <float> FLOAT
%token <string> STRING
%token <string> IDENT

/* ---- Precedence (lowest to highest) ---- */
%nonassoc WALRUS
%right ASSIGN ADD_ASSIGN SUB_ASSIGN MUL_ASSIGN DIV_ASSIGN MOD_ASSIGN
       POW_ASSIGN FLOORDIV_ASSIGN BITAND_ASSIGN BITOR_ASSIGN BITXOR_ASSIGN
       LSHIFT_ASSIGN RSHIFT_ASSIGN
%left OR
%left AND
%nonassoc NOT
%left IS IN
%nonassoc EQEQ NEQ
%nonassoc LT GT LE GE
%left BITOR
%left BITXOR
%left BITAND
%left LSHIFT RSHIFT
%left PLUS MINUS
%left STAR SLASH PERCENT FLOORDIV
%right POW
%nonassoc NEG BITNOT
%nonassoc INC DEC
%left DOT LPAREN LBRACK
%nonassoc ARROW

/* ---- Start symbol ---- */
%start program
%type <Ast.program> program

%%

program:
  | ds = top_decl* EOF
      { { decls = ds; file = "" } }
  ;

top_decl:
  | d = func_decl      { d }
  | d = class_decl     { d }
  | d = struct_decl    { d }
  | d = enum_decl      { d }
  | d = import_decl    { d }
  ;

func_decl:
  | DEF n = IDENT LPAREN ps = separated_list(COMMA, param) RPAREN b = block
      { mk_decl (DFuncDef (n, ps, None, b)) ($startpos) }
  | DEF n = IDENT LPAREN ps = separated_list(COMMA, param) RPAREN SEMI
      { mk_decl (DExternFunc (n, ps)) ($startpos) }
  ;

param:
  | n = IDENT d = option(preceded(ASSIGN, expr))
      { { p_name = n; p_typ = None; p_default = d; p_pos = mk_pos ($startpos) } }
  ;

class_decl:
  | CLASS n = IDENT LBRACE ms = list(member_decl) RBRACE SEMI
      { let stmts = List.map (fun d -> mk_stmt_from_pos (SDecl d) d.d_pos) ms in
        mk_decl (DClassDef (n, [], mk_stmt (SBlock stmts) ($startpos))) ($startpos) }
  ;

import_decl:
  | IMPORT m = IDENT a = option(preceded(AS, IDENT)) SEMI
      { mk_decl (DImport (m, a)) ($startpos) }
  | FROM m = IDENT IMPORT ns = separated_nonempty_list(COMMA, import_item) SEMI
      { mk_decl (DFromImport (m, ns)) ($startpos) }
  ;

import_item:
  | n = IDENT a = option(preceded(AS, IDENT)) { (n, a) }
  ;

struct_decl:
  | STRUCT n = IDENT LBRACE ms = list(member_decl) RBRACE SEMI
      { mk_decl (DStructDecl (n, Some ms)) ($startpos) }
  | STRUCT n = IDENT SEMI
      { mk_decl (DStructDecl (n, None)) ($startpos) }
  ;

member_decl:
  | n = IDENT SEMI { mk_decl (DVarDecl (n, None)) ($startpos) }
  ;

enum_decl:
  | ENUM n = IDENT LBRACE es = separated_list(COMMA, enum_item) RBRACE SEMI
      { mk_decl (DEnumDecl (n, es)) ($startpos) }
  | ENUM n = IDENT SEMI
      { mk_decl (DEnumDecl (n, [])) ($startpos) }
  ;

enum_item:
  | n = IDENT v = option(preceded(ASSIGN, expr)) { (n, v) }
  ;

block:
  | LBRACE stmts = list(stmt) RBRACE
      { mk_stmt (SBlock stmts) ($startpos) }
  ;

stmt:
  | s = block           { s }
  | IF LPAREN c = expr RPAREN t = stmt e = elif_else
      { mk_stmt (SIf (c, t, e)) ($startpos) }
  | WHILE LPAREN c = expr RPAREN b = stmt
      { mk_stmt (SWhile (c, b)) ($startpos) }
  | FOR LPAREN i = stmt c = expr SEMI s = expr RPAREN b = stmt
      { mk_stmt (SFor (i, c, s, b)) ($startpos) }
  | FOR LPAREN v = expr IN it = expr RPAREN b = stmt
      { mk_stmt (SForIn (v, it, b)) ($startpos) }
  | RETURN e = option(expr) SEMI
      { mk_stmt (SReturn e) ($startpos) }
  | BREAK SEMI    { mk_stmt SBreak ($startpos) }
  | CONTINUE SEMI { mk_stmt SContinue ($startpos) }
  | TRY b = block ecs = list(except_clause) f = option(finally_clause)
      { mk_stmt (STry (b, ecs, f)) ($startpos) }
  | RAISE e = option(expr) SEMI
      { mk_stmt (SRaise e) ($startpos) }
  | WITH es = separated_nonempty_list(COMMA, with_item) b = stmt
      { mk_stmt (SWith (List.map fst es, b)) ($startpos) }
  | GLOBAL ns = separated_nonempty_list(COMMA, IDENT) SEMI
      { mk_stmt (SGlobal ns) ($startpos) }
  | NONLOCAL ns = separated_nonempty_list(COMMA, IDENT) SEMI
      { mk_stmt (SNonlocal ns) ($startpos) }
  | DEL e = expr SEMI
      { mk_stmt (SDelete e) ($startpos) }
  | ASSERT c = expr SEMI
      { mk_stmt (SAssert (c, None)) ($startpos) }
  | ASSERT c = expr COMMA m = expr SEMI
      { mk_stmt (SAssert (c, Some m)) ($startpos) }
  | YIELD SEMI            { mk_stmt (SYield None) ($startpos) }
  | YIELD e = expr SEMI   { mk_stmt (SYield (Some e)) ($startpos) }
  | d = decl_stmt         { mk_stmt (SDecl d) ($startpos) }
  | e = expr SEMI         { mk_stmt (SExpr e) ($startpos) }
  ;

decl_stmt:
  | d = func_decl   { d }
  | d = struct_decl { d }
  | d = enum_decl   { d }
  | d = import_decl { d }
  | PASS SEMI       { mk_decl (DVarDecl ("_", None)) ($startpos) }
  ;

except_clause:
  | EXCEPT e = option(expr) a = option(preceded(AS, IDENT)) b = stmt
      { (e, a, b) }
  ;

finally_clause:
  | FINALLY b = stmt { b }
  ;

with_item:
  | e = expr a = option(preceded(AS, IDENT)) { (e, a) }
  ;

elif_else:
  | ELIF LPAREN c = expr RPAREN t = stmt e = elif_else
      { Some (mk_stmt (SIf (c, t, e)) ($startpos)) }
  | ELSE b = stmt
      { Some b }
  | (* empty *)
      { None }
  ;

expr:
  | e = if_expr     { e }
  | e = lambda_expr { e }
  ;

lambda_expr:
  | LAMBDA ps = separated_list(COMMA, param) DARROW e = expr
      { mk_expr (ELambda (ps, e)) ($startpos) }
  ;

if_expr:
  | e = walrus_expr IF c = expr ELSE a = if_expr
      { mk_expr (ETernary (c, e, a)) ($startpos) }
  | e = walrus_expr { e }
  ;

walrus_expr:
  | n = IDENT WALRUS e = walrus_expr
      { mk_expr (EWalrus (n, e)) ($startpos) }
  | e = assign_expr { e }
  ;

assign_expr:
  | e = or_expr
      { e }
  | e = or_expr ASSIGN a = assign_expr
      { mk_expr (EBinary (Assign, e, a)) ($startpos) }
  | e = or_expr ADD_ASSIGN a = assign_expr
      { mk_expr (EBinary (AddAssign, e, a)) ($startpos) }
  | e = or_expr SUB_ASSIGN a = assign_expr
      { mk_expr (EBinary (SubAssign, e, a)) ($startpos) }
  | e = or_expr MUL_ASSIGN a = assign_expr
      { mk_expr (EBinary (MulAssign, e, a)) ($startpos) }
  | e = or_expr DIV_ASSIGN a = assign_expr
      { mk_expr (EBinary (DivAssign, e, a)) ($startpos) }
  | e = or_expr MOD_ASSIGN a = assign_expr
      { mk_expr (EBinary (ModAssign, e, a)) ($startpos) }
  | e = or_expr POW_ASSIGN a = assign_expr
      { mk_expr (EBinary (PowAssign, e, a)) ($startpos) }
  | e = or_expr FLOORDIV_ASSIGN a = assign_expr
      { mk_expr (EBinary (FloorDivAssign, e, a)) ($startpos) }
  ;

or_expr:
  | e = and_expr
      { e }
  | e = and_expr OR a = or_expr
      { mk_expr (EBinary (Or, e, a)) ($startpos) }
  ;

and_expr:
  | e = not_expr
      { e }
  | e = not_expr AND a = and_expr
      { mk_expr (EBinary (And, e, a)) ($startpos) }
  ;

not_expr:
  | e = is_expr       { e }
  | NOT e = not_expr  { mk_expr (EUnary (Not, e)) ($startpos) }
  ;

is_expr:
  | e = in_expr       { e }
  | e = in_expr IS a = is_expr     { mk_expr (EBinary (Is, e, a)) ($startpos) }
  | e = in_expr IS NOT a = is_expr { mk_expr (EBinary (IsNot, e, a)) ($startpos) }
  ;

in_expr:
  | e = eq_expr         { e }
  | e = eq_expr IN a = in_expr      { mk_expr (EBinary (In, e, a)) ($startpos) }
  | e = eq_expr NOT IN a = in_expr  { mk_expr (EBinary (NotIn, e, a)) ($startpos) }
  ;

eq_expr:
  | e = bit_or_expr
      { e }
  | e = bit_or_expr EQEQ a = eq_expr
      { mk_expr (EBinary (Eq, e, a)) ($startpos) }
  | e = bit_or_expr NEQ a = eq_expr
      { mk_expr (EBinary (Ne, e, a)) ($startpos) }
  ;

bit_or_expr:
  | e = bit_xor_expr      { e }
  | e = bit_xor_expr BITOR a = bit_or_expr
      { mk_expr (EBinary (BitOr, e, a)) ($startpos) }
  ;

bit_xor_expr:
  | e = bit_and_expr      { e }
  | e = bit_and_expr BITXOR a = bit_xor_expr
      { mk_expr (EBinary (BitXor, e, a)) ($startpos) }
  ;

bit_and_expr:
  | e = shift_expr      { e }
  | e = shift_expr BITAND a = bit_and_expr
      { mk_expr (EBinary (BitAnd, e, a)) ($startpos) }
  ;

shift_expr:
  | e = cmp_expr        { e }
  | e = cmp_expr LSHIFT a = shift_expr
      { mk_expr (EBinary (LShift, e, a)) ($startpos) }
  | e = cmp_expr RSHIFT a = shift_expr
      { mk_expr (EBinary (RShift, e, a)) ($startpos) }
  ;

cmp_expr:
  | e = add_expr
      { e }
  | e = add_expr LT a = cmp_expr
      { mk_expr (EBinary (Lt, e, a)) ($startpos) }
  | e = add_expr GT a = cmp_expr
      { mk_expr (EBinary (Gt, e, a)) ($startpos) }
  | e = add_expr LE a = cmp_expr
      { mk_expr (EBinary (Le, e, a)) ($startpos) }
  | e = add_expr GE a = cmp_expr
      { mk_expr (EBinary (Ge, e, a)) ($startpos) }
  ;

add_expr:
  | e = mul_expr
      { e }
  | e = mul_expr PLUS a = add_expr
      { mk_expr (EBinary (Add, e, a)) ($startpos) }
  | e = mul_expr MINUS a = add_expr
      { mk_expr (EBinary (Sub, e, a)) ($startpos) }
  ;

mul_expr:
  | e = pow_expr
      { e }
  | e = pow_expr STAR a = mul_expr
      { mk_expr (EBinary (Mul, e, a)) ($startpos) }
  | e = pow_expr SLASH a = mul_expr
      { mk_expr (EBinary (Div, e, a)) ($startpos) }
  | e = pow_expr PERCENT a = mul_expr
      { mk_expr (EBinary (Mod, e, a)) ($startpos) }
  | e = pow_expr FLOORDIV a = mul_expr
      { mk_expr (EBinary (FloorDiv, e, a)) ($startpos) }
  ;

pow_expr:
  | e = unary_expr        { e }
  | e = unary_expr POW a = pow_expr
      { mk_expr (EBinary (Pow, e, a)) ($startpos) }
  ;

unary_expr:
  | e = postfix_expr          { e }
  | MINUS e = unary_expr %prec NEG
      { mk_expr (EUnary (Neg, e)) ($startpos) }
  | BITNOT e = unary_expr %prec BITNOT
      { mk_expr (EUnary (BitNot, e)) ($startpos) }
  | PLUS e = unary_expr %prec NEG
      { mk_expr (EUnary (Plus, e)) ($startpos) }
  | INC e = unary_expr %prec INC
      { mk_expr (EUnary (PreInc, e)) ($startpos) }
  | DEC e = unary_expr %prec DEC
      { mk_expr (EUnary (PreDec, e)) ($startpos) }
  ;

postfix_expr:
  | e = primary_expr
      { e }
  | e = postfix_expr INC
      { mk_expr (EUnary (PostInc, e)) ($startpos) }
  | e = postfix_expr DEC
      { mk_expr (EUnary (PostDec, e)) ($startpos) }
  | e = postfix_expr LBRACK i = expr RBRACK
      { mk_expr (EIndex (e, i)) ($startpos) }
  | e = postfix_expr LBRACK i1 = expr_option COLON i2 = expr_option RBRACK
      { mk_expr (ESlice (i1, i2, None)) ($startpos) }
  | e = postfix_expr LBRACK i1 = expr_option COLON i2 = expr_option COLON i3 = expr_option RBRACK
      { mk_expr (ESlice (i1, i2, i3)) ($startpos) }
  | e = postfix_expr LPAREN args = separated_list(COMMA, expr) RPAREN
      { mk_expr (ECall (e, args)) ($startpos) }
  | e = postfix_expr DOT n = IDENT
      { mk_expr (EMember (e, n)) ($startpos) }
  ;

primary_expr:
  | n = INT
      { mk_expr (ELiteral (LInt n)) ($startpos) }
  | f = FLOAT
      { mk_expr (ELiteral (LFloat f)) ($startpos) }
  | s = STRING
      { mk_expr (ELiteral (LString s)) ($startpos) }
  | TRUE
      { mk_expr (ELiteral (LBool true)) ($startpos) }
  | FALSE
      { mk_expr (ELiteral (LBool false)) ($startpos) }
  | NULL  { mk_expr (ELiteral LNull) ($startpos) }
  | NONE  { mk_expr (ELiteral LNull) ($startpos) }
  | n = IDENT
      { mk_expr (EIdent n) ($startpos) }
  | SELF
      { mk_expr (EIdent "self") ($startpos) }
  | SUPER LPAREN args = separated_list(COMMA, expr) RPAREN
      { mk_expr (ECall (mk_expr (EIdent "super") ($startpos), args)) ($startpos) }
  | LPAREN e = expr RPAREN
      { e }
  | LBRACK items = separated_list(COMMA, expr) RBRACK
      { mk_expr (EList items) ($startpos) }
  | LBRACE items = separated_list(COMMA, dict_item) RBRACE
      { mk_expr (EDict items) ($startpos) }
  | LBRACE items = separated_nonempty_list(COMMA, expr) RBRACE
      { mk_expr (ESet items) ($startpos) }
  ;

dict_item:
  | k = expr COLON v = expr { (k, v) }
  ;

expr_option:
  | e = expr { Some e }
  |          { None }
  ;
