
(* The type of tokens. *)

type token = 
  | YIELD
  | WITH
  | WHILE
  | WALRUS
  | TRY
  | TRUE
  | SUPER
  | SUB_ASSIGN
  | STRUCT
  | STRING of (string)
  | STAR
  | SLASH
  | SEMI
  | SELF
  | RSHIFT_ASSIGN
  | RSHIFT
  | RPAREN
  | RETURN
  | RBRACK
  | RBRACE
  | RAISE
  | POW_ASSIGN
  | POW
  | PLUS
  | PERCENT
  | PASS
  | OR
  | NULL
  | NOT
  | NONLOCAL
  | NONE
  | NEQ
  | MUL_ASSIGN
  | MOD_ASSIGN
  | MINUS
  | MATCH
  | LT
  | LSHIFT_ASSIGN
  | LSHIFT
  | LPAREN
  | LE
  | LBRACK
  | LBRACE
  | LAMBDA
  | IS
  | INT of (int)
  | INC
  | IN
  | IMPORT
  | IF
  | IDENT of (string)
  | GT
  | GLOBAL
  | GE
  | FROM
  | FOR
  | FLOORDIV_ASSIGN
  | FLOORDIV
  | FLOAT of (float)
  | FINALLY
  | FALSE
  | EXCEPT
  | EQEQ
  | EOF
  | ENUM
  | ELSE
  | ELIF
  | DOT
  | DIV_ASSIGN
  | DEL
  | DEF
  | DEC
  | DARROW
  | CONTINUE
  | COMMA
  | COLON
  | CLASS
  | CASE
  | BREAK
  | BITXOR_ASSIGN
  | BITXOR
  | BITOR_ASSIGN
  | BITOR
  | BITNOT
  | BITAND_ASSIGN
  | BITAND
  | ASSIGN
  | ASSERT
  | AS
  | ARROW
  | AND
  | ADD_ASSIGN

(* This exception is raised by the monolithic API functions. *)

exception Error

(* The monolithic API. *)

val program: (Lexing.lexbuf -> token) -> Lexing.lexbuf -> (Ast.program)
