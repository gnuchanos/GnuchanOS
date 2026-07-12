
module MenhirBasics = struct
  
  exception Error
  
  let _eRR =
    fun _s ->
      raise Error
  
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
    | STRING of 
# 44 "src/parser.mly"
       (string)
# 24 "src/parser.ml"
  
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
    | INT of 
# 42 "src/parser.mly"
       (int)
# 64 "src/parser.ml"
  
    | INC
    | IN
    | IMPORT
    | IF
    | IDENT of 
# 45 "src/parser.mly"
       (string)
# 73 "src/parser.ml"
  
    | GT
    | GLOBAL
    | GE
    | FROM
    | FOR
    | FLOORDIV_ASSIGN
    | FLOORDIV
    | FLOAT of 
# 43 "src/parser.mly"
       (float)
# 85 "src/parser.ml"
  
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
  
end

include MenhirBasics

# 6 "src/parser.mly"
  
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

# 138 "src/parser.ml"

type ('s, 'r) _menhir_state = 
  | MenhirState000 : ('s, _menhir_box_program) _menhir_state
    (** State 000.
        Stack shape : <empty>.
        Start symbol: program. *)

  | MenhirState004 : (('s, _menhir_box_program) _menhir_cell1_STRUCT _menhir_cell0_IDENT _menhir_cell0_LBRACE, _menhir_box_program) _menhir_state
    (** State 004.
        Stack shape : STRUCT IDENT LBRACE.
        Start symbol: program. *)

  | MenhirState007 : (('s, _menhir_box_program) _menhir_cell1_member_decl, _menhir_box_program) _menhir_state
    (** State 007.
        Stack shape : member_decl.
        Start symbol: program. *)

  | MenhirState013 : (('s, _menhir_box_program) _menhir_cell1_IMPORT _menhir_cell0_IDENT, _menhir_box_program) _menhir_state
    (** State 013.
        Stack shape : IMPORT IDENT.
        Start symbol: program. *)

  | MenhirState020 : (('s, _menhir_box_program) _menhir_cell1_FROM _menhir_cell0_IDENT _menhir_cell0_IMPORT, _menhir_box_program) _menhir_state
    (** State 020.
        Stack shape : FROM IDENT IMPORT.
        Start symbol: program. *)

  | MenhirState021 : (('s, _menhir_box_program) _menhir_cell1_IDENT, _menhir_box_program) _menhir_state
    (** State 021.
        Stack shape : IDENT.
        Start symbol: program. *)

  | MenhirState026 : (('s, _menhir_box_program) _menhir_cell1_import_item, _menhir_box_program) _menhir_state
    (** State 026.
        Stack shape : import_item.
        Start symbol: program. *)

  | MenhirState031 : (('s, _menhir_box_program) _menhir_cell1_ENUM _menhir_cell0_IDENT _menhir_cell0_LBRACE, _menhir_box_program) _menhir_state
    (** State 031.
        Stack shape : ENUM IDENT LBRACE.
        Start symbol: program. *)

  | MenhirState032 : (('s, _menhir_box_program) _menhir_cell1_IDENT, _menhir_box_program) _menhir_state
    (** State 032.
        Stack shape : IDENT.
        Start symbol: program. *)

  | MenhirState033 : ((('s, _menhir_box_program) _menhir_cell1_IDENT, _menhir_box_program) _menhir_cell1_ASSIGN, _menhir_box_program) _menhir_state
    (** State 033.
        Stack shape : IDENT ASSIGN.
        Start symbol: program. *)

  | MenhirState036 : (('s, _menhir_box_program) _menhir_cell1_SUPER _menhir_cell0_LPAREN, _menhir_box_program) _menhir_state
    (** State 036.
        Stack shape : SUPER LPAREN.
        Start symbol: program. *)

  | MenhirState039 : (('s, _menhir_box_program) _menhir_cell1_PLUS, _menhir_box_program) _menhir_state
    (** State 039.
        Stack shape : PLUS.
        Start symbol: program. *)

  | MenhirState042 : (('s, _menhir_box_program) _menhir_cell1_MINUS, _menhir_box_program) _menhir_state
    (** State 042.
        Stack shape : MINUS.
        Start symbol: program. *)

  | MenhirState043 : (('s, _menhir_box_program) _menhir_cell1_LPAREN, _menhir_box_program) _menhir_state
    (** State 043.
        Stack shape : LPAREN.
        Start symbol: program. *)

  | MenhirState044 : (('s, _menhir_box_program) _menhir_cell1_NOT, _menhir_box_program) _menhir_state
    (** State 044.
        Stack shape : NOT.
        Start symbol: program. *)

  | MenhirState045 : (('s, _menhir_box_program) _menhir_cell1_LBRACK, _menhir_box_program) _menhir_state
    (** State 045.
        Stack shape : LBRACK.
        Start symbol: program. *)

  | MenhirState046 : (('s, _menhir_box_program) _menhir_cell1_LBRACE, _menhir_box_program) _menhir_state
    (** State 046.
        Stack shape : LBRACE.
        Start symbol: program. *)

  | MenhirState047 : (('s, _menhir_box_program) _menhir_cell1_LAMBDA, _menhir_box_program) _menhir_state
    (** State 047.
        Stack shape : LAMBDA.
        Start symbol: program. *)

  | MenhirState048 : (('s, _menhir_box_program) _menhir_cell1_IDENT, _menhir_box_program) _menhir_state
    (** State 048.
        Stack shape : IDENT.
        Start symbol: program. *)

  | MenhirState052 : (('s, _menhir_box_program) _menhir_cell1_param, _menhir_box_program) _menhir_state
    (** State 052.
        Stack shape : param.
        Start symbol: program. *)

  | MenhirState055 : ((('s, _menhir_box_program) _menhir_cell1_LAMBDA, _menhir_box_program) _menhir_cell1_loption_separated_nonempty_list_COMMA_param__, _menhir_box_program) _menhir_state
    (** State 055.
        Stack shape : LAMBDA loption(separated_nonempty_list(COMMA,param)).
        Start symbol: program. *)

  | MenhirState057 : (('s, _menhir_box_program) _menhir_cell1_INC, _menhir_box_program) _menhir_state
    (** State 057.
        Stack shape : INC.
        Start symbol: program. *)

  | MenhirState061 : (('s, _menhir_box_program) _menhir_cell1_DEC, _menhir_box_program) _menhir_state
    (** State 061.
        Stack shape : DEC.
        Start symbol: program. *)

  | MenhirState062 : (('s, _menhir_box_program) _menhir_cell1_BITNOT, _menhir_box_program) _menhir_state
    (** State 062.
        Stack shape : BITNOT.
        Start symbol: program. *)

  | MenhirState066 : (('s, _menhir_box_program) _menhir_cell1_postfix_expr _menhir_cell0_LPAREN, _menhir_box_program) _menhir_state
    (** State 066.
        Stack shape : postfix_expr LPAREN.
        Start symbol: program. *)

  | MenhirState068 : (('s, _menhir_box_program) _menhir_cell1_IDENT, _menhir_box_program) _menhir_state
    (** State 068.
        Stack shape : IDENT.
        Start symbol: program. *)

  | MenhirState071 : (('s, _menhir_box_program) _menhir_cell1_unary_expr, _menhir_box_program) _menhir_state
    (** State 071.
        Stack shape : unary_expr.
        Start symbol: program. *)

  | MenhirState074 : (('s, _menhir_box_program) _menhir_cell1_shift_expr, _menhir_box_program) _menhir_state
    (** State 074.
        Stack shape : shift_expr.
        Start symbol: program. *)

  | MenhirState076 : (('s, _menhir_box_program) _menhir_cell1_pow_expr, _menhir_box_program) _menhir_state
    (** State 076.
        Stack shape : pow_expr.
        Start symbol: program. *)

  | MenhirState078 : (('s, _menhir_box_program) _menhir_cell1_pow_expr, _menhir_box_program) _menhir_state
    (** State 078.
        Stack shape : pow_expr.
        Start symbol: program. *)

  | MenhirState080 : (('s, _menhir_box_program) _menhir_cell1_pow_expr, _menhir_box_program) _menhir_state
    (** State 080.
        Stack shape : pow_expr.
        Start symbol: program. *)

  | MenhirState082 : (('s, _menhir_box_program) _menhir_cell1_pow_expr, _menhir_box_program) _menhir_state
    (** State 082.
        Stack shape : pow_expr.
        Start symbol: program. *)

  | MenhirState085 : (('s, _menhir_box_program) _menhir_cell1_mul_expr _menhir_cell0_PLUS, _menhir_box_program) _menhir_state
    (** State 085.
        Stack shape : mul_expr PLUS.
        Start symbol: program. *)

  | MenhirState087 : (('s, _menhir_box_program) _menhir_cell1_mul_expr _menhir_cell0_MINUS, _menhir_box_program) _menhir_state
    (** State 087.
        Stack shape : mul_expr MINUS.
        Start symbol: program. *)

  | MenhirState090 : (('s, _menhir_box_program) _menhir_cell1_cmp_expr, _menhir_box_program) _menhir_state
    (** State 090.
        Stack shape : cmp_expr.
        Start symbol: program. *)

  | MenhirState093 : (('s, _menhir_box_program) _menhir_cell1_add_expr, _menhir_box_program) _menhir_state
    (** State 093.
        Stack shape : add_expr.
        Start symbol: program. *)

  | MenhirState095 : (('s, _menhir_box_program) _menhir_cell1_add_expr, _menhir_box_program) _menhir_state
    (** State 095.
        Stack shape : add_expr.
        Start symbol: program. *)

  | MenhirState097 : (('s, _menhir_box_program) _menhir_cell1_add_expr, _menhir_box_program) _menhir_state
    (** State 097.
        Stack shape : add_expr.
        Start symbol: program. *)

  | MenhirState099 : (('s, _menhir_box_program) _menhir_cell1_add_expr, _menhir_box_program) _menhir_state
    (** State 099.
        Stack shape : add_expr.
        Start symbol: program. *)

  | MenhirState101 : (('s, _menhir_box_program) _menhir_cell1_cmp_expr, _menhir_box_program) _menhir_state
    (** State 101.
        Stack shape : cmp_expr.
        Start symbol: program. *)

  | MenhirState105 : (('s, _menhir_box_program) _menhir_cell1_or_expr, _menhir_box_program) _menhir_state
    (** State 105.
        Stack shape : or_expr.
        Start symbol: program. *)

  | MenhirState107 : (('s, _menhir_box_program) _menhir_cell1_not_expr, _menhir_box_program) _menhir_state
    (** State 107.
        Stack shape : not_expr.
        Start symbol: program. *)

  | MenhirState110 : (('s, _menhir_box_program) _menhir_cell1_in_expr, _menhir_box_program) _menhir_state
    (** State 110.
        Stack shape : in_expr.
        Start symbol: program. *)

  | MenhirState111 : ((('s, _menhir_box_program) _menhir_cell1_in_expr, _menhir_box_program) _menhir_cell1_NOT, _menhir_box_program) _menhir_state
    (** State 111.
        Stack shape : in_expr NOT.
        Start symbol: program. *)

  | MenhirState115 : (('s, _menhir_box_program) _menhir_cell1_eq_expr _menhir_cell0_NOT, _menhir_box_program) _menhir_state
    (** State 115.
        Stack shape : eq_expr NOT.
        Start symbol: program. *)

  | MenhirState118 : (('s, _menhir_box_program) _menhir_cell1_bit_xor_expr, _menhir_box_program) _menhir_state
    (** State 118.
        Stack shape : bit_xor_expr.
        Start symbol: program. *)

  | MenhirState121 : (('s, _menhir_box_program) _menhir_cell1_bit_and_expr, _menhir_box_program) _menhir_state
    (** State 121.
        Stack shape : bit_and_expr.
        Start symbol: program. *)

  | MenhirState124 : (('s, _menhir_box_program) _menhir_cell1_bit_or_expr, _menhir_box_program) _menhir_state
    (** State 124.
        Stack shape : bit_or_expr.
        Start symbol: program. *)

  | MenhirState126 : (('s, _menhir_box_program) _menhir_cell1_bit_or_expr, _menhir_box_program) _menhir_state
    (** State 126.
        Stack shape : bit_or_expr.
        Start symbol: program. *)

  | MenhirState128 : (('s, _menhir_box_program) _menhir_cell1_eq_expr, _menhir_box_program) _menhir_state
    (** State 128.
        Stack shape : eq_expr.
        Start symbol: program. *)

  | MenhirState134 : (('s, _menhir_box_program) _menhir_cell1_and_expr, _menhir_box_program) _menhir_state
    (** State 134.
        Stack shape : and_expr.
        Start symbol: program. *)

  | MenhirState136 : (('s, _menhir_box_program) _menhir_cell1_or_expr, _menhir_box_program) _menhir_state
    (** State 136.
        Stack shape : or_expr.
        Start symbol: program. *)

  | MenhirState138 : (('s, _menhir_box_program) _menhir_cell1_or_expr, _menhir_box_program) _menhir_state
    (** State 138.
        Stack shape : or_expr.
        Start symbol: program. *)

  | MenhirState140 : (('s, _menhir_box_program) _menhir_cell1_or_expr, _menhir_box_program) _menhir_state
    (** State 140.
        Stack shape : or_expr.
        Start symbol: program. *)

  | MenhirState142 : (('s, _menhir_box_program) _menhir_cell1_or_expr, _menhir_box_program) _menhir_state
    (** State 142.
        Stack shape : or_expr.
        Start symbol: program. *)

  | MenhirState144 : (('s, _menhir_box_program) _menhir_cell1_or_expr, _menhir_box_program) _menhir_state
    (** State 144.
        Stack shape : or_expr.
        Start symbol: program. *)

  | MenhirState146 : (('s, _menhir_box_program) _menhir_cell1_or_expr, _menhir_box_program) _menhir_state
    (** State 146.
        Stack shape : or_expr.
        Start symbol: program. *)

  | MenhirState148 : (('s, _menhir_box_program) _menhir_cell1_or_expr, _menhir_box_program) _menhir_state
    (** State 148.
        Stack shape : or_expr.
        Start symbol: program. *)

  | MenhirState152 : (('s, _menhir_box_program) _menhir_cell1_walrus_expr _menhir_cell0_IF, _menhir_box_program) _menhir_state
    (** State 152.
        Stack shape : walrus_expr IF.
        Start symbol: program. *)

  | MenhirState156 : ((('s, _menhir_box_program) _menhir_cell1_walrus_expr _menhir_cell0_IF, _menhir_box_program) _menhir_cell1_expr, _menhir_box_program) _menhir_state
    (** State 156.
        Stack shape : walrus_expr IF expr.
        Start symbol: program. *)

  | MenhirState162 : (('s, _menhir_box_program) _menhir_cell1_expr, _menhir_box_program) _menhir_state
    (** State 162.
        Stack shape : expr.
        Start symbol: program. *)

  | MenhirState164 : (('s, _menhir_box_program) _menhir_cell1_postfix_expr _menhir_cell0_LBRACK, _menhir_box_program) _menhir_state
    (** State 164.
        Stack shape : postfix_expr LBRACK.
        Start symbol: program. *)

  | MenhirState166 : ((('s, _menhir_box_program) _menhir_cell1_postfix_expr _menhir_cell0_LBRACK, _menhir_box_program) _menhir_cell1_expr_option, _menhir_box_program) _menhir_state
    (** State 166.
        Stack shape : postfix_expr LBRACK expr_option.
        Start symbol: program. *)

  | MenhirState169 : (((('s, _menhir_box_program) _menhir_cell1_postfix_expr _menhir_cell0_LBRACK, _menhir_box_program) _menhir_cell1_expr_option, _menhir_box_program) _menhir_cell1_expr_option, _menhir_box_program) _menhir_state
    (** State 169.
        Stack shape : postfix_expr LBRACK expr_option expr_option.
        Start symbol: program. *)

  | MenhirState188 : (('s, _menhir_box_program) _menhir_cell1_expr, _menhir_box_program) _menhir_state
    (** State 188.
        Stack shape : expr.
        Start symbol: program. *)

  | MenhirState191 : (('s, _menhir_box_program) _menhir_cell1_dict_item, _menhir_box_program) _menhir_state
    (** State 191.
        Stack shape : dict_item.
        Start symbol: program. *)

  | MenhirState210 : (('s, _menhir_box_program) _menhir_cell1_enum_item, _menhir_box_program) _menhir_state
    (** State 210.
        Stack shape : enum_item.
        Start symbol: program. *)

  | MenhirState214 : (('s, _menhir_box_program) _menhir_cell1_DEF _menhir_cell0_IDENT _menhir_cell0_LPAREN, _menhir_box_program) _menhir_state
    (** State 214.
        Stack shape : DEF IDENT LPAREN.
        Start symbol: program. *)

  | MenhirState216 : ((('s, _menhir_box_program) _menhir_cell1_DEF _menhir_cell0_IDENT _menhir_cell0_LPAREN, _menhir_box_program) _menhir_cell1_loption_separated_nonempty_list_COMMA_param__, _menhir_box_program) _menhir_state
    (** State 216.
        Stack shape : DEF IDENT LPAREN loption(separated_nonempty_list(COMMA,param)).
        Start symbol: program. *)

  | MenhirState218 : (('s, _menhir_box_program) _menhir_cell1_LBRACE, _menhir_box_program) _menhir_state
    (** State 218.
        Stack shape : LBRACE.
        Start symbol: program. *)

  | MenhirState219 : (('s, _menhir_box_program) _menhir_cell1_YIELD, _menhir_box_program) _menhir_state
    (** State 219.
        Stack shape : YIELD.
        Start symbol: program. *)

  | MenhirState223 : (('s, _menhir_box_program) _menhir_cell1_WITH, _menhir_box_program) _menhir_state
    (** State 223.
        Stack shape : WITH.
        Start symbol: program. *)

  | MenhirState225 : (('s, _menhir_box_program) _menhir_cell1_with_item, _menhir_box_program) _menhir_state
    (** State 225.
        Stack shape : with_item.
        Start symbol: program. *)

  | MenhirState227 : (('s, _menhir_box_program) _menhir_cell1_expr, _menhir_box_program) _menhir_state
    (** State 227.
        Stack shape : expr.
        Start symbol: program. *)

  | MenhirState229 : ((('s, _menhir_box_program) _menhir_cell1_WITH, _menhir_box_program) _menhir_cell1_separated_nonempty_list_COMMA_with_item_, _menhir_box_program) _menhir_state
    (** State 229.
        Stack shape : WITH separated_nonempty_list(COMMA,with_item).
        Start symbol: program. *)

  | MenhirState231 : (('s, _menhir_box_program) _menhir_cell1_WHILE _menhir_cell0_LPAREN, _menhir_box_program) _menhir_state
    (** State 231.
        Stack shape : WHILE LPAREN.
        Start symbol: program. *)

  | MenhirState233 : ((('s, _menhir_box_program) _menhir_cell1_WHILE _menhir_cell0_LPAREN, _menhir_box_program) _menhir_cell1_expr, _menhir_box_program) _menhir_state
    (** State 233.
        Stack shape : WHILE LPAREN expr.
        Start symbol: program. *)

  | MenhirState234 : (('s, _menhir_box_program) _menhir_cell1_TRY, _menhir_box_program) _menhir_state
    (** State 234.
        Stack shape : TRY.
        Start symbol: program. *)

  | MenhirState235 : ((('s, _menhir_box_program) _menhir_cell1_TRY, _menhir_box_program) _menhir_cell1_block, _menhir_box_program) _menhir_state
    (** State 235.
        Stack shape : TRY block.
        Start symbol: program. *)

  | MenhirState236 : (('s, _menhir_box_program) _menhir_cell1_EXCEPT, _menhir_box_program) _menhir_state
    (** State 236.
        Stack shape : EXCEPT.
        Start symbol: program. *)

  | MenhirState237 : ((('s, _menhir_box_program) _menhir_cell1_EXCEPT, _menhir_box_program) _menhir_cell1_option_expr_, _menhir_box_program) _menhir_state
    (** State 237.
        Stack shape : EXCEPT option(expr).
        Start symbol: program. *)

  | MenhirState238 : (((('s, _menhir_box_program) _menhir_cell1_EXCEPT, _menhir_box_program) _menhir_cell1_option_expr_, _menhir_box_program) _menhir_cell1_option_preceded_AS_IDENT__, _menhir_box_program) _menhir_state
    (** State 238.
        Stack shape : EXCEPT option(expr) option(preceded(AS,IDENT)).
        Start symbol: program. *)

  | MenhirState239 : (('s, _menhir_box_program) _menhir_cell1_RETURN, _menhir_box_program) _menhir_state
    (** State 239.
        Stack shape : RETURN.
        Start symbol: program. *)

  | MenhirState243 : (('s, _menhir_box_program) _menhir_cell1_RAISE, _menhir_box_program) _menhir_state
    (** State 243.
        Stack shape : RAISE.
        Start symbol: program. *)

  | MenhirState248 : (('s, _menhir_box_program) _menhir_cell1_NONLOCAL, _menhir_box_program) _menhir_state
    (** State 248.
        Stack shape : NONLOCAL.
        Start symbol: program. *)

  | MenhirState250 : (('s, _menhir_box_program) _menhir_cell1_IDENT, _menhir_box_program) _menhir_state
    (** State 250.
        Stack shape : IDENT.
        Start symbol: program. *)

  | MenhirState254 : (('s, _menhir_box_program) _menhir_cell1_LBRACE, _menhir_box_program) _menhir_state
    (** State 254.
        Stack shape : LBRACE.
        Start symbol: program. *)

  | MenhirState256 : (('s, _menhir_box_program) _menhir_cell1_IF _menhir_cell0_LPAREN, _menhir_box_program) _menhir_state
    (** State 256.
        Stack shape : IF LPAREN.
        Start symbol: program. *)

  | MenhirState258 : ((('s, _menhir_box_program) _menhir_cell1_IF _menhir_cell0_LPAREN, _menhir_box_program) _menhir_cell1_expr, _menhir_box_program) _menhir_state
    (** State 258.
        Stack shape : IF LPAREN expr.
        Start symbol: program. *)

  | MenhirState259 : (('s, _menhir_box_program) _menhir_cell1_GLOBAL, _menhir_box_program) _menhir_state
    (** State 259.
        Stack shape : GLOBAL.
        Start symbol: program. *)

  | MenhirState263 : (('s, _menhir_box_program) _menhir_cell1_FOR _menhir_cell0_LPAREN, _menhir_box_program) _menhir_state
    (** State 263.
        Stack shape : FOR LPAREN.
        Start symbol: program. *)

  | MenhirState264 : (('s, _menhir_box_program) _menhir_cell1_DEL, _menhir_box_program) _menhir_state
    (** State 264.
        Stack shape : DEL.
        Start symbol: program. *)

  | MenhirState271 : (('s, _menhir_box_program) _menhir_cell1_ASSERT, _menhir_box_program) _menhir_state
    (** State 271.
        Stack shape : ASSERT.
        Start symbol: program. *)

  | MenhirState274 : ((('s, _menhir_box_program) _menhir_cell1_ASSERT, _menhir_box_program) _menhir_cell1_expr, _menhir_box_program) _menhir_state
    (** State 274.
        Stack shape : ASSERT expr.
        Start symbol: program. *)

  | MenhirState278 : ((('s, _menhir_box_program) _menhir_cell1_FOR _menhir_cell0_LPAREN, _menhir_box_program) _menhir_cell1_stmt, _menhir_box_program) _menhir_state
    (** State 278.
        Stack shape : FOR LPAREN stmt.
        Start symbol: program. *)

  | MenhirState280 : (((('s, _menhir_box_program) _menhir_cell1_FOR _menhir_cell0_LPAREN, _menhir_box_program) _menhir_cell1_stmt, _menhir_box_program) _menhir_cell1_expr, _menhir_box_program) _menhir_state
    (** State 280.
        Stack shape : FOR LPAREN stmt expr.
        Start symbol: program. *)

  | MenhirState282 : ((((('s, _menhir_box_program) _menhir_cell1_FOR _menhir_cell0_LPAREN, _menhir_box_program) _menhir_cell1_stmt, _menhir_box_program) _menhir_cell1_expr, _menhir_box_program) _menhir_cell1_expr, _menhir_box_program) _menhir_state
    (** State 282.
        Stack shape : FOR LPAREN stmt expr expr.
        Start symbol: program. *)

  | MenhirState292 : ((('s, _menhir_box_program) _menhir_cell1_FOR _menhir_cell0_LPAREN, _menhir_box_program) _menhir_cell1_expr, _menhir_box_program) _menhir_state
    (** State 292.
        Stack shape : FOR LPAREN expr.
        Start symbol: program. *)

  | MenhirState294 : (((('s, _menhir_box_program) _menhir_cell1_FOR _menhir_cell0_LPAREN, _menhir_box_program) _menhir_cell1_expr, _menhir_box_program) _menhir_cell1_expr, _menhir_box_program) _menhir_state
    (** State 294.
        Stack shape : FOR LPAREN expr expr.
        Start symbol: program. *)

  | MenhirState296 : (((('s, _menhir_box_program) _menhir_cell1_IF _menhir_cell0_LPAREN, _menhir_box_program) _menhir_cell1_expr, _menhir_box_program) _menhir_cell1_stmt, _menhir_box_program) _menhir_state
    (** State 296.
        Stack shape : IF LPAREN expr stmt.
        Start symbol: program. *)

  | MenhirState297 : (((('s _menhir_cell0_LPAREN, _menhir_box_program) _menhir_cell1_expr, _menhir_box_program) _menhir_cell1_stmt, _menhir_box_program) _menhir_cell1_ELSE, _menhir_box_program) _menhir_state
    (** State 297.
        Stack shape : LPAREN expr stmt ELSE.
        Start symbol: program. *)

  | MenhirState300 : (((('s _menhir_cell0_LPAREN, _menhir_box_program) _menhir_cell1_expr, _menhir_box_program) _menhir_cell1_stmt, _menhir_box_program) _menhir_cell1_ELIF _menhir_cell0_LPAREN, _menhir_box_program) _menhir_state
    (** State 300.
        Stack shape : LPAREN expr stmt ELIF LPAREN.
        Start symbol: program. *)

  | MenhirState302 : ((((('s _menhir_cell0_LPAREN, _menhir_box_program) _menhir_cell1_expr, _menhir_box_program) _menhir_cell1_stmt, _menhir_box_program) _menhir_cell1_ELIF _menhir_cell0_LPAREN, _menhir_box_program) _menhir_cell1_expr, _menhir_box_program) _menhir_state
    (** State 302.
        Stack shape : LPAREN expr stmt ELIF LPAREN expr.
        Start symbol: program. *)

  | MenhirState303 : (((((('s _menhir_cell0_LPAREN, _menhir_box_program) _menhir_cell1_expr, _menhir_box_program) _menhir_cell1_stmt, _menhir_box_program) _menhir_cell1_ELIF _menhir_cell0_LPAREN, _menhir_box_program) _menhir_cell1_expr, _menhir_box_program) _menhir_cell1_stmt, _menhir_box_program) _menhir_state
    (** State 303.
        Stack shape : LPAREN expr stmt ELIF LPAREN expr stmt.
        Start symbol: program. *)

  | MenhirState306 : (('s, _menhir_box_program) _menhir_cell1_stmt, _menhir_box_program) _menhir_state
    (** State 306.
        Stack shape : stmt.
        Start symbol: program. *)

  | MenhirState313 : (((('s, _menhir_box_program) _menhir_cell1_TRY, _menhir_box_program) _menhir_cell1_block, _menhir_box_program) _menhir_cell1_list_except_clause_, _menhir_box_program) _menhir_state
    (** State 313.
        Stack shape : TRY block list(except_clause).
        Start symbol: program. *)

  | MenhirState317 : (('s, _menhir_box_program) _menhir_cell1_except_clause, _menhir_box_program) _menhir_state
    (** State 317.
        Stack shape : except_clause.
        Start symbol: program. *)

  | MenhirState324 : (('s, _menhir_box_program) _menhir_cell1_CLASS _menhir_cell0_IDENT _menhir_cell0_LBRACE, _menhir_box_program) _menhir_state
    (** State 324.
        Stack shape : CLASS IDENT LBRACE.
        Start symbol: program. *)

  | MenhirState328 : (('s, _menhir_box_program) _menhir_cell1_top_decl, _menhir_box_program) _menhir_state
    (** State 328.
        Stack shape : top_decl.
        Start symbol: program. *)


and ('s, 'r) _menhir_cell1_add_expr = 
  | MenhirCell1_add_expr of 's * ('s, 'r) _menhir_state * (Ast.expr) * Lexing.position

and ('s, 'r) _menhir_cell1_and_expr = 
  | MenhirCell1_and_expr of 's * ('s, 'r) _menhir_state * (Ast.expr) * Lexing.position

and ('s, 'r) _menhir_cell1_bit_and_expr = 
  | MenhirCell1_bit_and_expr of 's * ('s, 'r) _menhir_state * (Ast.expr) * Lexing.position

and ('s, 'r) _menhir_cell1_bit_or_expr = 
  | MenhirCell1_bit_or_expr of 's * ('s, 'r) _menhir_state * (Ast.expr) * Lexing.position

and ('s, 'r) _menhir_cell1_bit_xor_expr = 
  | MenhirCell1_bit_xor_expr of 's * ('s, 'r) _menhir_state * (Ast.expr) * Lexing.position

and ('s, 'r) _menhir_cell1_block = 
  | MenhirCell1_block of 's * ('s, 'r) _menhir_state * (Ast.stmt)

and ('s, 'r) _menhir_cell1_cmp_expr = 
  | MenhirCell1_cmp_expr of 's * ('s, 'r) _menhir_state * (Ast.expr) * Lexing.position

and ('s, 'r) _menhir_cell1_dict_item = 
  | MenhirCell1_dict_item of 's * ('s, 'r) _menhir_state * (Ast.expr * Ast.expr)

and ('s, 'r) _menhir_cell1_enum_item = 
  | MenhirCell1_enum_item of 's * ('s, 'r) _menhir_state * (string * Ast.expr option)

and ('s, 'r) _menhir_cell1_eq_expr = 
  | MenhirCell1_eq_expr of 's * ('s, 'r) _menhir_state * (Ast.expr) * Lexing.position

and ('s, 'r) _menhir_cell1_except_clause = 
  | MenhirCell1_except_clause of 's * ('s, 'r) _menhir_state * (Ast.expr option * string option * Ast.stmt)

and ('s, 'r) _menhir_cell1_expr = 
  | MenhirCell1_expr of 's * ('s, 'r) _menhir_state * (Ast.expr) * Lexing.position

and ('s, 'r) _menhir_cell1_expr_option = 
  | MenhirCell1_expr_option of 's * ('s, 'r) _menhir_state * (Ast.expr option)

and ('s, 'r) _menhir_cell1_import_item = 
  | MenhirCell1_import_item of 's * ('s, 'r) _menhir_state * (string * string option)

and ('s, 'r) _menhir_cell1_in_expr = 
  | MenhirCell1_in_expr of 's * ('s, 'r) _menhir_state * (Ast.expr) * Lexing.position

and ('s, 'r) _menhir_cell1_list_except_clause_ = 
  | MenhirCell1_list_except_clause_ of 's * ('s, 'r) _menhir_state * ((Ast.expr option * string option * Ast.stmt) list)

and ('s, 'r) _menhir_cell1_loption_separated_nonempty_list_COMMA_param__ = 
  | MenhirCell1_loption_separated_nonempty_list_COMMA_param__ of 's * ('s, 'r) _menhir_state * (Ast.param list)

and ('s, 'r) _menhir_cell1_member_decl = 
  | MenhirCell1_member_decl of 's * ('s, 'r) _menhir_state * (Ast.decl)

and ('s, 'r) _menhir_cell1_mul_expr = 
  | MenhirCell1_mul_expr of 's * ('s, 'r) _menhir_state * (Ast.expr) * Lexing.position

and ('s, 'r) _menhir_cell1_not_expr = 
  | MenhirCell1_not_expr of 's * ('s, 'r) _menhir_state * (Ast.expr) * Lexing.position

and ('s, 'r) _menhir_cell1_option_expr_ = 
  | MenhirCell1_option_expr_ of 's * ('s, 'r) _menhir_state * (Ast.expr option)

and ('s, 'r) _menhir_cell1_option_preceded_AS_IDENT__ = 
  | MenhirCell1_option_preceded_AS_IDENT__ of 's * ('s, 'r) _menhir_state * (string option)

and ('s, 'r) _menhir_cell1_or_expr = 
  | MenhirCell1_or_expr of 's * ('s, 'r) _menhir_state * (Ast.expr) * Lexing.position

and ('s, 'r) _menhir_cell1_param = 
  | MenhirCell1_param of 's * ('s, 'r) _menhir_state * (Ast.param)

and ('s, 'r) _menhir_cell1_postfix_expr = 
  | MenhirCell1_postfix_expr of 's * ('s, 'r) _menhir_state * (Ast.expr) * Lexing.position

and ('s, 'r) _menhir_cell1_pow_expr = 
  | MenhirCell1_pow_expr of 's * ('s, 'r) _menhir_state * (Ast.expr) * Lexing.position

and ('s, 'r) _menhir_cell1_separated_nonempty_list_COMMA_with_item_ = 
  | MenhirCell1_separated_nonempty_list_COMMA_with_item_ of 's * ('s, 'r) _menhir_state * ((Ast.expr * string option) list)

and ('s, 'r) _menhir_cell1_shift_expr = 
  | MenhirCell1_shift_expr of 's * ('s, 'r) _menhir_state * (Ast.expr) * Lexing.position

and ('s, 'r) _menhir_cell1_stmt = 
  | MenhirCell1_stmt of 's * ('s, 'r) _menhir_state * (Ast.stmt)

and ('s, 'r) _menhir_cell1_top_decl = 
  | MenhirCell1_top_decl of 's * ('s, 'r) _menhir_state * (Ast.decl)

and ('s, 'r) _menhir_cell1_unary_expr = 
  | MenhirCell1_unary_expr of 's * ('s, 'r) _menhir_state * (Ast.expr) * Lexing.position

and ('s, 'r) _menhir_cell1_walrus_expr = 
  | MenhirCell1_walrus_expr of 's * ('s, 'r) _menhir_state * (Ast.expr) * Lexing.position

and ('s, 'r) _menhir_cell1_with_item = 
  | MenhirCell1_with_item of 's * ('s, 'r) _menhir_state * (Ast.expr * string option)

and ('s, 'r) _menhir_cell1_ASSERT = 
  | MenhirCell1_ASSERT of 's * ('s, 'r) _menhir_state * Lexing.position

and ('s, 'r) _menhir_cell1_ASSIGN = 
  | MenhirCell1_ASSIGN of 's * ('s, 'r) _menhir_state

and ('s, 'r) _menhir_cell1_BITNOT = 
  | MenhirCell1_BITNOT of 's * ('s, 'r) _menhir_state * Lexing.position

and ('s, 'r) _menhir_cell1_CLASS = 
  | MenhirCell1_CLASS of 's * ('s, 'r) _menhir_state * Lexing.position

and ('s, 'r) _menhir_cell1_DEC = 
  | MenhirCell1_DEC of 's * ('s, 'r) _menhir_state * Lexing.position

and ('s, 'r) _menhir_cell1_DEF = 
  | MenhirCell1_DEF of 's * ('s, 'r) _menhir_state * Lexing.position

and ('s, 'r) _menhir_cell1_DEL = 
  | MenhirCell1_DEL of 's * ('s, 'r) _menhir_state * Lexing.position

and ('s, 'r) _menhir_cell1_ELIF = 
  | MenhirCell1_ELIF of 's * ('s, 'r) _menhir_state * Lexing.position

and ('s, 'r) _menhir_cell1_ELSE = 
  | MenhirCell1_ELSE of 's * ('s, 'r) _menhir_state

and ('s, 'r) _menhir_cell1_ENUM = 
  | MenhirCell1_ENUM of 's * ('s, 'r) _menhir_state * Lexing.position

and ('s, 'r) _menhir_cell1_EXCEPT = 
  | MenhirCell1_EXCEPT of 's * ('s, 'r) _menhir_state

and ('s, 'r) _menhir_cell1_FOR = 
  | MenhirCell1_FOR of 's * ('s, 'r) _menhir_state * Lexing.position

and ('s, 'r) _menhir_cell1_FROM = 
  | MenhirCell1_FROM of 's * ('s, 'r) _menhir_state * Lexing.position

and ('s, 'r) _menhir_cell1_GLOBAL = 
  | MenhirCell1_GLOBAL of 's * ('s, 'r) _menhir_state * Lexing.position

and ('s, 'r) _menhir_cell1_IDENT = 
  | MenhirCell1_IDENT of 's * ('s, 'r) _menhir_state * 
# 45 "src/parser.mly"
       (string)
# 832 "src/parser.ml"
 * Lexing.position

and 's _menhir_cell0_IDENT = 
  | MenhirCell0_IDENT of 's * 
# 45 "src/parser.mly"
       (string)
# 839 "src/parser.ml"
 * Lexing.position

and ('s, 'r) _menhir_cell1_IF = 
  | MenhirCell1_IF of 's * ('s, 'r) _menhir_state * Lexing.position

and 's _menhir_cell0_IF = 
  | MenhirCell0_IF of 's * Lexing.position

and ('s, 'r) _menhir_cell1_IMPORT = 
  | MenhirCell1_IMPORT of 's * ('s, 'r) _menhir_state * Lexing.position

and 's _menhir_cell0_IMPORT = 
  | MenhirCell0_IMPORT of 's * Lexing.position

and ('s, 'r) _menhir_cell1_INC = 
  | MenhirCell1_INC of 's * ('s, 'r) _menhir_state * Lexing.position

and ('s, 'r) _menhir_cell1_LAMBDA = 
  | MenhirCell1_LAMBDA of 's * ('s, 'r) _menhir_state * Lexing.position

and ('s, 'r) _menhir_cell1_LBRACE = 
  | MenhirCell1_LBRACE of 's * ('s, 'r) _menhir_state * Lexing.position

and 's _menhir_cell0_LBRACE = 
  | MenhirCell0_LBRACE of 's * Lexing.position

and ('s, 'r) _menhir_cell1_LBRACK = 
  | MenhirCell1_LBRACK of 's * ('s, 'r) _menhir_state * Lexing.position

and 's _menhir_cell0_LBRACK = 
  | MenhirCell0_LBRACK of 's * Lexing.position

and ('s, 'r) _menhir_cell1_LPAREN = 
  | MenhirCell1_LPAREN of 's * ('s, 'r) _menhir_state * Lexing.position

and 's _menhir_cell0_LPAREN = 
  | MenhirCell0_LPAREN of 's * Lexing.position

and ('s, 'r) _menhir_cell1_MINUS = 
  | MenhirCell1_MINUS of 's * ('s, 'r) _menhir_state * Lexing.position

and 's _menhir_cell0_MINUS = 
  | MenhirCell0_MINUS of 's * Lexing.position

and ('s, 'r) _menhir_cell1_NONLOCAL = 
  | MenhirCell1_NONLOCAL of 's * ('s, 'r) _menhir_state * Lexing.position

and ('s, 'r) _menhir_cell1_NOT = 
  | MenhirCell1_NOT of 's * ('s, 'r) _menhir_state * Lexing.position

and 's _menhir_cell0_NOT = 
  | MenhirCell0_NOT of 's * Lexing.position

and ('s, 'r) _menhir_cell1_PLUS = 
  | MenhirCell1_PLUS of 's * ('s, 'r) _menhir_state * Lexing.position

and 's _menhir_cell0_PLUS = 
  | MenhirCell0_PLUS of 's * Lexing.position

and ('s, 'r) _menhir_cell1_RAISE = 
  | MenhirCell1_RAISE of 's * ('s, 'r) _menhir_state * Lexing.position

and ('s, 'r) _menhir_cell1_RETURN = 
  | MenhirCell1_RETURN of 's * ('s, 'r) _menhir_state * Lexing.position

and ('s, 'r) _menhir_cell1_STRUCT = 
  | MenhirCell1_STRUCT of 's * ('s, 'r) _menhir_state * Lexing.position

and ('s, 'r) _menhir_cell1_SUPER = 
  | MenhirCell1_SUPER of 's * ('s, 'r) _menhir_state * Lexing.position

and ('s, 'r) _menhir_cell1_TRY = 
  | MenhirCell1_TRY of 's * ('s, 'r) _menhir_state * Lexing.position

and ('s, 'r) _menhir_cell1_WHILE = 
  | MenhirCell1_WHILE of 's * ('s, 'r) _menhir_state * Lexing.position

and ('s, 'r) _menhir_cell1_WITH = 
  | MenhirCell1_WITH of 's * ('s, 'r) _menhir_state * Lexing.position

and ('s, 'r) _menhir_cell1_YIELD = 
  | MenhirCell1_YIELD of 's * ('s, 'r) _menhir_state * Lexing.position

and _menhir_box_program = 
  | MenhirBox_program of (Ast.program) [@@unboxed]

let _menhir_action_001 =
  fun e ->
    (
# 335 "src/parser.mly"
      ( e )
# 931 "src/parser.ml"
     : (Ast.expr))

let _menhir_action_002 =
  fun _startpos_e_ a e ->
    let _startpos = _startpos_e_ in
    (
# 337 "src/parser.mly"
      ( mk_expr (EBinary (Add, e, a)) (_startpos) )
# 940 "src/parser.ml"
     : (Ast.expr))

let _menhir_action_003 =
  fun _startpos_e_ a e ->
    let _startpos = _startpos_e_ in
    (
# 339 "src/parser.mly"
      ( mk_expr (EBinary (Sub, e, a)) (_startpos) )
# 949 "src/parser.ml"
     : (Ast.expr))

let _menhir_action_004 =
  fun e ->
    (
# 263 "src/parser.mly"
      ( e )
# 957 "src/parser.ml"
     : (Ast.expr))

let _menhir_action_005 =
  fun _startpos_e_ a e ->
    let _startpos = _startpos_e_ in
    (
# 265 "src/parser.mly"
      ( mk_expr (EBinary (And, e, a)) (_startpos) )
# 966 "src/parser.ml"
     : (Ast.expr))

let _menhir_action_006 =
  fun e ->
    (
# 235 "src/parser.mly"
      ( e )
# 974 "src/parser.ml"
     : (Ast.expr))

let _menhir_action_007 =
  fun _startpos_e_ a e ->
    let _startpos = _startpos_e_ in
    (
# 237 "src/parser.mly"
      ( mk_expr (EBinary (Assign, e, a)) (_startpos) )
# 983 "src/parser.ml"
     : (Ast.expr))

let _menhir_action_008 =
  fun _startpos_e_ a e ->
    let _startpos = _startpos_e_ in
    (
# 239 "src/parser.mly"
      ( mk_expr (EBinary (AddAssign, e, a)) (_startpos) )
# 992 "src/parser.ml"
     : (Ast.expr))

let _menhir_action_009 =
  fun _startpos_e_ a e ->
    let _startpos = _startpos_e_ in
    (
# 241 "src/parser.mly"
      ( mk_expr (EBinary (SubAssign, e, a)) (_startpos) )
# 1001 "src/parser.ml"
     : (Ast.expr))

let _menhir_action_010 =
  fun _startpos_e_ a e ->
    let _startpos = _startpos_e_ in
    (
# 243 "src/parser.mly"
      ( mk_expr (EBinary (MulAssign, e, a)) (_startpos) )
# 1010 "src/parser.ml"
     : (Ast.expr))

let _menhir_action_011 =
  fun _startpos_e_ a e ->
    let _startpos = _startpos_e_ in
    (
# 245 "src/parser.mly"
      ( mk_expr (EBinary (DivAssign, e, a)) (_startpos) )
# 1019 "src/parser.ml"
     : (Ast.expr))

let _menhir_action_012 =
  fun _startpos_e_ a e ->
    let _startpos = _startpos_e_ in
    (
# 247 "src/parser.mly"
      ( mk_expr (EBinary (ModAssign, e, a)) (_startpos) )
# 1028 "src/parser.ml"
     : (Ast.expr))

let _menhir_action_013 =
  fun _startpos_e_ a e ->
    let _startpos = _startpos_e_ in
    (
# 249 "src/parser.mly"
      ( mk_expr (EBinary (PowAssign, e, a)) (_startpos) )
# 1037 "src/parser.ml"
     : (Ast.expr))

let _menhir_action_014 =
  fun _startpos_e_ a e ->
    let _startpos = _startpos_e_ in
    (
# 251 "src/parser.mly"
      ( mk_expr (EBinary (FloorDivAssign, e, a)) (_startpos) )
# 1046 "src/parser.ml"
     : (Ast.expr))

let _menhir_action_015 =
  fun e ->
    (
# 307 "src/parser.mly"
                        ( e )
# 1054 "src/parser.ml"
     : (Ast.expr))

let _menhir_action_016 =
  fun _startpos_e_ a e ->
    let _startpos = _startpos_e_ in
    (
# 309 "src/parser.mly"
      ( mk_expr (EBinary (BitAnd, e, a)) (_startpos) )
# 1063 "src/parser.ml"
     : (Ast.expr))

let _menhir_action_017 =
  fun e ->
    (
# 295 "src/parser.mly"
                          ( e )
# 1071 "src/parser.ml"
     : (Ast.expr))

let _menhir_action_018 =
  fun _startpos_e_ a e ->
    let _startpos = _startpos_e_ in
    (
# 297 "src/parser.mly"
      ( mk_expr (EBinary (BitOr, e, a)) (_startpos) )
# 1080 "src/parser.ml"
     : (Ast.expr))

let _menhir_action_019 =
  fun e ->
    (
# 301 "src/parser.mly"
                          ( e )
# 1088 "src/parser.ml"
     : (Ast.expr))

let _menhir_action_020 =
  fun _startpos_e_ a e ->
    let _startpos = _startpos_e_ in
    (
# 303 "src/parser.mly"
      ( mk_expr (EBinary (BitXor, e, a)) (_startpos) )
# 1097 "src/parser.ml"
     : (Ast.expr))

let _menhir_action_021 =
  fun _startpos__1_ stmts ->
    let _startpos = _startpos__1_ in
    (
# 142 "src/parser.mly"
      ( mk_stmt (SBlock stmts) (_startpos) )
# 1106 "src/parser.ml"
     : (Ast.stmt))

let _menhir_action_022 =
  fun _startpos__1_ ms n ->
    let _startpos = _startpos__1_ in
    (
# 103 "src/parser.mly"
      ( let stmts = List.map (fun d -> mk_stmt_from_pos (SDecl d) d.d_pos) ms in
        mk_decl (DClassDef (n, [], mk_stmt (SBlock stmts) (_startpos))) (_startpos) )
# 1116 "src/parser.ml"
     : (Ast.decl))

let _menhir_action_023 =
  fun e ->
    (
# 322 "src/parser.mly"
      ( e )
# 1124 "src/parser.ml"
     : (Ast.expr))

let _menhir_action_024 =
  fun _startpos_e_ a e ->
    let _startpos = _startpos_e_ in
    (
# 324 "src/parser.mly"
      ( mk_expr (EBinary (Lt, e, a)) (_startpos) )
# 1133 "src/parser.ml"
     : (Ast.expr))

let _menhir_action_025 =
  fun _startpos_e_ a e ->
    let _startpos = _startpos_e_ in
    (
# 326 "src/parser.mly"
      ( mk_expr (EBinary (Gt, e, a)) (_startpos) )
# 1142 "src/parser.ml"
     : (Ast.expr))

let _menhir_action_026 =
  fun _startpos_e_ a e ->
    let _startpos = _startpos_e_ in
    (
# 328 "src/parser.mly"
      ( mk_expr (EBinary (Le, e, a)) (_startpos) )
# 1151 "src/parser.ml"
     : (Ast.expr))

let _menhir_action_027 =
  fun _startpos_e_ a e ->
    let _startpos = _startpos_e_ in
    (
# 330 "src/parser.mly"
      ( mk_expr (EBinary (Ge, e, a)) (_startpos) )
# 1160 "src/parser.ml"
     : (Ast.expr))

let _menhir_action_028 =
  fun d ->
    (
# 182 "src/parser.mly"
                    ( d )
# 1168 "src/parser.ml"
     : (Ast.decl))

let _menhir_action_029 =
  fun d ->
    (
# 183 "src/parser.mly"
                    ( d )
# 1176 "src/parser.ml"
     : (Ast.decl))

let _menhir_action_030 =
  fun d ->
    (
# 184 "src/parser.mly"
                    ( d )
# 1184 "src/parser.ml"
     : (Ast.decl))

let _menhir_action_031 =
  fun d ->
    (
# 185 "src/parser.mly"
                    ( d )
# 1192 "src/parser.ml"
     : (Ast.decl))

let _menhir_action_032 =
  fun _startpos__1_ ->
    let _startpos = _startpos__1_ in
    (
# 186 "src/parser.mly"
                    ( mk_decl (DVarDecl ("_", None)) (_startpos) )
# 1201 "src/parser.ml"
     : (Ast.decl))

let _menhir_action_033 =
  fun k v ->
    (
# 424 "src/parser.mly"
                            ( (k, v) )
# 1209 "src/parser.ml"
     : (Ast.expr * Ast.expr))

let _menhir_action_034 =
  fun _startpos__1_ c e t ->
    let _startpos = _startpos__1_ in
    (
# 204 "src/parser.mly"
      ( Some (mk_stmt (SIf (c, t, e)) (_startpos)) )
# 1218 "src/parser.ml"
     : (Ast.stmt option))

let _menhir_action_035 =
  fun b ->
    (
# 206 "src/parser.mly"
      ( Some b )
# 1226 "src/parser.ml"
     : (Ast.stmt option))

let _menhir_action_036 =
  fun () ->
    (
# 208 "src/parser.mly"
      ( None )
# 1234 "src/parser.ml"
     : (Ast.stmt option))

let _menhir_action_037 =
  fun _startpos__1_ n xs ->
    let es = 
# 241 "<standard.mly>"
    ( xs )
# 1242 "src/parser.ml"
     in
    let _startpos = _startpos__1_ in
    (
# 131 "src/parser.mly"
      ( mk_decl (DEnumDecl (n, es)) (_startpos) )
# 1248 "src/parser.ml"
     : (Ast.decl))

let _menhir_action_038 =
  fun _startpos__1_ n ->
    let _startpos = _startpos__1_ in
    (
# 133 "src/parser.mly"
      ( mk_decl (DEnumDecl (n, [])) (_startpos) )
# 1257 "src/parser.ml"
     : (Ast.decl))

let _menhir_action_039 =
  fun n v ->
    (
# 137 "src/parser.mly"
                                                 ( (n, v) )
# 1265 "src/parser.ml"
     : (string * Ast.expr option))

let _menhir_action_040 =
  fun e ->
    (
# 287 "src/parser.mly"
      ( e )
# 1273 "src/parser.ml"
     : (Ast.expr))

let _menhir_action_041 =
  fun _startpos_e_ a e ->
    let _startpos = _startpos_e_ in
    (
# 289 "src/parser.mly"
      ( mk_expr (EBinary (Eq, e, a)) (_startpos) )
# 1282 "src/parser.ml"
     : (Ast.expr))

let _menhir_action_042 =
  fun _startpos_e_ a e ->
    let _startpos = _startpos_e_ in
    (
# 291 "src/parser.mly"
      ( mk_expr (EBinary (Ne, e, a)) (_startpos) )
# 1291 "src/parser.ml"
     : (Ast.expr))

let _menhir_action_043 =
  fun a b e ->
    (
# 191 "src/parser.mly"
      ( (e, a, b) )
# 1299 "src/parser.ml"
     : (Ast.expr option * string option * Ast.stmt))

let _menhir_action_044 =
  fun e ->
    (
# 212 "src/parser.mly"
                    ( e )
# 1307 "src/parser.ml"
     : (Ast.expr))

let _menhir_action_045 =
  fun e ->
    (
# 213 "src/parser.mly"
                    ( e )
# 1315 "src/parser.ml"
     : (Ast.expr))

let _menhir_action_046 =
  fun e ->
    (
# 428 "src/parser.mly"
             ( Some e )
# 1323 "src/parser.ml"
     : (Ast.expr option))

let _menhir_action_047 =
  fun () ->
    (
# 429 "src/parser.mly"
             ( None )
# 1331 "src/parser.ml"
     : (Ast.expr option))

let _menhir_action_048 =
  fun b ->
    (
# 195 "src/parser.mly"
                     ( b )
# 1339 "src/parser.ml"
     : (Ast.stmt))

let _menhir_action_049 =
  fun _startpos__1_ b n xs ->
    let ps = 
# 241 "<standard.mly>"
    ( xs )
# 1347 "src/parser.ml"
     in
    let _startpos = _startpos__1_ in
    (
# 91 "src/parser.mly"
      ( mk_decl (DFuncDef (n, ps, None, b)) (_startpos) )
# 1353 "src/parser.ml"
     : (Ast.decl))

let _menhir_action_050 =
  fun _startpos__1_ n xs ->
    let ps = 
# 241 "<standard.mly>"
    ( xs )
# 1361 "src/parser.ml"
     in
    let _startpos = _startpos__1_ in
    (
# 93 "src/parser.mly"
      ( mk_decl (DExternFunc (n, ps)) (_startpos) )
# 1367 "src/parser.ml"
     : (Ast.decl))

let _menhir_action_051 =
  fun _startpos_e_ a c e ->
    let _startpos = _startpos_e_ in
    (
# 223 "src/parser.mly"
      ( mk_expr (ETernary (c, e, a)) (_startpos) )
# 1376 "src/parser.ml"
     : (Ast.expr))

let _menhir_action_052 =
  fun e ->
    (
# 224 "src/parser.mly"
                    ( e )
# 1384 "src/parser.ml"
     : (Ast.expr))

let _menhir_action_053 =
  fun _startpos__1_ a m ->
    let _startpos = _startpos__1_ in
    (
# 109 "src/parser.mly"
      ( mk_decl (DImport (m, a)) (_startpos) )
# 1393 "src/parser.ml"
     : (Ast.decl))

let _menhir_action_054 =
  fun _startpos__1_ m ns ->
    let _startpos = _startpos__1_ in
    (
# 111 "src/parser.mly"
      ( mk_decl (DFromImport (m, ns)) (_startpos) )
# 1402 "src/parser.ml"
     : (Ast.decl))

let _menhir_action_055 =
  fun a n ->
    (
# 115 "src/parser.mly"
                                              ( (n, a) )
# 1410 "src/parser.ml"
     : (string * string option))

let _menhir_action_056 =
  fun e ->
    (
# 280 "src/parser.mly"
                        ( e )
# 1418 "src/parser.ml"
     : (Ast.expr))

let _menhir_action_057 =
  fun _startpos_e_ a e ->
    let _startpos = _startpos_e_ in
    (
# 281 "src/parser.mly"
                                    ( mk_expr (EBinary (In, e, a)) (_startpos) )
# 1427 "src/parser.ml"
     : (Ast.expr))

let _menhir_action_058 =
  fun _startpos_e_ a e ->
    let _startpos = _startpos_e_ in
    (
# 282 "src/parser.mly"
                                    ( mk_expr (EBinary (NotIn, e, a)) (_startpos) )
# 1436 "src/parser.ml"
     : (Ast.expr))

let _menhir_action_059 =
  fun e ->
    (
# 274 "src/parser.mly"
                      ( e )
# 1444 "src/parser.ml"
     : (Ast.expr))

let _menhir_action_060 =
  fun _startpos_e_ a e ->
    let _startpos = _startpos_e_ in
    (
# 275 "src/parser.mly"
                                   ( mk_expr (EBinary (Is, e, a)) (_startpos) )
# 1453 "src/parser.ml"
     : (Ast.expr))

let _menhir_action_061 =
  fun _startpos_e_ a e ->
    let _startpos = _startpos_e_ in
    (
# 276 "src/parser.mly"
                                   ( mk_expr (EBinary (IsNot, e, a)) (_startpos) )
# 1462 "src/parser.ml"
     : (Ast.expr))

let _menhir_action_062 =
  fun _startpos__1_ e xs ->
    let ps = 
# 241 "<standard.mly>"
    ( xs )
# 1470 "src/parser.ml"
     in
    let _startpos = _startpos__1_ in
    (
# 218 "src/parser.mly"
      ( mk_expr (ELambda (ps, e)) (_startpos) )
# 1476 "src/parser.ml"
     : (Ast.expr))

let _menhir_action_063 =
  fun () ->
    (
# 216 "<standard.mly>"
    ( [] )
# 1484 "src/parser.ml"
     : ((Ast.expr option * string option * Ast.stmt) list))

let _menhir_action_064 =
  fun x xs ->
    (
# 219 "<standard.mly>"
    ( x :: xs )
# 1492 "src/parser.ml"
     : ((Ast.expr option * string option * Ast.stmt) list))

let _menhir_action_065 =
  fun () ->
    (
# 216 "<standard.mly>"
    ( [] )
# 1500 "src/parser.ml"
     : (Ast.decl list))

let _menhir_action_066 =
  fun x xs ->
    (
# 219 "<standard.mly>"
    ( x :: xs )
# 1508 "src/parser.ml"
     : (Ast.decl list))

let _menhir_action_067 =
  fun () ->
    (
# 216 "<standard.mly>"
    ( [] )
# 1516 "src/parser.ml"
     : (Ast.stmt list))

let _menhir_action_068 =
  fun x xs ->
    (
# 219 "<standard.mly>"
    ( x :: xs )
# 1524 "src/parser.ml"
     : (Ast.stmt list))

let _menhir_action_069 =
  fun () ->
    (
# 216 "<standard.mly>"
    ( [] )
# 1532 "src/parser.ml"
     : (Ast.decl list))

let _menhir_action_070 =
  fun x xs ->
    (
# 219 "<standard.mly>"
    ( x :: xs )
# 1540 "src/parser.ml"
     : (Ast.decl list))

let _menhir_action_071 =
  fun () ->
    (
# 145 "<standard.mly>"
    ( [] )
# 1548 "src/parser.ml"
     : ((Ast.expr * Ast.expr) list))

let _menhir_action_072 =
  fun x ->
    (
# 148 "<standard.mly>"
    ( x )
# 1556 "src/parser.ml"
     : ((Ast.expr * Ast.expr) list))

let _menhir_action_073 =
  fun () ->
    (
# 145 "<standard.mly>"
    ( [] )
# 1564 "src/parser.ml"
     : ((string * Ast.expr option) list))

let _menhir_action_074 =
  fun x ->
    (
# 148 "<standard.mly>"
    ( x )
# 1572 "src/parser.ml"
     : ((string * Ast.expr option) list))

let _menhir_action_075 =
  fun () ->
    (
# 145 "<standard.mly>"
    ( [] )
# 1580 "src/parser.ml"
     : (Ast.expr list))

let _menhir_action_076 =
  fun x ->
    (
# 148 "<standard.mly>"
    ( x )
# 1588 "src/parser.ml"
     : (Ast.expr list))

let _menhir_action_077 =
  fun () ->
    (
# 145 "<standard.mly>"
    ( [] )
# 1596 "src/parser.ml"
     : (Ast.param list))

let _menhir_action_078 =
  fun x ->
    (
# 148 "<standard.mly>"
    ( x )
# 1604 "src/parser.ml"
     : (Ast.param list))

let _menhir_action_079 =
  fun _startpos_n_ n ->
    let _startpos = _startpos_n_ in
    (
# 126 "src/parser.mly"
                   ( mk_decl (DVarDecl (n, None)) (_startpos) )
# 1613 "src/parser.ml"
     : (Ast.decl))

let _menhir_action_080 =
  fun e ->
    (
# 344 "src/parser.mly"
      ( e )
# 1621 "src/parser.ml"
     : (Ast.expr))

let _menhir_action_081 =
  fun _startpos_e_ a e ->
    let _startpos = _startpos_e_ in
    (
# 346 "src/parser.mly"
      ( mk_expr (EBinary (Mul, e, a)) (_startpos) )
# 1630 "src/parser.ml"
     : (Ast.expr))

let _menhir_action_082 =
  fun _startpos_e_ a e ->
    let _startpos = _startpos_e_ in
    (
# 348 "src/parser.mly"
      ( mk_expr (EBinary (Div, e, a)) (_startpos) )
# 1639 "src/parser.ml"
     : (Ast.expr))

let _menhir_action_083 =
  fun _startpos_e_ a e ->
    let _startpos = _startpos_e_ in
    (
# 350 "src/parser.mly"
      ( mk_expr (EBinary (Mod, e, a)) (_startpos) )
# 1648 "src/parser.ml"
     : (Ast.expr))

let _menhir_action_084 =
  fun _startpos_e_ a e ->
    let _startpos = _startpos_e_ in
    (
# 352 "src/parser.mly"
      ( mk_expr (EBinary (FloorDiv, e, a)) (_startpos) )
# 1657 "src/parser.ml"
     : (Ast.expr))

let _menhir_action_085 =
  fun e ->
    (
# 269 "src/parser.mly"
                      ( e )
# 1665 "src/parser.ml"
     : (Ast.expr))

let _menhir_action_086 =
  fun _startpos__1_ e ->
    let _startpos = _startpos__1_ in
    (
# 270 "src/parser.mly"
                      ( mk_expr (EUnary (Not, e)) (_startpos) )
# 1674 "src/parser.ml"
     : (Ast.expr))

let _menhir_action_087 =
  fun () ->
    (
# 111 "<standard.mly>"
    ( None )
# 1682 "src/parser.ml"
     : (Ast.expr option))

let _menhir_action_088 =
  fun x ->
    (
# 114 "<standard.mly>"
    ( Some x )
# 1690 "src/parser.ml"
     : (Ast.expr option))

let _menhir_action_089 =
  fun () ->
    (
# 111 "<standard.mly>"
    ( None )
# 1698 "src/parser.ml"
     : (Ast.stmt option))

let _menhir_action_090 =
  fun x ->
    (
# 114 "<standard.mly>"
    ( Some x )
# 1706 "src/parser.ml"
     : (Ast.stmt option))

let _menhir_action_091 =
  fun () ->
    (
# 111 "<standard.mly>"
    ( None )
# 1714 "src/parser.ml"
     : (string option))

let _menhir_action_092 =
  fun x ->
    let x = 
# 188 "<standard.mly>"
    ( x )
# 1722 "src/parser.ml"
     in
    (
# 114 "<standard.mly>"
    ( Some x )
# 1727 "src/parser.ml"
     : (string option))

let _menhir_action_093 =
  fun () ->
    (
# 111 "<standard.mly>"
    ( None )
# 1735 "src/parser.ml"
     : (Ast.expr option))

let _menhir_action_094 =
  fun x ->
    let x = 
# 188 "<standard.mly>"
    ( x )
# 1743 "src/parser.ml"
     in
    (
# 114 "<standard.mly>"
    ( Some x )
# 1748 "src/parser.ml"
     : (Ast.expr option))

let _menhir_action_095 =
  fun e ->
    (
# 256 "src/parser.mly"
      ( e )
# 1756 "src/parser.ml"
     : (Ast.expr))

let _menhir_action_096 =
  fun _startpos_e_ a e ->
    let _startpos = _startpos_e_ in
    (
# 258 "src/parser.mly"
      ( mk_expr (EBinary (Or, e, a)) (_startpos) )
# 1765 "src/parser.ml"
     : (Ast.expr))

let _menhir_action_097 =
  fun _startpos_n_ d n ->
    let _startpos = _startpos_n_ in
    (
# 98 "src/parser.mly"
      ( { p_name = n; p_typ = None; p_default = d; p_pos = mk_pos (_startpos) } )
# 1774 "src/parser.ml"
     : (Ast.param))

let _menhir_action_098 =
  fun e ->
    (
# 377 "src/parser.mly"
      ( e )
# 1782 "src/parser.ml"
     : (Ast.expr))

let _menhir_action_099 =
  fun _startpos_e_ e ->
    let _startpos = _startpos_e_ in
    (
# 379 "src/parser.mly"
      ( mk_expr (EUnary (PostInc, e)) (_startpos) )
# 1791 "src/parser.ml"
     : (Ast.expr))

let _menhir_action_100 =
  fun _startpos_e_ e ->
    let _startpos = _startpos_e_ in
    (
# 381 "src/parser.mly"
      ( mk_expr (EUnary (PostDec, e)) (_startpos) )
# 1800 "src/parser.ml"
     : (Ast.expr))

let _menhir_action_101 =
  fun _startpos_e_ e i ->
    let _startpos = _startpos_e_ in
    (
# 383 "src/parser.mly"
      ( mk_expr (EIndex (e, i)) (_startpos) )
# 1809 "src/parser.ml"
     : (Ast.expr))

let _menhir_action_102 =
  fun _startpos_e_ e i1 i2 ->
    let _startpos = _startpos_e_ in
    (
# 385 "src/parser.mly"
      ( mk_expr (ESlice (i1, i2, None)) (_startpos) )
# 1818 "src/parser.ml"
     : (Ast.expr))

let _menhir_action_103 =
  fun _startpos_e_ e i1 i2 i3 ->
    let _startpos = _startpos_e_ in
    (
# 387 "src/parser.mly"
      ( mk_expr (ESlice (i1, i2, i3)) (_startpos) )
# 1827 "src/parser.ml"
     : (Ast.expr))

let _menhir_action_104 =
  fun _startpos_e_ e xs ->
    let args = 
# 241 "<standard.mly>"
    ( xs )
# 1835 "src/parser.ml"
     in
    let _startpos = _startpos_e_ in
    (
# 389 "src/parser.mly"
      ( mk_expr (ECall (e, args)) (_startpos) )
# 1841 "src/parser.ml"
     : (Ast.expr))

let _menhir_action_105 =
  fun _startpos_e_ e n ->
    let _startpos = _startpos_e_ in
    (
# 391 "src/parser.mly"
      ( mk_expr (EMember (e, n)) (_startpos) )
# 1850 "src/parser.ml"
     : (Ast.expr))

let _menhir_action_106 =
  fun e ->
    (
# 356 "src/parser.mly"
                          ( e )
# 1858 "src/parser.ml"
     : (Ast.expr))

let _menhir_action_107 =
  fun _startpos_e_ a e ->
    let _startpos = _startpos_e_ in
    (
# 358 "src/parser.mly"
      ( mk_expr (EBinary (Pow, e, a)) (_startpos) )
# 1867 "src/parser.ml"
     : (Ast.expr))

let _menhir_action_108 =
  fun _startpos_n_ n ->
    let _startpos = _startpos_n_ in
    (
# 396 "src/parser.mly"
      ( mk_expr (ELiteral (LInt n)) (_startpos) )
# 1876 "src/parser.ml"
     : (Ast.expr))

let _menhir_action_109 =
  fun _startpos_f_ f ->
    let _startpos = _startpos_f_ in
    (
# 398 "src/parser.mly"
      ( mk_expr (ELiteral (LFloat f)) (_startpos) )
# 1885 "src/parser.ml"
     : (Ast.expr))

let _menhir_action_110 =
  fun _startpos_s_ s ->
    let _startpos = _startpos_s_ in
    (
# 400 "src/parser.mly"
      ( mk_expr (ELiteral (LString s)) (_startpos) )
# 1894 "src/parser.ml"
     : (Ast.expr))

let _menhir_action_111 =
  fun _startpos__1_ ->
    let _startpos = _startpos__1_ in
    (
# 402 "src/parser.mly"
      ( mk_expr (ELiteral (LBool true)) (_startpos) )
# 1903 "src/parser.ml"
     : (Ast.expr))

let _menhir_action_112 =
  fun _startpos__1_ ->
    let _startpos = _startpos__1_ in
    (
# 404 "src/parser.mly"
      ( mk_expr (ELiteral (LBool false)) (_startpos) )
# 1912 "src/parser.ml"
     : (Ast.expr))

let _menhir_action_113 =
  fun _startpos__1_ ->
    let _startpos = _startpos__1_ in
    (
# 405 "src/parser.mly"
          ( mk_expr (ELiteral LNull) (_startpos) )
# 1921 "src/parser.ml"
     : (Ast.expr))

let _menhir_action_114 =
  fun _startpos__1_ ->
    let _startpos = _startpos__1_ in
    (
# 406 "src/parser.mly"
          ( mk_expr (ELiteral LNull) (_startpos) )
# 1930 "src/parser.ml"
     : (Ast.expr))

let _menhir_action_115 =
  fun _startpos_n_ n ->
    let _startpos = _startpos_n_ in
    (
# 408 "src/parser.mly"
      ( mk_expr (EIdent n) (_startpos) )
# 1939 "src/parser.ml"
     : (Ast.expr))

let _menhir_action_116 =
  fun _startpos__1_ ->
    let _startpos = _startpos__1_ in
    (
# 410 "src/parser.mly"
      ( mk_expr (EIdent "self") (_startpos) )
# 1948 "src/parser.ml"
     : (Ast.expr))

let _menhir_action_117 =
  fun _startpos__1_ xs ->
    let args = 
# 241 "<standard.mly>"
    ( xs )
# 1956 "src/parser.ml"
     in
    let _startpos = _startpos__1_ in
    (
# 412 "src/parser.mly"
      ( mk_expr (ECall (mk_expr (EIdent "super") (_startpos), args)) (_startpos) )
# 1962 "src/parser.ml"
     : (Ast.expr))

let _menhir_action_118 =
  fun e ->
    (
# 414 "src/parser.mly"
      ( e )
# 1970 "src/parser.ml"
     : (Ast.expr))

let _menhir_action_119 =
  fun _startpos__1_ xs ->
    let items = 
# 241 "<standard.mly>"
    ( xs )
# 1978 "src/parser.ml"
     in
    let _startpos = _startpos__1_ in
    (
# 416 "src/parser.mly"
      ( mk_expr (EList items) (_startpos) )
# 1984 "src/parser.ml"
     : (Ast.expr))

let _menhir_action_120 =
  fun _startpos__1_ xs ->
    let items = 
# 241 "<standard.mly>"
    ( xs )
# 1992 "src/parser.ml"
     in
    let _startpos = _startpos__1_ in
    (
# 418 "src/parser.mly"
      ( mk_expr (EDict items) (_startpos) )
# 1998 "src/parser.ml"
     : (Ast.expr))

let _menhir_action_121 =
  fun _startpos__1_ items ->
    let _startpos = _startpos__1_ in
    (
# 420 "src/parser.mly"
      ( mk_expr (ESet items) (_startpos) )
# 2007 "src/parser.ml"
     : (Ast.expr))

let _menhir_action_122 =
  fun ds ->
    (
# 78 "src/parser.mly"
      ( { decls = ds; file = "" } )
# 2015 "src/parser.ml"
     : (Ast.program))

let _menhir_action_123 =
  fun x ->
    (
# 250 "<standard.mly>"
    ( [ x ] )
# 2023 "src/parser.ml"
     : (string list))

let _menhir_action_124 =
  fun x xs ->
    (
# 253 "<standard.mly>"
    ( x :: xs )
# 2031 "src/parser.ml"
     : (string list))

let _menhir_action_125 =
  fun x ->
    (
# 250 "<standard.mly>"
    ( [ x ] )
# 2039 "src/parser.ml"
     : ((Ast.expr * Ast.expr) list))

let _menhir_action_126 =
  fun x xs ->
    (
# 253 "<standard.mly>"
    ( x :: xs )
# 2047 "src/parser.ml"
     : ((Ast.expr * Ast.expr) list))

let _menhir_action_127 =
  fun x ->
    (
# 250 "<standard.mly>"
    ( [ x ] )
# 2055 "src/parser.ml"
     : ((string * Ast.expr option) list))

let _menhir_action_128 =
  fun x xs ->
    (
# 253 "<standard.mly>"
    ( x :: xs )
# 2063 "src/parser.ml"
     : ((string * Ast.expr option) list))

let _menhir_action_129 =
  fun x ->
    (
# 250 "<standard.mly>"
    ( [ x ] )
# 2071 "src/parser.ml"
     : (Ast.expr list))

let _menhir_action_130 =
  fun x xs ->
    (
# 253 "<standard.mly>"
    ( x :: xs )
# 2079 "src/parser.ml"
     : (Ast.expr list))

let _menhir_action_131 =
  fun x ->
    (
# 250 "<standard.mly>"
    ( [ x ] )
# 2087 "src/parser.ml"
     : ((string * string option) list))

let _menhir_action_132 =
  fun x xs ->
    (
# 253 "<standard.mly>"
    ( x :: xs )
# 2095 "src/parser.ml"
     : ((string * string option) list))

let _menhir_action_133 =
  fun x ->
    (
# 250 "<standard.mly>"
    ( [ x ] )
# 2103 "src/parser.ml"
     : (Ast.param list))

let _menhir_action_134 =
  fun x xs ->
    (
# 253 "<standard.mly>"
    ( x :: xs )
# 2111 "src/parser.ml"
     : (Ast.param list))

let _menhir_action_135 =
  fun x ->
    (
# 250 "<standard.mly>"
    ( [ x ] )
# 2119 "src/parser.ml"
     : ((Ast.expr * string option) list))

let _menhir_action_136 =
  fun x xs ->
    (
# 253 "<standard.mly>"
    ( x :: xs )
# 2127 "src/parser.ml"
     : ((Ast.expr * string option) list))

let _menhir_action_137 =
  fun e ->
    (
# 313 "src/parser.mly"
                        ( e )
# 2135 "src/parser.ml"
     : (Ast.expr))

let _menhir_action_138 =
  fun _startpos_e_ a e ->
    let _startpos = _startpos_e_ in
    (
# 315 "src/parser.mly"
      ( mk_expr (EBinary (LShift, e, a)) (_startpos) )
# 2144 "src/parser.ml"
     : (Ast.expr))

let _menhir_action_139 =
  fun _startpos_e_ a e ->
    let _startpos = _startpos_e_ in
    (
# 317 "src/parser.mly"
      ( mk_expr (EBinary (RShift, e, a)) (_startpos) )
# 2153 "src/parser.ml"
     : (Ast.expr))

let _menhir_action_140 =
  fun s ->
    (
# 146 "src/parser.mly"
                        ( s )
# 2161 "src/parser.ml"
     : (Ast.stmt))

let _menhir_action_141 =
  fun _startpos__1_ c e t ->
    let _startpos = _startpos__1_ in
    (
# 148 "src/parser.mly"
      ( mk_stmt (SIf (c, t, e)) (_startpos) )
# 2170 "src/parser.ml"
     : (Ast.stmt))

let _menhir_action_142 =
  fun _startpos__1_ b c ->
    let _startpos = _startpos__1_ in
    (
# 150 "src/parser.mly"
      ( mk_stmt (SWhile (c, b)) (_startpos) )
# 2179 "src/parser.ml"
     : (Ast.stmt))

let _menhir_action_143 =
  fun _startpos__1_ b c i s ->
    let _startpos = _startpos__1_ in
    (
# 152 "src/parser.mly"
      ( mk_stmt (SFor (i, c, s, b)) (_startpos) )
# 2188 "src/parser.ml"
     : (Ast.stmt))

let _menhir_action_144 =
  fun _startpos__1_ b it v ->
    let _startpos = _startpos__1_ in
    (
# 154 "src/parser.mly"
      ( mk_stmt (SForIn (v, it, b)) (_startpos) )
# 2197 "src/parser.ml"
     : (Ast.stmt))

let _menhir_action_145 =
  fun _startpos__1_ e ->
    let _startpos = _startpos__1_ in
    (
# 156 "src/parser.mly"
      ( mk_stmt (SReturn e) (_startpos) )
# 2206 "src/parser.ml"
     : (Ast.stmt))

let _menhir_action_146 =
  fun _startpos__1_ ->
    let _startpos = _startpos__1_ in
    (
# 157 "src/parser.mly"
                  ( mk_stmt SBreak (_startpos) )
# 2215 "src/parser.ml"
     : (Ast.stmt))

let _menhir_action_147 =
  fun _startpos__1_ ->
    let _startpos = _startpos__1_ in
    (
# 158 "src/parser.mly"
                  ( mk_stmt SContinue (_startpos) )
# 2224 "src/parser.ml"
     : (Ast.stmt))

let _menhir_action_148 =
  fun _startpos__1_ b ecs f ->
    let _startpos = _startpos__1_ in
    (
# 160 "src/parser.mly"
      ( mk_stmt (STry (b, ecs, f)) (_startpos) )
# 2233 "src/parser.ml"
     : (Ast.stmt))

let _menhir_action_149 =
  fun _startpos__1_ e ->
    let _startpos = _startpos__1_ in
    (
# 162 "src/parser.mly"
      ( mk_stmt (SRaise e) (_startpos) )
# 2242 "src/parser.ml"
     : (Ast.stmt))

let _menhir_action_150 =
  fun _startpos__1_ b es ->
    let _startpos = _startpos__1_ in
    (
# 164 "src/parser.mly"
      ( mk_stmt (SWith (List.map fst es, b)) (_startpos) )
# 2251 "src/parser.ml"
     : (Ast.stmt))

let _menhir_action_151 =
  fun _startpos__1_ ns ->
    let _startpos = _startpos__1_ in
    (
# 166 "src/parser.mly"
      ( mk_stmt (SGlobal ns) (_startpos) )
# 2260 "src/parser.ml"
     : (Ast.stmt))

let _menhir_action_152 =
  fun _startpos__1_ ns ->
    let _startpos = _startpos__1_ in
    (
# 168 "src/parser.mly"
      ( mk_stmt (SNonlocal ns) (_startpos) )
# 2269 "src/parser.ml"
     : (Ast.stmt))

let _menhir_action_153 =
  fun _startpos__1_ e ->
    let _startpos = _startpos__1_ in
    (
# 170 "src/parser.mly"
      ( mk_stmt (SDelete e) (_startpos) )
# 2278 "src/parser.ml"
     : (Ast.stmt))

let _menhir_action_154 =
  fun _startpos__1_ c ->
    let _startpos = _startpos__1_ in
    (
# 172 "src/parser.mly"
      ( mk_stmt (SAssert (c, None)) (_startpos) )
# 2287 "src/parser.ml"
     : (Ast.stmt))

let _menhir_action_155 =
  fun _startpos__1_ c m ->
    let _startpos = _startpos__1_ in
    (
# 174 "src/parser.mly"
      ( mk_stmt (SAssert (c, Some m)) (_startpos) )
# 2296 "src/parser.ml"
     : (Ast.stmt))

let _menhir_action_156 =
  fun _startpos__1_ ->
    let _startpos = _startpos__1_ in
    (
# 175 "src/parser.mly"
                          ( mk_stmt (SYield None) (_startpos) )
# 2305 "src/parser.ml"
     : (Ast.stmt))

let _menhir_action_157 =
  fun _startpos__1_ e ->
    let _startpos = _startpos__1_ in
    (
# 176 "src/parser.mly"
                          ( mk_stmt (SYield (Some e)) (_startpos) )
# 2314 "src/parser.ml"
     : (Ast.stmt))

let _menhir_action_158 =
  fun _startpos_d_ d ->
    let _startpos = _startpos_d_ in
    (
# 177 "src/parser.mly"
                          ( mk_stmt (SDecl d) (_startpos) )
# 2323 "src/parser.ml"
     : (Ast.stmt))

let _menhir_action_159 =
  fun _startpos_e_ e ->
    let _startpos = _startpos_e_ in
    (
# 178 "src/parser.mly"
                          ( mk_stmt (SExpr e) (_startpos) )
# 2332 "src/parser.ml"
     : (Ast.stmt))

let _menhir_action_160 =
  fun _startpos__1_ ms n ->
    let _startpos = _startpos__1_ in
    (
# 120 "src/parser.mly"
      ( mk_decl (DStructDecl (n, Some ms)) (_startpos) )
# 2341 "src/parser.ml"
     : (Ast.decl))

let _menhir_action_161 =
  fun _startpos__1_ n ->
    let _startpos = _startpos__1_ in
    (
# 122 "src/parser.mly"
      ( mk_decl (DStructDecl (n, None)) (_startpos) )
# 2350 "src/parser.ml"
     : (Ast.decl))

let _menhir_action_162 =
  fun d ->
    (
# 82 "src/parser.mly"
                       ( d )
# 2358 "src/parser.ml"
     : (Ast.decl))

let _menhir_action_163 =
  fun d ->
    (
# 83 "src/parser.mly"
                       ( d )
# 2366 "src/parser.ml"
     : (Ast.decl))

let _menhir_action_164 =
  fun d ->
    (
# 84 "src/parser.mly"
                       ( d )
# 2374 "src/parser.ml"
     : (Ast.decl))

let _menhir_action_165 =
  fun d ->
    (
# 85 "src/parser.mly"
                       ( d )
# 2382 "src/parser.ml"
     : (Ast.decl))

let _menhir_action_166 =
  fun d ->
    (
# 86 "src/parser.mly"
                       ( d )
# 2390 "src/parser.ml"
     : (Ast.decl))

let _menhir_action_167 =
  fun e ->
    (
# 362 "src/parser.mly"
                              ( e )
# 2398 "src/parser.ml"
     : (Ast.expr))

let _menhir_action_168 =
  fun _startpos__1_ e ->
    let _startpos = _startpos__1_ in
    (
# 364 "src/parser.mly"
      ( mk_expr (EUnary (Neg, e)) (_startpos) )
# 2407 "src/parser.ml"
     : (Ast.expr))

let _menhir_action_169 =
  fun _startpos__1_ e ->
    let _startpos = _startpos__1_ in
    (
# 366 "src/parser.mly"
      ( mk_expr (EUnary (BitNot, e)) (_startpos) )
# 2416 "src/parser.ml"
     : (Ast.expr))

let _menhir_action_170 =
  fun _startpos__1_ e ->
    let _startpos = _startpos__1_ in
    (
# 368 "src/parser.mly"
      ( mk_expr (EUnary (Plus, e)) (_startpos) )
# 2425 "src/parser.ml"
     : (Ast.expr))

let _menhir_action_171 =
  fun _startpos__1_ e ->
    let _startpos = _startpos__1_ in
    (
# 370 "src/parser.mly"
      ( mk_expr (EUnary (PreInc, e)) (_startpos) )
# 2434 "src/parser.ml"
     : (Ast.expr))

let _menhir_action_172 =
  fun _startpos__1_ e ->
    let _startpos = _startpos__1_ in
    (
# 372 "src/parser.mly"
      ( mk_expr (EUnary (PreDec, e)) (_startpos) )
# 2443 "src/parser.ml"
     : (Ast.expr))

let _menhir_action_173 =
  fun _startpos_n_ e n ->
    let _startpos = _startpos_n_ in
    (
# 229 "src/parser.mly"
      ( mk_expr (EWalrus (n, e)) (_startpos) )
# 2452 "src/parser.ml"
     : (Ast.expr))

let _menhir_action_174 =
  fun e ->
    (
# 230 "src/parser.mly"
                    ( e )
# 2460 "src/parser.ml"
     : (Ast.expr))

let _menhir_action_175 =
  fun a e ->
    (
# 199 "src/parser.mly"
                                             ( (e, a) )
# 2468 "src/parser.ml"
     : (Ast.expr * string option))

let _menhir_print_token : token -> string =
  fun _tok ->
    match _tok with
    | YIELD ->
        "YIELD"
    | WITH ->
        "WITH"
    | WHILE ->
        "WHILE"
    | WALRUS ->
        "WALRUS"
    | TRY ->
        "TRY"
    | TRUE ->
        "TRUE"
    | SUPER ->
        "SUPER"
    | SUB_ASSIGN ->
        "SUB_ASSIGN"
    | STRUCT ->
        "STRUCT"
    | STRING _ ->
        "STRING"
    | STAR ->
        "STAR"
    | SLASH ->
        "SLASH"
    | SEMI ->
        "SEMI"
    | SELF ->
        "SELF"
    | RSHIFT_ASSIGN ->
        "RSHIFT_ASSIGN"
    | RSHIFT ->
        "RSHIFT"
    | RPAREN ->
        "RPAREN"
    | RETURN ->
        "RETURN"
    | RBRACK ->
        "RBRACK"
    | RBRACE ->
        "RBRACE"
    | RAISE ->
        "RAISE"
    | POW_ASSIGN ->
        "POW_ASSIGN"
    | POW ->
        "POW"
    | PLUS ->
        "PLUS"
    | PERCENT ->
        "PERCENT"
    | PASS ->
        "PASS"
    | OR ->
        "OR"
    | NULL ->
        "NULL"
    | NOT ->
        "NOT"
    | NONLOCAL ->
        "NONLOCAL"
    | NONE ->
        "NONE"
    | NEQ ->
        "NEQ"
    | MUL_ASSIGN ->
        "MUL_ASSIGN"
    | MOD_ASSIGN ->
        "MOD_ASSIGN"
    | MINUS ->
        "MINUS"
    | MATCH ->
        "MATCH"
    | LT ->
        "LT"
    | LSHIFT_ASSIGN ->
        "LSHIFT_ASSIGN"
    | LSHIFT ->
        "LSHIFT"
    | LPAREN ->
        "LPAREN"
    | LE ->
        "LE"
    | LBRACK ->
        "LBRACK"
    | LBRACE ->
        "LBRACE"
    | LAMBDA ->
        "LAMBDA"
    | IS ->
        "IS"
    | INT _ ->
        "INT"
    | INC ->
        "INC"
    | IN ->
        "IN"
    | IMPORT ->
        "IMPORT"
    | IF ->
        "IF"
    | IDENT _ ->
        "IDENT"
    | GT ->
        "GT"
    | GLOBAL ->
        "GLOBAL"
    | GE ->
        "GE"
    | FROM ->
        "FROM"
    | FOR ->
        "FOR"
    | FLOORDIV_ASSIGN ->
        "FLOORDIV_ASSIGN"
    | FLOORDIV ->
        "FLOORDIV"
    | FLOAT _ ->
        "FLOAT"
    | FINALLY ->
        "FINALLY"
    | FALSE ->
        "FALSE"
    | EXCEPT ->
        "EXCEPT"
    | EQEQ ->
        "EQEQ"
    | EOF ->
        "EOF"
    | ENUM ->
        "ENUM"
    | ELSE ->
        "ELSE"
    | ELIF ->
        "ELIF"
    | DOT ->
        "DOT"
    | DIV_ASSIGN ->
        "DIV_ASSIGN"
    | DEL ->
        "DEL"
    | DEF ->
        "DEF"
    | DEC ->
        "DEC"
    | DARROW ->
        "DARROW"
    | CONTINUE ->
        "CONTINUE"
    | COMMA ->
        "COMMA"
    | COLON ->
        "COLON"
    | CLASS ->
        "CLASS"
    | CASE ->
        "CASE"
    | BREAK ->
        "BREAK"
    | BITXOR_ASSIGN ->
        "BITXOR_ASSIGN"
    | BITXOR ->
        "BITXOR"
    | BITOR_ASSIGN ->
        "BITOR_ASSIGN"
    | BITOR ->
        "BITOR"
    | BITNOT ->
        "BITNOT"
    | BITAND_ASSIGN ->
        "BITAND_ASSIGN"
    | BITAND ->
        "BITAND"
    | ASSIGN ->
        "ASSIGN"
    | ASSERT ->
        "ASSERT"
    | AS ->
        "AS"
    | ARROW ->
        "ARROW"
    | AND ->
        "AND"
    | ADD_ASSIGN ->
        "ADD_ASSIGN"

let _menhir_fail : unit -> 'a =
  fun () ->
    Printf.eprintf "Internal failure -- please contact the parser generator's developers.\n%!";
    assert false

include struct
  
  [@@@ocaml.warning "-4-37"]
  
  let _menhir_run_336 : type  ttv_stack. ttv_stack -> _ -> _menhir_box_program =
    fun _menhir_stack _v ->
      let ds = _v in
      let _v = _menhir_action_122 ds in
      MenhirBox_program _v
  
  let rec _menhir_run_330 : type  ttv_stack. (ttv_stack, _menhir_box_program) _menhir_cell1_top_decl -> _ -> _menhir_box_program =
    fun _menhir_stack _v ->
      let MenhirCell1_top_decl (_menhir_stack, _menhir_s, x) = _menhir_stack in
      let xs = _v in
      let _v = _menhir_action_070 x xs in
      _menhir_goto_list_top_decl_ _menhir_stack _v _menhir_s
  
  and _menhir_goto_list_top_decl_ : type  ttv_stack. ttv_stack -> _ -> (ttv_stack, _menhir_box_program) _menhir_state -> _menhir_box_program =
    fun _menhir_stack _v _menhir_s ->
      match _menhir_s with
      | MenhirState328 ->
          _menhir_run_330 _menhir_stack _v
      | MenhirState000 ->
          _menhir_run_336 _menhir_stack _v
      | _ ->
          _menhir_fail ()
  
  let rec _menhir_run_001 : type  ttv_stack. ttv_stack -> _ -> _ -> (ttv_stack, _menhir_box_program) _menhir_state -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s ->
      let _startpos = _menhir_lexbuf.Lexing.lex_start_p in
      let _tok = _menhir_lexer _menhir_lexbuf in
      match (_tok : MenhirBasics.token) with
      | IDENT _v ->
          let _startpos_0 = _menhir_lexbuf.Lexing.lex_start_p in
          let _tok = _menhir_lexer _menhir_lexbuf in
          (match (_tok : MenhirBasics.token) with
          | SEMI ->
              let _tok = _menhir_lexer _menhir_lexbuf in
              let (n, _startpos__1_) = (_v, _startpos) in
              let _v = _menhir_action_161 _startpos__1_ n in
              _menhir_goto_struct_decl _menhir_stack _menhir_lexbuf _menhir_lexer _startpos__1_ _v _menhir_s _tok
          | LBRACE ->
              let _menhir_stack = MenhirCell1_STRUCT (_menhir_stack, _menhir_s, _startpos) in
              let _menhir_stack = MenhirCell0_IDENT (_menhir_stack, _v, _startpos_0) in
              let _startpos_1 = _menhir_lexbuf.Lexing.lex_start_p in
              let _menhir_stack = MenhirCell0_LBRACE (_menhir_stack, _startpos_1) in
              let _tok = _menhir_lexer _menhir_lexbuf in
              (match (_tok : MenhirBasics.token) with
              | IDENT _v_2 ->
                  _menhir_run_005 _menhir_stack _menhir_lexbuf _menhir_lexer _v_2 MenhirState004
              | RBRACE ->
                  let _v_3 = _menhir_action_065 () in
                  _menhir_run_009 _menhir_stack _menhir_lexbuf _menhir_lexer _v_3
              | _ ->
                  _eRR ())
          | _ ->
              _eRR ())
      | _ ->
          _eRR ()
  
  and _menhir_goto_struct_decl : type  ttv_stack. ttv_stack -> _ -> _ -> _ -> _ -> (ttv_stack, _menhir_box_program) _menhir_state -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok ->
      match _menhir_s with
      | MenhirState218 ->
          _menhir_run_277 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState229 ->
          _menhir_run_277 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState233 ->
          _menhir_run_277 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState238 ->
          _menhir_run_277 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState254 ->
          _menhir_run_277 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState258 ->
          _menhir_run_277 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState263 ->
          _menhir_run_277 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState282 ->
          _menhir_run_277 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState294 ->
          _menhir_run_277 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState297 ->
          _menhir_run_277 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState302 ->
          _menhir_run_277 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState306 ->
          _menhir_run_277 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState313 ->
          _menhir_run_277 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState000 ->
          _menhir_run_329 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s _tok
      | MenhirState328 ->
          _menhir_run_329 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s _tok
      | _ ->
          _menhir_fail ()
  
  and _menhir_run_277 : type  ttv_stack. ttv_stack -> _ -> _ -> _ -> _ -> (ttv_stack, _menhir_box_program) _menhir_state -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok ->
      let (_startpos_d_, d) = (_startpos, _v) in
      let _v = _menhir_action_029 d in
      _menhir_goto_decl_stmt _menhir_stack _menhir_lexbuf _menhir_lexer _startpos_d_ _v _menhir_s _tok
  
  and _menhir_goto_decl_stmt : type  ttv_stack. ttv_stack -> _ -> _ -> _ -> _ -> (ttv_stack, _menhir_box_program) _menhir_state -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok ->
      let (_startpos_d_, d) = (_startpos, _v) in
      let _v = _menhir_action_158 _startpos_d_ d in
      _menhir_goto_stmt _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s _tok
  
  and _menhir_goto_stmt : type  ttv_stack. ttv_stack -> _ -> _ -> _ -> (ttv_stack, _menhir_box_program) _menhir_state -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s _tok ->
      match _menhir_s with
      | MenhirState263 ->
          _menhir_run_278 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s _tok
      | MenhirState282 ->
          _menhir_run_283 _menhir_stack _menhir_lexbuf _menhir_lexer _v _tok
      | MenhirState294 ->
          _menhir_run_295 _menhir_stack _menhir_lexbuf _menhir_lexer _v _tok
      | MenhirState258 ->
          _menhir_run_296 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s _tok
      | MenhirState297 ->
          _menhir_run_298 _menhir_stack _menhir_lexbuf _menhir_lexer _v _tok
      | MenhirState302 ->
          _menhir_run_303 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s _tok
      | MenhirState218 ->
          _menhir_run_306 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s _tok
      | MenhirState254 ->
          _menhir_run_306 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s _tok
      | MenhirState306 ->
          _menhir_run_306 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s _tok
      | MenhirState238 ->
          _menhir_run_311 _menhir_stack _menhir_lexbuf _menhir_lexer _v _tok
      | MenhirState313 ->
          _menhir_run_314 _menhir_stack _menhir_lexbuf _menhir_lexer _v _tok
      | MenhirState233 ->
          _menhir_run_319 _menhir_stack _menhir_lexbuf _menhir_lexer _v _tok
      | MenhirState229 ->
          _menhir_run_320 _menhir_stack _menhir_lexbuf _menhir_lexer _v _tok
      | _ ->
          _menhir_fail ()
  
  and _menhir_run_278 : type  ttv_stack. ((ttv_stack, _menhir_box_program) _menhir_cell1_FOR _menhir_cell0_LPAREN as 'stack) -> _ -> _ -> _ -> ('stack, _menhir_box_program) _menhir_state -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s _tok ->
      let _menhir_stack = MenhirCell1_stmt (_menhir_stack, _menhir_s, _v) in
      match (_tok : MenhirBasics.token) with
      | TRUE ->
          _menhir_run_034 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState278
      | SUPER ->
          _menhir_run_035 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState278
      | STRING _v_0 ->
          _menhir_run_037 _menhir_stack _menhir_lexbuf _menhir_lexer _v_0 MenhirState278
      | SELF ->
          _menhir_run_038 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState278
      | PLUS ->
          _menhir_run_039 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState278
      | NULL ->
          _menhir_run_040 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState278
      | NOT ->
          _menhir_run_044 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState278
      | NONE ->
          _menhir_run_041 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState278
      | MINUS ->
          _menhir_run_042 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState278
      | LPAREN ->
          _menhir_run_043 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState278
      | LBRACK ->
          _menhir_run_045 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState278
      | LBRACE ->
          _menhir_run_046 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState278
      | LAMBDA ->
          _menhir_run_047 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState278
      | INT _v_1 ->
          _menhir_run_056 _menhir_stack _menhir_lexbuf _menhir_lexer _v_1 MenhirState278
      | INC ->
          _menhir_run_057 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState278
      | IDENT _v_2 ->
          _menhir_run_067 _menhir_stack _menhir_lexbuf _menhir_lexer _v_2 MenhirState278
      | FLOAT _v_3 ->
          _menhir_run_059 _menhir_stack _menhir_lexbuf _menhir_lexer _v_3 MenhirState278
      | FALSE ->
          _menhir_run_060 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState278
      | DEC ->
          _menhir_run_061 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState278
      | BITNOT ->
          _menhir_run_062 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState278
      | _ ->
          _eRR ()
  
  and _menhir_run_034 : type  ttv_stack. ttv_stack -> _ -> _ -> (ttv_stack, _menhir_box_program) _menhir_state -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s ->
      let _startpos = _menhir_lexbuf.Lexing.lex_start_p in
      let _tok = _menhir_lexer _menhir_lexbuf in
      let _startpos__1_ = _startpos in
      let _v = _menhir_action_111 _startpos__1_ in
      _menhir_goto_primary_expr _menhir_stack _menhir_lexbuf _menhir_lexer _startpos__1_ _v _menhir_s _tok
  
  and _menhir_goto_primary_expr : type  ttv_stack. ttv_stack -> _ -> _ -> _ -> _ -> (ttv_stack, _menhir_box_program) _menhir_state -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok ->
      let (_startpos_e_, e) = (_startpos, _v) in
      let _v = _menhir_action_098 e in
      _menhir_goto_postfix_expr _menhir_stack _menhir_lexbuf _menhir_lexer _startpos_e_ _v _menhir_s _tok
  
  and _menhir_goto_postfix_expr : type  ttv_stack. ttv_stack -> _ -> _ -> _ -> _ -> (ttv_stack, _menhir_box_program) _menhir_state -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok ->
      match (_tok : MenhirBasics.token) with
      | LPAREN ->
          let _menhir_stack = MenhirCell1_postfix_expr (_menhir_stack, _menhir_s, _v, _startpos) in
          let _startpos_0 = _menhir_lexbuf.Lexing.lex_start_p in
          let _menhir_stack = MenhirCell0_LPAREN (_menhir_stack, _startpos_0) in
          let _tok = _menhir_lexer _menhir_lexbuf in
          (match (_tok : MenhirBasics.token) with
          | TRUE ->
              _menhir_run_034 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState066
          | SUPER ->
              _menhir_run_035 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState066
          | STRING _v_1 ->
              _menhir_run_037 _menhir_stack _menhir_lexbuf _menhir_lexer _v_1 MenhirState066
          | SELF ->
              _menhir_run_038 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState066
          | PLUS ->
              _menhir_run_039 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState066
          | NULL ->
              _menhir_run_040 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState066
          | NOT ->
              _menhir_run_044 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState066
          | NONE ->
              _menhir_run_041 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState066
          | MINUS ->
              _menhir_run_042 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState066
          | LPAREN ->
              _menhir_run_043 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState066
          | LBRACK ->
              _menhir_run_045 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState066
          | LBRACE ->
              _menhir_run_046 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState066
          | LAMBDA ->
              _menhir_run_047 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState066
          | INT _v_2 ->
              _menhir_run_056 _menhir_stack _menhir_lexbuf _menhir_lexer _v_2 MenhirState066
          | INC ->
              _menhir_run_057 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState066
          | IDENT _v_3 ->
              _menhir_run_067 _menhir_stack _menhir_lexbuf _menhir_lexer _v_3 MenhirState066
          | FLOAT _v_4 ->
              _menhir_run_059 _menhir_stack _menhir_lexbuf _menhir_lexer _v_4 MenhirState066
          | FALSE ->
              _menhir_run_060 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState066
          | DEC ->
              _menhir_run_061 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState066
          | BITNOT ->
              _menhir_run_062 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState066
          | RPAREN ->
              let _v_5 = _menhir_action_075 () in
              _menhir_run_159 _menhir_stack _menhir_lexbuf _menhir_lexer _v_5 _tok
          | _ ->
              _eRR ())
      | LBRACK ->
          let _menhir_stack = MenhirCell1_postfix_expr (_menhir_stack, _menhir_s, _v, _startpos) in
          let _startpos_6 = _menhir_lexbuf.Lexing.lex_start_p in
          let _menhir_stack = MenhirCell0_LBRACK (_menhir_stack, _startpos_6) in
          let _tok = _menhir_lexer _menhir_lexbuf in
          (match (_tok : MenhirBasics.token) with
          | TRUE ->
              _menhir_run_034 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState164
          | SUPER ->
              _menhir_run_035 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState164
          | STRING _v_7 ->
              _menhir_run_037 _menhir_stack _menhir_lexbuf _menhir_lexer _v_7 MenhirState164
          | SELF ->
              _menhir_run_038 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState164
          | PLUS ->
              _menhir_run_039 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState164
          | NULL ->
              _menhir_run_040 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState164
          | NOT ->
              _menhir_run_044 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState164
          | NONE ->
              _menhir_run_041 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState164
          | MINUS ->
              _menhir_run_042 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState164
          | LPAREN ->
              _menhir_run_043 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState164
          | LBRACK ->
              _menhir_run_045 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState164
          | LBRACE ->
              _menhir_run_046 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState164
          | LAMBDA ->
              _menhir_run_047 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState164
          | INT _v_8 ->
              _menhir_run_056 _menhir_stack _menhir_lexbuf _menhir_lexer _v_8 MenhirState164
          | INC ->
              _menhir_run_057 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState164
          | IDENT _v_9 ->
              _menhir_run_067 _menhir_stack _menhir_lexbuf _menhir_lexer _v_9 MenhirState164
          | FLOAT _v_10 ->
              _menhir_run_059 _menhir_stack _menhir_lexbuf _menhir_lexer _v_10 MenhirState164
          | FALSE ->
              _menhir_run_060 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState164
          | DEC ->
              _menhir_run_061 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState164
          | BITNOT ->
              _menhir_run_062 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState164
          | COLON ->
              let _v_11 = _menhir_action_047 () in
              _menhir_run_165 _menhir_stack _menhir_lexbuf _menhir_lexer _v_11 MenhirState164 _tok
          | _ ->
              _eRR ())
      | INC ->
          let _tok = _menhir_lexer _menhir_lexbuf in
          let (_startpos_e_, e) = (_startpos, _v) in
          let _v = _menhir_action_099 _startpos_e_ e in
          _menhir_goto_postfix_expr _menhir_stack _menhir_lexbuf _menhir_lexer _startpos_e_ _v _menhir_s _tok
      | DOT ->
          let _tok = _menhir_lexer _menhir_lexbuf in
          (match (_tok : MenhirBasics.token) with
          | IDENT _v_13 ->
              let _tok = _menhir_lexer _menhir_lexbuf in
              let (n, _startpos_e_, e) = (_v_13, _startpos, _v) in
              let _v = _menhir_action_105 _startpos_e_ e n in
              _menhir_goto_postfix_expr _menhir_stack _menhir_lexbuf _menhir_lexer _startpos_e_ _v _menhir_s _tok
          | _ ->
              _eRR ())
      | DEC ->
          let _tok = _menhir_lexer _menhir_lexbuf in
          let (_startpos_e_, e) = (_startpos, _v) in
          let _v = _menhir_action_100 _startpos_e_ e in
          _menhir_goto_postfix_expr _menhir_stack _menhir_lexbuf _menhir_lexer _startpos_e_ _v _menhir_s _tok
      | ADD_ASSIGN | AND | AS | ASSERT | ASSIGN | BITAND | BITNOT | BITOR | BITXOR | BREAK | COLON | COMMA | CONTINUE | DARROW | DEF | DEL | DIV_ASSIGN | ELSE | ENUM | EQEQ | FALSE | FLOAT _ | FLOORDIV | FLOORDIV_ASSIGN | FOR | FROM | GE | GLOBAL | GT | IDENT _ | IF | IMPORT | IN | INT _ | IS | LAMBDA | LBRACE | LE | LSHIFT | LT | MINUS | MOD_ASSIGN | MUL_ASSIGN | NEQ | NONE | NONLOCAL | NOT | NULL | OR | PASS | PERCENT | PLUS | POW | POW_ASSIGN | RAISE | RBRACE | RBRACK | RETURN | RPAREN | RSHIFT | SELF | SEMI | SLASH | STAR | STRING _ | STRUCT | SUB_ASSIGN | SUPER | TRUE | TRY | WHILE | WITH | YIELD ->
          let (_startpos_e_, e) = (_startpos, _v) in
          let _v = _menhir_action_167 e in
          _menhir_goto_unary_expr _menhir_stack _menhir_lexbuf _menhir_lexer _startpos_e_ _v _menhir_s _tok
      | _ ->
          _eRR ()
  
  and _menhir_run_035 : type  ttv_stack. ttv_stack -> _ -> _ -> (ttv_stack, _menhir_box_program) _menhir_state -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s ->
      let _startpos = _menhir_lexbuf.Lexing.lex_start_p in
      let _menhir_stack = MenhirCell1_SUPER (_menhir_stack, _menhir_s, _startpos) in
      let _tok = _menhir_lexer _menhir_lexbuf in
      match (_tok : MenhirBasics.token) with
      | LPAREN ->
          let _startpos_0 = _menhir_lexbuf.Lexing.lex_start_p in
          let _menhir_stack = MenhirCell0_LPAREN (_menhir_stack, _startpos_0) in
          let _tok = _menhir_lexer _menhir_lexbuf in
          (match (_tok : MenhirBasics.token) with
          | TRUE ->
              _menhir_run_034 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState036
          | SUPER ->
              _menhir_run_035 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState036
          | STRING _v ->
              _menhir_run_037 _menhir_stack _menhir_lexbuf _menhir_lexer _v MenhirState036
          | SELF ->
              _menhir_run_038 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState036
          | PLUS ->
              _menhir_run_039 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState036
          | NULL ->
              _menhir_run_040 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState036
          | NOT ->
              _menhir_run_044 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState036
          | NONE ->
              _menhir_run_041 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState036
          | MINUS ->
              _menhir_run_042 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState036
          | LPAREN ->
              _menhir_run_043 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState036
          | LBRACK ->
              _menhir_run_045 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState036
          | LBRACE ->
              _menhir_run_046 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState036
          | LAMBDA ->
              _menhir_run_047 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState036
          | INT _v ->
              _menhir_run_056 _menhir_stack _menhir_lexbuf _menhir_lexer _v MenhirState036
          | INC ->
              _menhir_run_057 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState036
          | IDENT _v ->
              _menhir_run_067 _menhir_stack _menhir_lexbuf _menhir_lexer _v MenhirState036
          | FLOAT _v ->
              _menhir_run_059 _menhir_stack _menhir_lexbuf _menhir_lexer _v MenhirState036
          | FALSE ->
              _menhir_run_060 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState036
          | DEC ->
              _menhir_run_061 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState036
          | BITNOT ->
              _menhir_run_062 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState036
          | RPAREN ->
              let _v = _menhir_action_075 () in
              _menhir_run_201 _menhir_stack _menhir_lexbuf _menhir_lexer _v _tok
          | _ ->
              _eRR ())
      | _ ->
          _eRR ()
  
  and _menhir_run_037 : type  ttv_stack. ttv_stack -> _ -> _ -> _ -> (ttv_stack, _menhir_box_program) _menhir_state -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s ->
      let _startpos = _menhir_lexbuf.Lexing.lex_start_p in
      let _tok = _menhir_lexer _menhir_lexbuf in
      let (_startpos_s_, s) = (_startpos, _v) in
      let _v = _menhir_action_110 _startpos_s_ s in
      _menhir_goto_primary_expr _menhir_stack _menhir_lexbuf _menhir_lexer _startpos_s_ _v _menhir_s _tok
  
  and _menhir_run_038 : type  ttv_stack. ttv_stack -> _ -> _ -> (ttv_stack, _menhir_box_program) _menhir_state -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s ->
      let _startpos = _menhir_lexbuf.Lexing.lex_start_p in
      let _tok = _menhir_lexer _menhir_lexbuf in
      let _startpos__1_ = _startpos in
      let _v = _menhir_action_116 _startpos__1_ in
      _menhir_goto_primary_expr _menhir_stack _menhir_lexbuf _menhir_lexer _startpos__1_ _v _menhir_s _tok
  
  and _menhir_run_039 : type  ttv_stack. ttv_stack -> _ -> _ -> (ttv_stack, _menhir_box_program) _menhir_state -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s ->
      let _startpos = _menhir_lexbuf.Lexing.lex_start_p in
      let _menhir_stack = MenhirCell1_PLUS (_menhir_stack, _menhir_s, _startpos) in
      let _menhir_s = MenhirState039 in
      let _tok = _menhir_lexer _menhir_lexbuf in
      match (_tok : MenhirBasics.token) with
      | TRUE ->
          _menhir_run_034 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | SUPER ->
          _menhir_run_035 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | STRING _v ->
          _menhir_run_037 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
      | SELF ->
          _menhir_run_038 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | PLUS ->
          _menhir_run_039 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | NULL ->
          _menhir_run_040 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | NONE ->
          _menhir_run_041 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | MINUS ->
          _menhir_run_042 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | LPAREN ->
          _menhir_run_043 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | LBRACK ->
          _menhir_run_045 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | LBRACE ->
          _menhir_run_046 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | INT _v ->
          _menhir_run_056 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
      | INC ->
          _menhir_run_057 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | IDENT _v ->
          _menhir_run_058 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
      | FLOAT _v ->
          _menhir_run_059 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
      | FALSE ->
          _menhir_run_060 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | DEC ->
          _menhir_run_061 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | BITNOT ->
          _menhir_run_062 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | _ ->
          _eRR ()
  
  and _menhir_run_040 : type  ttv_stack. ttv_stack -> _ -> _ -> (ttv_stack, _menhir_box_program) _menhir_state -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s ->
      let _startpos = _menhir_lexbuf.Lexing.lex_start_p in
      let _tok = _menhir_lexer _menhir_lexbuf in
      let _startpos__1_ = _startpos in
      let _v = _menhir_action_113 _startpos__1_ in
      _menhir_goto_primary_expr _menhir_stack _menhir_lexbuf _menhir_lexer _startpos__1_ _v _menhir_s _tok
  
  and _menhir_run_041 : type  ttv_stack. ttv_stack -> _ -> _ -> (ttv_stack, _menhir_box_program) _menhir_state -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s ->
      let _startpos = _menhir_lexbuf.Lexing.lex_start_p in
      let _tok = _menhir_lexer _menhir_lexbuf in
      let _startpos__1_ = _startpos in
      let _v = _menhir_action_114 _startpos__1_ in
      _menhir_goto_primary_expr _menhir_stack _menhir_lexbuf _menhir_lexer _startpos__1_ _v _menhir_s _tok
  
  and _menhir_run_042 : type  ttv_stack. ttv_stack -> _ -> _ -> (ttv_stack, _menhir_box_program) _menhir_state -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s ->
      let _startpos = _menhir_lexbuf.Lexing.lex_start_p in
      let _menhir_stack = MenhirCell1_MINUS (_menhir_stack, _menhir_s, _startpos) in
      let _menhir_s = MenhirState042 in
      let _tok = _menhir_lexer _menhir_lexbuf in
      match (_tok : MenhirBasics.token) with
      | TRUE ->
          _menhir_run_034 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | SUPER ->
          _menhir_run_035 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | STRING _v ->
          _menhir_run_037 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
      | SELF ->
          _menhir_run_038 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | PLUS ->
          _menhir_run_039 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | NULL ->
          _menhir_run_040 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | NONE ->
          _menhir_run_041 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | MINUS ->
          _menhir_run_042 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | LPAREN ->
          _menhir_run_043 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | LBRACK ->
          _menhir_run_045 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | LBRACE ->
          _menhir_run_046 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | INT _v ->
          _menhir_run_056 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
      | INC ->
          _menhir_run_057 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | IDENT _v ->
          _menhir_run_058 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
      | FLOAT _v ->
          _menhir_run_059 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
      | FALSE ->
          _menhir_run_060 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | DEC ->
          _menhir_run_061 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | BITNOT ->
          _menhir_run_062 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | _ ->
          _eRR ()
  
  and _menhir_run_043 : type  ttv_stack. ttv_stack -> _ -> _ -> (ttv_stack, _menhir_box_program) _menhir_state -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s ->
      let _startpos = _menhir_lexbuf.Lexing.lex_start_p in
      let _menhir_stack = MenhirCell1_LPAREN (_menhir_stack, _menhir_s, _startpos) in
      let _menhir_s = MenhirState043 in
      let _tok = _menhir_lexer _menhir_lexbuf in
      match (_tok : MenhirBasics.token) with
      | TRUE ->
          _menhir_run_034 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | SUPER ->
          _menhir_run_035 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | STRING _v ->
          _menhir_run_037 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
      | SELF ->
          _menhir_run_038 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | PLUS ->
          _menhir_run_039 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | NULL ->
          _menhir_run_040 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | NOT ->
          _menhir_run_044 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | NONE ->
          _menhir_run_041 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | MINUS ->
          _menhir_run_042 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | LPAREN ->
          _menhir_run_043 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | LBRACK ->
          _menhir_run_045 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | LBRACE ->
          _menhir_run_046 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | LAMBDA ->
          _menhir_run_047 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | INT _v ->
          _menhir_run_056 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
      | INC ->
          _menhir_run_057 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | IDENT _v ->
          _menhir_run_067 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
      | FLOAT _v ->
          _menhir_run_059 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
      | FALSE ->
          _menhir_run_060 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | DEC ->
          _menhir_run_061 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | BITNOT ->
          _menhir_run_062 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | _ ->
          _eRR ()
  
  and _menhir_run_044 : type  ttv_stack. ttv_stack -> _ -> _ -> (ttv_stack, _menhir_box_program) _menhir_state -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s ->
      let _startpos = _menhir_lexbuf.Lexing.lex_start_p in
      let _menhir_stack = MenhirCell1_NOT (_menhir_stack, _menhir_s, _startpos) in
      let _menhir_s = MenhirState044 in
      let _tok = _menhir_lexer _menhir_lexbuf in
      match (_tok : MenhirBasics.token) with
      | TRUE ->
          _menhir_run_034 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | SUPER ->
          _menhir_run_035 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | STRING _v ->
          _menhir_run_037 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
      | SELF ->
          _menhir_run_038 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | PLUS ->
          _menhir_run_039 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | NULL ->
          _menhir_run_040 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | NOT ->
          _menhir_run_044 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | NONE ->
          _menhir_run_041 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | MINUS ->
          _menhir_run_042 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | LPAREN ->
          _menhir_run_043 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | LBRACK ->
          _menhir_run_045 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | LBRACE ->
          _menhir_run_046 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | INT _v ->
          _menhir_run_056 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
      | INC ->
          _menhir_run_057 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | IDENT _v ->
          _menhir_run_058 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
      | FLOAT _v ->
          _menhir_run_059 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
      | FALSE ->
          _menhir_run_060 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | DEC ->
          _menhir_run_061 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | BITNOT ->
          _menhir_run_062 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | _ ->
          _eRR ()
  
  and _menhir_run_045 : type  ttv_stack. ttv_stack -> _ -> _ -> (ttv_stack, _menhir_box_program) _menhir_state -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s ->
      let _startpos = _menhir_lexbuf.Lexing.lex_start_p in
      let _menhir_stack = MenhirCell1_LBRACK (_menhir_stack, _menhir_s, _startpos) in
      let _tok = _menhir_lexer _menhir_lexbuf in
      match (_tok : MenhirBasics.token) with
      | TRUE ->
          _menhir_run_034 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState045
      | SUPER ->
          _menhir_run_035 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState045
      | STRING _v ->
          _menhir_run_037 _menhir_stack _menhir_lexbuf _menhir_lexer _v MenhirState045
      | SELF ->
          _menhir_run_038 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState045
      | PLUS ->
          _menhir_run_039 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState045
      | NULL ->
          _menhir_run_040 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState045
      | NOT ->
          _menhir_run_044 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState045
      | NONE ->
          _menhir_run_041 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState045
      | MINUS ->
          _menhir_run_042 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState045
      | LPAREN ->
          _menhir_run_043 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState045
      | LBRACK ->
          _menhir_run_045 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState045
      | LBRACE ->
          _menhir_run_046 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState045
      | LAMBDA ->
          _menhir_run_047 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState045
      | INT _v ->
          _menhir_run_056 _menhir_stack _menhir_lexbuf _menhir_lexer _v MenhirState045
      | INC ->
          _menhir_run_057 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState045
      | IDENT _v ->
          _menhir_run_067 _menhir_stack _menhir_lexbuf _menhir_lexer _v MenhirState045
      | FLOAT _v ->
          _menhir_run_059 _menhir_stack _menhir_lexbuf _menhir_lexer _v MenhirState045
      | FALSE ->
          _menhir_run_060 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState045
      | DEC ->
          _menhir_run_061 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState045
      | BITNOT ->
          _menhir_run_062 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState045
      | RBRACK ->
          let _v = _menhir_action_075 () in
          _menhir_run_194 _menhir_stack _menhir_lexbuf _menhir_lexer _v _tok
      | _ ->
          _eRR ()
  
  and _menhir_run_046 : type  ttv_stack. ttv_stack -> _ -> _ -> (ttv_stack, _menhir_box_program) _menhir_state -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s ->
      let _startpos = _menhir_lexbuf.Lexing.lex_start_p in
      let _menhir_stack = MenhirCell1_LBRACE (_menhir_stack, _menhir_s, _startpos) in
      let _menhir_s = MenhirState046 in
      let _tok = _menhir_lexer _menhir_lexbuf in
      match (_tok : MenhirBasics.token) with
      | TRUE ->
          _menhir_run_034 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | SUPER ->
          _menhir_run_035 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | STRING _v ->
          _menhir_run_037 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
      | SELF ->
          _menhir_run_038 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | PLUS ->
          _menhir_run_039 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | NULL ->
          _menhir_run_040 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | NOT ->
          _menhir_run_044 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | NONE ->
          _menhir_run_041 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | MINUS ->
          _menhir_run_042 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | LPAREN ->
          _menhir_run_043 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | LBRACK ->
          _menhir_run_045 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | LBRACE ->
          _menhir_run_046 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | LAMBDA ->
          _menhir_run_047 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | INT _v ->
          _menhir_run_056 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
      | INC ->
          _menhir_run_057 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | IDENT _v ->
          _menhir_run_067 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
      | FLOAT _v ->
          _menhir_run_059 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
      | FALSE ->
          _menhir_run_060 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | DEC ->
          _menhir_run_061 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | BITNOT ->
          _menhir_run_062 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | RBRACE ->
          _menhir_reduce_071 _menhir_stack _menhir_lexbuf _menhir_lexer
      | _ ->
          _eRR ()
  
  and _menhir_run_047 : type  ttv_stack. ttv_stack -> _ -> _ -> (ttv_stack, _menhir_box_program) _menhir_state -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s ->
      let _startpos = _menhir_lexbuf.Lexing.lex_start_p in
      let _menhir_stack = MenhirCell1_LAMBDA (_menhir_stack, _menhir_s, _startpos) in
      let _tok = _menhir_lexer _menhir_lexbuf in
      match (_tok : MenhirBasics.token) with
      | IDENT _v ->
          _menhir_run_048 _menhir_stack _menhir_lexbuf _menhir_lexer _v MenhirState047
      | DARROW ->
          let _v = _menhir_action_077 () in
          _menhir_run_054 _menhir_stack _menhir_lexbuf _menhir_lexer _v MenhirState047 _tok
      | _ ->
          _eRR ()
  
  and _menhir_run_048 : type  ttv_stack. ttv_stack -> _ -> _ -> _ -> (ttv_stack, _menhir_box_program) _menhir_state -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s ->
      let _startpos = _menhir_lexbuf.Lexing.lex_start_p in
      let _menhir_stack = MenhirCell1_IDENT (_menhir_stack, _menhir_s, _v, _startpos) in
      let _tok = _menhir_lexer _menhir_lexbuf in
      match (_tok : MenhirBasics.token) with
      | ASSIGN ->
          _menhir_run_033 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState048
      | COMMA | DARROW | RPAREN ->
          let _v_0 = _menhir_action_093 () in
          _menhir_run_049 _menhir_stack _menhir_lexbuf _menhir_lexer _v_0 _tok
      | _ ->
          _eRR ()
  
  and _menhir_run_033 : type  ttv_stack. ((ttv_stack, _menhir_box_program) _menhir_cell1_IDENT as 'stack) -> _ -> _ -> ('stack, _menhir_box_program) _menhir_state -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s ->
      let _menhir_stack = MenhirCell1_ASSIGN (_menhir_stack, _menhir_s) in
      let _menhir_s = MenhirState033 in
      let _tok = _menhir_lexer _menhir_lexbuf in
      match (_tok : MenhirBasics.token) with
      | TRUE ->
          _menhir_run_034 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | SUPER ->
          _menhir_run_035 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | STRING _v ->
          _menhir_run_037 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
      | SELF ->
          _menhir_run_038 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | PLUS ->
          _menhir_run_039 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | NULL ->
          _menhir_run_040 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | NOT ->
          _menhir_run_044 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | NONE ->
          _menhir_run_041 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | MINUS ->
          _menhir_run_042 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | LPAREN ->
          _menhir_run_043 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | LBRACK ->
          _menhir_run_045 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | LBRACE ->
          _menhir_run_046 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | LAMBDA ->
          _menhir_run_047 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | INT _v ->
          _menhir_run_056 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
      | INC ->
          _menhir_run_057 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | IDENT _v ->
          _menhir_run_067 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
      | FLOAT _v ->
          _menhir_run_059 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
      | FALSE ->
          _menhir_run_060 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | DEC ->
          _menhir_run_061 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | BITNOT ->
          _menhir_run_062 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | _ ->
          _eRR ()
  
  and _menhir_run_056 : type  ttv_stack. ttv_stack -> _ -> _ -> _ -> (ttv_stack, _menhir_box_program) _menhir_state -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s ->
      let _startpos = _menhir_lexbuf.Lexing.lex_start_p in
      let _tok = _menhir_lexer _menhir_lexbuf in
      let (_startpos_n_, n) = (_startpos, _v) in
      let _v = _menhir_action_108 _startpos_n_ n in
      _menhir_goto_primary_expr _menhir_stack _menhir_lexbuf _menhir_lexer _startpos_n_ _v _menhir_s _tok
  
  and _menhir_run_057 : type  ttv_stack. ttv_stack -> _ -> _ -> (ttv_stack, _menhir_box_program) _menhir_state -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s ->
      let _startpos = _menhir_lexbuf.Lexing.lex_start_p in
      let _menhir_stack = MenhirCell1_INC (_menhir_stack, _menhir_s, _startpos) in
      let _menhir_s = MenhirState057 in
      let _tok = _menhir_lexer _menhir_lexbuf in
      match (_tok : MenhirBasics.token) with
      | TRUE ->
          _menhir_run_034 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | SUPER ->
          _menhir_run_035 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | STRING _v ->
          _menhir_run_037 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
      | SELF ->
          _menhir_run_038 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | PLUS ->
          _menhir_run_039 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | NULL ->
          _menhir_run_040 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | NONE ->
          _menhir_run_041 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | MINUS ->
          _menhir_run_042 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | LPAREN ->
          _menhir_run_043 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | LBRACK ->
          _menhir_run_045 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | LBRACE ->
          _menhir_run_046 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | INT _v ->
          _menhir_run_056 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
      | INC ->
          _menhir_run_057 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | IDENT _v ->
          _menhir_run_058 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
      | FLOAT _v ->
          _menhir_run_059 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
      | FALSE ->
          _menhir_run_060 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | DEC ->
          _menhir_run_061 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | BITNOT ->
          _menhir_run_062 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | _ ->
          _eRR ()
  
  and _menhir_run_058 : type  ttv_stack. ttv_stack -> _ -> _ -> _ -> (ttv_stack, _menhir_box_program) _menhir_state -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s ->
      let _startpos = _menhir_lexbuf.Lexing.lex_start_p in
      let _tok = _menhir_lexer _menhir_lexbuf in
      let (_startpos_n_, n) = (_startpos, _v) in
      let _v = _menhir_action_115 _startpos_n_ n in
      _menhir_goto_primary_expr _menhir_stack _menhir_lexbuf _menhir_lexer _startpos_n_ _v _menhir_s _tok
  
  and _menhir_run_059 : type  ttv_stack. ttv_stack -> _ -> _ -> _ -> (ttv_stack, _menhir_box_program) _menhir_state -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s ->
      let _startpos = _menhir_lexbuf.Lexing.lex_start_p in
      let _tok = _menhir_lexer _menhir_lexbuf in
      let (_startpos_f_, f) = (_startpos, _v) in
      let _v = _menhir_action_109 _startpos_f_ f in
      _menhir_goto_primary_expr _menhir_stack _menhir_lexbuf _menhir_lexer _startpos_f_ _v _menhir_s _tok
  
  and _menhir_run_060 : type  ttv_stack. ttv_stack -> _ -> _ -> (ttv_stack, _menhir_box_program) _menhir_state -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s ->
      let _startpos = _menhir_lexbuf.Lexing.lex_start_p in
      let _tok = _menhir_lexer _menhir_lexbuf in
      let _startpos__1_ = _startpos in
      let _v = _menhir_action_112 _startpos__1_ in
      _menhir_goto_primary_expr _menhir_stack _menhir_lexbuf _menhir_lexer _startpos__1_ _v _menhir_s _tok
  
  and _menhir_run_061 : type  ttv_stack. ttv_stack -> _ -> _ -> (ttv_stack, _menhir_box_program) _menhir_state -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s ->
      let _startpos = _menhir_lexbuf.Lexing.lex_start_p in
      let _menhir_stack = MenhirCell1_DEC (_menhir_stack, _menhir_s, _startpos) in
      let _menhir_s = MenhirState061 in
      let _tok = _menhir_lexer _menhir_lexbuf in
      match (_tok : MenhirBasics.token) with
      | TRUE ->
          _menhir_run_034 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | SUPER ->
          _menhir_run_035 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | STRING _v ->
          _menhir_run_037 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
      | SELF ->
          _menhir_run_038 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | PLUS ->
          _menhir_run_039 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | NULL ->
          _menhir_run_040 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | NONE ->
          _menhir_run_041 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | MINUS ->
          _menhir_run_042 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | LPAREN ->
          _menhir_run_043 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | LBRACK ->
          _menhir_run_045 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | LBRACE ->
          _menhir_run_046 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | INT _v ->
          _menhir_run_056 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
      | INC ->
          _menhir_run_057 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | IDENT _v ->
          _menhir_run_058 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
      | FLOAT _v ->
          _menhir_run_059 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
      | FALSE ->
          _menhir_run_060 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | DEC ->
          _menhir_run_061 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | BITNOT ->
          _menhir_run_062 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | _ ->
          _eRR ()
  
  and _menhir_run_062 : type  ttv_stack. ttv_stack -> _ -> _ -> (ttv_stack, _menhir_box_program) _menhir_state -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s ->
      let _startpos = _menhir_lexbuf.Lexing.lex_start_p in
      let _menhir_stack = MenhirCell1_BITNOT (_menhir_stack, _menhir_s, _startpos) in
      let _menhir_s = MenhirState062 in
      let _tok = _menhir_lexer _menhir_lexbuf in
      match (_tok : MenhirBasics.token) with
      | TRUE ->
          _menhir_run_034 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | SUPER ->
          _menhir_run_035 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | STRING _v ->
          _menhir_run_037 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
      | SELF ->
          _menhir_run_038 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | PLUS ->
          _menhir_run_039 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | NULL ->
          _menhir_run_040 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | NONE ->
          _menhir_run_041 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | MINUS ->
          _menhir_run_042 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | LPAREN ->
          _menhir_run_043 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | LBRACK ->
          _menhir_run_045 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | LBRACE ->
          _menhir_run_046 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | INT _v ->
          _menhir_run_056 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
      | INC ->
          _menhir_run_057 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | IDENT _v ->
          _menhir_run_058 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
      | FLOAT _v ->
          _menhir_run_059 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
      | FALSE ->
          _menhir_run_060 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | DEC ->
          _menhir_run_061 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | BITNOT ->
          _menhir_run_062 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | _ ->
          _eRR ()
  
  and _menhir_run_067 : type  ttv_stack. ttv_stack -> _ -> _ -> _ -> (ttv_stack, _menhir_box_program) _menhir_state -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s ->
      let _startpos = _menhir_lexbuf.Lexing.lex_start_p in
      let _tok = _menhir_lexer _menhir_lexbuf in
      match (_tok : MenhirBasics.token) with
      | WALRUS ->
          let _menhir_stack = MenhirCell1_IDENT (_menhir_stack, _menhir_s, _v, _startpos) in
          let _menhir_s = MenhirState068 in
          let _tok = _menhir_lexer _menhir_lexbuf in
          (match (_tok : MenhirBasics.token) with
          | TRUE ->
              _menhir_run_034 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | SUPER ->
              _menhir_run_035 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | STRING _v ->
              _menhir_run_037 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | SELF ->
              _menhir_run_038 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | PLUS ->
              _menhir_run_039 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | NULL ->
              _menhir_run_040 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | NOT ->
              _menhir_run_044 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | NONE ->
              _menhir_run_041 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | MINUS ->
              _menhir_run_042 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | LPAREN ->
              _menhir_run_043 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | LBRACK ->
              _menhir_run_045 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | LBRACE ->
              _menhir_run_046 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | INT _v ->
              _menhir_run_056 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | INC ->
              _menhir_run_057 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | IDENT _v ->
              _menhir_run_067 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | FLOAT _v ->
              _menhir_run_059 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | FALSE ->
              _menhir_run_060 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | DEC ->
              _menhir_run_061 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | BITNOT ->
              _menhir_run_062 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | _ ->
              _eRR ())
      | ADD_ASSIGN | AND | AS | ASSERT | ASSIGN | BITAND | BITNOT | BITOR | BITXOR | BREAK | COLON | COMMA | CONTINUE | DARROW | DEC | DEF | DEL | DIV_ASSIGN | DOT | ELSE | ENUM | EQEQ | FALSE | FLOAT _ | FLOORDIV | FLOORDIV_ASSIGN | FOR | FROM | GE | GLOBAL | GT | IDENT _ | IF | IMPORT | IN | INC | INT _ | IS | LAMBDA | LBRACE | LBRACK | LE | LPAREN | LSHIFT | LT | MINUS | MOD_ASSIGN | MUL_ASSIGN | NEQ | NONE | NONLOCAL | NOT | NULL | OR | PASS | PERCENT | PLUS | POW | POW_ASSIGN | RAISE | RBRACE | RBRACK | RETURN | RPAREN | RSHIFT | SELF | SEMI | SLASH | STAR | STRING _ | STRUCT | SUB_ASSIGN | SUPER | TRUE | TRY | WHILE | WITH | YIELD ->
          let (_startpos_n_, n) = (_startpos, _v) in
          let _v = _menhir_action_115 _startpos_n_ n in
          _menhir_goto_primary_expr _menhir_stack _menhir_lexbuf _menhir_lexer _startpos_n_ _v _menhir_s _tok
      | _ ->
          _eRR ()
  
  and _menhir_run_049 : type  ttv_stack. (ttv_stack, _menhir_box_program) _menhir_cell1_IDENT -> _ -> _ -> _ -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _v _tok ->
      let MenhirCell1_IDENT (_menhir_stack, _menhir_s, n, _startpos_n_) = _menhir_stack in
      let d = _v in
      let _v = _menhir_action_097 _startpos_n_ d n in
      match (_tok : MenhirBasics.token) with
      | COMMA ->
          let _menhir_stack = MenhirCell1_param (_menhir_stack, _menhir_s, _v) in
          let _menhir_s = MenhirState052 in
          let _tok = _menhir_lexer _menhir_lexbuf in
          (match (_tok : MenhirBasics.token) with
          | IDENT _v ->
              _menhir_run_048 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | _ ->
              _eRR ())
      | DARROW | RPAREN ->
          let x = _v in
          let _v = _menhir_action_133 x in
          _menhir_goto_separated_nonempty_list_COMMA_param_ _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s _tok
      | _ ->
          _eRR ()
  
  and _menhir_goto_separated_nonempty_list_COMMA_param_ : type  ttv_stack. ttv_stack -> _ -> _ -> _ -> (ttv_stack, _menhir_box_program) _menhir_state -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s _tok ->
      match _menhir_s with
      | MenhirState047 ->
          _menhir_run_050 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s _tok
      | MenhirState214 ->
          _menhir_run_050 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s _tok
      | MenhirState052 ->
          _menhir_run_053 _menhir_stack _menhir_lexbuf _menhir_lexer _v _tok
      | _ ->
          _menhir_fail ()
  
  and _menhir_run_050 : type  ttv_stack. ttv_stack -> _ -> _ -> _ -> (ttv_stack, _menhir_box_program) _menhir_state -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s _tok ->
      let x = _v in
      let _v = _menhir_action_078 x in
      _menhir_goto_loption_separated_nonempty_list_COMMA_param__ _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s _tok
  
  and _menhir_goto_loption_separated_nonempty_list_COMMA_param__ : type  ttv_stack. ttv_stack -> _ -> _ -> _ -> (ttv_stack, _menhir_box_program) _menhir_state -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s _tok ->
      match _menhir_s with
      | MenhirState047 ->
          _menhir_run_054 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s _tok
      | MenhirState214 ->
          _menhir_run_215 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s _tok
      | _ ->
          _menhir_fail ()
  
  and _menhir_run_054 : type  ttv_stack. ((ttv_stack, _menhir_box_program) _menhir_cell1_LAMBDA as 'stack) -> _ -> _ -> _ -> ('stack, _menhir_box_program) _menhir_state -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s _tok ->
      let _menhir_stack = MenhirCell1_loption_separated_nonempty_list_COMMA_param__ (_menhir_stack, _menhir_s, _v) in
      match (_tok : MenhirBasics.token) with
      | DARROW ->
          let _menhir_s = MenhirState055 in
          let _tok = _menhir_lexer _menhir_lexbuf in
          (match (_tok : MenhirBasics.token) with
          | TRUE ->
              _menhir_run_034 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | SUPER ->
              _menhir_run_035 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | STRING _v ->
              _menhir_run_037 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | SELF ->
              _menhir_run_038 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | PLUS ->
              _menhir_run_039 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | NULL ->
              _menhir_run_040 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | NOT ->
              _menhir_run_044 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | NONE ->
              _menhir_run_041 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | MINUS ->
              _menhir_run_042 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | LPAREN ->
              _menhir_run_043 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | LBRACK ->
              _menhir_run_045 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | LBRACE ->
              _menhir_run_046 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | LAMBDA ->
              _menhir_run_047 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | INT _v ->
              _menhir_run_056 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | INC ->
              _menhir_run_057 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | IDENT _v ->
              _menhir_run_067 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | FLOAT _v ->
              _menhir_run_059 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | FALSE ->
              _menhir_run_060 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | DEC ->
              _menhir_run_061 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | BITNOT ->
              _menhir_run_062 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | _ ->
              _eRR ())
      | _ ->
          _eRR ()
  
  and _menhir_run_215 : type  ttv_stack. ((ttv_stack, _menhir_box_program) _menhir_cell1_DEF _menhir_cell0_IDENT _menhir_cell0_LPAREN as 'stack) -> _ -> _ -> _ -> ('stack, _menhir_box_program) _menhir_state -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s _tok ->
      match (_tok : MenhirBasics.token) with
      | RPAREN ->
          let _tok = _menhir_lexer _menhir_lexbuf in
          (match (_tok : MenhirBasics.token) with
          | SEMI ->
              let _tok = _menhir_lexer _menhir_lexbuf in
              let MenhirCell0_LPAREN (_menhir_stack, _) = _menhir_stack in
              let MenhirCell0_IDENT (_menhir_stack, n, _) = _menhir_stack in
              let MenhirCell1_DEF (_menhir_stack, _menhir_s, _startpos__1_) = _menhir_stack in
              let xs = _v in
              let _v = _menhir_action_050 _startpos__1_ n xs in
              _menhir_goto_func_decl _menhir_stack _menhir_lexbuf _menhir_lexer _startpos__1_ _v _menhir_s _tok
          | LBRACE ->
              let _menhir_stack = MenhirCell1_loption_separated_nonempty_list_COMMA_param__ (_menhir_stack, _menhir_s, _v) in
              _menhir_run_218 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState216
          | _ ->
              _eRR ())
      | _ ->
          _eRR ()
  
  and _menhir_goto_func_decl : type  ttv_stack. ttv_stack -> _ -> _ -> _ -> _ -> (ttv_stack, _menhir_box_program) _menhir_state -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok ->
      match _menhir_s with
      | MenhirState218 ->
          _menhir_run_285 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState229 ->
          _menhir_run_285 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState233 ->
          _menhir_run_285 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState238 ->
          _menhir_run_285 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState254 ->
          _menhir_run_285 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState258 ->
          _menhir_run_285 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState263 ->
          _menhir_run_285 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState282 ->
          _menhir_run_285 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState294 ->
          _menhir_run_285 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState297 ->
          _menhir_run_285 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState302 ->
          _menhir_run_285 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState306 ->
          _menhir_run_285 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState313 ->
          _menhir_run_285 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState000 ->
          _menhir_run_332 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s _tok
      | MenhirState328 ->
          _menhir_run_332 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s _tok
      | _ ->
          _menhir_fail ()
  
  and _menhir_run_285 : type  ttv_stack. ttv_stack -> _ -> _ -> _ -> _ -> (ttv_stack, _menhir_box_program) _menhir_state -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok ->
      let (_startpos_d_, d) = (_startpos, _v) in
      let _v = _menhir_action_028 d in
      _menhir_goto_decl_stmt _menhir_stack _menhir_lexbuf _menhir_lexer _startpos_d_ _v _menhir_s _tok
  
  and _menhir_run_332 : type  ttv_stack. ttv_stack -> _ -> _ -> _ -> (ttv_stack, _menhir_box_program) _menhir_state -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s _tok ->
      let d = _v in
      let _v = _menhir_action_162 d in
      _menhir_goto_top_decl _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s _tok
  
  and _menhir_goto_top_decl : type  ttv_stack. ttv_stack -> _ -> _ -> _ -> (ttv_stack, _menhir_box_program) _menhir_state -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s _tok ->
      let _menhir_stack = MenhirCell1_top_decl (_menhir_stack, _menhir_s, _v) in
      match (_tok : MenhirBasics.token) with
      | STRUCT ->
          _menhir_run_001 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState328
      | IMPORT ->
          _menhir_run_012 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState328
      | FROM ->
          _menhir_run_018 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState328
      | ENUM ->
          _menhir_run_028 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState328
      | DEF ->
          _menhir_run_212 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState328
      | CLASS ->
          _menhir_run_322 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState328
      | EOF ->
          let _v_0 = _menhir_action_069 () in
          _menhir_run_330 _menhir_stack _v_0
      | _ ->
          _eRR ()
  
  and _menhir_run_012 : type  ttv_stack. ttv_stack -> _ -> _ -> (ttv_stack, _menhir_box_program) _menhir_state -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s ->
      let _startpos = _menhir_lexbuf.Lexing.lex_start_p in
      let _menhir_stack = MenhirCell1_IMPORT (_menhir_stack, _menhir_s, _startpos) in
      let _tok = _menhir_lexer _menhir_lexbuf in
      match (_tok : MenhirBasics.token) with
      | IDENT _v ->
          let _startpos_0 = _menhir_lexbuf.Lexing.lex_start_p in
          let _menhir_stack = MenhirCell0_IDENT (_menhir_stack, _v, _startpos_0) in
          let _tok = _menhir_lexer _menhir_lexbuf in
          (match (_tok : MenhirBasics.token) with
          | AS ->
              _menhir_run_014 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState013
          | SEMI ->
              let _v_1 = _menhir_action_091 () in
              _menhir_run_016 _menhir_stack _menhir_lexbuf _menhir_lexer _v_1 _tok
          | _ ->
              _eRR ())
      | _ ->
          _eRR ()
  
  and _menhir_run_014 : type  ttv_stack. ttv_stack -> _ -> _ -> (ttv_stack, _menhir_box_program) _menhir_state -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s ->
      let _tok = _menhir_lexer _menhir_lexbuf in
      match (_tok : MenhirBasics.token) with
      | IDENT _v ->
          let _tok = _menhir_lexer _menhir_lexbuf in
          let x = _v in
          let _v = _menhir_action_092 x in
          _menhir_goto_option_preceded_AS_IDENT__ _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s _tok
      | _ ->
          _eRR ()
  
  and _menhir_goto_option_preceded_AS_IDENT__ : type  ttv_stack. ttv_stack -> _ -> _ -> _ -> (ttv_stack, _menhir_box_program) _menhir_state -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s _tok ->
      match _menhir_s with
      | MenhirState013 ->
          _menhir_run_016 _menhir_stack _menhir_lexbuf _menhir_lexer _v _tok
      | MenhirState021 ->
          _menhir_run_022 _menhir_stack _menhir_lexbuf _menhir_lexer _v _tok
      | MenhirState227 ->
          _menhir_run_228 _menhir_stack _menhir_lexbuf _menhir_lexer _v _tok
      | MenhirState237 ->
          _menhir_run_238 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s _tok
      | _ ->
          _menhir_fail ()
  
  and _menhir_run_016 : type  ttv_stack. (ttv_stack, _menhir_box_program) _menhir_cell1_IMPORT _menhir_cell0_IDENT -> _ -> _ -> _ -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _v _tok ->
      match (_tok : MenhirBasics.token) with
      | SEMI ->
          let _tok = _menhir_lexer _menhir_lexbuf in
          let MenhirCell0_IDENT (_menhir_stack, m, _) = _menhir_stack in
          let MenhirCell1_IMPORT (_menhir_stack, _menhir_s, _startpos__1_) = _menhir_stack in
          let a = _v in
          let _v = _menhir_action_053 _startpos__1_ a m in
          _menhir_goto_import_decl _menhir_stack _menhir_lexbuf _menhir_lexer _startpos__1_ _v _menhir_s _tok
      | _ ->
          _eRR ()
  
  and _menhir_goto_import_decl : type  ttv_stack. ttv_stack -> _ -> _ -> _ -> _ -> (ttv_stack, _menhir_box_program) _menhir_state -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok ->
      match _menhir_s with
      | MenhirState218 ->
          _menhir_run_284 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState229 ->
          _menhir_run_284 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState233 ->
          _menhir_run_284 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState238 ->
          _menhir_run_284 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState254 ->
          _menhir_run_284 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState258 ->
          _menhir_run_284 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState263 ->
          _menhir_run_284 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState282 ->
          _menhir_run_284 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState294 ->
          _menhir_run_284 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState297 ->
          _menhir_run_284 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState302 ->
          _menhir_run_284 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState306 ->
          _menhir_run_284 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState313 ->
          _menhir_run_284 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState000 ->
          _menhir_run_331 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s _tok
      | MenhirState328 ->
          _menhir_run_331 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s _tok
      | _ ->
          _menhir_fail ()
  
  and _menhir_run_284 : type  ttv_stack. ttv_stack -> _ -> _ -> _ -> _ -> (ttv_stack, _menhir_box_program) _menhir_state -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok ->
      let (_startpos_d_, d) = (_startpos, _v) in
      let _v = _menhir_action_031 d in
      _menhir_goto_decl_stmt _menhir_stack _menhir_lexbuf _menhir_lexer _startpos_d_ _v _menhir_s _tok
  
  and _menhir_run_331 : type  ttv_stack. ttv_stack -> _ -> _ -> _ -> (ttv_stack, _menhir_box_program) _menhir_state -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s _tok ->
      let d = _v in
      let _v = _menhir_action_166 d in
      _menhir_goto_top_decl _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s _tok
  
  and _menhir_run_022 : type  ttv_stack. (ttv_stack, _menhir_box_program) _menhir_cell1_IDENT -> _ -> _ -> _ -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _v _tok ->
      let MenhirCell1_IDENT (_menhir_stack, _menhir_s, n, _) = _menhir_stack in
      let a = _v in
      let _v = _menhir_action_055 a n in
      match (_tok : MenhirBasics.token) with
      | COMMA ->
          let _menhir_stack = MenhirCell1_import_item (_menhir_stack, _menhir_s, _v) in
          let _menhir_s = MenhirState026 in
          let _tok = _menhir_lexer _menhir_lexbuf in
          (match (_tok : MenhirBasics.token) with
          | IDENT _v ->
              _menhir_run_021 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | _ ->
              _eRR ())
      | SEMI ->
          let x = _v in
          let _v = _menhir_action_131 x in
          _menhir_goto_separated_nonempty_list_COMMA_import_item_ _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
      | _ ->
          _eRR ()
  
  and _menhir_run_021 : type  ttv_stack. ttv_stack -> _ -> _ -> _ -> (ttv_stack, _menhir_box_program) _menhir_state -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s ->
      let _startpos = _menhir_lexbuf.Lexing.lex_start_p in
      let _menhir_stack = MenhirCell1_IDENT (_menhir_stack, _menhir_s, _v, _startpos) in
      let _tok = _menhir_lexer _menhir_lexbuf in
      match (_tok : MenhirBasics.token) with
      | AS ->
          _menhir_run_014 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState021
      | COMMA | SEMI ->
          let _v_0 = _menhir_action_091 () in
          _menhir_run_022 _menhir_stack _menhir_lexbuf _menhir_lexer _v_0 _tok
      | _ ->
          _eRR ()
  
  and _menhir_goto_separated_nonempty_list_COMMA_import_item_ : type  ttv_stack. ttv_stack -> _ -> _ -> _ -> (ttv_stack, _menhir_box_program) _menhir_state -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s ->
      match _menhir_s with
      | MenhirState020 ->
          _menhir_run_023 _menhir_stack _menhir_lexbuf _menhir_lexer _v
      | MenhirState026 ->
          _menhir_run_027 _menhir_stack _menhir_lexbuf _menhir_lexer _v
      | _ ->
          _menhir_fail ()
  
  and _menhir_run_023 : type  ttv_stack. (ttv_stack, _menhir_box_program) _menhir_cell1_FROM _menhir_cell0_IDENT _menhir_cell0_IMPORT -> _ -> _ -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _v ->
      let _tok = _menhir_lexer _menhir_lexbuf in
      let MenhirCell0_IMPORT (_menhir_stack, _) = _menhir_stack in
      let MenhirCell0_IDENT (_menhir_stack, m, _) = _menhir_stack in
      let MenhirCell1_FROM (_menhir_stack, _menhir_s, _startpos__1_) = _menhir_stack in
      let ns = _v in
      let _v = _menhir_action_054 _startpos__1_ m ns in
      _menhir_goto_import_decl _menhir_stack _menhir_lexbuf _menhir_lexer _startpos__1_ _v _menhir_s _tok
  
  and _menhir_run_027 : type  ttv_stack. (ttv_stack, _menhir_box_program) _menhir_cell1_import_item -> _ -> _ -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _v ->
      let MenhirCell1_import_item (_menhir_stack, _menhir_s, x) = _menhir_stack in
      let xs = _v in
      let _v = _menhir_action_132 x xs in
      _menhir_goto_separated_nonempty_list_COMMA_import_item_ _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
  
  and _menhir_run_228 : type  ttv_stack. (ttv_stack, _menhir_box_program) _menhir_cell1_expr -> _ -> _ -> _ -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _v _tok ->
      let MenhirCell1_expr (_menhir_stack, _menhir_s, e, _) = _menhir_stack in
      let a = _v in
      let _v = _menhir_action_175 a e in
      match (_tok : MenhirBasics.token) with
      | COMMA ->
          let _menhir_stack = MenhirCell1_with_item (_menhir_stack, _menhir_s, _v) in
          let _menhir_s = MenhirState225 in
          let _tok = _menhir_lexer _menhir_lexbuf in
          (match (_tok : MenhirBasics.token) with
          | TRUE ->
              _menhir_run_034 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | SUPER ->
              _menhir_run_035 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | STRING _v ->
              _menhir_run_037 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | SELF ->
              _menhir_run_038 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | PLUS ->
              _menhir_run_039 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | NULL ->
              _menhir_run_040 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | NOT ->
              _menhir_run_044 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | NONE ->
              _menhir_run_041 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | MINUS ->
              _menhir_run_042 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | LPAREN ->
              _menhir_run_043 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | LBRACK ->
              _menhir_run_045 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | LBRACE ->
              _menhir_run_046 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | LAMBDA ->
              _menhir_run_047 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | INT _v ->
              _menhir_run_056 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | INC ->
              _menhir_run_057 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | IDENT _v ->
              _menhir_run_067 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | FLOAT _v ->
              _menhir_run_059 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | FALSE ->
              _menhir_run_060 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | DEC ->
              _menhir_run_061 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | BITNOT ->
              _menhir_run_062 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | _ ->
              _eRR ())
      | ASSERT | BITNOT | BREAK | CONTINUE | DEC | DEF | DEL | ENUM | FALSE | FLOAT _ | FOR | FROM | GLOBAL | IDENT _ | IF | IMPORT | INC | INT _ | LAMBDA | LBRACE | LBRACK | LPAREN | MINUS | NONE | NONLOCAL | NOT | NULL | PASS | PLUS | RAISE | RETURN | SELF | STRING _ | STRUCT | SUPER | TRUE | TRY | WHILE | WITH | YIELD ->
          let x = _v in
          let _v = _menhir_action_135 x in
          _menhir_goto_separated_nonempty_list_COMMA_with_item_ _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s _tok
      | _ ->
          _eRR ()
  
  and _menhir_goto_separated_nonempty_list_COMMA_with_item_ : type  ttv_stack. ttv_stack -> _ -> _ -> _ -> (ttv_stack, _menhir_box_program) _menhir_state -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s _tok ->
      match _menhir_s with
      | MenhirState225 ->
          _menhir_run_226 _menhir_stack _menhir_lexbuf _menhir_lexer _v _tok
      | MenhirState223 ->
          _menhir_run_229 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s _tok
      | _ ->
          _menhir_fail ()
  
  and _menhir_run_226 : type  ttv_stack. (ttv_stack, _menhir_box_program) _menhir_cell1_with_item -> _ -> _ -> _ -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _v _tok ->
      let MenhirCell1_with_item (_menhir_stack, _menhir_s, x) = _menhir_stack in
      let xs = _v in
      let _v = _menhir_action_136 x xs in
      _menhir_goto_separated_nonempty_list_COMMA_with_item_ _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s _tok
  
  and _menhir_run_229 : type  ttv_stack. ((ttv_stack, _menhir_box_program) _menhir_cell1_WITH as 'stack) -> _ -> _ -> _ -> ('stack, _menhir_box_program) _menhir_state -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s _tok ->
      let _menhir_stack = MenhirCell1_separated_nonempty_list_COMMA_with_item_ (_menhir_stack, _menhir_s, _v) in
      match (_tok : MenhirBasics.token) with
      | YIELD ->
          _menhir_run_219 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState229
      | WITH ->
          _menhir_run_223 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState229
      | WHILE ->
          _menhir_run_230 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState229
      | TRY ->
          _menhir_run_234 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState229
      | TRUE ->
          _menhir_run_034 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState229
      | SUPER ->
          _menhir_run_035 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState229
      | STRUCT ->
          _menhir_run_001 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState229
      | STRING _v_0 ->
          _menhir_run_037 _menhir_stack _menhir_lexbuf _menhir_lexer _v_0 MenhirState229
      | SELF ->
          _menhir_run_038 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState229
      | RETURN ->
          _menhir_run_239 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState229
      | RAISE ->
          _menhir_run_243 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState229
      | PLUS ->
          _menhir_run_039 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState229
      | PASS ->
          _menhir_run_246 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState229
      | NULL ->
          _menhir_run_040 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState229
      | NOT ->
          _menhir_run_044 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState229
      | NONLOCAL ->
          _menhir_run_248 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState229
      | NONE ->
          _menhir_run_041 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState229
      | MINUS ->
          _menhir_run_042 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState229
      | LPAREN ->
          _menhir_run_043 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState229
      | LBRACK ->
          _menhir_run_045 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState229
      | LBRACE ->
          _menhir_run_254 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState229
      | LAMBDA ->
          _menhir_run_047 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState229
      | INT _v_1 ->
          _menhir_run_056 _menhir_stack _menhir_lexbuf _menhir_lexer _v_1 MenhirState229
      | INC ->
          _menhir_run_057 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState229
      | IMPORT ->
          _menhir_run_012 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState229
      | IF ->
          _menhir_run_255 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState229
      | IDENT _v_2 ->
          _menhir_run_067 _menhir_stack _menhir_lexbuf _menhir_lexer _v_2 MenhirState229
      | GLOBAL ->
          _menhir_run_259 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState229
      | FROM ->
          _menhir_run_018 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState229
      | FOR ->
          _menhir_run_262 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState229
      | FLOAT _v_3 ->
          _menhir_run_059 _menhir_stack _menhir_lexbuf _menhir_lexer _v_3 MenhirState229
      | FALSE ->
          _menhir_run_060 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState229
      | ENUM ->
          _menhir_run_028 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState229
      | DEL ->
          _menhir_run_264 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState229
      | DEF ->
          _menhir_run_212 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState229
      | DEC ->
          _menhir_run_061 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState229
      | CONTINUE ->
          _menhir_run_267 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState229
      | BREAK ->
          _menhir_run_269 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState229
      | BITNOT ->
          _menhir_run_062 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState229
      | ASSERT ->
          _menhir_run_271 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState229
      | _ ->
          _menhir_fail ()
  
  and _menhir_run_219 : type  ttv_stack. ttv_stack -> _ -> _ -> (ttv_stack, _menhir_box_program) _menhir_state -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s ->
      let _startpos = _menhir_lexbuf.Lexing.lex_start_p in
      let _tok = _menhir_lexer _menhir_lexbuf in
      match (_tok : MenhirBasics.token) with
      | TRUE ->
          let _menhir_stack = MenhirCell1_YIELD (_menhir_stack, _menhir_s, _startpos) in
          _menhir_run_034 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState219
      | SUPER ->
          let _menhir_stack = MenhirCell1_YIELD (_menhir_stack, _menhir_s, _startpos) in
          _menhir_run_035 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState219
      | STRING _v ->
          let _menhir_stack = MenhirCell1_YIELD (_menhir_stack, _menhir_s, _startpos) in
          _menhir_run_037 _menhir_stack _menhir_lexbuf _menhir_lexer _v MenhirState219
      | SEMI ->
          let _tok = _menhir_lexer _menhir_lexbuf in
          let _startpos__1_ = _startpos in
          let _v = _menhir_action_156 _startpos__1_ in
          _menhir_goto_stmt _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s _tok
      | SELF ->
          let _menhir_stack = MenhirCell1_YIELD (_menhir_stack, _menhir_s, _startpos) in
          _menhir_run_038 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState219
      | PLUS ->
          let _menhir_stack = MenhirCell1_YIELD (_menhir_stack, _menhir_s, _startpos) in
          _menhir_run_039 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState219
      | NULL ->
          let _menhir_stack = MenhirCell1_YIELD (_menhir_stack, _menhir_s, _startpos) in
          _menhir_run_040 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState219
      | NOT ->
          let _menhir_stack = MenhirCell1_YIELD (_menhir_stack, _menhir_s, _startpos) in
          _menhir_run_044 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState219
      | NONE ->
          let _menhir_stack = MenhirCell1_YIELD (_menhir_stack, _menhir_s, _startpos) in
          _menhir_run_041 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState219
      | MINUS ->
          let _menhir_stack = MenhirCell1_YIELD (_menhir_stack, _menhir_s, _startpos) in
          _menhir_run_042 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState219
      | LPAREN ->
          let _menhir_stack = MenhirCell1_YIELD (_menhir_stack, _menhir_s, _startpos) in
          _menhir_run_043 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState219
      | LBRACK ->
          let _menhir_stack = MenhirCell1_YIELD (_menhir_stack, _menhir_s, _startpos) in
          _menhir_run_045 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState219
      | LBRACE ->
          let _menhir_stack = MenhirCell1_YIELD (_menhir_stack, _menhir_s, _startpos) in
          _menhir_run_046 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState219
      | LAMBDA ->
          let _menhir_stack = MenhirCell1_YIELD (_menhir_stack, _menhir_s, _startpos) in
          _menhir_run_047 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState219
      | INT _v ->
          let _menhir_stack = MenhirCell1_YIELD (_menhir_stack, _menhir_s, _startpos) in
          _menhir_run_056 _menhir_stack _menhir_lexbuf _menhir_lexer _v MenhirState219
      | INC ->
          let _menhir_stack = MenhirCell1_YIELD (_menhir_stack, _menhir_s, _startpos) in
          _menhir_run_057 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState219
      | IDENT _v ->
          let _menhir_stack = MenhirCell1_YIELD (_menhir_stack, _menhir_s, _startpos) in
          _menhir_run_067 _menhir_stack _menhir_lexbuf _menhir_lexer _v MenhirState219
      | FLOAT _v ->
          let _menhir_stack = MenhirCell1_YIELD (_menhir_stack, _menhir_s, _startpos) in
          _menhir_run_059 _menhir_stack _menhir_lexbuf _menhir_lexer _v MenhirState219
      | FALSE ->
          let _menhir_stack = MenhirCell1_YIELD (_menhir_stack, _menhir_s, _startpos) in
          _menhir_run_060 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState219
      | DEC ->
          let _menhir_stack = MenhirCell1_YIELD (_menhir_stack, _menhir_s, _startpos) in
          _menhir_run_061 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState219
      | BITNOT ->
          let _menhir_stack = MenhirCell1_YIELD (_menhir_stack, _menhir_s, _startpos) in
          _menhir_run_062 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState219
      | _ ->
          _eRR ()
  
  and _menhir_run_223 : type  ttv_stack. ttv_stack -> _ -> _ -> (ttv_stack, _menhir_box_program) _menhir_state -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s ->
      let _startpos = _menhir_lexbuf.Lexing.lex_start_p in
      let _menhir_stack = MenhirCell1_WITH (_menhir_stack, _menhir_s, _startpos) in
      let _menhir_s = MenhirState223 in
      let _tok = _menhir_lexer _menhir_lexbuf in
      match (_tok : MenhirBasics.token) with
      | TRUE ->
          _menhir_run_034 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | SUPER ->
          _menhir_run_035 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | STRING _v ->
          _menhir_run_037 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
      | SELF ->
          _menhir_run_038 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | PLUS ->
          _menhir_run_039 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | NULL ->
          _menhir_run_040 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | NOT ->
          _menhir_run_044 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | NONE ->
          _menhir_run_041 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | MINUS ->
          _menhir_run_042 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | LPAREN ->
          _menhir_run_043 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | LBRACK ->
          _menhir_run_045 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | LBRACE ->
          _menhir_run_046 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | LAMBDA ->
          _menhir_run_047 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | INT _v ->
          _menhir_run_056 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
      | INC ->
          _menhir_run_057 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | IDENT _v ->
          _menhir_run_067 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
      | FLOAT _v ->
          _menhir_run_059 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
      | FALSE ->
          _menhir_run_060 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | DEC ->
          _menhir_run_061 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | BITNOT ->
          _menhir_run_062 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | _ ->
          _eRR ()
  
  and _menhir_run_230 : type  ttv_stack. ttv_stack -> _ -> _ -> (ttv_stack, _menhir_box_program) _menhir_state -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s ->
      let _startpos = _menhir_lexbuf.Lexing.lex_start_p in
      let _menhir_stack = MenhirCell1_WHILE (_menhir_stack, _menhir_s, _startpos) in
      let _tok = _menhir_lexer _menhir_lexbuf in
      match (_tok : MenhirBasics.token) with
      | LPAREN ->
          let _startpos = _menhir_lexbuf.Lexing.lex_start_p in
          let _menhir_stack = MenhirCell0_LPAREN (_menhir_stack, _startpos) in
          let _menhir_s = MenhirState231 in
          let _tok = _menhir_lexer _menhir_lexbuf in
          (match (_tok : MenhirBasics.token) with
          | TRUE ->
              _menhir_run_034 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | SUPER ->
              _menhir_run_035 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | STRING _v ->
              _menhir_run_037 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | SELF ->
              _menhir_run_038 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | PLUS ->
              _menhir_run_039 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | NULL ->
              _menhir_run_040 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | NOT ->
              _menhir_run_044 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | NONE ->
              _menhir_run_041 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | MINUS ->
              _menhir_run_042 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | LPAREN ->
              _menhir_run_043 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | LBRACK ->
              _menhir_run_045 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | LBRACE ->
              _menhir_run_046 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | LAMBDA ->
              _menhir_run_047 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | INT _v ->
              _menhir_run_056 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | INC ->
              _menhir_run_057 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | IDENT _v ->
              _menhir_run_067 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | FLOAT _v ->
              _menhir_run_059 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | FALSE ->
              _menhir_run_060 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | DEC ->
              _menhir_run_061 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | BITNOT ->
              _menhir_run_062 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | _ ->
              _eRR ())
      | _ ->
          _eRR ()
  
  and _menhir_run_234 : type  ttv_stack. ttv_stack -> _ -> _ -> (ttv_stack, _menhir_box_program) _menhir_state -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s ->
      let _startpos = _menhir_lexbuf.Lexing.lex_start_p in
      let _menhir_stack = MenhirCell1_TRY (_menhir_stack, _menhir_s, _startpos) in
      let _menhir_s = MenhirState234 in
      let _tok = _menhir_lexer _menhir_lexbuf in
      match (_tok : MenhirBasics.token) with
      | LBRACE ->
          _menhir_run_218 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | _ ->
          _eRR ()
  
  and _menhir_run_218 : type  ttv_stack. ttv_stack -> _ -> _ -> (ttv_stack, _menhir_box_program) _menhir_state -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s ->
      let _startpos = _menhir_lexbuf.Lexing.lex_start_p in
      let _menhir_stack = MenhirCell1_LBRACE (_menhir_stack, _menhir_s, _startpos) in
      let _tok = _menhir_lexer _menhir_lexbuf in
      match (_tok : MenhirBasics.token) with
      | YIELD ->
          _menhir_run_219 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState218
      | WITH ->
          _menhir_run_223 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState218
      | WHILE ->
          _menhir_run_230 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState218
      | TRY ->
          _menhir_run_234 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState218
      | TRUE ->
          _menhir_run_034 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState218
      | SUPER ->
          _menhir_run_035 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState218
      | STRUCT ->
          _menhir_run_001 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState218
      | STRING _v ->
          _menhir_run_037 _menhir_stack _menhir_lexbuf _menhir_lexer _v MenhirState218
      | SELF ->
          _menhir_run_038 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState218
      | RETURN ->
          _menhir_run_239 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState218
      | RAISE ->
          _menhir_run_243 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState218
      | PLUS ->
          _menhir_run_039 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState218
      | PASS ->
          _menhir_run_246 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState218
      | NULL ->
          _menhir_run_040 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState218
      | NOT ->
          _menhir_run_044 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState218
      | NONLOCAL ->
          _menhir_run_248 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState218
      | NONE ->
          _menhir_run_041 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState218
      | MINUS ->
          _menhir_run_042 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState218
      | LPAREN ->
          _menhir_run_043 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState218
      | LBRACK ->
          _menhir_run_045 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState218
      | LBRACE ->
          _menhir_run_254 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState218
      | LAMBDA ->
          _menhir_run_047 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState218
      | INT _v ->
          _menhir_run_056 _menhir_stack _menhir_lexbuf _menhir_lexer _v MenhirState218
      | INC ->
          _menhir_run_057 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState218
      | IMPORT ->
          _menhir_run_012 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState218
      | IF ->
          _menhir_run_255 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState218
      | IDENT _v ->
          _menhir_run_067 _menhir_stack _menhir_lexbuf _menhir_lexer _v MenhirState218
      | GLOBAL ->
          _menhir_run_259 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState218
      | FROM ->
          _menhir_run_018 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState218
      | FOR ->
          _menhir_run_262 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState218
      | FLOAT _v ->
          _menhir_run_059 _menhir_stack _menhir_lexbuf _menhir_lexer _v MenhirState218
      | FALSE ->
          _menhir_run_060 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState218
      | ENUM ->
          _menhir_run_028 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState218
      | DEL ->
          _menhir_run_264 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState218
      | DEF ->
          _menhir_run_212 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState218
      | DEC ->
          _menhir_run_061 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState218
      | CONTINUE ->
          _menhir_run_267 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState218
      | BREAK ->
          _menhir_run_269 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState218
      | BITNOT ->
          _menhir_run_062 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState218
      | ASSERT ->
          _menhir_run_271 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState218
      | RBRACE ->
          let _v = _menhir_action_067 () in
          _menhir_run_308 _menhir_stack _menhir_lexbuf _menhir_lexer _v
      | _ ->
          _eRR ()
  
  and _menhir_run_239 : type  ttv_stack. ttv_stack -> _ -> _ -> (ttv_stack, _menhir_box_program) _menhir_state -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s ->
      let _startpos = _menhir_lexbuf.Lexing.lex_start_p in
      let _menhir_stack = MenhirCell1_RETURN (_menhir_stack, _menhir_s, _startpos) in
      let _tok = _menhir_lexer _menhir_lexbuf in
      match (_tok : MenhirBasics.token) with
      | TRUE ->
          _menhir_run_034 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState239
      | SUPER ->
          _menhir_run_035 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState239
      | STRING _v ->
          _menhir_run_037 _menhir_stack _menhir_lexbuf _menhir_lexer _v MenhirState239
      | SELF ->
          _menhir_run_038 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState239
      | PLUS ->
          _menhir_run_039 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState239
      | NULL ->
          _menhir_run_040 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState239
      | NOT ->
          _menhir_run_044 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState239
      | NONE ->
          _menhir_run_041 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState239
      | MINUS ->
          _menhir_run_042 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState239
      | LPAREN ->
          _menhir_run_043 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState239
      | LBRACK ->
          _menhir_run_045 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState239
      | LBRACE ->
          _menhir_run_046 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState239
      | LAMBDA ->
          _menhir_run_047 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState239
      | INT _v ->
          _menhir_run_056 _menhir_stack _menhir_lexbuf _menhir_lexer _v MenhirState239
      | INC ->
          _menhir_run_057 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState239
      | IDENT _v ->
          _menhir_run_067 _menhir_stack _menhir_lexbuf _menhir_lexer _v MenhirState239
      | FLOAT _v ->
          _menhir_run_059 _menhir_stack _menhir_lexbuf _menhir_lexer _v MenhirState239
      | FALSE ->
          _menhir_run_060 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState239
      | DEC ->
          _menhir_run_061 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState239
      | BITNOT ->
          _menhir_run_062 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState239
      | SEMI ->
          let _v = _menhir_action_087 () in
          _menhir_run_240 _menhir_stack _menhir_lexbuf _menhir_lexer _v _tok
      | _ ->
          _eRR ()
  
  and _menhir_run_240 : type  ttv_stack. (ttv_stack, _menhir_box_program) _menhir_cell1_RETURN -> _ -> _ -> _ -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _v _tok ->
      match (_tok : MenhirBasics.token) with
      | SEMI ->
          let _tok = _menhir_lexer _menhir_lexbuf in
          let MenhirCell1_RETURN (_menhir_stack, _menhir_s, _startpos__1_) = _menhir_stack in
          let e = _v in
          let _v = _menhir_action_145 _startpos__1_ e in
          _menhir_goto_stmt _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s _tok
      | _ ->
          _eRR ()
  
  and _menhir_run_243 : type  ttv_stack. ttv_stack -> _ -> _ -> (ttv_stack, _menhir_box_program) _menhir_state -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s ->
      let _startpos = _menhir_lexbuf.Lexing.lex_start_p in
      let _menhir_stack = MenhirCell1_RAISE (_menhir_stack, _menhir_s, _startpos) in
      let _tok = _menhir_lexer _menhir_lexbuf in
      match (_tok : MenhirBasics.token) with
      | TRUE ->
          _menhir_run_034 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState243
      | SUPER ->
          _menhir_run_035 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState243
      | STRING _v ->
          _menhir_run_037 _menhir_stack _menhir_lexbuf _menhir_lexer _v MenhirState243
      | SELF ->
          _menhir_run_038 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState243
      | PLUS ->
          _menhir_run_039 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState243
      | NULL ->
          _menhir_run_040 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState243
      | NOT ->
          _menhir_run_044 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState243
      | NONE ->
          _menhir_run_041 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState243
      | MINUS ->
          _menhir_run_042 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState243
      | LPAREN ->
          _menhir_run_043 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState243
      | LBRACK ->
          _menhir_run_045 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState243
      | LBRACE ->
          _menhir_run_046 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState243
      | LAMBDA ->
          _menhir_run_047 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState243
      | INT _v ->
          _menhir_run_056 _menhir_stack _menhir_lexbuf _menhir_lexer _v MenhirState243
      | INC ->
          _menhir_run_057 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState243
      | IDENT _v ->
          _menhir_run_067 _menhir_stack _menhir_lexbuf _menhir_lexer _v MenhirState243
      | FLOAT _v ->
          _menhir_run_059 _menhir_stack _menhir_lexbuf _menhir_lexer _v MenhirState243
      | FALSE ->
          _menhir_run_060 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState243
      | DEC ->
          _menhir_run_061 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState243
      | BITNOT ->
          _menhir_run_062 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState243
      | SEMI ->
          let _v = _menhir_action_087 () in
          _menhir_run_244 _menhir_stack _menhir_lexbuf _menhir_lexer _v _tok
      | _ ->
          _eRR ()
  
  and _menhir_run_244 : type  ttv_stack. (ttv_stack, _menhir_box_program) _menhir_cell1_RAISE -> _ -> _ -> _ -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _v _tok ->
      match (_tok : MenhirBasics.token) with
      | SEMI ->
          let _tok = _menhir_lexer _menhir_lexbuf in
          let MenhirCell1_RAISE (_menhir_stack, _menhir_s, _startpos__1_) = _menhir_stack in
          let e = _v in
          let _v = _menhir_action_149 _startpos__1_ e in
          _menhir_goto_stmt _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s _tok
      | _ ->
          _eRR ()
  
  and _menhir_run_246 : type  ttv_stack. ttv_stack -> _ -> _ -> (ttv_stack, _menhir_box_program) _menhir_state -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s ->
      let _startpos = _menhir_lexbuf.Lexing.lex_start_p in
      let _tok = _menhir_lexer _menhir_lexbuf in
      match (_tok : MenhirBasics.token) with
      | SEMI ->
          let _tok = _menhir_lexer _menhir_lexbuf in
          let _startpos__1_ = _startpos in
          let _v = _menhir_action_032 _startpos__1_ in
          _menhir_goto_decl_stmt _menhir_stack _menhir_lexbuf _menhir_lexer _startpos__1_ _v _menhir_s _tok
      | _ ->
          _eRR ()
  
  and _menhir_run_248 : type  ttv_stack. ttv_stack -> _ -> _ -> (ttv_stack, _menhir_box_program) _menhir_state -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s ->
      let _startpos = _menhir_lexbuf.Lexing.lex_start_p in
      let _menhir_stack = MenhirCell1_NONLOCAL (_menhir_stack, _menhir_s, _startpos) in
      let _menhir_s = MenhirState248 in
      let _tok = _menhir_lexer _menhir_lexbuf in
      match (_tok : MenhirBasics.token) with
      | IDENT _v ->
          _menhir_run_249 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
      | _ ->
          _eRR ()
  
  and _menhir_run_249 : type  ttv_stack. ttv_stack -> _ -> _ -> _ -> (ttv_stack, _menhir_box_program) _menhir_state -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s ->
      let _startpos = _menhir_lexbuf.Lexing.lex_start_p in
      let _tok = _menhir_lexer _menhir_lexbuf in
      match (_tok : MenhirBasics.token) with
      | COMMA ->
          let _menhir_stack = MenhirCell1_IDENT (_menhir_stack, _menhir_s, _v, _startpos) in
          let _menhir_s = MenhirState250 in
          let _tok = _menhir_lexer _menhir_lexbuf in
          (match (_tok : MenhirBasics.token) with
          | IDENT _v ->
              _menhir_run_249 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | _ ->
              _eRR ())
      | SEMI ->
          let x = _v in
          let _v = _menhir_action_123 x in
          _menhir_goto_separated_nonempty_list_COMMA_IDENT_ _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
      | _ ->
          _eRR ()
  
  and _menhir_goto_separated_nonempty_list_COMMA_IDENT_ : type  ttv_stack. ttv_stack -> _ -> _ -> _ -> (ttv_stack, _menhir_box_program) _menhir_state -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s ->
      match _menhir_s with
      | MenhirState250 ->
          _menhir_run_251 _menhir_stack _menhir_lexbuf _menhir_lexer _v
      | MenhirState248 ->
          _menhir_run_252 _menhir_stack _menhir_lexbuf _menhir_lexer _v
      | MenhirState259 ->
          _menhir_run_260 _menhir_stack _menhir_lexbuf _menhir_lexer _v
      | _ ->
          _menhir_fail ()
  
  and _menhir_run_251 : type  ttv_stack. (ttv_stack, _menhir_box_program) _menhir_cell1_IDENT -> _ -> _ -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _v ->
      let MenhirCell1_IDENT (_menhir_stack, _menhir_s, x, _) = _menhir_stack in
      let xs = _v in
      let _v = _menhir_action_124 x xs in
      _menhir_goto_separated_nonempty_list_COMMA_IDENT_ _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
  
  and _menhir_run_252 : type  ttv_stack. (ttv_stack, _menhir_box_program) _menhir_cell1_NONLOCAL -> _ -> _ -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _v ->
      let _tok = _menhir_lexer _menhir_lexbuf in
      let MenhirCell1_NONLOCAL (_menhir_stack, _menhir_s, _startpos__1_) = _menhir_stack in
      let ns = _v in
      let _v = _menhir_action_152 _startpos__1_ ns in
      _menhir_goto_stmt _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s _tok
  
  and _menhir_run_260 : type  ttv_stack. (ttv_stack, _menhir_box_program) _menhir_cell1_GLOBAL -> _ -> _ -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _v ->
      let _tok = _menhir_lexer _menhir_lexbuf in
      let MenhirCell1_GLOBAL (_menhir_stack, _menhir_s, _startpos__1_) = _menhir_stack in
      let ns = _v in
      let _v = _menhir_action_151 _startpos__1_ ns in
      _menhir_goto_stmt _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s _tok
  
  and _menhir_run_254 : type  ttv_stack. ttv_stack -> _ -> _ -> (ttv_stack, _menhir_box_program) _menhir_state -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s ->
      let _startpos = _menhir_lexbuf.Lexing.lex_start_p in
      let _menhir_stack = MenhirCell1_LBRACE (_menhir_stack, _menhir_s, _startpos) in
      let _menhir_s = MenhirState254 in
      let _tok = _menhir_lexer _menhir_lexbuf in
      match (_tok : MenhirBasics.token) with
      | YIELD ->
          _menhir_run_219 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | WITH ->
          _menhir_run_223 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | WHILE ->
          _menhir_run_230 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | TRY ->
          _menhir_run_234 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | TRUE ->
          _menhir_run_034 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | SUPER ->
          _menhir_run_035 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | STRUCT ->
          _menhir_run_001 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | STRING _v ->
          _menhir_run_037 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
      | SELF ->
          _menhir_run_038 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | RETURN ->
          _menhir_run_239 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | RAISE ->
          _menhir_run_243 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | PLUS ->
          _menhir_run_039 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | PASS ->
          _menhir_run_246 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | NULL ->
          _menhir_run_040 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | NOT ->
          _menhir_run_044 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | NONLOCAL ->
          _menhir_run_248 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | NONE ->
          _menhir_run_041 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | MINUS ->
          _menhir_run_042 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | LPAREN ->
          _menhir_run_043 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | LBRACK ->
          _menhir_run_045 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | LBRACE ->
          _menhir_run_254 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | LAMBDA ->
          _menhir_run_047 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | INT _v ->
          _menhir_run_056 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
      | INC ->
          _menhir_run_057 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | IMPORT ->
          _menhir_run_012 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | IF ->
          _menhir_run_255 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | IDENT _v ->
          _menhir_run_067 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
      | GLOBAL ->
          _menhir_run_259 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | FROM ->
          _menhir_run_018 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | FOR ->
          _menhir_run_262 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | FLOAT _v ->
          _menhir_run_059 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
      | FALSE ->
          _menhir_run_060 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | ENUM ->
          _menhir_run_028 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | DEL ->
          _menhir_run_264 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | DEF ->
          _menhir_run_212 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | DEC ->
          _menhir_run_061 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | CONTINUE ->
          _menhir_run_267 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | BREAK ->
          _menhir_run_269 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | BITNOT ->
          _menhir_run_062 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | ASSERT ->
          _menhir_run_271 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | RBRACE ->
          _menhir_reduce_071 _menhir_stack _menhir_lexbuf _menhir_lexer
      | _ ->
          _eRR ()
  
  and _menhir_run_255 : type  ttv_stack. ttv_stack -> _ -> _ -> (ttv_stack, _menhir_box_program) _menhir_state -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s ->
      let _startpos = _menhir_lexbuf.Lexing.lex_start_p in
      let _menhir_stack = MenhirCell1_IF (_menhir_stack, _menhir_s, _startpos) in
      let _tok = _menhir_lexer _menhir_lexbuf in
      match (_tok : MenhirBasics.token) with
      | LPAREN ->
          let _startpos = _menhir_lexbuf.Lexing.lex_start_p in
          let _menhir_stack = MenhirCell0_LPAREN (_menhir_stack, _startpos) in
          let _menhir_s = MenhirState256 in
          let _tok = _menhir_lexer _menhir_lexbuf in
          (match (_tok : MenhirBasics.token) with
          | TRUE ->
              _menhir_run_034 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | SUPER ->
              _menhir_run_035 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | STRING _v ->
              _menhir_run_037 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | SELF ->
              _menhir_run_038 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | PLUS ->
              _menhir_run_039 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | NULL ->
              _menhir_run_040 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | NOT ->
              _menhir_run_044 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | NONE ->
              _menhir_run_041 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | MINUS ->
              _menhir_run_042 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | LPAREN ->
              _menhir_run_043 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | LBRACK ->
              _menhir_run_045 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | LBRACE ->
              _menhir_run_046 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | LAMBDA ->
              _menhir_run_047 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | INT _v ->
              _menhir_run_056 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | INC ->
              _menhir_run_057 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | IDENT _v ->
              _menhir_run_067 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | FLOAT _v ->
              _menhir_run_059 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | FALSE ->
              _menhir_run_060 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | DEC ->
              _menhir_run_061 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | BITNOT ->
              _menhir_run_062 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | _ ->
              _eRR ())
      | _ ->
          _eRR ()
  
  and _menhir_run_259 : type  ttv_stack. ttv_stack -> _ -> _ -> (ttv_stack, _menhir_box_program) _menhir_state -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s ->
      let _startpos = _menhir_lexbuf.Lexing.lex_start_p in
      let _menhir_stack = MenhirCell1_GLOBAL (_menhir_stack, _menhir_s, _startpos) in
      let _menhir_s = MenhirState259 in
      let _tok = _menhir_lexer _menhir_lexbuf in
      match (_tok : MenhirBasics.token) with
      | IDENT _v ->
          _menhir_run_249 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
      | _ ->
          _eRR ()
  
  and _menhir_run_018 : type  ttv_stack. ttv_stack -> _ -> _ -> (ttv_stack, _menhir_box_program) _menhir_state -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s ->
      let _startpos = _menhir_lexbuf.Lexing.lex_start_p in
      let _menhir_stack = MenhirCell1_FROM (_menhir_stack, _menhir_s, _startpos) in
      let _tok = _menhir_lexer _menhir_lexbuf in
      match (_tok : MenhirBasics.token) with
      | IDENT _v ->
          let _startpos = _menhir_lexbuf.Lexing.lex_start_p in
          let _menhir_stack = MenhirCell0_IDENT (_menhir_stack, _v, _startpos) in
          let _tok = _menhir_lexer _menhir_lexbuf in
          (match (_tok : MenhirBasics.token) with
          | IMPORT ->
              let _startpos = _menhir_lexbuf.Lexing.lex_start_p in
              let _menhir_stack = MenhirCell0_IMPORT (_menhir_stack, _startpos) in
              let _menhir_s = MenhirState020 in
              let _tok = _menhir_lexer _menhir_lexbuf in
              (match (_tok : MenhirBasics.token) with
              | IDENT _v ->
                  _menhir_run_021 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
              | _ ->
                  _eRR ())
          | _ ->
              _eRR ())
      | _ ->
          _eRR ()
  
  and _menhir_run_262 : type  ttv_stack. ttv_stack -> _ -> _ -> (ttv_stack, _menhir_box_program) _menhir_state -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s ->
      let _startpos = _menhir_lexbuf.Lexing.lex_start_p in
      let _menhir_stack = MenhirCell1_FOR (_menhir_stack, _menhir_s, _startpos) in
      let _tok = _menhir_lexer _menhir_lexbuf in
      match (_tok : MenhirBasics.token) with
      | LPAREN ->
          let _startpos = _menhir_lexbuf.Lexing.lex_start_p in
          let _menhir_stack = MenhirCell0_LPAREN (_menhir_stack, _startpos) in
          let _menhir_s = MenhirState263 in
          let _tok = _menhir_lexer _menhir_lexbuf in
          (match (_tok : MenhirBasics.token) with
          | YIELD ->
              _menhir_run_219 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | WITH ->
              _menhir_run_223 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | WHILE ->
              _menhir_run_230 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | TRY ->
              _menhir_run_234 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | TRUE ->
              _menhir_run_034 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | SUPER ->
              _menhir_run_035 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | STRUCT ->
              _menhir_run_001 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | STRING _v ->
              _menhir_run_037 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | SELF ->
              _menhir_run_038 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | RETURN ->
              _menhir_run_239 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | RAISE ->
              _menhir_run_243 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | PLUS ->
              _menhir_run_039 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | PASS ->
              _menhir_run_246 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | NULL ->
              _menhir_run_040 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | NOT ->
              _menhir_run_044 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | NONLOCAL ->
              _menhir_run_248 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | NONE ->
              _menhir_run_041 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | MINUS ->
              _menhir_run_042 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | LPAREN ->
              _menhir_run_043 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | LBRACK ->
              _menhir_run_045 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | LBRACE ->
              _menhir_run_254 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | LAMBDA ->
              _menhir_run_047 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | INT _v ->
              _menhir_run_056 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | INC ->
              _menhir_run_057 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | IMPORT ->
              _menhir_run_012 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | IF ->
              _menhir_run_255 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | IDENT _v ->
              _menhir_run_067 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | GLOBAL ->
              _menhir_run_259 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | FROM ->
              _menhir_run_018 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | FOR ->
              _menhir_run_262 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | FLOAT _v ->
              _menhir_run_059 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | FALSE ->
              _menhir_run_060 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | ENUM ->
              _menhir_run_028 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | DEL ->
              _menhir_run_264 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | DEF ->
              _menhir_run_212 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | DEC ->
              _menhir_run_061 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | CONTINUE ->
              _menhir_run_267 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | BREAK ->
              _menhir_run_269 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | BITNOT ->
              _menhir_run_062 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | ASSERT ->
              _menhir_run_271 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | _ ->
              _eRR ())
      | _ ->
          _eRR ()
  
  and _menhir_run_028 : type  ttv_stack. ttv_stack -> _ -> _ -> (ttv_stack, _menhir_box_program) _menhir_state -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s ->
      let _startpos = _menhir_lexbuf.Lexing.lex_start_p in
      let _tok = _menhir_lexer _menhir_lexbuf in
      match (_tok : MenhirBasics.token) with
      | IDENT _v ->
          let _startpos_0 = _menhir_lexbuf.Lexing.lex_start_p in
          let _tok = _menhir_lexer _menhir_lexbuf in
          (match (_tok : MenhirBasics.token) with
          | SEMI ->
              let _tok = _menhir_lexer _menhir_lexbuf in
              let (n, _startpos__1_) = (_v, _startpos) in
              let _v = _menhir_action_038 _startpos__1_ n in
              _menhir_goto_enum_decl _menhir_stack _menhir_lexbuf _menhir_lexer _startpos__1_ _v _menhir_s _tok
          | LBRACE ->
              let _menhir_stack = MenhirCell1_ENUM (_menhir_stack, _menhir_s, _startpos) in
              let _menhir_stack = MenhirCell0_IDENT (_menhir_stack, _v, _startpos_0) in
              let _startpos = _menhir_lexbuf.Lexing.lex_start_p in
              let _menhir_stack = MenhirCell0_LBRACE (_menhir_stack, _startpos) in
              let _menhir_s = MenhirState031 in
              let _tok = _menhir_lexer _menhir_lexbuf in
              (match (_tok : MenhirBasics.token) with
              | IDENT _v ->
                  _menhir_run_032 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
              | RBRACE ->
                  let _v = _menhir_action_073 () in
                  _menhir_goto_loption_separated_nonempty_list_COMMA_enum_item__ _menhir_stack _menhir_lexbuf _menhir_lexer _v
              | _ ->
                  _eRR ())
          | _ ->
              _eRR ())
      | _ ->
          _eRR ()
  
  and _menhir_goto_enum_decl : type  ttv_stack. ttv_stack -> _ -> _ -> _ -> _ -> (ttv_stack, _menhir_box_program) _menhir_state -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok ->
      match _menhir_s with
      | MenhirState218 ->
          _menhir_run_288 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState229 ->
          _menhir_run_288 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState233 ->
          _menhir_run_288 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState238 ->
          _menhir_run_288 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState254 ->
          _menhir_run_288 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState258 ->
          _menhir_run_288 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState263 ->
          _menhir_run_288 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState282 ->
          _menhir_run_288 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState294 ->
          _menhir_run_288 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState297 ->
          _menhir_run_288 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState302 ->
          _menhir_run_288 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState306 ->
          _menhir_run_288 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState313 ->
          _menhir_run_288 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState000 ->
          _menhir_run_333 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s _tok
      | MenhirState328 ->
          _menhir_run_333 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s _tok
      | _ ->
          _menhir_fail ()
  
  and _menhir_run_288 : type  ttv_stack. ttv_stack -> _ -> _ -> _ -> _ -> (ttv_stack, _menhir_box_program) _menhir_state -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok ->
      let (_startpos_d_, d) = (_startpos, _v) in
      let _v = _menhir_action_030 d in
      _menhir_goto_decl_stmt _menhir_stack _menhir_lexbuf _menhir_lexer _startpos_d_ _v _menhir_s _tok
  
  and _menhir_run_333 : type  ttv_stack. ttv_stack -> _ -> _ -> _ -> (ttv_stack, _menhir_box_program) _menhir_state -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s _tok ->
      let d = _v in
      let _v = _menhir_action_165 d in
      _menhir_goto_top_decl _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s _tok
  
  and _menhir_run_032 : type  ttv_stack. ttv_stack -> _ -> _ -> _ -> (ttv_stack, _menhir_box_program) _menhir_state -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s ->
      let _startpos = _menhir_lexbuf.Lexing.lex_start_p in
      let _menhir_stack = MenhirCell1_IDENT (_menhir_stack, _menhir_s, _v, _startpos) in
      let _tok = _menhir_lexer _menhir_lexbuf in
      match (_tok : MenhirBasics.token) with
      | ASSIGN ->
          _menhir_run_033 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState032
      | COMMA | RBRACE ->
          let _v_0 = _menhir_action_093 () in
          _menhir_run_204 _menhir_stack _menhir_lexbuf _menhir_lexer _v_0 _tok
      | _ ->
          _eRR ()
  
  and _menhir_run_204 : type  ttv_stack. (ttv_stack, _menhir_box_program) _menhir_cell1_IDENT -> _ -> _ -> _ -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _v _tok ->
      let MenhirCell1_IDENT (_menhir_stack, _menhir_s, n, _) = _menhir_stack in
      let v = _v in
      let _v = _menhir_action_039 n v in
      match (_tok : MenhirBasics.token) with
      | COMMA ->
          let _menhir_stack = MenhirCell1_enum_item (_menhir_stack, _menhir_s, _v) in
          let _menhir_s = MenhirState210 in
          let _tok = _menhir_lexer _menhir_lexbuf in
          (match (_tok : MenhirBasics.token) with
          | IDENT _v ->
              _menhir_run_032 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | _ ->
              _eRR ())
      | RBRACE ->
          let x = _v in
          let _v = _menhir_action_127 x in
          _menhir_goto_separated_nonempty_list_COMMA_enum_item_ _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
      | _ ->
          _eRR ()
  
  and _menhir_goto_separated_nonempty_list_COMMA_enum_item_ : type  ttv_stack. ttv_stack -> _ -> _ -> _ -> (ttv_stack, _menhir_box_program) _menhir_state -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s ->
      match _menhir_s with
      | MenhirState031 ->
          _menhir_run_205 _menhir_stack _menhir_lexbuf _menhir_lexer _v
      | MenhirState210 ->
          _menhir_run_211 _menhir_stack _menhir_lexbuf _menhir_lexer _v
      | _ ->
          _menhir_fail ()
  
  and _menhir_run_205 : type  ttv_stack. (ttv_stack, _menhir_box_program) _menhir_cell1_ENUM _menhir_cell0_IDENT _menhir_cell0_LBRACE -> _ -> _ -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _v ->
      let x = _v in
      let _v = _menhir_action_074 x in
      _menhir_goto_loption_separated_nonempty_list_COMMA_enum_item__ _menhir_stack _menhir_lexbuf _menhir_lexer _v
  
  and _menhir_goto_loption_separated_nonempty_list_COMMA_enum_item__ : type  ttv_stack. (ttv_stack, _menhir_box_program) _menhir_cell1_ENUM _menhir_cell0_IDENT _menhir_cell0_LBRACE -> _ -> _ -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _v ->
      let _tok = _menhir_lexer _menhir_lexbuf in
      match (_tok : MenhirBasics.token) with
      | SEMI ->
          let _tok = _menhir_lexer _menhir_lexbuf in
          let MenhirCell0_LBRACE (_menhir_stack, _) = _menhir_stack in
          let MenhirCell0_IDENT (_menhir_stack, n, _) = _menhir_stack in
          let MenhirCell1_ENUM (_menhir_stack, _menhir_s, _startpos__1_) = _menhir_stack in
          let xs = _v in
          let _v = _menhir_action_037 _startpos__1_ n xs in
          _menhir_goto_enum_decl _menhir_stack _menhir_lexbuf _menhir_lexer _startpos__1_ _v _menhir_s _tok
      | _ ->
          _eRR ()
  
  and _menhir_run_211 : type  ttv_stack. (ttv_stack, _menhir_box_program) _menhir_cell1_enum_item -> _ -> _ -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _v ->
      let MenhirCell1_enum_item (_menhir_stack, _menhir_s, x) = _menhir_stack in
      let xs = _v in
      let _v = _menhir_action_128 x xs in
      _menhir_goto_separated_nonempty_list_COMMA_enum_item_ _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
  
  and _menhir_run_264 : type  ttv_stack. ttv_stack -> _ -> _ -> (ttv_stack, _menhir_box_program) _menhir_state -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s ->
      let _startpos = _menhir_lexbuf.Lexing.lex_start_p in
      let _menhir_stack = MenhirCell1_DEL (_menhir_stack, _menhir_s, _startpos) in
      let _menhir_s = MenhirState264 in
      let _tok = _menhir_lexer _menhir_lexbuf in
      match (_tok : MenhirBasics.token) with
      | TRUE ->
          _menhir_run_034 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | SUPER ->
          _menhir_run_035 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | STRING _v ->
          _menhir_run_037 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
      | SELF ->
          _menhir_run_038 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | PLUS ->
          _menhir_run_039 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | NULL ->
          _menhir_run_040 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | NOT ->
          _menhir_run_044 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | NONE ->
          _menhir_run_041 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | MINUS ->
          _menhir_run_042 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | LPAREN ->
          _menhir_run_043 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | LBRACK ->
          _menhir_run_045 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | LBRACE ->
          _menhir_run_046 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | LAMBDA ->
          _menhir_run_047 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | INT _v ->
          _menhir_run_056 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
      | INC ->
          _menhir_run_057 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | IDENT _v ->
          _menhir_run_067 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
      | FLOAT _v ->
          _menhir_run_059 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
      | FALSE ->
          _menhir_run_060 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | DEC ->
          _menhir_run_061 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | BITNOT ->
          _menhir_run_062 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | _ ->
          _eRR ()
  
  and _menhir_run_212 : type  ttv_stack. ttv_stack -> _ -> _ -> (ttv_stack, _menhir_box_program) _menhir_state -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s ->
      let _startpos = _menhir_lexbuf.Lexing.lex_start_p in
      let _menhir_stack = MenhirCell1_DEF (_menhir_stack, _menhir_s, _startpos) in
      let _tok = _menhir_lexer _menhir_lexbuf in
      match (_tok : MenhirBasics.token) with
      | IDENT _v ->
          let _startpos_0 = _menhir_lexbuf.Lexing.lex_start_p in
          let _menhir_stack = MenhirCell0_IDENT (_menhir_stack, _v, _startpos_0) in
          let _tok = _menhir_lexer _menhir_lexbuf in
          (match (_tok : MenhirBasics.token) with
          | LPAREN ->
              let _startpos_1 = _menhir_lexbuf.Lexing.lex_start_p in
              let _menhir_stack = MenhirCell0_LPAREN (_menhir_stack, _startpos_1) in
              let _tok = _menhir_lexer _menhir_lexbuf in
              (match (_tok : MenhirBasics.token) with
              | IDENT _v_2 ->
                  _menhir_run_048 _menhir_stack _menhir_lexbuf _menhir_lexer _v_2 MenhirState214
              | RPAREN ->
                  let _v_3 = _menhir_action_077 () in
                  _menhir_run_215 _menhir_stack _menhir_lexbuf _menhir_lexer _v_3 MenhirState214 _tok
              | _ ->
                  _eRR ())
          | _ ->
              _eRR ())
      | _ ->
          _eRR ()
  
  and _menhir_run_267 : type  ttv_stack. ttv_stack -> _ -> _ -> (ttv_stack, _menhir_box_program) _menhir_state -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s ->
      let _startpos = _menhir_lexbuf.Lexing.lex_start_p in
      let _tok = _menhir_lexer _menhir_lexbuf in
      match (_tok : MenhirBasics.token) with
      | SEMI ->
          let _tok = _menhir_lexer _menhir_lexbuf in
          let _startpos__1_ = _startpos in
          let _v = _menhir_action_147 _startpos__1_ in
          _menhir_goto_stmt _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s _tok
      | _ ->
          _eRR ()
  
  and _menhir_run_269 : type  ttv_stack. ttv_stack -> _ -> _ -> (ttv_stack, _menhir_box_program) _menhir_state -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s ->
      let _startpos = _menhir_lexbuf.Lexing.lex_start_p in
      let _tok = _menhir_lexer _menhir_lexbuf in
      match (_tok : MenhirBasics.token) with
      | SEMI ->
          let _tok = _menhir_lexer _menhir_lexbuf in
          let _startpos__1_ = _startpos in
          let _v = _menhir_action_146 _startpos__1_ in
          _menhir_goto_stmt _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s _tok
      | _ ->
          _eRR ()
  
  and _menhir_run_271 : type  ttv_stack. ttv_stack -> _ -> _ -> (ttv_stack, _menhir_box_program) _menhir_state -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s ->
      let _startpos = _menhir_lexbuf.Lexing.lex_start_p in
      let _menhir_stack = MenhirCell1_ASSERT (_menhir_stack, _menhir_s, _startpos) in
      let _menhir_s = MenhirState271 in
      let _tok = _menhir_lexer _menhir_lexbuf in
      match (_tok : MenhirBasics.token) with
      | TRUE ->
          _menhir_run_034 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | SUPER ->
          _menhir_run_035 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | STRING _v ->
          _menhir_run_037 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
      | SELF ->
          _menhir_run_038 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | PLUS ->
          _menhir_run_039 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | NULL ->
          _menhir_run_040 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | NOT ->
          _menhir_run_044 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | NONE ->
          _menhir_run_041 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | MINUS ->
          _menhir_run_042 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | LPAREN ->
          _menhir_run_043 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | LBRACK ->
          _menhir_run_045 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | LBRACE ->
          _menhir_run_046 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | LAMBDA ->
          _menhir_run_047 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | INT _v ->
          _menhir_run_056 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
      | INC ->
          _menhir_run_057 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | IDENT _v ->
          _menhir_run_067 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
      | FLOAT _v ->
          _menhir_run_059 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
      | FALSE ->
          _menhir_run_060 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | DEC ->
          _menhir_run_061 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | BITNOT ->
          _menhir_run_062 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | _ ->
          _eRR ()
  
  and _menhir_reduce_071 : type  ttv_stack. (ttv_stack, _menhir_box_program) _menhir_cell1_LBRACE -> _ -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer ->
      let _v = _menhir_action_071 () in
      _menhir_goto_loption_separated_nonempty_list_COMMA_dict_item__ _menhir_stack _menhir_lexbuf _menhir_lexer _v
  
  and _menhir_goto_loption_separated_nonempty_list_COMMA_dict_item__ : type  ttv_stack. (ttv_stack, _menhir_box_program) _menhir_cell1_LBRACE -> _ -> _ -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _v ->
      let _tok = _menhir_lexer _menhir_lexbuf in
      let MenhirCell1_LBRACE (_menhir_stack, _menhir_s, _startpos__1_) = _menhir_stack in
      let xs = _v in
      let _v = _menhir_action_120 _startpos__1_ xs in
      _menhir_goto_primary_expr _menhir_stack _menhir_lexbuf _menhir_lexer _startpos__1_ _v _menhir_s _tok
  
  and _menhir_run_308 : type  ttv_stack. (ttv_stack, _menhir_box_program) _menhir_cell1_LBRACE -> _ -> _ -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _v ->
      let _tok = _menhir_lexer _menhir_lexbuf in
      let MenhirCell1_LBRACE (_menhir_stack, _menhir_s, _startpos__1_) = _menhir_stack in
      let stmts = _v in
      let _v = _menhir_action_021 _startpos__1_ stmts in
      _menhir_goto_block _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s _tok
  
  and _menhir_goto_block : type  ttv_stack. ttv_stack -> _ -> _ -> _ -> (ttv_stack, _menhir_box_program) _menhir_state -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s _tok ->
      match _menhir_s with
      | MenhirState234 ->
          _menhir_run_235 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s _tok
      | MenhirState218 ->
          _menhir_run_290 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s _tok
      | MenhirState229 ->
          _menhir_run_290 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s _tok
      | MenhirState233 ->
          _menhir_run_290 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s _tok
      | MenhirState238 ->
          _menhir_run_290 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s _tok
      | MenhirState254 ->
          _menhir_run_290 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s _tok
      | MenhirState258 ->
          _menhir_run_290 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s _tok
      | MenhirState263 ->
          _menhir_run_290 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s _tok
      | MenhirState282 ->
          _menhir_run_290 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s _tok
      | MenhirState294 ->
          _menhir_run_290 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s _tok
      | MenhirState297 ->
          _menhir_run_290 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s _tok
      | MenhirState302 ->
          _menhir_run_290 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s _tok
      | MenhirState306 ->
          _menhir_run_290 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s _tok
      | MenhirState313 ->
          _menhir_run_290 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s _tok
      | MenhirState216 ->
          _menhir_run_321 _menhir_stack _menhir_lexbuf _menhir_lexer _v _tok
      | _ ->
          _menhir_fail ()
  
  and _menhir_run_235 : type  ttv_stack. ((ttv_stack, _menhir_box_program) _menhir_cell1_TRY as 'stack) -> _ -> _ -> _ -> ('stack, _menhir_box_program) _menhir_state -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s _tok ->
      let _menhir_stack = MenhirCell1_block (_menhir_stack, _menhir_s, _v) in
      match (_tok : MenhirBasics.token) with
      | EXCEPT ->
          _menhir_run_236 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState235
      | ASSERT | BITNOT | BREAK | CONTINUE | DEC | DEF | DEL | ELIF | ELSE | ENUM | FALSE | FINALLY | FLOAT _ | FOR | FROM | GLOBAL | IDENT _ | IF | IMPORT | INC | INT _ | LAMBDA | LBRACE | LBRACK | LPAREN | MINUS | NONE | NONLOCAL | NOT | NULL | PASS | PLUS | RAISE | RBRACE | RETURN | SELF | STRING _ | STRUCT | SUPER | TRUE | TRY | WHILE | WITH | YIELD ->
          let _v_0 = _menhir_action_063 () in
          _menhir_run_312 _menhir_stack _menhir_lexbuf _menhir_lexer _v_0 MenhirState235 _tok
      | _ ->
          _eRR ()
  
  and _menhir_run_236 : type  ttv_stack. ttv_stack -> _ -> _ -> (ttv_stack, _menhir_box_program) _menhir_state -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s ->
      let _menhir_stack = MenhirCell1_EXCEPT (_menhir_stack, _menhir_s) in
      let _tok = _menhir_lexer _menhir_lexbuf in
      match (_tok : MenhirBasics.token) with
      | TRUE ->
          _menhir_run_034 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState236
      | SUPER ->
          _menhir_run_035 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState236
      | STRING _v ->
          _menhir_run_037 _menhir_stack _menhir_lexbuf _menhir_lexer _v MenhirState236
      | SELF ->
          _menhir_run_038 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState236
      | PLUS ->
          _menhir_run_039 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState236
      | NULL ->
          _menhir_run_040 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState236
      | NOT ->
          _menhir_run_044 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState236
      | NONE ->
          _menhir_run_041 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState236
      | MINUS ->
          _menhir_run_042 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState236
      | LPAREN ->
          _menhir_run_043 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState236
      | LBRACK ->
          _menhir_run_045 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState236
      | LBRACE ->
          _menhir_run_046 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState236
      | LAMBDA ->
          _menhir_run_047 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState236
      | INT _v ->
          _menhir_run_056 _menhir_stack _menhir_lexbuf _menhir_lexer _v MenhirState236
      | INC ->
          _menhir_run_057 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState236
      | IDENT _v ->
          _menhir_run_067 _menhir_stack _menhir_lexbuf _menhir_lexer _v MenhirState236
      | FLOAT _v ->
          _menhir_run_059 _menhir_stack _menhir_lexbuf _menhir_lexer _v MenhirState236
      | FALSE ->
          _menhir_run_060 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState236
      | DEC ->
          _menhir_run_061 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState236
      | BITNOT ->
          _menhir_run_062 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState236
      | AS | ASSERT | BREAK | CONTINUE | DEF | DEL | ENUM | FOR | FROM | GLOBAL | IF | IMPORT | NONLOCAL | PASS | RAISE | RETURN | STRUCT | TRY | WHILE | WITH | YIELD ->
          let _v = _menhir_action_087 () in
          _menhir_run_237 _menhir_stack _menhir_lexbuf _menhir_lexer _v MenhirState236 _tok
      | _ ->
          _eRR ()
  
  and _menhir_run_237 : type  ttv_stack. ((ttv_stack, _menhir_box_program) _menhir_cell1_EXCEPT as 'stack) -> _ -> _ -> _ -> ('stack, _menhir_box_program) _menhir_state -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s _tok ->
      let _menhir_stack = MenhirCell1_option_expr_ (_menhir_stack, _menhir_s, _v) in
      match (_tok : MenhirBasics.token) with
      | AS ->
          _menhir_run_014 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState237
      | ASSERT | BITNOT | BREAK | CONTINUE | DEC | DEF | DEL | ENUM | FALSE | FLOAT _ | FOR | FROM | GLOBAL | IDENT _ | IF | IMPORT | INC | INT _ | LAMBDA | LBRACE | LBRACK | LPAREN | MINUS | NONE | NONLOCAL | NOT | NULL | PASS | PLUS | RAISE | RETURN | SELF | STRING _ | STRUCT | SUPER | TRUE | TRY | WHILE | WITH | YIELD ->
          let _v_0 = _menhir_action_091 () in
          _menhir_run_238 _menhir_stack _menhir_lexbuf _menhir_lexer _v_0 MenhirState237 _tok
      | _ ->
          _eRR ()
  
  and _menhir_run_238 : type  ttv_stack. (((ttv_stack, _menhir_box_program) _menhir_cell1_EXCEPT, _menhir_box_program) _menhir_cell1_option_expr_ as 'stack) -> _ -> _ -> _ -> ('stack, _menhir_box_program) _menhir_state -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s _tok ->
      let _menhir_stack = MenhirCell1_option_preceded_AS_IDENT__ (_menhir_stack, _menhir_s, _v) in
      match (_tok : MenhirBasics.token) with
      | YIELD ->
          _menhir_run_219 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState238
      | WITH ->
          _menhir_run_223 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState238
      | WHILE ->
          _menhir_run_230 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState238
      | TRY ->
          _menhir_run_234 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState238
      | TRUE ->
          _menhir_run_034 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState238
      | SUPER ->
          _menhir_run_035 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState238
      | STRUCT ->
          _menhir_run_001 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState238
      | STRING _v_0 ->
          _menhir_run_037 _menhir_stack _menhir_lexbuf _menhir_lexer _v_0 MenhirState238
      | SELF ->
          _menhir_run_038 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState238
      | RETURN ->
          _menhir_run_239 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState238
      | RAISE ->
          _menhir_run_243 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState238
      | PLUS ->
          _menhir_run_039 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState238
      | PASS ->
          _menhir_run_246 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState238
      | NULL ->
          _menhir_run_040 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState238
      | NOT ->
          _menhir_run_044 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState238
      | NONLOCAL ->
          _menhir_run_248 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState238
      | NONE ->
          _menhir_run_041 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState238
      | MINUS ->
          _menhir_run_042 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState238
      | LPAREN ->
          _menhir_run_043 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState238
      | LBRACK ->
          _menhir_run_045 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState238
      | LBRACE ->
          _menhir_run_254 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState238
      | LAMBDA ->
          _menhir_run_047 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState238
      | INT _v_1 ->
          _menhir_run_056 _menhir_stack _menhir_lexbuf _menhir_lexer _v_1 MenhirState238
      | INC ->
          _menhir_run_057 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState238
      | IMPORT ->
          _menhir_run_012 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState238
      | IF ->
          _menhir_run_255 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState238
      | IDENT _v_2 ->
          _menhir_run_067 _menhir_stack _menhir_lexbuf _menhir_lexer _v_2 MenhirState238
      | GLOBAL ->
          _menhir_run_259 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState238
      | FROM ->
          _menhir_run_018 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState238
      | FOR ->
          _menhir_run_262 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState238
      | FLOAT _v_3 ->
          _menhir_run_059 _menhir_stack _menhir_lexbuf _menhir_lexer _v_3 MenhirState238
      | FALSE ->
          _menhir_run_060 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState238
      | ENUM ->
          _menhir_run_028 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState238
      | DEL ->
          _menhir_run_264 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState238
      | DEF ->
          _menhir_run_212 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState238
      | DEC ->
          _menhir_run_061 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState238
      | CONTINUE ->
          _menhir_run_267 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState238
      | BREAK ->
          _menhir_run_269 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState238
      | BITNOT ->
          _menhir_run_062 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState238
      | ASSERT ->
          _menhir_run_271 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState238
      | _ ->
          _eRR ()
  
  and _menhir_run_312 : type  ttv_stack. (((ttv_stack, _menhir_box_program) _menhir_cell1_TRY, _menhir_box_program) _menhir_cell1_block as 'stack) -> _ -> _ -> _ -> ('stack, _menhir_box_program) _menhir_state -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s _tok ->
      let _menhir_stack = MenhirCell1_list_except_clause_ (_menhir_stack, _menhir_s, _v) in
      match (_tok : MenhirBasics.token) with
      | FINALLY ->
          let _menhir_s = MenhirState313 in
          let _tok = _menhir_lexer _menhir_lexbuf in
          (match (_tok : MenhirBasics.token) with
          | YIELD ->
              _menhir_run_219 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | WITH ->
              _menhir_run_223 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | WHILE ->
              _menhir_run_230 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | TRY ->
              _menhir_run_234 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | TRUE ->
              _menhir_run_034 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | SUPER ->
              _menhir_run_035 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | STRUCT ->
              _menhir_run_001 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | STRING _v ->
              _menhir_run_037 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | SELF ->
              _menhir_run_038 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | RETURN ->
              _menhir_run_239 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | RAISE ->
              _menhir_run_243 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | PLUS ->
              _menhir_run_039 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | PASS ->
              _menhir_run_246 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | NULL ->
              _menhir_run_040 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | NOT ->
              _menhir_run_044 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | NONLOCAL ->
              _menhir_run_248 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | NONE ->
              _menhir_run_041 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | MINUS ->
              _menhir_run_042 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | LPAREN ->
              _menhir_run_043 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | LBRACK ->
              _menhir_run_045 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | LBRACE ->
              _menhir_run_254 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | LAMBDA ->
              _menhir_run_047 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | INT _v ->
              _menhir_run_056 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | INC ->
              _menhir_run_057 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | IMPORT ->
              _menhir_run_012 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | IF ->
              _menhir_run_255 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | IDENT _v ->
              _menhir_run_067 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | GLOBAL ->
              _menhir_run_259 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | FROM ->
              _menhir_run_018 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | FOR ->
              _menhir_run_262 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | FLOAT _v ->
              _menhir_run_059 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | FALSE ->
              _menhir_run_060 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | ENUM ->
              _menhir_run_028 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | DEL ->
              _menhir_run_264 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | DEF ->
              _menhir_run_212 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | DEC ->
              _menhir_run_061 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | CONTINUE ->
              _menhir_run_267 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | BREAK ->
              _menhir_run_269 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | BITNOT ->
              _menhir_run_062 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | ASSERT ->
              _menhir_run_271 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | _ ->
              _eRR ())
      | ASSERT | BITNOT | BREAK | CONTINUE | DEC | DEF | DEL | ELIF | ELSE | ENUM | EXCEPT | FALSE | FLOAT _ | FOR | FROM | GLOBAL | IDENT _ | IF | IMPORT | INC | INT _ | LAMBDA | LBRACE | LBRACK | LPAREN | MINUS | NONE | NONLOCAL | NOT | NULL | PASS | PLUS | RAISE | RBRACE | RETURN | SELF | STRING _ | STRUCT | SUPER | TRUE | TRY | WHILE | WITH | YIELD ->
          let _v = _menhir_action_089 () in
          _menhir_goto_option_finally_clause_ _menhir_stack _menhir_lexbuf _menhir_lexer _v _tok
      | _ ->
          _menhir_fail ()
  
  and _menhir_goto_option_finally_clause_ : type  ttv_stack. (((ttv_stack, _menhir_box_program) _menhir_cell1_TRY, _menhir_box_program) _menhir_cell1_block, _menhir_box_program) _menhir_cell1_list_except_clause_ -> _ -> _ -> _ -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _v _tok ->
      let MenhirCell1_list_except_clause_ (_menhir_stack, _, ecs) = _menhir_stack in
      let MenhirCell1_block (_menhir_stack, _, b) = _menhir_stack in
      let MenhirCell1_TRY (_menhir_stack, _menhir_s, _startpos__1_) = _menhir_stack in
      let f = _v in
      let _v = _menhir_action_148 _startpos__1_ b ecs f in
      _menhir_goto_stmt _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s _tok
  
  and _menhir_run_290 : type  ttv_stack. ttv_stack -> _ -> _ -> _ -> (ttv_stack, _menhir_box_program) _menhir_state -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s _tok ->
      let s = _v in
      let _v = _menhir_action_140 s in
      _menhir_goto_stmt _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s _tok
  
  and _menhir_run_321 : type  ttv_stack. ((ttv_stack, _menhir_box_program) _menhir_cell1_DEF _menhir_cell0_IDENT _menhir_cell0_LPAREN, _menhir_box_program) _menhir_cell1_loption_separated_nonempty_list_COMMA_param__ -> _ -> _ -> _ -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _v _tok ->
      let MenhirCell1_loption_separated_nonempty_list_COMMA_param__ (_menhir_stack, _, xs) = _menhir_stack in
      let MenhirCell0_LPAREN (_menhir_stack, _) = _menhir_stack in
      let MenhirCell0_IDENT (_menhir_stack, n, _) = _menhir_stack in
      let MenhirCell1_DEF (_menhir_stack, _menhir_s, _startpos__1_) = _menhir_stack in
      let b = _v in
      let _v = _menhir_action_049 _startpos__1_ b n xs in
      _menhir_goto_func_decl _menhir_stack _menhir_lexbuf _menhir_lexer _startpos__1_ _v _menhir_s _tok
  
  and _menhir_run_322 : type  ttv_stack. ttv_stack -> _ -> _ -> (ttv_stack, _menhir_box_program) _menhir_state -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s ->
      let _startpos = _menhir_lexbuf.Lexing.lex_start_p in
      let _menhir_stack = MenhirCell1_CLASS (_menhir_stack, _menhir_s, _startpos) in
      let _tok = _menhir_lexer _menhir_lexbuf in
      match (_tok : MenhirBasics.token) with
      | IDENT _v ->
          let _startpos_0 = _menhir_lexbuf.Lexing.lex_start_p in
          let _menhir_stack = MenhirCell0_IDENT (_menhir_stack, _v, _startpos_0) in
          let _tok = _menhir_lexer _menhir_lexbuf in
          (match (_tok : MenhirBasics.token) with
          | LBRACE ->
              let _startpos_1 = _menhir_lexbuf.Lexing.lex_start_p in
              let _menhir_stack = MenhirCell0_LBRACE (_menhir_stack, _startpos_1) in
              let _tok = _menhir_lexer _menhir_lexbuf in
              (match (_tok : MenhirBasics.token) with
              | IDENT _v_2 ->
                  _menhir_run_005 _menhir_stack _menhir_lexbuf _menhir_lexer _v_2 MenhirState324
              | RBRACE ->
                  let _v_3 = _menhir_action_065 () in
                  _menhir_run_325 _menhir_stack _menhir_lexbuf _menhir_lexer _v_3
              | _ ->
                  _eRR ())
          | _ ->
              _eRR ())
      | _ ->
          _eRR ()
  
  and _menhir_run_005 : type  ttv_stack. ttv_stack -> _ -> _ -> _ -> (ttv_stack, _menhir_box_program) _menhir_state -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s ->
      let _startpos = _menhir_lexbuf.Lexing.lex_start_p in
      let _tok = _menhir_lexer _menhir_lexbuf in
      match (_tok : MenhirBasics.token) with
      | SEMI ->
          let _tok = _menhir_lexer _menhir_lexbuf in
          let (_startpos_n_, n) = (_startpos, _v) in
          let _v = _menhir_action_079 _startpos_n_ n in
          let _menhir_stack = MenhirCell1_member_decl (_menhir_stack, _menhir_s, _v) in
          (match (_tok : MenhirBasics.token) with
          | IDENT _v_0 ->
              _menhir_run_005 _menhir_stack _menhir_lexbuf _menhir_lexer _v_0 MenhirState007
          | RBRACE ->
              let _v_1 = _menhir_action_065 () in
              _menhir_run_008 _menhir_stack _menhir_lexbuf _menhir_lexer _v_1
          | _ ->
              _eRR ())
      | _ ->
          _eRR ()
  
  and _menhir_run_008 : type  ttv_stack. (ttv_stack, _menhir_box_program) _menhir_cell1_member_decl -> _ -> _ -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _v ->
      let MenhirCell1_member_decl (_menhir_stack, _menhir_s, x) = _menhir_stack in
      let xs = _v in
      let _v = _menhir_action_066 x xs in
      _menhir_goto_list_member_decl_ _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
  
  and _menhir_goto_list_member_decl_ : type  ttv_stack. ttv_stack -> _ -> _ -> _ -> (ttv_stack, _menhir_box_program) _menhir_state -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s ->
      match _menhir_s with
      | MenhirState007 ->
          _menhir_run_008 _menhir_stack _menhir_lexbuf _menhir_lexer _v
      | MenhirState004 ->
          _menhir_run_009 _menhir_stack _menhir_lexbuf _menhir_lexer _v
      | MenhirState324 ->
          _menhir_run_325 _menhir_stack _menhir_lexbuf _menhir_lexer _v
      | _ ->
          _menhir_fail ()
  
  and _menhir_run_009 : type  ttv_stack. (ttv_stack, _menhir_box_program) _menhir_cell1_STRUCT _menhir_cell0_IDENT _menhir_cell0_LBRACE -> _ -> _ -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _v ->
      let _tok = _menhir_lexer _menhir_lexbuf in
      match (_tok : MenhirBasics.token) with
      | SEMI ->
          let _tok = _menhir_lexer _menhir_lexbuf in
          let MenhirCell0_LBRACE (_menhir_stack, _) = _menhir_stack in
          let MenhirCell0_IDENT (_menhir_stack, n, _) = _menhir_stack in
          let MenhirCell1_STRUCT (_menhir_stack, _menhir_s, _startpos__1_) = _menhir_stack in
          let ms = _v in
          let _v = _menhir_action_160 _startpos__1_ ms n in
          _menhir_goto_struct_decl _menhir_stack _menhir_lexbuf _menhir_lexer _startpos__1_ _v _menhir_s _tok
      | _ ->
          _eRR ()
  
  and _menhir_run_325 : type  ttv_stack. (ttv_stack, _menhir_box_program) _menhir_cell1_CLASS _menhir_cell0_IDENT _menhir_cell0_LBRACE -> _ -> _ -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _v ->
      let _tok = _menhir_lexer _menhir_lexbuf in
      match (_tok : MenhirBasics.token) with
      | SEMI ->
          let _tok = _menhir_lexer _menhir_lexbuf in
          let MenhirCell0_LBRACE (_menhir_stack, _) = _menhir_stack in
          let MenhirCell0_IDENT (_menhir_stack, n, _) = _menhir_stack in
          let MenhirCell1_CLASS (_menhir_stack, _menhir_s, _startpos__1_) = _menhir_stack in
          let ms = _v in
          let _v = _menhir_action_022 _startpos__1_ ms n in
          let d = _v in
          let _v = _menhir_action_163 d in
          _menhir_goto_top_decl _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s _tok
      | _ ->
          _eRR ()
  
  and _menhir_run_053 : type  ttv_stack. (ttv_stack, _menhir_box_program) _menhir_cell1_param -> _ -> _ -> _ -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _v _tok ->
      let MenhirCell1_param (_menhir_stack, _menhir_s, x) = _menhir_stack in
      let xs = _v in
      let _v = _menhir_action_134 x xs in
      _menhir_goto_separated_nonempty_list_COMMA_param_ _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s _tok
  
  and _menhir_run_194 : type  ttv_stack. (ttv_stack, _menhir_box_program) _menhir_cell1_LBRACK -> _ -> _ -> _ -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _v _tok ->
      match (_tok : MenhirBasics.token) with
      | RBRACK ->
          let _tok = _menhir_lexer _menhir_lexbuf in
          let MenhirCell1_LBRACK (_menhir_stack, _menhir_s, _startpos__1_) = _menhir_stack in
          let xs = _v in
          let _v = _menhir_action_119 _startpos__1_ xs in
          _menhir_goto_primary_expr _menhir_stack _menhir_lexbuf _menhir_lexer _startpos__1_ _v _menhir_s _tok
      | _ ->
          _eRR ()
  
  and _menhir_run_201 : type  ttv_stack. (ttv_stack, _menhir_box_program) _menhir_cell1_SUPER _menhir_cell0_LPAREN -> _ -> _ -> _ -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _v _tok ->
      match (_tok : MenhirBasics.token) with
      | RPAREN ->
          let _tok = _menhir_lexer _menhir_lexbuf in
          let MenhirCell0_LPAREN (_menhir_stack, _) = _menhir_stack in
          let MenhirCell1_SUPER (_menhir_stack, _menhir_s, _startpos__1_) = _menhir_stack in
          let xs = _v in
          let _v = _menhir_action_117 _startpos__1_ xs in
          _menhir_goto_primary_expr _menhir_stack _menhir_lexbuf _menhir_lexer _startpos__1_ _v _menhir_s _tok
      | _ ->
          _eRR ()
  
  and _menhir_run_159 : type  ttv_stack. (ttv_stack, _menhir_box_program) _menhir_cell1_postfix_expr _menhir_cell0_LPAREN -> _ -> _ -> _ -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _v _tok ->
      match (_tok : MenhirBasics.token) with
      | RPAREN ->
          let _tok = _menhir_lexer _menhir_lexbuf in
          let MenhirCell0_LPAREN (_menhir_stack, _) = _menhir_stack in
          let MenhirCell1_postfix_expr (_menhir_stack, _menhir_s, e, _startpos_e_) = _menhir_stack in
          let xs = _v in
          let _v = _menhir_action_104 _startpos_e_ e xs in
          _menhir_goto_postfix_expr _menhir_stack _menhir_lexbuf _menhir_lexer _startpos_e_ _v _menhir_s _tok
      | _ ->
          _eRR ()
  
  and _menhir_run_165 : type  ttv_stack. ((ttv_stack, _menhir_box_program) _menhir_cell1_postfix_expr _menhir_cell0_LBRACK as 'stack) -> _ -> _ -> _ -> ('stack, _menhir_box_program) _menhir_state -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s _tok ->
      let _menhir_stack = MenhirCell1_expr_option (_menhir_stack, _menhir_s, _v) in
      match (_tok : MenhirBasics.token) with
      | COLON ->
          let _tok = _menhir_lexer _menhir_lexbuf in
          (match (_tok : MenhirBasics.token) with
          | TRUE ->
              _menhir_run_034 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState166
          | SUPER ->
              _menhir_run_035 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState166
          | STRING _v_0 ->
              _menhir_run_037 _menhir_stack _menhir_lexbuf _menhir_lexer _v_0 MenhirState166
          | SELF ->
              _menhir_run_038 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState166
          | PLUS ->
              _menhir_run_039 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState166
          | NULL ->
              _menhir_run_040 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState166
          | NOT ->
              _menhir_run_044 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState166
          | NONE ->
              _menhir_run_041 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState166
          | MINUS ->
              _menhir_run_042 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState166
          | LPAREN ->
              _menhir_run_043 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState166
          | LBRACK ->
              _menhir_run_045 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState166
          | LBRACE ->
              _menhir_run_046 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState166
          | LAMBDA ->
              _menhir_run_047 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState166
          | INT _v_1 ->
              _menhir_run_056 _menhir_stack _menhir_lexbuf _menhir_lexer _v_1 MenhirState166
          | INC ->
              _menhir_run_057 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState166
          | IDENT _v_2 ->
              _menhir_run_067 _menhir_stack _menhir_lexbuf _menhir_lexer _v_2 MenhirState166
          | FLOAT _v_3 ->
              _menhir_run_059 _menhir_stack _menhir_lexbuf _menhir_lexer _v_3 MenhirState166
          | FALSE ->
              _menhir_run_060 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState166
          | DEC ->
              _menhir_run_061 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState166
          | BITNOT ->
              _menhir_run_062 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState166
          | COLON | RBRACK ->
              let _v_4 = _menhir_action_047 () in
              _menhir_run_167 _menhir_stack _menhir_lexbuf _menhir_lexer _v_4 MenhirState166 _tok
          | _ ->
              _eRR ())
      | _ ->
          _eRR ()
  
  and _menhir_run_167 : type  ttv_stack. (((ttv_stack, _menhir_box_program) _menhir_cell1_postfix_expr _menhir_cell0_LBRACK, _menhir_box_program) _menhir_cell1_expr_option as 'stack) -> _ -> _ -> _ -> ('stack, _menhir_box_program) _menhir_state -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s _tok ->
      match (_tok : MenhirBasics.token) with
      | RBRACK ->
          let _tok = _menhir_lexer _menhir_lexbuf in
          let MenhirCell1_expr_option (_menhir_stack, _, i1) = _menhir_stack in
          let MenhirCell0_LBRACK (_menhir_stack, _) = _menhir_stack in
          let MenhirCell1_postfix_expr (_menhir_stack, _menhir_s, e, _startpos_e_) = _menhir_stack in
          let i2 = _v in
          let _v = _menhir_action_102 _startpos_e_ e i1 i2 in
          _menhir_goto_postfix_expr _menhir_stack _menhir_lexbuf _menhir_lexer _startpos_e_ _v _menhir_s _tok
      | COLON ->
          let _menhir_stack = MenhirCell1_expr_option (_menhir_stack, _menhir_s, _v) in
          let _tok = _menhir_lexer _menhir_lexbuf in
          (match (_tok : MenhirBasics.token) with
          | TRUE ->
              _menhir_run_034 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState169
          | SUPER ->
              _menhir_run_035 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState169
          | STRING _v_0 ->
              _menhir_run_037 _menhir_stack _menhir_lexbuf _menhir_lexer _v_0 MenhirState169
          | SELF ->
              _menhir_run_038 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState169
          | PLUS ->
              _menhir_run_039 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState169
          | NULL ->
              _menhir_run_040 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState169
          | NOT ->
              _menhir_run_044 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState169
          | NONE ->
              _menhir_run_041 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState169
          | MINUS ->
              _menhir_run_042 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState169
          | LPAREN ->
              _menhir_run_043 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState169
          | LBRACK ->
              _menhir_run_045 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState169
          | LBRACE ->
              _menhir_run_046 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState169
          | LAMBDA ->
              _menhir_run_047 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState169
          | INT _v_1 ->
              _menhir_run_056 _menhir_stack _menhir_lexbuf _menhir_lexer _v_1 MenhirState169
          | INC ->
              _menhir_run_057 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState169
          | IDENT _v_2 ->
              _menhir_run_067 _menhir_stack _menhir_lexbuf _menhir_lexer _v_2 MenhirState169
          | FLOAT _v_3 ->
              _menhir_run_059 _menhir_stack _menhir_lexbuf _menhir_lexer _v_3 MenhirState169
          | FALSE ->
              _menhir_run_060 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState169
          | DEC ->
              _menhir_run_061 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState169
          | BITNOT ->
              _menhir_run_062 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState169
          | RBRACK ->
              let _v_4 = _menhir_action_047 () in
              _menhir_run_170 _menhir_stack _menhir_lexbuf _menhir_lexer _v_4 _tok
          | _ ->
              _eRR ())
      | _ ->
          _eRR ()
  
  and _menhir_run_170 : type  ttv_stack. (((ttv_stack, _menhir_box_program) _menhir_cell1_postfix_expr _menhir_cell0_LBRACK, _menhir_box_program) _menhir_cell1_expr_option, _menhir_box_program) _menhir_cell1_expr_option -> _ -> _ -> _ -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _v _tok ->
      match (_tok : MenhirBasics.token) with
      | RBRACK ->
          let _tok = _menhir_lexer _menhir_lexbuf in
          let MenhirCell1_expr_option (_menhir_stack, _, i2) = _menhir_stack in
          let MenhirCell1_expr_option (_menhir_stack, _, i1) = _menhir_stack in
          let MenhirCell0_LBRACK (_menhir_stack, _) = _menhir_stack in
          let MenhirCell1_postfix_expr (_menhir_stack, _menhir_s, e, _startpos_e_) = _menhir_stack in
          let i3 = _v in
          let _v = _menhir_action_103 _startpos_e_ e i1 i2 i3 in
          _menhir_goto_postfix_expr _menhir_stack _menhir_lexbuf _menhir_lexer _startpos_e_ _v _menhir_s _tok
      | _ ->
          _eRR ()
  
  and _menhir_goto_unary_expr : type  ttv_stack. ttv_stack -> _ -> _ -> _ -> _ -> (ttv_stack, _menhir_box_program) _menhir_state -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok ->
      match _menhir_s with
      | MenhirState062 ->
          _menhir_run_063 _menhir_stack _menhir_lexbuf _menhir_lexer _v _tok
      | MenhirState033 ->
          _menhir_run_070 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState036 ->
          _menhir_run_070 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState043 ->
          _menhir_run_070 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState044 ->
          _menhir_run_070 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState045 ->
          _menhir_run_070 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState046 ->
          _menhir_run_070 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState055 ->
          _menhir_run_070 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState066 ->
          _menhir_run_070 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState068 ->
          _menhir_run_070 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState071 ->
          _menhir_run_070 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState074 ->
          _menhir_run_070 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState076 ->
          _menhir_run_070 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState078 ->
          _menhir_run_070 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState080 ->
          _menhir_run_070 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState082 ->
          _menhir_run_070 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState085 ->
          _menhir_run_070 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState087 ->
          _menhir_run_070 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState090 ->
          _menhir_run_070 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState093 ->
          _menhir_run_070 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState095 ->
          _menhir_run_070 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState097 ->
          _menhir_run_070 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState099 ->
          _menhir_run_070 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState101 ->
          _menhir_run_070 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState105 ->
          _menhir_run_070 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState107 ->
          _menhir_run_070 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState110 ->
          _menhir_run_070 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState111 ->
          _menhir_run_070 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState115 ->
          _menhir_run_070 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState118 ->
          _menhir_run_070 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState121 ->
          _menhir_run_070 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState124 ->
          _menhir_run_070 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState126 ->
          _menhir_run_070 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState128 ->
          _menhir_run_070 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState134 ->
          _menhir_run_070 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState136 ->
          _menhir_run_070 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState138 ->
          _menhir_run_070 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState140 ->
          _menhir_run_070 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState142 ->
          _menhir_run_070 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState144 ->
          _menhir_run_070 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState146 ->
          _menhir_run_070 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState148 ->
          _menhir_run_070 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState152 ->
          _menhir_run_070 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState156 ->
          _menhir_run_070 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState162 ->
          _menhir_run_070 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState164 ->
          _menhir_run_070 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState166 ->
          _menhir_run_070 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState169 ->
          _menhir_run_070 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState188 ->
          _menhir_run_070 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState191 ->
          _menhir_run_070 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState218 ->
          _menhir_run_070 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState219 ->
          _menhir_run_070 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState223 ->
          _menhir_run_070 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState225 ->
          _menhir_run_070 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState229 ->
          _menhir_run_070 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState231 ->
          _menhir_run_070 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState233 ->
          _menhir_run_070 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState236 ->
          _menhir_run_070 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState238 ->
          _menhir_run_070 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState239 ->
          _menhir_run_070 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState243 ->
          _menhir_run_070 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState254 ->
          _menhir_run_070 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState256 ->
          _menhir_run_070 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState258 ->
          _menhir_run_070 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState263 ->
          _menhir_run_070 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState264 ->
          _menhir_run_070 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState271 ->
          _menhir_run_070 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState274 ->
          _menhir_run_070 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState278 ->
          _menhir_run_070 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState280 ->
          _menhir_run_070 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState282 ->
          _menhir_run_070 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState292 ->
          _menhir_run_070 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState294 ->
          _menhir_run_070 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState297 ->
          _menhir_run_070 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState300 ->
          _menhir_run_070 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState302 ->
          _menhir_run_070 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState306 ->
          _menhir_run_070 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState313 ->
          _menhir_run_070 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState061 ->
          _menhir_run_179 _menhir_stack _menhir_lexbuf _menhir_lexer _v _tok
      | MenhirState057 ->
          _menhir_run_180 _menhir_stack _menhir_lexbuf _menhir_lexer _v _tok
      | MenhirState042 ->
          _menhir_run_199 _menhir_stack _menhir_lexbuf _menhir_lexer _v _tok
      | MenhirState039 ->
          _menhir_run_200 _menhir_stack _menhir_lexbuf _menhir_lexer _v _tok
      | _ ->
          _menhir_fail ()
  
  and _menhir_run_063 : type  ttv_stack. (ttv_stack, _menhir_box_program) _menhir_cell1_BITNOT -> _ -> _ -> _ -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _v _tok ->
      let MenhirCell1_BITNOT (_menhir_stack, _menhir_s, _startpos__1_) = _menhir_stack in
      let e = _v in
      let _v = _menhir_action_169 _startpos__1_ e in
      _menhir_goto_unary_expr _menhir_stack _menhir_lexbuf _menhir_lexer _startpos__1_ _v _menhir_s _tok
  
  and _menhir_run_070 : type  ttv_stack. ttv_stack -> _ -> _ -> _ -> _ -> (ttv_stack, _menhir_box_program) _menhir_state -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok ->
      match (_tok : MenhirBasics.token) with
      | POW ->
          let _menhir_stack = MenhirCell1_unary_expr (_menhir_stack, _menhir_s, _v, _startpos) in
          let _menhir_s = MenhirState071 in
          let _tok = _menhir_lexer _menhir_lexbuf in
          (match (_tok : MenhirBasics.token) with
          | TRUE ->
              _menhir_run_034 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | SUPER ->
              _menhir_run_035 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | STRING _v ->
              _menhir_run_037 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | SELF ->
              _menhir_run_038 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | PLUS ->
              _menhir_run_039 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | NULL ->
              _menhir_run_040 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | NONE ->
              _menhir_run_041 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | MINUS ->
              _menhir_run_042 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | LPAREN ->
              _menhir_run_043 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | LBRACK ->
              _menhir_run_045 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | LBRACE ->
              _menhir_run_046 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | INT _v ->
              _menhir_run_056 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | INC ->
              _menhir_run_057 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | IDENT _v ->
              _menhir_run_058 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | FLOAT _v ->
              _menhir_run_059 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | FALSE ->
              _menhir_run_060 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | DEC ->
              _menhir_run_061 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | BITNOT ->
              _menhir_run_062 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | _ ->
              _eRR ())
      | ADD_ASSIGN | AND | AS | ASSERT | ASSIGN | BITAND | BITNOT | BITOR | BITXOR | BREAK | COLON | COMMA | CONTINUE | DARROW | DEC | DEF | DEL | DIV_ASSIGN | ELSE | ENUM | EQEQ | FALSE | FLOAT _ | FLOORDIV | FLOORDIV_ASSIGN | FOR | FROM | GE | GLOBAL | GT | IDENT _ | IF | IMPORT | IN | INC | INT _ | IS | LAMBDA | LBRACE | LBRACK | LE | LPAREN | LSHIFT | LT | MINUS | MOD_ASSIGN | MUL_ASSIGN | NEQ | NONE | NONLOCAL | NOT | NULL | OR | PASS | PERCENT | PLUS | POW_ASSIGN | RAISE | RBRACE | RBRACK | RETURN | RPAREN | RSHIFT | SELF | SEMI | SLASH | STAR | STRING _ | STRUCT | SUB_ASSIGN | SUPER | TRUE | TRY | WHILE | WITH | YIELD ->
          let (_startpos_e_, e) = (_startpos, _v) in
          let _v = _menhir_action_106 e in
          _menhir_goto_pow_expr _menhir_stack _menhir_lexbuf _menhir_lexer _startpos_e_ _v _menhir_s _tok
      | _ ->
          _menhir_fail ()
  
  and _menhir_goto_pow_expr : type  ttv_stack. ttv_stack -> _ -> _ -> _ -> _ -> (ttv_stack, _menhir_box_program) _menhir_state -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok ->
      match _menhir_s with
      | MenhirState071 ->
          _menhir_run_072 _menhir_stack _menhir_lexbuf _menhir_lexer _v _tok
      | MenhirState033 ->
          _menhir_run_075 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState036 ->
          _menhir_run_075 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState043 ->
          _menhir_run_075 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState044 ->
          _menhir_run_075 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState045 ->
          _menhir_run_075 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState046 ->
          _menhir_run_075 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState055 ->
          _menhir_run_075 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState066 ->
          _menhir_run_075 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState068 ->
          _menhir_run_075 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState074 ->
          _menhir_run_075 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState076 ->
          _menhir_run_075 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState078 ->
          _menhir_run_075 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState080 ->
          _menhir_run_075 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState082 ->
          _menhir_run_075 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState085 ->
          _menhir_run_075 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState087 ->
          _menhir_run_075 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState090 ->
          _menhir_run_075 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState093 ->
          _menhir_run_075 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState095 ->
          _menhir_run_075 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState097 ->
          _menhir_run_075 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState099 ->
          _menhir_run_075 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState101 ->
          _menhir_run_075 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState105 ->
          _menhir_run_075 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState107 ->
          _menhir_run_075 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState110 ->
          _menhir_run_075 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState111 ->
          _menhir_run_075 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState115 ->
          _menhir_run_075 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState118 ->
          _menhir_run_075 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState121 ->
          _menhir_run_075 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState124 ->
          _menhir_run_075 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState126 ->
          _menhir_run_075 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState128 ->
          _menhir_run_075 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState134 ->
          _menhir_run_075 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState136 ->
          _menhir_run_075 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState138 ->
          _menhir_run_075 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState140 ->
          _menhir_run_075 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState142 ->
          _menhir_run_075 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState144 ->
          _menhir_run_075 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState146 ->
          _menhir_run_075 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState148 ->
          _menhir_run_075 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState152 ->
          _menhir_run_075 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState156 ->
          _menhir_run_075 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState162 ->
          _menhir_run_075 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState164 ->
          _menhir_run_075 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState166 ->
          _menhir_run_075 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState169 ->
          _menhir_run_075 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState188 ->
          _menhir_run_075 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState191 ->
          _menhir_run_075 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState218 ->
          _menhir_run_075 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState219 ->
          _menhir_run_075 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState223 ->
          _menhir_run_075 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState225 ->
          _menhir_run_075 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState229 ->
          _menhir_run_075 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState231 ->
          _menhir_run_075 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState233 ->
          _menhir_run_075 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState236 ->
          _menhir_run_075 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState238 ->
          _menhir_run_075 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState239 ->
          _menhir_run_075 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState243 ->
          _menhir_run_075 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState254 ->
          _menhir_run_075 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState256 ->
          _menhir_run_075 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState258 ->
          _menhir_run_075 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState263 ->
          _menhir_run_075 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState264 ->
          _menhir_run_075 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState271 ->
          _menhir_run_075 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState274 ->
          _menhir_run_075 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState278 ->
          _menhir_run_075 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState280 ->
          _menhir_run_075 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState282 ->
          _menhir_run_075 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState292 ->
          _menhir_run_075 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState294 ->
          _menhir_run_075 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState297 ->
          _menhir_run_075 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState300 ->
          _menhir_run_075 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState302 ->
          _menhir_run_075 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState306 ->
          _menhir_run_075 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState313 ->
          _menhir_run_075 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | _ ->
          _menhir_fail ()
  
  and _menhir_run_072 : type  ttv_stack. (ttv_stack, _menhir_box_program) _menhir_cell1_unary_expr -> _ -> _ -> _ -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _v _tok ->
      let MenhirCell1_unary_expr (_menhir_stack, _menhir_s, e, _startpos_e_) = _menhir_stack in
      let a = _v in
      let _v = _menhir_action_107 _startpos_e_ a e in
      _menhir_goto_pow_expr _menhir_stack _menhir_lexbuf _menhir_lexer _startpos_e_ _v _menhir_s _tok
  
  and _menhir_run_075 : type  ttv_stack. ttv_stack -> _ -> _ -> _ -> _ -> (ttv_stack, _menhir_box_program) _menhir_state -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok ->
      match (_tok : MenhirBasics.token) with
      | STAR ->
          let _menhir_stack = MenhirCell1_pow_expr (_menhir_stack, _menhir_s, _v, _startpos) in
          let _menhir_s = MenhirState076 in
          let _tok = _menhir_lexer _menhir_lexbuf in
          (match (_tok : MenhirBasics.token) with
          | TRUE ->
              _menhir_run_034 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | SUPER ->
              _menhir_run_035 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | STRING _v ->
              _menhir_run_037 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | SELF ->
              _menhir_run_038 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | PLUS ->
              _menhir_run_039 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | NULL ->
              _menhir_run_040 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | NONE ->
              _menhir_run_041 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | MINUS ->
              _menhir_run_042 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | LPAREN ->
              _menhir_run_043 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | LBRACK ->
              _menhir_run_045 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | LBRACE ->
              _menhir_run_046 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | INT _v ->
              _menhir_run_056 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | INC ->
              _menhir_run_057 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | IDENT _v ->
              _menhir_run_058 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | FLOAT _v ->
              _menhir_run_059 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | FALSE ->
              _menhir_run_060 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | DEC ->
              _menhir_run_061 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | BITNOT ->
              _menhir_run_062 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | _ ->
              _eRR ())
      | SLASH ->
          let _menhir_stack = MenhirCell1_pow_expr (_menhir_stack, _menhir_s, _v, _startpos) in
          let _menhir_s = MenhirState078 in
          let _tok = _menhir_lexer _menhir_lexbuf in
          (match (_tok : MenhirBasics.token) with
          | TRUE ->
              _menhir_run_034 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | SUPER ->
              _menhir_run_035 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | STRING _v ->
              _menhir_run_037 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | SELF ->
              _menhir_run_038 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | PLUS ->
              _menhir_run_039 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | NULL ->
              _menhir_run_040 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | NONE ->
              _menhir_run_041 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | MINUS ->
              _menhir_run_042 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | LPAREN ->
              _menhir_run_043 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | LBRACK ->
              _menhir_run_045 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | LBRACE ->
              _menhir_run_046 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | INT _v ->
              _menhir_run_056 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | INC ->
              _menhir_run_057 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | IDENT _v ->
              _menhir_run_058 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | FLOAT _v ->
              _menhir_run_059 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | FALSE ->
              _menhir_run_060 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | DEC ->
              _menhir_run_061 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | BITNOT ->
              _menhir_run_062 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | _ ->
              _eRR ())
      | PERCENT ->
          let _menhir_stack = MenhirCell1_pow_expr (_menhir_stack, _menhir_s, _v, _startpos) in
          let _menhir_s = MenhirState080 in
          let _tok = _menhir_lexer _menhir_lexbuf in
          (match (_tok : MenhirBasics.token) with
          | TRUE ->
              _menhir_run_034 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | SUPER ->
              _menhir_run_035 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | STRING _v ->
              _menhir_run_037 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | SELF ->
              _menhir_run_038 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | PLUS ->
              _menhir_run_039 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | NULL ->
              _menhir_run_040 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | NONE ->
              _menhir_run_041 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | MINUS ->
              _menhir_run_042 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | LPAREN ->
              _menhir_run_043 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | LBRACK ->
              _menhir_run_045 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | LBRACE ->
              _menhir_run_046 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | INT _v ->
              _menhir_run_056 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | INC ->
              _menhir_run_057 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | IDENT _v ->
              _menhir_run_058 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | FLOAT _v ->
              _menhir_run_059 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | FALSE ->
              _menhir_run_060 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | DEC ->
              _menhir_run_061 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | BITNOT ->
              _menhir_run_062 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | _ ->
              _eRR ())
      | FLOORDIV ->
          let _menhir_stack = MenhirCell1_pow_expr (_menhir_stack, _menhir_s, _v, _startpos) in
          let _menhir_s = MenhirState082 in
          let _tok = _menhir_lexer _menhir_lexbuf in
          (match (_tok : MenhirBasics.token) with
          | TRUE ->
              _menhir_run_034 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | SUPER ->
              _menhir_run_035 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | STRING _v ->
              _menhir_run_037 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | SELF ->
              _menhir_run_038 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | PLUS ->
              _menhir_run_039 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | NULL ->
              _menhir_run_040 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | NONE ->
              _menhir_run_041 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | MINUS ->
              _menhir_run_042 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | LPAREN ->
              _menhir_run_043 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | LBRACK ->
              _menhir_run_045 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | LBRACE ->
              _menhir_run_046 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | INT _v ->
              _menhir_run_056 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | INC ->
              _menhir_run_057 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | IDENT _v ->
              _menhir_run_058 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | FLOAT _v ->
              _menhir_run_059 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | FALSE ->
              _menhir_run_060 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | DEC ->
              _menhir_run_061 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | BITNOT ->
              _menhir_run_062 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | _ ->
              _eRR ())
      | ADD_ASSIGN | AND | AS | ASSERT | ASSIGN | BITAND | BITNOT | BITOR | BITXOR | BREAK | COLON | COMMA | CONTINUE | DARROW | DEC | DEF | DEL | DIV_ASSIGN | ELSE | ENUM | EQEQ | FALSE | FLOAT _ | FLOORDIV_ASSIGN | FOR | FROM | GE | GLOBAL | GT | IDENT _ | IF | IMPORT | IN | INC | INT _ | IS | LAMBDA | LBRACE | LBRACK | LE | LPAREN | LSHIFT | LT | MINUS | MOD_ASSIGN | MUL_ASSIGN | NEQ | NONE | NONLOCAL | NOT | NULL | OR | PASS | PLUS | POW_ASSIGN | RAISE | RBRACE | RBRACK | RETURN | RPAREN | RSHIFT | SELF | SEMI | STRING _ | STRUCT | SUB_ASSIGN | SUPER | TRUE | TRY | WHILE | WITH | YIELD ->
          let (_startpos_e_, e) = (_startpos, _v) in
          let _v = _menhir_action_080 e in
          _menhir_goto_mul_expr _menhir_stack _menhir_lexbuf _menhir_lexer _startpos_e_ _v _menhir_s _tok
      | _ ->
          _menhir_fail ()
  
  and _menhir_goto_mul_expr : type  ttv_stack. ttv_stack -> _ -> _ -> _ -> _ -> (ttv_stack, _menhir_box_program) _menhir_state -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok ->
      match _menhir_s with
      | MenhirState076 ->
          _menhir_run_077 _menhir_stack _menhir_lexbuf _menhir_lexer _v _tok
      | MenhirState078 ->
          _menhir_run_079 _menhir_stack _menhir_lexbuf _menhir_lexer _v _tok
      | MenhirState080 ->
          _menhir_run_081 _menhir_stack _menhir_lexbuf _menhir_lexer _v _tok
      | MenhirState082 ->
          _menhir_run_083 _menhir_stack _menhir_lexbuf _menhir_lexer _v _tok
      | MenhirState033 ->
          _menhir_run_084 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState036 ->
          _menhir_run_084 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState043 ->
          _menhir_run_084 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState044 ->
          _menhir_run_084 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState045 ->
          _menhir_run_084 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState046 ->
          _menhir_run_084 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState055 ->
          _menhir_run_084 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState066 ->
          _menhir_run_084 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState068 ->
          _menhir_run_084 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState074 ->
          _menhir_run_084 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState085 ->
          _menhir_run_084 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState087 ->
          _menhir_run_084 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState090 ->
          _menhir_run_084 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState093 ->
          _menhir_run_084 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState095 ->
          _menhir_run_084 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState097 ->
          _menhir_run_084 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState099 ->
          _menhir_run_084 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState101 ->
          _menhir_run_084 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState105 ->
          _menhir_run_084 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState107 ->
          _menhir_run_084 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState110 ->
          _menhir_run_084 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState111 ->
          _menhir_run_084 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState115 ->
          _menhir_run_084 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState118 ->
          _menhir_run_084 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState121 ->
          _menhir_run_084 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState124 ->
          _menhir_run_084 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState126 ->
          _menhir_run_084 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState128 ->
          _menhir_run_084 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState134 ->
          _menhir_run_084 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState136 ->
          _menhir_run_084 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState138 ->
          _menhir_run_084 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState140 ->
          _menhir_run_084 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState142 ->
          _menhir_run_084 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState144 ->
          _menhir_run_084 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState146 ->
          _menhir_run_084 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState148 ->
          _menhir_run_084 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState152 ->
          _menhir_run_084 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState156 ->
          _menhir_run_084 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState162 ->
          _menhir_run_084 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState164 ->
          _menhir_run_084 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState166 ->
          _menhir_run_084 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState169 ->
          _menhir_run_084 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState188 ->
          _menhir_run_084 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState191 ->
          _menhir_run_084 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState218 ->
          _menhir_run_084 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState219 ->
          _menhir_run_084 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState223 ->
          _menhir_run_084 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState225 ->
          _menhir_run_084 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState229 ->
          _menhir_run_084 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState231 ->
          _menhir_run_084 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState233 ->
          _menhir_run_084 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState236 ->
          _menhir_run_084 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState238 ->
          _menhir_run_084 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState239 ->
          _menhir_run_084 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState243 ->
          _menhir_run_084 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState254 ->
          _menhir_run_084 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState256 ->
          _menhir_run_084 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState258 ->
          _menhir_run_084 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState263 ->
          _menhir_run_084 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState264 ->
          _menhir_run_084 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState271 ->
          _menhir_run_084 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState274 ->
          _menhir_run_084 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState278 ->
          _menhir_run_084 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState280 ->
          _menhir_run_084 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState282 ->
          _menhir_run_084 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState292 ->
          _menhir_run_084 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState294 ->
          _menhir_run_084 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState297 ->
          _menhir_run_084 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState300 ->
          _menhir_run_084 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState302 ->
          _menhir_run_084 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState306 ->
          _menhir_run_084 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState313 ->
          _menhir_run_084 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | _ ->
          _menhir_fail ()
  
  and _menhir_run_077 : type  ttv_stack. (ttv_stack, _menhir_box_program) _menhir_cell1_pow_expr -> _ -> _ -> _ -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _v _tok ->
      let MenhirCell1_pow_expr (_menhir_stack, _menhir_s, e, _startpos_e_) = _menhir_stack in
      let a = _v in
      let _v = _menhir_action_081 _startpos_e_ a e in
      _menhir_goto_mul_expr _menhir_stack _menhir_lexbuf _menhir_lexer _startpos_e_ _v _menhir_s _tok
  
  and _menhir_run_079 : type  ttv_stack. (ttv_stack, _menhir_box_program) _menhir_cell1_pow_expr -> _ -> _ -> _ -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _v _tok ->
      let MenhirCell1_pow_expr (_menhir_stack, _menhir_s, e, _startpos_e_) = _menhir_stack in
      let a = _v in
      let _v = _menhir_action_082 _startpos_e_ a e in
      _menhir_goto_mul_expr _menhir_stack _menhir_lexbuf _menhir_lexer _startpos_e_ _v _menhir_s _tok
  
  and _menhir_run_081 : type  ttv_stack. (ttv_stack, _menhir_box_program) _menhir_cell1_pow_expr -> _ -> _ -> _ -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _v _tok ->
      let MenhirCell1_pow_expr (_menhir_stack, _menhir_s, e, _startpos_e_) = _menhir_stack in
      let a = _v in
      let _v = _menhir_action_083 _startpos_e_ a e in
      _menhir_goto_mul_expr _menhir_stack _menhir_lexbuf _menhir_lexer _startpos_e_ _v _menhir_s _tok
  
  and _menhir_run_083 : type  ttv_stack. (ttv_stack, _menhir_box_program) _menhir_cell1_pow_expr -> _ -> _ -> _ -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _v _tok ->
      let MenhirCell1_pow_expr (_menhir_stack, _menhir_s, e, _startpos_e_) = _menhir_stack in
      let a = _v in
      let _v = _menhir_action_084 _startpos_e_ a e in
      _menhir_goto_mul_expr _menhir_stack _menhir_lexbuf _menhir_lexer _startpos_e_ _v _menhir_s _tok
  
  and _menhir_run_084 : type  ttv_stack. ttv_stack -> _ -> _ -> _ -> _ -> (ttv_stack, _menhir_box_program) _menhir_state -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok ->
      match (_tok : MenhirBasics.token) with
      | PLUS ->
          let _menhir_stack = MenhirCell1_mul_expr (_menhir_stack, _menhir_s, _v, _startpos) in
          let _startpos = _menhir_lexbuf.Lexing.lex_start_p in
          let _menhir_stack = MenhirCell0_PLUS (_menhir_stack, _startpos) in
          let _menhir_s = MenhirState085 in
          let _tok = _menhir_lexer _menhir_lexbuf in
          (match (_tok : MenhirBasics.token) with
          | TRUE ->
              _menhir_run_034 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | SUPER ->
              _menhir_run_035 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | STRING _v ->
              _menhir_run_037 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | SELF ->
              _menhir_run_038 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | PLUS ->
              _menhir_run_039 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | NULL ->
              _menhir_run_040 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | NONE ->
              _menhir_run_041 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | MINUS ->
              _menhir_run_042 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | LPAREN ->
              _menhir_run_043 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | LBRACK ->
              _menhir_run_045 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | LBRACE ->
              _menhir_run_046 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | INT _v ->
              _menhir_run_056 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | INC ->
              _menhir_run_057 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | IDENT _v ->
              _menhir_run_058 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | FLOAT _v ->
              _menhir_run_059 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | FALSE ->
              _menhir_run_060 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | DEC ->
              _menhir_run_061 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | BITNOT ->
              _menhir_run_062 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | _ ->
              _eRR ())
      | MINUS ->
          let _menhir_stack = MenhirCell1_mul_expr (_menhir_stack, _menhir_s, _v, _startpos) in
          let _startpos = _menhir_lexbuf.Lexing.lex_start_p in
          let _menhir_stack = MenhirCell0_MINUS (_menhir_stack, _startpos) in
          let _menhir_s = MenhirState087 in
          let _tok = _menhir_lexer _menhir_lexbuf in
          (match (_tok : MenhirBasics.token) with
          | TRUE ->
              _menhir_run_034 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | SUPER ->
              _menhir_run_035 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | STRING _v ->
              _menhir_run_037 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | SELF ->
              _menhir_run_038 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | PLUS ->
              _menhir_run_039 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | NULL ->
              _menhir_run_040 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | NONE ->
              _menhir_run_041 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | MINUS ->
              _menhir_run_042 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | LPAREN ->
              _menhir_run_043 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | LBRACK ->
              _menhir_run_045 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | LBRACE ->
              _menhir_run_046 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | INT _v ->
              _menhir_run_056 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | INC ->
              _menhir_run_057 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | IDENT _v ->
              _menhir_run_058 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | FLOAT _v ->
              _menhir_run_059 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | FALSE ->
              _menhir_run_060 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | DEC ->
              _menhir_run_061 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | BITNOT ->
              _menhir_run_062 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | _ ->
              _eRR ())
      | ADD_ASSIGN | AND | AS | ASSERT | ASSIGN | BITAND | BITNOT | BITOR | BITXOR | BREAK | COLON | COMMA | CONTINUE | DARROW | DEC | DEF | DEL | DIV_ASSIGN | ELSE | ENUM | EQEQ | FALSE | FLOAT _ | FLOORDIV_ASSIGN | FOR | FROM | GE | GLOBAL | GT | IDENT _ | IF | IMPORT | IN | INC | INT _ | IS | LAMBDA | LBRACE | LBRACK | LE | LPAREN | LSHIFT | LT | MOD_ASSIGN | MUL_ASSIGN | NEQ | NONE | NONLOCAL | NOT | NULL | OR | PASS | POW_ASSIGN | RAISE | RBRACE | RBRACK | RETURN | RPAREN | RSHIFT | SELF | SEMI | STRING _ | STRUCT | SUB_ASSIGN | SUPER | TRUE | TRY | WHILE | WITH | YIELD ->
          let (_startpos_e_, e) = (_startpos, _v) in
          let _v = _menhir_action_001 e in
          _menhir_goto_add_expr _menhir_stack _menhir_lexbuf _menhir_lexer _startpos_e_ _v _menhir_s _tok
      | _ ->
          _menhir_fail ()
  
  and _menhir_goto_add_expr : type  ttv_stack. ttv_stack -> _ -> _ -> _ -> _ -> (ttv_stack, _menhir_box_program) _menhir_state -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok ->
      match _menhir_s with
      | MenhirState085 ->
          _menhir_run_086 _menhir_stack _menhir_lexbuf _menhir_lexer _v _tok
      | MenhirState087 ->
          _menhir_run_088 _menhir_stack _menhir_lexbuf _menhir_lexer _v _tok
      | MenhirState033 ->
          _menhir_run_092 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState036 ->
          _menhir_run_092 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState043 ->
          _menhir_run_092 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState044 ->
          _menhir_run_092 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState045 ->
          _menhir_run_092 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState046 ->
          _menhir_run_092 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState055 ->
          _menhir_run_092 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState066 ->
          _menhir_run_092 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState068 ->
          _menhir_run_092 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState074 ->
          _menhir_run_092 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState090 ->
          _menhir_run_092 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState093 ->
          _menhir_run_092 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState095 ->
          _menhir_run_092 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState097 ->
          _menhir_run_092 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState099 ->
          _menhir_run_092 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState101 ->
          _menhir_run_092 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState105 ->
          _menhir_run_092 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState107 ->
          _menhir_run_092 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState110 ->
          _menhir_run_092 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState111 ->
          _menhir_run_092 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState115 ->
          _menhir_run_092 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState118 ->
          _menhir_run_092 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState121 ->
          _menhir_run_092 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState124 ->
          _menhir_run_092 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState126 ->
          _menhir_run_092 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState128 ->
          _menhir_run_092 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState134 ->
          _menhir_run_092 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState136 ->
          _menhir_run_092 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState138 ->
          _menhir_run_092 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState140 ->
          _menhir_run_092 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState142 ->
          _menhir_run_092 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState144 ->
          _menhir_run_092 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState146 ->
          _menhir_run_092 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState148 ->
          _menhir_run_092 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState152 ->
          _menhir_run_092 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState156 ->
          _menhir_run_092 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState162 ->
          _menhir_run_092 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState164 ->
          _menhir_run_092 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState166 ->
          _menhir_run_092 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState169 ->
          _menhir_run_092 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState188 ->
          _menhir_run_092 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState191 ->
          _menhir_run_092 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState218 ->
          _menhir_run_092 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState219 ->
          _menhir_run_092 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState223 ->
          _menhir_run_092 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState225 ->
          _menhir_run_092 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState229 ->
          _menhir_run_092 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState231 ->
          _menhir_run_092 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState233 ->
          _menhir_run_092 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState236 ->
          _menhir_run_092 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState238 ->
          _menhir_run_092 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState239 ->
          _menhir_run_092 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState243 ->
          _menhir_run_092 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState254 ->
          _menhir_run_092 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState256 ->
          _menhir_run_092 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState258 ->
          _menhir_run_092 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState263 ->
          _menhir_run_092 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState264 ->
          _menhir_run_092 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState271 ->
          _menhir_run_092 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState274 ->
          _menhir_run_092 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState278 ->
          _menhir_run_092 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState280 ->
          _menhir_run_092 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState282 ->
          _menhir_run_092 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState292 ->
          _menhir_run_092 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState294 ->
          _menhir_run_092 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState297 ->
          _menhir_run_092 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState300 ->
          _menhir_run_092 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState302 ->
          _menhir_run_092 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState306 ->
          _menhir_run_092 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState313 ->
          _menhir_run_092 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | _ ->
          _menhir_fail ()
  
  and _menhir_run_086 : type  ttv_stack. (ttv_stack, _menhir_box_program) _menhir_cell1_mul_expr _menhir_cell0_PLUS -> _ -> _ -> _ -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _v _tok ->
      let MenhirCell0_PLUS (_menhir_stack, _) = _menhir_stack in
      let MenhirCell1_mul_expr (_menhir_stack, _menhir_s, e, _startpos_e_) = _menhir_stack in
      let a = _v in
      let _v = _menhir_action_002 _startpos_e_ a e in
      _menhir_goto_add_expr _menhir_stack _menhir_lexbuf _menhir_lexer _startpos_e_ _v _menhir_s _tok
  
  and _menhir_run_088 : type  ttv_stack. (ttv_stack, _menhir_box_program) _menhir_cell1_mul_expr _menhir_cell0_MINUS -> _ -> _ -> _ -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _v _tok ->
      let MenhirCell0_MINUS (_menhir_stack, _) = _menhir_stack in
      let MenhirCell1_mul_expr (_menhir_stack, _menhir_s, e, _startpos_e_) = _menhir_stack in
      let a = _v in
      let _v = _menhir_action_003 _startpos_e_ a e in
      _menhir_goto_add_expr _menhir_stack _menhir_lexbuf _menhir_lexer _startpos_e_ _v _menhir_s _tok
  
  and _menhir_run_092 : type  ttv_stack. ttv_stack -> _ -> _ -> _ -> _ -> (ttv_stack, _menhir_box_program) _menhir_state -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok ->
      match (_tok : MenhirBasics.token) with
      | LT ->
          let _menhir_stack = MenhirCell1_add_expr (_menhir_stack, _menhir_s, _v, _startpos) in
          let _menhir_s = MenhirState093 in
          let _tok = _menhir_lexer _menhir_lexbuf in
          (match (_tok : MenhirBasics.token) with
          | TRUE ->
              _menhir_run_034 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | SUPER ->
              _menhir_run_035 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | STRING _v ->
              _menhir_run_037 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | SELF ->
              _menhir_run_038 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | PLUS ->
              _menhir_run_039 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | NULL ->
              _menhir_run_040 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | NONE ->
              _menhir_run_041 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | MINUS ->
              _menhir_run_042 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | LPAREN ->
              _menhir_run_043 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | LBRACK ->
              _menhir_run_045 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | LBRACE ->
              _menhir_run_046 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | INT _v ->
              _menhir_run_056 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | INC ->
              _menhir_run_057 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | IDENT _v ->
              _menhir_run_058 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | FLOAT _v ->
              _menhir_run_059 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | FALSE ->
              _menhir_run_060 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | DEC ->
              _menhir_run_061 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | BITNOT ->
              _menhir_run_062 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | _ ->
              _eRR ())
      | LE ->
          let _menhir_stack = MenhirCell1_add_expr (_menhir_stack, _menhir_s, _v, _startpos) in
          let _menhir_s = MenhirState095 in
          let _tok = _menhir_lexer _menhir_lexbuf in
          (match (_tok : MenhirBasics.token) with
          | TRUE ->
              _menhir_run_034 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | SUPER ->
              _menhir_run_035 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | STRING _v ->
              _menhir_run_037 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | SELF ->
              _menhir_run_038 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | PLUS ->
              _menhir_run_039 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | NULL ->
              _menhir_run_040 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | NONE ->
              _menhir_run_041 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | MINUS ->
              _menhir_run_042 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | LPAREN ->
              _menhir_run_043 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | LBRACK ->
              _menhir_run_045 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | LBRACE ->
              _menhir_run_046 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | INT _v ->
              _menhir_run_056 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | INC ->
              _menhir_run_057 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | IDENT _v ->
              _menhir_run_058 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | FLOAT _v ->
              _menhir_run_059 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | FALSE ->
              _menhir_run_060 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | DEC ->
              _menhir_run_061 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | BITNOT ->
              _menhir_run_062 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | _ ->
              _eRR ())
      | GT ->
          let _menhir_stack = MenhirCell1_add_expr (_menhir_stack, _menhir_s, _v, _startpos) in
          let _menhir_s = MenhirState097 in
          let _tok = _menhir_lexer _menhir_lexbuf in
          (match (_tok : MenhirBasics.token) with
          | TRUE ->
              _menhir_run_034 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | SUPER ->
              _menhir_run_035 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | STRING _v ->
              _menhir_run_037 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | SELF ->
              _menhir_run_038 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | PLUS ->
              _menhir_run_039 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | NULL ->
              _menhir_run_040 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | NONE ->
              _menhir_run_041 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | MINUS ->
              _menhir_run_042 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | LPAREN ->
              _menhir_run_043 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | LBRACK ->
              _menhir_run_045 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | LBRACE ->
              _menhir_run_046 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | INT _v ->
              _menhir_run_056 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | INC ->
              _menhir_run_057 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | IDENT _v ->
              _menhir_run_058 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | FLOAT _v ->
              _menhir_run_059 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | FALSE ->
              _menhir_run_060 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | DEC ->
              _menhir_run_061 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | BITNOT ->
              _menhir_run_062 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | _ ->
              _eRR ())
      | GE ->
          let _menhir_stack = MenhirCell1_add_expr (_menhir_stack, _menhir_s, _v, _startpos) in
          let _menhir_s = MenhirState099 in
          let _tok = _menhir_lexer _menhir_lexbuf in
          (match (_tok : MenhirBasics.token) with
          | TRUE ->
              _menhir_run_034 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | SUPER ->
              _menhir_run_035 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | STRING _v ->
              _menhir_run_037 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | SELF ->
              _menhir_run_038 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | PLUS ->
              _menhir_run_039 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | NULL ->
              _menhir_run_040 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | NONE ->
              _menhir_run_041 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | MINUS ->
              _menhir_run_042 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | LPAREN ->
              _menhir_run_043 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | LBRACK ->
              _menhir_run_045 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | LBRACE ->
              _menhir_run_046 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | INT _v ->
              _menhir_run_056 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | INC ->
              _menhir_run_057 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | IDENT _v ->
              _menhir_run_058 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | FLOAT _v ->
              _menhir_run_059 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | FALSE ->
              _menhir_run_060 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | DEC ->
              _menhir_run_061 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | BITNOT ->
              _menhir_run_062 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | _ ->
              _eRR ())
      | ADD_ASSIGN | AND | AS | ASSERT | ASSIGN | BITAND | BITNOT | BITOR | BITXOR | BREAK | COLON | COMMA | CONTINUE | DARROW | DEC | DEF | DEL | DIV_ASSIGN | ELSE | ENUM | EQEQ | FALSE | FLOAT _ | FLOORDIV_ASSIGN | FOR | FROM | GLOBAL | IDENT _ | IF | IMPORT | IN | INC | INT _ | IS | LAMBDA | LBRACE | LBRACK | LPAREN | LSHIFT | MINUS | MOD_ASSIGN | MUL_ASSIGN | NEQ | NONE | NONLOCAL | NOT | NULL | OR | PASS | PLUS | POW_ASSIGN | RAISE | RBRACE | RBRACK | RETURN | RPAREN | RSHIFT | SELF | SEMI | STRING _ | STRUCT | SUB_ASSIGN | SUPER | TRUE | TRY | WHILE | WITH | YIELD ->
          let (_startpos_e_, e) = (_startpos, _v) in
          let _v = _menhir_action_023 e in
          _menhir_goto_cmp_expr _menhir_stack _menhir_lexbuf _menhir_lexer _startpos_e_ _v _menhir_s _tok
      | _ ->
          _menhir_fail ()
  
  and _menhir_goto_cmp_expr : type  ttv_stack. ttv_stack -> _ -> _ -> _ -> _ -> (ttv_stack, _menhir_box_program) _menhir_state -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok ->
      match _menhir_s with
      | MenhirState033 ->
          _menhir_run_089 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState036 ->
          _menhir_run_089 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState043 ->
          _menhir_run_089 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState044 ->
          _menhir_run_089 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState045 ->
          _menhir_run_089 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState046 ->
          _menhir_run_089 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState055 ->
          _menhir_run_089 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState066 ->
          _menhir_run_089 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState068 ->
          _menhir_run_089 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState074 ->
          _menhir_run_089 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState090 ->
          _menhir_run_089 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState101 ->
          _menhir_run_089 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState105 ->
          _menhir_run_089 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState107 ->
          _menhir_run_089 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState110 ->
          _menhir_run_089 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState111 ->
          _menhir_run_089 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState115 ->
          _menhir_run_089 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState118 ->
          _menhir_run_089 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState121 ->
          _menhir_run_089 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState124 ->
          _menhir_run_089 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState126 ->
          _menhir_run_089 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState128 ->
          _menhir_run_089 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState134 ->
          _menhir_run_089 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState136 ->
          _menhir_run_089 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState138 ->
          _menhir_run_089 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState140 ->
          _menhir_run_089 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState142 ->
          _menhir_run_089 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState144 ->
          _menhir_run_089 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState146 ->
          _menhir_run_089 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState148 ->
          _menhir_run_089 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState152 ->
          _menhir_run_089 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState156 ->
          _menhir_run_089 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState162 ->
          _menhir_run_089 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState164 ->
          _menhir_run_089 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState166 ->
          _menhir_run_089 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState169 ->
          _menhir_run_089 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState188 ->
          _menhir_run_089 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState191 ->
          _menhir_run_089 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState218 ->
          _menhir_run_089 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState219 ->
          _menhir_run_089 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState223 ->
          _menhir_run_089 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState225 ->
          _menhir_run_089 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState229 ->
          _menhir_run_089 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState231 ->
          _menhir_run_089 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState233 ->
          _menhir_run_089 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState236 ->
          _menhir_run_089 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState238 ->
          _menhir_run_089 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState239 ->
          _menhir_run_089 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState243 ->
          _menhir_run_089 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState254 ->
          _menhir_run_089 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState256 ->
          _menhir_run_089 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState258 ->
          _menhir_run_089 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState263 ->
          _menhir_run_089 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState264 ->
          _menhir_run_089 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState271 ->
          _menhir_run_089 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState274 ->
          _menhir_run_089 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState278 ->
          _menhir_run_089 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState280 ->
          _menhir_run_089 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState282 ->
          _menhir_run_089 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState292 ->
          _menhir_run_089 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState294 ->
          _menhir_run_089 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState297 ->
          _menhir_run_089 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState300 ->
          _menhir_run_089 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState302 ->
          _menhir_run_089 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState306 ->
          _menhir_run_089 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState313 ->
          _menhir_run_089 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState093 ->
          _menhir_run_094 _menhir_stack _menhir_lexbuf _menhir_lexer _v _tok
      | MenhirState095 ->
          _menhir_run_096 _menhir_stack _menhir_lexbuf _menhir_lexer _v _tok
      | MenhirState097 ->
          _menhir_run_098 _menhir_stack _menhir_lexbuf _menhir_lexer _v _tok
      | MenhirState099 ->
          _menhir_run_100 _menhir_stack _menhir_lexbuf _menhir_lexer _v _tok
      | _ ->
          _menhir_fail ()
  
  and _menhir_run_089 : type  ttv_stack. ttv_stack -> _ -> _ -> _ -> _ -> (ttv_stack, _menhir_box_program) _menhir_state -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok ->
      match (_tok : MenhirBasics.token) with
      | RSHIFT ->
          let _menhir_stack = MenhirCell1_cmp_expr (_menhir_stack, _menhir_s, _v, _startpos) in
          let _menhir_s = MenhirState090 in
          let _tok = _menhir_lexer _menhir_lexbuf in
          (match (_tok : MenhirBasics.token) with
          | TRUE ->
              _menhir_run_034 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | SUPER ->
              _menhir_run_035 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | STRING _v ->
              _menhir_run_037 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | SELF ->
              _menhir_run_038 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | PLUS ->
              _menhir_run_039 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | NULL ->
              _menhir_run_040 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | NONE ->
              _menhir_run_041 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | MINUS ->
              _menhir_run_042 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | LPAREN ->
              _menhir_run_043 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | LBRACK ->
              _menhir_run_045 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | LBRACE ->
              _menhir_run_046 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | INT _v ->
              _menhir_run_056 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | INC ->
              _menhir_run_057 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | IDENT _v ->
              _menhir_run_058 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | FLOAT _v ->
              _menhir_run_059 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | FALSE ->
              _menhir_run_060 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | DEC ->
              _menhir_run_061 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | BITNOT ->
              _menhir_run_062 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | _ ->
              _eRR ())
      | LSHIFT ->
          let _menhir_stack = MenhirCell1_cmp_expr (_menhir_stack, _menhir_s, _v, _startpos) in
          let _menhir_s = MenhirState101 in
          let _tok = _menhir_lexer _menhir_lexbuf in
          (match (_tok : MenhirBasics.token) with
          | TRUE ->
              _menhir_run_034 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | SUPER ->
              _menhir_run_035 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | STRING _v ->
              _menhir_run_037 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | SELF ->
              _menhir_run_038 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | PLUS ->
              _menhir_run_039 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | NULL ->
              _menhir_run_040 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | NONE ->
              _menhir_run_041 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | MINUS ->
              _menhir_run_042 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | LPAREN ->
              _menhir_run_043 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | LBRACK ->
              _menhir_run_045 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | LBRACE ->
              _menhir_run_046 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | INT _v ->
              _menhir_run_056 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | INC ->
              _menhir_run_057 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | IDENT _v ->
              _menhir_run_058 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | FLOAT _v ->
              _menhir_run_059 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | FALSE ->
              _menhir_run_060 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | DEC ->
              _menhir_run_061 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | BITNOT ->
              _menhir_run_062 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | _ ->
              _eRR ())
      | ADD_ASSIGN | AND | AS | ASSERT | ASSIGN | BITAND | BITNOT | BITOR | BITXOR | BREAK | COLON | COMMA | CONTINUE | DARROW | DEC | DEF | DEL | DIV_ASSIGN | ELSE | ENUM | EQEQ | FALSE | FLOAT _ | FLOORDIV_ASSIGN | FOR | FROM | GLOBAL | IDENT _ | IF | IMPORT | IN | INC | INT _ | IS | LAMBDA | LBRACE | LBRACK | LPAREN | MINUS | MOD_ASSIGN | MUL_ASSIGN | NEQ | NONE | NONLOCAL | NOT | NULL | OR | PASS | PLUS | POW_ASSIGN | RAISE | RBRACE | RBRACK | RETURN | RPAREN | SELF | SEMI | STRING _ | STRUCT | SUB_ASSIGN | SUPER | TRUE | TRY | WHILE | WITH | YIELD ->
          let (_startpos_e_, e) = (_startpos, _v) in
          let _v = _menhir_action_137 e in
          _menhir_goto_shift_expr _menhir_stack _menhir_lexbuf _menhir_lexer _startpos_e_ _v _menhir_s _tok
      | _ ->
          _menhir_fail ()
  
  and _menhir_goto_shift_expr : type  ttv_stack. ttv_stack -> _ -> _ -> _ -> _ -> (ttv_stack, _menhir_box_program) _menhir_state -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok ->
      match _menhir_s with
      | MenhirState033 ->
          _menhir_run_073 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState036 ->
          _menhir_run_073 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState043 ->
          _menhir_run_073 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState044 ->
          _menhir_run_073 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState045 ->
          _menhir_run_073 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState046 ->
          _menhir_run_073 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState055 ->
          _menhir_run_073 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState066 ->
          _menhir_run_073 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState068 ->
          _menhir_run_073 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState074 ->
          _menhir_run_073 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState105 ->
          _menhir_run_073 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState107 ->
          _menhir_run_073 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState110 ->
          _menhir_run_073 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState111 ->
          _menhir_run_073 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState115 ->
          _menhir_run_073 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState118 ->
          _menhir_run_073 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState121 ->
          _menhir_run_073 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState124 ->
          _menhir_run_073 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState126 ->
          _menhir_run_073 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState128 ->
          _menhir_run_073 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState134 ->
          _menhir_run_073 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState136 ->
          _menhir_run_073 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState138 ->
          _menhir_run_073 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState140 ->
          _menhir_run_073 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState142 ->
          _menhir_run_073 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState144 ->
          _menhir_run_073 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState146 ->
          _menhir_run_073 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState148 ->
          _menhir_run_073 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState152 ->
          _menhir_run_073 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState156 ->
          _menhir_run_073 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState162 ->
          _menhir_run_073 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState164 ->
          _menhir_run_073 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState166 ->
          _menhir_run_073 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState169 ->
          _menhir_run_073 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState188 ->
          _menhir_run_073 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState191 ->
          _menhir_run_073 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState218 ->
          _menhir_run_073 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState219 ->
          _menhir_run_073 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState223 ->
          _menhir_run_073 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState225 ->
          _menhir_run_073 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState229 ->
          _menhir_run_073 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState231 ->
          _menhir_run_073 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState233 ->
          _menhir_run_073 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState236 ->
          _menhir_run_073 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState238 ->
          _menhir_run_073 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState239 ->
          _menhir_run_073 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState243 ->
          _menhir_run_073 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState254 ->
          _menhir_run_073 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState256 ->
          _menhir_run_073 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState258 ->
          _menhir_run_073 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState263 ->
          _menhir_run_073 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState264 ->
          _menhir_run_073 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState271 ->
          _menhir_run_073 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState274 ->
          _menhir_run_073 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState278 ->
          _menhir_run_073 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState280 ->
          _menhir_run_073 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState282 ->
          _menhir_run_073 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState292 ->
          _menhir_run_073 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState294 ->
          _menhir_run_073 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState297 ->
          _menhir_run_073 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState300 ->
          _menhir_run_073 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState302 ->
          _menhir_run_073 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState306 ->
          _menhir_run_073 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState313 ->
          _menhir_run_073 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState090 ->
          _menhir_run_091 _menhir_stack _menhir_lexbuf _menhir_lexer _v _tok
      | MenhirState101 ->
          _menhir_run_102 _menhir_stack _menhir_lexbuf _menhir_lexer _v _tok
      | _ ->
          _menhir_fail ()
  
  and _menhir_run_073 : type  ttv_stack. ttv_stack -> _ -> _ -> _ -> _ -> (ttv_stack, _menhir_box_program) _menhir_state -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok ->
      match (_tok : MenhirBasics.token) with
      | BITAND ->
          let _menhir_stack = MenhirCell1_shift_expr (_menhir_stack, _menhir_s, _v, _startpos) in
          let _menhir_s = MenhirState074 in
          let _tok = _menhir_lexer _menhir_lexbuf in
          (match (_tok : MenhirBasics.token) with
          | TRUE ->
              _menhir_run_034 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | SUPER ->
              _menhir_run_035 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | STRING _v ->
              _menhir_run_037 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | SELF ->
              _menhir_run_038 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | PLUS ->
              _menhir_run_039 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | NULL ->
              _menhir_run_040 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | NONE ->
              _menhir_run_041 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | MINUS ->
              _menhir_run_042 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | LPAREN ->
              _menhir_run_043 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | LBRACK ->
              _menhir_run_045 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | LBRACE ->
              _menhir_run_046 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | INT _v ->
              _menhir_run_056 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | INC ->
              _menhir_run_057 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | IDENT _v ->
              _menhir_run_058 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | FLOAT _v ->
              _menhir_run_059 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | FALSE ->
              _menhir_run_060 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | DEC ->
              _menhir_run_061 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | BITNOT ->
              _menhir_run_062 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | _ ->
              _eRR ())
      | ADD_ASSIGN | AND | AS | ASSERT | ASSIGN | BITNOT | BITOR | BITXOR | BREAK | COLON | COMMA | CONTINUE | DARROW | DEC | DEF | DEL | DIV_ASSIGN | ELSE | ENUM | EQEQ | FALSE | FLOAT _ | FLOORDIV_ASSIGN | FOR | FROM | GLOBAL | IDENT _ | IF | IMPORT | IN | INC | INT _ | IS | LAMBDA | LBRACE | LBRACK | LPAREN | MINUS | MOD_ASSIGN | MUL_ASSIGN | NEQ | NONE | NONLOCAL | NOT | NULL | OR | PASS | PLUS | POW_ASSIGN | RAISE | RBRACE | RBRACK | RETURN | RPAREN | SELF | SEMI | STRING _ | STRUCT | SUB_ASSIGN | SUPER | TRUE | TRY | WHILE | WITH | YIELD ->
          let (_startpos_e_, e) = (_startpos, _v) in
          let _v = _menhir_action_015 e in
          _menhir_goto_bit_and_expr _menhir_stack _menhir_lexbuf _menhir_lexer _startpos_e_ _v _menhir_s _tok
      | _ ->
          _menhir_fail ()
  
  and _menhir_goto_bit_and_expr : type  ttv_stack. ttv_stack -> _ -> _ -> _ -> _ -> (ttv_stack, _menhir_box_program) _menhir_state -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok ->
      match _menhir_s with
      | MenhirState074 ->
          _menhir_run_103 _menhir_stack _menhir_lexbuf _menhir_lexer _v _tok
      | MenhirState033 ->
          _menhir_run_120 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState036 ->
          _menhir_run_120 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState043 ->
          _menhir_run_120 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState044 ->
          _menhir_run_120 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState045 ->
          _menhir_run_120 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState046 ->
          _menhir_run_120 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState055 ->
          _menhir_run_120 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState066 ->
          _menhir_run_120 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState068 ->
          _menhir_run_120 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState105 ->
          _menhir_run_120 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState107 ->
          _menhir_run_120 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState110 ->
          _menhir_run_120 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState111 ->
          _menhir_run_120 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState115 ->
          _menhir_run_120 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState118 ->
          _menhir_run_120 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState121 ->
          _menhir_run_120 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState124 ->
          _menhir_run_120 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState126 ->
          _menhir_run_120 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState128 ->
          _menhir_run_120 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState134 ->
          _menhir_run_120 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState136 ->
          _menhir_run_120 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState138 ->
          _menhir_run_120 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState140 ->
          _menhir_run_120 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState142 ->
          _menhir_run_120 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState144 ->
          _menhir_run_120 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState146 ->
          _menhir_run_120 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState148 ->
          _menhir_run_120 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState152 ->
          _menhir_run_120 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState156 ->
          _menhir_run_120 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState162 ->
          _menhir_run_120 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState164 ->
          _menhir_run_120 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState166 ->
          _menhir_run_120 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState169 ->
          _menhir_run_120 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState188 ->
          _menhir_run_120 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState191 ->
          _menhir_run_120 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState218 ->
          _menhir_run_120 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState219 ->
          _menhir_run_120 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState223 ->
          _menhir_run_120 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState225 ->
          _menhir_run_120 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState229 ->
          _menhir_run_120 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState231 ->
          _menhir_run_120 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState233 ->
          _menhir_run_120 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState236 ->
          _menhir_run_120 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState238 ->
          _menhir_run_120 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState239 ->
          _menhir_run_120 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState243 ->
          _menhir_run_120 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState254 ->
          _menhir_run_120 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState256 ->
          _menhir_run_120 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState258 ->
          _menhir_run_120 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState263 ->
          _menhir_run_120 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState264 ->
          _menhir_run_120 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState271 ->
          _menhir_run_120 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState274 ->
          _menhir_run_120 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState278 ->
          _menhir_run_120 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState280 ->
          _menhir_run_120 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState282 ->
          _menhir_run_120 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState292 ->
          _menhir_run_120 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState294 ->
          _menhir_run_120 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState297 ->
          _menhir_run_120 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState300 ->
          _menhir_run_120 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState302 ->
          _menhir_run_120 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState306 ->
          _menhir_run_120 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState313 ->
          _menhir_run_120 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | _ ->
          _menhir_fail ()
  
  and _menhir_run_103 : type  ttv_stack. (ttv_stack, _menhir_box_program) _menhir_cell1_shift_expr -> _ -> _ -> _ -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _v _tok ->
      let MenhirCell1_shift_expr (_menhir_stack, _menhir_s, e, _startpos_e_) = _menhir_stack in
      let a = _v in
      let _v = _menhir_action_016 _startpos_e_ a e in
      _menhir_goto_bit_and_expr _menhir_stack _menhir_lexbuf _menhir_lexer _startpos_e_ _v _menhir_s _tok
  
  and _menhir_run_120 : type  ttv_stack. ttv_stack -> _ -> _ -> _ -> _ -> (ttv_stack, _menhir_box_program) _menhir_state -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok ->
      match (_tok : MenhirBasics.token) with
      | BITXOR ->
          let _menhir_stack = MenhirCell1_bit_and_expr (_menhir_stack, _menhir_s, _v, _startpos) in
          let _menhir_s = MenhirState121 in
          let _tok = _menhir_lexer _menhir_lexbuf in
          (match (_tok : MenhirBasics.token) with
          | TRUE ->
              _menhir_run_034 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | SUPER ->
              _menhir_run_035 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | STRING _v ->
              _menhir_run_037 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | SELF ->
              _menhir_run_038 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | PLUS ->
              _menhir_run_039 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | NULL ->
              _menhir_run_040 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | NONE ->
              _menhir_run_041 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | MINUS ->
              _menhir_run_042 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | LPAREN ->
              _menhir_run_043 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | LBRACK ->
              _menhir_run_045 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | LBRACE ->
              _menhir_run_046 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | INT _v ->
              _menhir_run_056 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | INC ->
              _menhir_run_057 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | IDENT _v ->
              _menhir_run_058 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | FLOAT _v ->
              _menhir_run_059 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | FALSE ->
              _menhir_run_060 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | DEC ->
              _menhir_run_061 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | BITNOT ->
              _menhir_run_062 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | _ ->
              _eRR ())
      | ADD_ASSIGN | AND | AS | ASSERT | ASSIGN | BITNOT | BITOR | BREAK | COLON | COMMA | CONTINUE | DARROW | DEC | DEF | DEL | DIV_ASSIGN | ELSE | ENUM | EQEQ | FALSE | FLOAT _ | FLOORDIV_ASSIGN | FOR | FROM | GLOBAL | IDENT _ | IF | IMPORT | IN | INC | INT _ | IS | LAMBDA | LBRACE | LBRACK | LPAREN | MINUS | MOD_ASSIGN | MUL_ASSIGN | NEQ | NONE | NONLOCAL | NOT | NULL | OR | PASS | PLUS | POW_ASSIGN | RAISE | RBRACE | RBRACK | RETURN | RPAREN | SELF | SEMI | STRING _ | STRUCT | SUB_ASSIGN | SUPER | TRUE | TRY | WHILE | WITH | YIELD ->
          let (_startpos_e_, e) = (_startpos, _v) in
          let _v = _menhir_action_019 e in
          _menhir_goto_bit_xor_expr _menhir_stack _menhir_lexbuf _menhir_lexer _startpos_e_ _v _menhir_s _tok
      | _ ->
          _menhir_fail ()
  
  and _menhir_goto_bit_xor_expr : type  ttv_stack. ttv_stack -> _ -> _ -> _ -> _ -> (ttv_stack, _menhir_box_program) _menhir_state -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok ->
      match _menhir_s with
      | MenhirState033 ->
          _menhir_run_117 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState036 ->
          _menhir_run_117 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState043 ->
          _menhir_run_117 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState044 ->
          _menhir_run_117 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState045 ->
          _menhir_run_117 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState046 ->
          _menhir_run_117 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState055 ->
          _menhir_run_117 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState066 ->
          _menhir_run_117 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState068 ->
          _menhir_run_117 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState105 ->
          _menhir_run_117 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState107 ->
          _menhir_run_117 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState110 ->
          _menhir_run_117 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState111 ->
          _menhir_run_117 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState115 ->
          _menhir_run_117 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState118 ->
          _menhir_run_117 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState124 ->
          _menhir_run_117 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState126 ->
          _menhir_run_117 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState128 ->
          _menhir_run_117 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState134 ->
          _menhir_run_117 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState136 ->
          _menhir_run_117 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState138 ->
          _menhir_run_117 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState140 ->
          _menhir_run_117 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState142 ->
          _menhir_run_117 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState144 ->
          _menhir_run_117 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState146 ->
          _menhir_run_117 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState148 ->
          _menhir_run_117 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState152 ->
          _menhir_run_117 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState156 ->
          _menhir_run_117 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState162 ->
          _menhir_run_117 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState164 ->
          _menhir_run_117 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState166 ->
          _menhir_run_117 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState169 ->
          _menhir_run_117 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState188 ->
          _menhir_run_117 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState191 ->
          _menhir_run_117 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState218 ->
          _menhir_run_117 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState219 ->
          _menhir_run_117 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState223 ->
          _menhir_run_117 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState225 ->
          _menhir_run_117 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState229 ->
          _menhir_run_117 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState231 ->
          _menhir_run_117 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState233 ->
          _menhir_run_117 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState236 ->
          _menhir_run_117 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState238 ->
          _menhir_run_117 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState239 ->
          _menhir_run_117 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState243 ->
          _menhir_run_117 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState254 ->
          _menhir_run_117 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState256 ->
          _menhir_run_117 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState258 ->
          _menhir_run_117 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState263 ->
          _menhir_run_117 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState264 ->
          _menhir_run_117 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState271 ->
          _menhir_run_117 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState274 ->
          _menhir_run_117 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState278 ->
          _menhir_run_117 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState280 ->
          _menhir_run_117 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState282 ->
          _menhir_run_117 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState292 ->
          _menhir_run_117 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState294 ->
          _menhir_run_117 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState297 ->
          _menhir_run_117 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState300 ->
          _menhir_run_117 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState302 ->
          _menhir_run_117 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState306 ->
          _menhir_run_117 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState313 ->
          _menhir_run_117 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState121 ->
          _menhir_run_122 _menhir_stack _menhir_lexbuf _menhir_lexer _v _tok
      | _ ->
          _menhir_fail ()
  
  and _menhir_run_117 : type  ttv_stack. ttv_stack -> _ -> _ -> _ -> _ -> (ttv_stack, _menhir_box_program) _menhir_state -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok ->
      match (_tok : MenhirBasics.token) with
      | BITOR ->
          let _menhir_stack = MenhirCell1_bit_xor_expr (_menhir_stack, _menhir_s, _v, _startpos) in
          let _menhir_s = MenhirState118 in
          let _tok = _menhir_lexer _menhir_lexbuf in
          (match (_tok : MenhirBasics.token) with
          | TRUE ->
              _menhir_run_034 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | SUPER ->
              _menhir_run_035 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | STRING _v ->
              _menhir_run_037 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | SELF ->
              _menhir_run_038 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | PLUS ->
              _menhir_run_039 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | NULL ->
              _menhir_run_040 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | NONE ->
              _menhir_run_041 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | MINUS ->
              _menhir_run_042 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | LPAREN ->
              _menhir_run_043 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | LBRACK ->
              _menhir_run_045 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | LBRACE ->
              _menhir_run_046 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | INT _v ->
              _menhir_run_056 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | INC ->
              _menhir_run_057 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | IDENT _v ->
              _menhir_run_058 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | FLOAT _v ->
              _menhir_run_059 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | FALSE ->
              _menhir_run_060 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | DEC ->
              _menhir_run_061 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | BITNOT ->
              _menhir_run_062 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | _ ->
              _eRR ())
      | ADD_ASSIGN | AND | AS | ASSERT | ASSIGN | BITNOT | BREAK | COLON | COMMA | CONTINUE | DARROW | DEC | DEF | DEL | DIV_ASSIGN | ELSE | ENUM | EQEQ | FALSE | FLOAT _ | FLOORDIV_ASSIGN | FOR | FROM | GLOBAL | IDENT _ | IF | IMPORT | IN | INC | INT _ | IS | LAMBDA | LBRACE | LBRACK | LPAREN | MINUS | MOD_ASSIGN | MUL_ASSIGN | NEQ | NONE | NONLOCAL | NOT | NULL | OR | PASS | PLUS | POW_ASSIGN | RAISE | RBRACE | RBRACK | RETURN | RPAREN | SELF | SEMI | STRING _ | STRUCT | SUB_ASSIGN | SUPER | TRUE | TRY | WHILE | WITH | YIELD ->
          let (_startpos_e_, e) = (_startpos, _v) in
          let _v = _menhir_action_017 e in
          _menhir_goto_bit_or_expr _menhir_stack _menhir_lexbuf _menhir_lexer _startpos_e_ _v _menhir_s _tok
      | _ ->
          _menhir_fail ()
  
  and _menhir_goto_bit_or_expr : type  ttv_stack. ttv_stack -> _ -> _ -> _ -> _ -> (ttv_stack, _menhir_box_program) _menhir_state -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok ->
      match _menhir_s with
      | MenhirState118 ->
          _menhir_run_119 _menhir_stack _menhir_lexbuf _menhir_lexer _v _tok
      | MenhirState033 ->
          _menhir_run_123 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState036 ->
          _menhir_run_123 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState043 ->
          _menhir_run_123 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState044 ->
          _menhir_run_123 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState045 ->
          _menhir_run_123 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState046 ->
          _menhir_run_123 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState055 ->
          _menhir_run_123 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState066 ->
          _menhir_run_123 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState068 ->
          _menhir_run_123 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState105 ->
          _menhir_run_123 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState107 ->
          _menhir_run_123 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState110 ->
          _menhir_run_123 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState111 ->
          _menhir_run_123 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState115 ->
          _menhir_run_123 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState124 ->
          _menhir_run_123 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState126 ->
          _menhir_run_123 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState128 ->
          _menhir_run_123 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState134 ->
          _menhir_run_123 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState136 ->
          _menhir_run_123 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState138 ->
          _menhir_run_123 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState140 ->
          _menhir_run_123 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState142 ->
          _menhir_run_123 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState144 ->
          _menhir_run_123 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState146 ->
          _menhir_run_123 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState148 ->
          _menhir_run_123 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState152 ->
          _menhir_run_123 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState156 ->
          _menhir_run_123 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState162 ->
          _menhir_run_123 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState164 ->
          _menhir_run_123 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState166 ->
          _menhir_run_123 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState169 ->
          _menhir_run_123 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState188 ->
          _menhir_run_123 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState191 ->
          _menhir_run_123 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState218 ->
          _menhir_run_123 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState219 ->
          _menhir_run_123 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState223 ->
          _menhir_run_123 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState225 ->
          _menhir_run_123 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState229 ->
          _menhir_run_123 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState231 ->
          _menhir_run_123 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState233 ->
          _menhir_run_123 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState236 ->
          _menhir_run_123 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState238 ->
          _menhir_run_123 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState239 ->
          _menhir_run_123 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState243 ->
          _menhir_run_123 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState254 ->
          _menhir_run_123 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState256 ->
          _menhir_run_123 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState258 ->
          _menhir_run_123 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState263 ->
          _menhir_run_123 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState264 ->
          _menhir_run_123 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState271 ->
          _menhir_run_123 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState274 ->
          _menhir_run_123 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState278 ->
          _menhir_run_123 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState280 ->
          _menhir_run_123 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState282 ->
          _menhir_run_123 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState292 ->
          _menhir_run_123 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState294 ->
          _menhir_run_123 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState297 ->
          _menhir_run_123 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState300 ->
          _menhir_run_123 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState302 ->
          _menhir_run_123 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState306 ->
          _menhir_run_123 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState313 ->
          _menhir_run_123 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | _ ->
          _menhir_fail ()
  
  and _menhir_run_119 : type  ttv_stack. (ttv_stack, _menhir_box_program) _menhir_cell1_bit_xor_expr -> _ -> _ -> _ -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _v _tok ->
      let MenhirCell1_bit_xor_expr (_menhir_stack, _menhir_s, e, _startpos_e_) = _menhir_stack in
      let a = _v in
      let _v = _menhir_action_018 _startpos_e_ a e in
      _menhir_goto_bit_or_expr _menhir_stack _menhir_lexbuf _menhir_lexer _startpos_e_ _v _menhir_s _tok
  
  and _menhir_run_123 : type  ttv_stack. ttv_stack -> _ -> _ -> _ -> _ -> (ttv_stack, _menhir_box_program) _menhir_state -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok ->
      match (_tok : MenhirBasics.token) with
      | NEQ ->
          let _menhir_stack = MenhirCell1_bit_or_expr (_menhir_stack, _menhir_s, _v, _startpos) in
          let _menhir_s = MenhirState124 in
          let _tok = _menhir_lexer _menhir_lexbuf in
          (match (_tok : MenhirBasics.token) with
          | TRUE ->
              _menhir_run_034 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | SUPER ->
              _menhir_run_035 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | STRING _v ->
              _menhir_run_037 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | SELF ->
              _menhir_run_038 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | PLUS ->
              _menhir_run_039 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | NULL ->
              _menhir_run_040 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | NONE ->
              _menhir_run_041 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | MINUS ->
              _menhir_run_042 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | LPAREN ->
              _menhir_run_043 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | LBRACK ->
              _menhir_run_045 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | LBRACE ->
              _menhir_run_046 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | INT _v ->
              _menhir_run_056 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | INC ->
              _menhir_run_057 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | IDENT _v ->
              _menhir_run_058 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | FLOAT _v ->
              _menhir_run_059 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | FALSE ->
              _menhir_run_060 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | DEC ->
              _menhir_run_061 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | BITNOT ->
              _menhir_run_062 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | _ ->
              _eRR ())
      | EQEQ ->
          let _menhir_stack = MenhirCell1_bit_or_expr (_menhir_stack, _menhir_s, _v, _startpos) in
          let _menhir_s = MenhirState126 in
          let _tok = _menhir_lexer _menhir_lexbuf in
          (match (_tok : MenhirBasics.token) with
          | TRUE ->
              _menhir_run_034 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | SUPER ->
              _menhir_run_035 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | STRING _v ->
              _menhir_run_037 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | SELF ->
              _menhir_run_038 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | PLUS ->
              _menhir_run_039 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | NULL ->
              _menhir_run_040 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | NONE ->
              _menhir_run_041 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | MINUS ->
              _menhir_run_042 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | LPAREN ->
              _menhir_run_043 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | LBRACK ->
              _menhir_run_045 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | LBRACE ->
              _menhir_run_046 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | INT _v ->
              _menhir_run_056 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | INC ->
              _menhir_run_057 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | IDENT _v ->
              _menhir_run_058 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | FLOAT _v ->
              _menhir_run_059 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | FALSE ->
              _menhir_run_060 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | DEC ->
              _menhir_run_061 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | BITNOT ->
              _menhir_run_062 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | _ ->
              _eRR ())
      | ADD_ASSIGN | AND | AS | ASSERT | ASSIGN | BITNOT | BREAK | COLON | COMMA | CONTINUE | DARROW | DEC | DEF | DEL | DIV_ASSIGN | ELSE | ENUM | FALSE | FLOAT _ | FLOORDIV_ASSIGN | FOR | FROM | GLOBAL | IDENT _ | IF | IMPORT | IN | INC | INT _ | IS | LAMBDA | LBRACE | LBRACK | LPAREN | MINUS | MOD_ASSIGN | MUL_ASSIGN | NONE | NONLOCAL | NOT | NULL | OR | PASS | PLUS | POW_ASSIGN | RAISE | RBRACE | RBRACK | RETURN | RPAREN | SELF | SEMI | STRING _ | STRUCT | SUB_ASSIGN | SUPER | TRUE | TRY | WHILE | WITH | YIELD ->
          let (_startpos_e_, e) = (_startpos, _v) in
          let _v = _menhir_action_040 e in
          _menhir_goto_eq_expr _menhir_stack _menhir_lexbuf _menhir_lexer _startpos_e_ _v _menhir_s _tok
      | _ ->
          _menhir_fail ()
  
  and _menhir_goto_eq_expr : type  ttv_stack. ttv_stack -> _ -> _ -> _ -> _ -> (ttv_stack, _menhir_box_program) _menhir_state -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok ->
      match _menhir_s with
      | MenhirState033 ->
          _menhir_run_113 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState036 ->
          _menhir_run_113 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState043 ->
          _menhir_run_113 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState044 ->
          _menhir_run_113 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState045 ->
          _menhir_run_113 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState046 ->
          _menhir_run_113 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState055 ->
          _menhir_run_113 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState066 ->
          _menhir_run_113 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState068 ->
          _menhir_run_113 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState105 ->
          _menhir_run_113 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState107 ->
          _menhir_run_113 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState110 ->
          _menhir_run_113 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState111 ->
          _menhir_run_113 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState115 ->
          _menhir_run_113 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState128 ->
          _menhir_run_113 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState134 ->
          _menhir_run_113 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState136 ->
          _menhir_run_113 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState138 ->
          _menhir_run_113 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState140 ->
          _menhir_run_113 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState142 ->
          _menhir_run_113 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState144 ->
          _menhir_run_113 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState146 ->
          _menhir_run_113 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState148 ->
          _menhir_run_113 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState152 ->
          _menhir_run_113 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState156 ->
          _menhir_run_113 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState162 ->
          _menhir_run_113 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState164 ->
          _menhir_run_113 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState166 ->
          _menhir_run_113 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState169 ->
          _menhir_run_113 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState188 ->
          _menhir_run_113 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState191 ->
          _menhir_run_113 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState218 ->
          _menhir_run_113 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState219 ->
          _menhir_run_113 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState223 ->
          _menhir_run_113 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState225 ->
          _menhir_run_113 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState229 ->
          _menhir_run_113 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState231 ->
          _menhir_run_113 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState233 ->
          _menhir_run_113 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState236 ->
          _menhir_run_113 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState238 ->
          _menhir_run_113 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState239 ->
          _menhir_run_113 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState243 ->
          _menhir_run_113 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState254 ->
          _menhir_run_113 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState256 ->
          _menhir_run_113 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState258 ->
          _menhir_run_113 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState263 ->
          _menhir_run_113 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState264 ->
          _menhir_run_113 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState271 ->
          _menhir_run_113 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState274 ->
          _menhir_run_113 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState278 ->
          _menhir_run_113 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState280 ->
          _menhir_run_113 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState282 ->
          _menhir_run_113 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState292 ->
          _menhir_run_113 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState294 ->
          _menhir_run_113 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState297 ->
          _menhir_run_113 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState300 ->
          _menhir_run_113 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState302 ->
          _menhir_run_113 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState306 ->
          _menhir_run_113 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState313 ->
          _menhir_run_113 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState124 ->
          _menhir_run_125 _menhir_stack _menhir_lexbuf _menhir_lexer _v _tok
      | MenhirState126 ->
          _menhir_run_127 _menhir_stack _menhir_lexbuf _menhir_lexer _v _tok
      | _ ->
          _menhir_fail ()
  
  and _menhir_run_113 : type  ttv_stack. ttv_stack -> _ -> _ -> _ -> _ -> (ttv_stack, _menhir_box_program) _menhir_state -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok ->
      match (_tok : MenhirBasics.token) with
      | NOT ->
          let _menhir_stack = MenhirCell1_eq_expr (_menhir_stack, _menhir_s, _v, _startpos) in
          let _startpos = _menhir_lexbuf.Lexing.lex_start_p in
          let _menhir_stack = MenhirCell0_NOT (_menhir_stack, _startpos) in
          let _tok = _menhir_lexer _menhir_lexbuf in
          (match (_tok : MenhirBasics.token) with
          | IN ->
              let _menhir_s = MenhirState115 in
              let _tok = _menhir_lexer _menhir_lexbuf in
              (match (_tok : MenhirBasics.token) with
              | TRUE ->
                  _menhir_run_034 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
              | SUPER ->
                  _menhir_run_035 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
              | STRING _v ->
                  _menhir_run_037 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
              | SELF ->
                  _menhir_run_038 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
              | PLUS ->
                  _menhir_run_039 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
              | NULL ->
                  _menhir_run_040 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
              | NONE ->
                  _menhir_run_041 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
              | MINUS ->
                  _menhir_run_042 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
              | LPAREN ->
                  _menhir_run_043 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
              | LBRACK ->
                  _menhir_run_045 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
              | LBRACE ->
                  _menhir_run_046 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
              | INT _v ->
                  _menhir_run_056 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
              | INC ->
                  _menhir_run_057 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
              | IDENT _v ->
                  _menhir_run_058 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
              | FLOAT _v ->
                  _menhir_run_059 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
              | FALSE ->
                  _menhir_run_060 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
              | DEC ->
                  _menhir_run_061 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
              | BITNOT ->
                  _menhir_run_062 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
              | _ ->
                  _eRR ())
          | _ ->
              _eRR ())
      | IN ->
          let _menhir_stack = MenhirCell1_eq_expr (_menhir_stack, _menhir_s, _v, _startpos) in
          let _menhir_s = MenhirState128 in
          let _tok = _menhir_lexer _menhir_lexbuf in
          (match (_tok : MenhirBasics.token) with
          | TRUE ->
              _menhir_run_034 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | SUPER ->
              _menhir_run_035 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | STRING _v ->
              _menhir_run_037 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | SELF ->
              _menhir_run_038 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | PLUS ->
              _menhir_run_039 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | NULL ->
              _menhir_run_040 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | NONE ->
              _menhir_run_041 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | MINUS ->
              _menhir_run_042 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | LPAREN ->
              _menhir_run_043 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | LBRACK ->
              _menhir_run_045 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | LBRACE ->
              _menhir_run_046 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | INT _v ->
              _menhir_run_056 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | INC ->
              _menhir_run_057 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | IDENT _v ->
              _menhir_run_058 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | FLOAT _v ->
              _menhir_run_059 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | FALSE ->
              _menhir_run_060 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | DEC ->
              _menhir_run_061 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | BITNOT ->
              _menhir_run_062 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | _ ->
              _eRR ())
      | ADD_ASSIGN | AND | AS | ASSERT | ASSIGN | BITNOT | BREAK | COLON | COMMA | CONTINUE | DARROW | DEC | DEF | DEL | DIV_ASSIGN | ELSE | ENUM | FALSE | FLOAT _ | FLOORDIV_ASSIGN | FOR | FROM | GLOBAL | IDENT _ | IF | IMPORT | INC | INT _ | IS | LAMBDA | LBRACE | LBRACK | LPAREN | MINUS | MOD_ASSIGN | MUL_ASSIGN | NONE | NONLOCAL | NULL | OR | PASS | PLUS | POW_ASSIGN | RAISE | RBRACE | RBRACK | RETURN | RPAREN | SELF | SEMI | STRING _ | STRUCT | SUB_ASSIGN | SUPER | TRUE | TRY | WHILE | WITH | YIELD ->
          let (_startpos_e_, e) = (_startpos, _v) in
          let _v = _menhir_action_056 e in
          _menhir_goto_in_expr _menhir_stack _menhir_lexbuf _menhir_lexer _startpos_e_ _v _menhir_s _tok
      | _ ->
          _menhir_fail ()
  
  and _menhir_goto_in_expr : type  ttv_stack. ttv_stack -> _ -> _ -> _ -> _ -> (ttv_stack, _menhir_box_program) _menhir_state -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok ->
      match _menhir_s with
      | MenhirState033 ->
          _menhir_run_109 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState036 ->
          _menhir_run_109 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState043 ->
          _menhir_run_109 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState044 ->
          _menhir_run_109 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState045 ->
          _menhir_run_109 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState046 ->
          _menhir_run_109 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState055 ->
          _menhir_run_109 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState066 ->
          _menhir_run_109 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState068 ->
          _menhir_run_109 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState105 ->
          _menhir_run_109 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState107 ->
          _menhir_run_109 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState110 ->
          _menhir_run_109 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState111 ->
          _menhir_run_109 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState134 ->
          _menhir_run_109 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState136 ->
          _menhir_run_109 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState138 ->
          _menhir_run_109 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState140 ->
          _menhir_run_109 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState142 ->
          _menhir_run_109 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState144 ->
          _menhir_run_109 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState146 ->
          _menhir_run_109 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState148 ->
          _menhir_run_109 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState152 ->
          _menhir_run_109 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState156 ->
          _menhir_run_109 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState162 ->
          _menhir_run_109 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState164 ->
          _menhir_run_109 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState166 ->
          _menhir_run_109 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState169 ->
          _menhir_run_109 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState188 ->
          _menhir_run_109 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState191 ->
          _menhir_run_109 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState218 ->
          _menhir_run_109 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState219 ->
          _menhir_run_109 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState223 ->
          _menhir_run_109 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState225 ->
          _menhir_run_109 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState229 ->
          _menhir_run_109 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState231 ->
          _menhir_run_109 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState233 ->
          _menhir_run_109 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState236 ->
          _menhir_run_109 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState238 ->
          _menhir_run_109 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState239 ->
          _menhir_run_109 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState243 ->
          _menhir_run_109 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState254 ->
          _menhir_run_109 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState256 ->
          _menhir_run_109 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState258 ->
          _menhir_run_109 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState263 ->
          _menhir_run_109 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState264 ->
          _menhir_run_109 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState271 ->
          _menhir_run_109 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState274 ->
          _menhir_run_109 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState278 ->
          _menhir_run_109 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState280 ->
          _menhir_run_109 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState282 ->
          _menhir_run_109 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState292 ->
          _menhir_run_109 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState294 ->
          _menhir_run_109 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState297 ->
          _menhir_run_109 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState300 ->
          _menhir_run_109 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState302 ->
          _menhir_run_109 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState306 ->
          _menhir_run_109 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState313 ->
          _menhir_run_109 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState115 ->
          _menhir_run_116 _menhir_stack _menhir_lexbuf _menhir_lexer _v _tok
      | MenhirState128 ->
          _menhir_run_129 _menhir_stack _menhir_lexbuf _menhir_lexer _v _tok
      | _ ->
          _menhir_fail ()
  
  and _menhir_run_109 : type  ttv_stack. ttv_stack -> _ -> _ -> _ -> _ -> (ttv_stack, _menhir_box_program) _menhir_state -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok ->
      match (_tok : MenhirBasics.token) with
      | IS ->
          let _menhir_stack = MenhirCell1_in_expr (_menhir_stack, _menhir_s, _v, _startpos) in
          let _menhir_s = MenhirState110 in
          let _tok = _menhir_lexer _menhir_lexbuf in
          (match (_tok : MenhirBasics.token) with
          | TRUE ->
              _menhir_run_034 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | SUPER ->
              _menhir_run_035 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | STRING _v ->
              _menhir_run_037 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | SELF ->
              _menhir_run_038 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | PLUS ->
              _menhir_run_039 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | NULL ->
              _menhir_run_040 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | NOT ->
              let _startpos = _menhir_lexbuf.Lexing.lex_start_p in
              let _menhir_stack = MenhirCell1_NOT (_menhir_stack, _menhir_s, _startpos) in
              let _menhir_s = MenhirState111 in
              let _tok = _menhir_lexer _menhir_lexbuf in
              (match (_tok : MenhirBasics.token) with
              | TRUE ->
                  _menhir_run_034 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
              | SUPER ->
                  _menhir_run_035 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
              | STRING _v ->
                  _menhir_run_037 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
              | SELF ->
                  _menhir_run_038 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
              | PLUS ->
                  _menhir_run_039 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
              | NULL ->
                  _menhir_run_040 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
              | NONE ->
                  _menhir_run_041 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
              | MINUS ->
                  _menhir_run_042 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
              | LPAREN ->
                  _menhir_run_043 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
              | LBRACK ->
                  _menhir_run_045 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
              | LBRACE ->
                  _menhir_run_046 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
              | INT _v ->
                  _menhir_run_056 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
              | INC ->
                  _menhir_run_057 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
              | IDENT _v ->
                  _menhir_run_058 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
              | FLOAT _v ->
                  _menhir_run_059 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
              | FALSE ->
                  _menhir_run_060 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
              | DEC ->
                  _menhir_run_061 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
              | BITNOT ->
                  _menhir_run_062 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
              | _ ->
                  _eRR ())
          | NONE ->
              _menhir_run_041 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | MINUS ->
              _menhir_run_042 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | LPAREN ->
              _menhir_run_043 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | LBRACK ->
              _menhir_run_045 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | LBRACE ->
              _menhir_run_046 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | INT _v ->
              _menhir_run_056 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | INC ->
              _menhir_run_057 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | IDENT _v ->
              _menhir_run_058 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | FLOAT _v ->
              _menhir_run_059 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | FALSE ->
              _menhir_run_060 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | DEC ->
              _menhir_run_061 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | BITNOT ->
              _menhir_run_062 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | _ ->
              _eRR ())
      | ADD_ASSIGN | AND | AS | ASSERT | ASSIGN | BITNOT | BREAK | COLON | COMMA | CONTINUE | DARROW | DEC | DEF | DEL | DIV_ASSIGN | ELSE | ENUM | FALSE | FLOAT _ | FLOORDIV_ASSIGN | FOR | FROM | GLOBAL | IDENT _ | IF | IMPORT | IN | INC | INT _ | LAMBDA | LBRACE | LBRACK | LPAREN | MINUS | MOD_ASSIGN | MUL_ASSIGN | NONE | NONLOCAL | NOT | NULL | OR | PASS | PLUS | POW_ASSIGN | RAISE | RBRACE | RBRACK | RETURN | RPAREN | SELF | SEMI | STRING _ | STRUCT | SUB_ASSIGN | SUPER | TRUE | TRY | WHILE | WITH | YIELD ->
          let (_startpos_e_, e) = (_startpos, _v) in
          let _v = _menhir_action_059 e in
          _menhir_goto_is_expr _menhir_stack _menhir_lexbuf _menhir_lexer _startpos_e_ _v _menhir_s _tok
      | _ ->
          _menhir_fail ()
  
  and _menhir_goto_is_expr : type  ttv_stack. ttv_stack -> _ -> _ -> _ -> _ -> (ttv_stack, _menhir_box_program) _menhir_state -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok ->
      match _menhir_s with
      | MenhirState033 ->
          _menhir_run_108 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState036 ->
          _menhir_run_108 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState043 ->
          _menhir_run_108 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState044 ->
          _menhir_run_108 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState045 ->
          _menhir_run_108 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState046 ->
          _menhir_run_108 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState055 ->
          _menhir_run_108 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState066 ->
          _menhir_run_108 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState068 ->
          _menhir_run_108 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState105 ->
          _menhir_run_108 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState107 ->
          _menhir_run_108 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState134 ->
          _menhir_run_108 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState136 ->
          _menhir_run_108 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState138 ->
          _menhir_run_108 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState140 ->
          _menhir_run_108 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState142 ->
          _menhir_run_108 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState144 ->
          _menhir_run_108 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState146 ->
          _menhir_run_108 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState148 ->
          _menhir_run_108 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState152 ->
          _menhir_run_108 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState156 ->
          _menhir_run_108 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState162 ->
          _menhir_run_108 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState164 ->
          _menhir_run_108 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState166 ->
          _menhir_run_108 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState169 ->
          _menhir_run_108 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState188 ->
          _menhir_run_108 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState191 ->
          _menhir_run_108 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState218 ->
          _menhir_run_108 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState219 ->
          _menhir_run_108 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState223 ->
          _menhir_run_108 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState225 ->
          _menhir_run_108 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState229 ->
          _menhir_run_108 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState231 ->
          _menhir_run_108 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState233 ->
          _menhir_run_108 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState236 ->
          _menhir_run_108 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState238 ->
          _menhir_run_108 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState239 ->
          _menhir_run_108 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState243 ->
          _menhir_run_108 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState254 ->
          _menhir_run_108 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState256 ->
          _menhir_run_108 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState258 ->
          _menhir_run_108 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState263 ->
          _menhir_run_108 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState264 ->
          _menhir_run_108 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState271 ->
          _menhir_run_108 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState274 ->
          _menhir_run_108 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState278 ->
          _menhir_run_108 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState280 ->
          _menhir_run_108 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState282 ->
          _menhir_run_108 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState292 ->
          _menhir_run_108 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState294 ->
          _menhir_run_108 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState297 ->
          _menhir_run_108 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState300 ->
          _menhir_run_108 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState302 ->
          _menhir_run_108 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState306 ->
          _menhir_run_108 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState313 ->
          _menhir_run_108 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState111 ->
          _menhir_run_112 _menhir_stack _menhir_lexbuf _menhir_lexer _v _tok
      | MenhirState110 ->
          _menhir_run_130 _menhir_stack _menhir_lexbuf _menhir_lexer _v _tok
      | _ ->
          _menhir_fail ()
  
  and _menhir_run_108 : type  ttv_stack. ttv_stack -> _ -> _ -> _ -> _ -> (ttv_stack, _menhir_box_program) _menhir_state -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok ->
      let (_startpos_e_, e) = (_startpos, _v) in
      let _v = _menhir_action_085 e in
      _menhir_goto_not_expr _menhir_stack _menhir_lexbuf _menhir_lexer _startpos_e_ _v _menhir_s _tok
  
  and _menhir_goto_not_expr : type  ttv_stack. ttv_stack -> _ -> _ -> _ -> _ -> (ttv_stack, _menhir_box_program) _menhir_state -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok ->
      match _menhir_s with
      | MenhirState033 ->
          _menhir_run_106 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState036 ->
          _menhir_run_106 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState043 ->
          _menhir_run_106 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState045 ->
          _menhir_run_106 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState046 ->
          _menhir_run_106 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState055 ->
          _menhir_run_106 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState066 ->
          _menhir_run_106 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState068 ->
          _menhir_run_106 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState105 ->
          _menhir_run_106 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState107 ->
          _menhir_run_106 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState134 ->
          _menhir_run_106 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState136 ->
          _menhir_run_106 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState138 ->
          _menhir_run_106 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState140 ->
          _menhir_run_106 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState142 ->
          _menhir_run_106 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState144 ->
          _menhir_run_106 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState146 ->
          _menhir_run_106 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState148 ->
          _menhir_run_106 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState152 ->
          _menhir_run_106 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState156 ->
          _menhir_run_106 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState162 ->
          _menhir_run_106 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState164 ->
          _menhir_run_106 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState166 ->
          _menhir_run_106 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState169 ->
          _menhir_run_106 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState188 ->
          _menhir_run_106 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState191 ->
          _menhir_run_106 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState218 ->
          _menhir_run_106 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState219 ->
          _menhir_run_106 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState223 ->
          _menhir_run_106 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState225 ->
          _menhir_run_106 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState229 ->
          _menhir_run_106 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState231 ->
          _menhir_run_106 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState233 ->
          _menhir_run_106 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState236 ->
          _menhir_run_106 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState238 ->
          _menhir_run_106 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState239 ->
          _menhir_run_106 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState243 ->
          _menhir_run_106 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState254 ->
          _menhir_run_106 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState256 ->
          _menhir_run_106 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState258 ->
          _menhir_run_106 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState263 ->
          _menhir_run_106 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState264 ->
          _menhir_run_106 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState271 ->
          _menhir_run_106 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState274 ->
          _menhir_run_106 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState278 ->
          _menhir_run_106 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState280 ->
          _menhir_run_106 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState282 ->
          _menhir_run_106 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState292 ->
          _menhir_run_106 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState294 ->
          _menhir_run_106 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState297 ->
          _menhir_run_106 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState300 ->
          _menhir_run_106 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState302 ->
          _menhir_run_106 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState306 ->
          _menhir_run_106 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState313 ->
          _menhir_run_106 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState044 ->
          _menhir_run_196 _menhir_stack _menhir_lexbuf _menhir_lexer _v _tok
      | _ ->
          _menhir_fail ()
  
  and _menhir_run_106 : type  ttv_stack. ttv_stack -> _ -> _ -> _ -> _ -> (ttv_stack, _menhir_box_program) _menhir_state -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok ->
      match (_tok : MenhirBasics.token) with
      | AND ->
          let _menhir_stack = MenhirCell1_not_expr (_menhir_stack, _menhir_s, _v, _startpos) in
          let _menhir_s = MenhirState107 in
          let _tok = _menhir_lexer _menhir_lexbuf in
          (match (_tok : MenhirBasics.token) with
          | TRUE ->
              _menhir_run_034 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | SUPER ->
              _menhir_run_035 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | STRING _v ->
              _menhir_run_037 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | SELF ->
              _menhir_run_038 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | PLUS ->
              _menhir_run_039 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | NULL ->
              _menhir_run_040 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | NOT ->
              _menhir_run_044 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | NONE ->
              _menhir_run_041 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | MINUS ->
              _menhir_run_042 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | LPAREN ->
              _menhir_run_043 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | LBRACK ->
              _menhir_run_045 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | LBRACE ->
              _menhir_run_046 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | INT _v ->
              _menhir_run_056 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | INC ->
              _menhir_run_057 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | IDENT _v ->
              _menhir_run_058 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | FLOAT _v ->
              _menhir_run_059 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | FALSE ->
              _menhir_run_060 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | DEC ->
              _menhir_run_061 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | BITNOT ->
              _menhir_run_062 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | _ ->
              _eRR ())
      | ADD_ASSIGN | AS | ASSERT | ASSIGN | BITNOT | BREAK | COLON | COMMA | CONTINUE | DARROW | DEC | DEF | DEL | DIV_ASSIGN | ELSE | ENUM | FALSE | FLOAT _ | FLOORDIV_ASSIGN | FOR | FROM | GLOBAL | IDENT _ | IF | IMPORT | IN | INC | INT _ | LAMBDA | LBRACE | LBRACK | LPAREN | MINUS | MOD_ASSIGN | MUL_ASSIGN | NONE | NONLOCAL | NOT | NULL | OR | PASS | PLUS | POW_ASSIGN | RAISE | RBRACE | RBRACK | RETURN | RPAREN | SELF | SEMI | STRING _ | STRUCT | SUB_ASSIGN | SUPER | TRUE | TRY | WHILE | WITH | YIELD ->
          let (_startpos_e_, e) = (_startpos, _v) in
          let _v = _menhir_action_004 e in
          _menhir_goto_and_expr _menhir_stack _menhir_lexbuf _menhir_lexer _startpos_e_ _v _menhir_s _tok
      | _ ->
          _menhir_fail ()
  
  and _menhir_goto_and_expr : type  ttv_stack. ttv_stack -> _ -> _ -> _ -> _ -> (ttv_stack, _menhir_box_program) _menhir_state -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok ->
      match _menhir_s with
      | MenhirState107 ->
          _menhir_run_131 _menhir_stack _menhir_lexbuf _menhir_lexer _v _tok
      | MenhirState033 ->
          _menhir_run_133 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState036 ->
          _menhir_run_133 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState043 ->
          _menhir_run_133 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState045 ->
          _menhir_run_133 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState046 ->
          _menhir_run_133 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState055 ->
          _menhir_run_133 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState066 ->
          _menhir_run_133 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState068 ->
          _menhir_run_133 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState105 ->
          _menhir_run_133 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState134 ->
          _menhir_run_133 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState136 ->
          _menhir_run_133 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState138 ->
          _menhir_run_133 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState140 ->
          _menhir_run_133 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState142 ->
          _menhir_run_133 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState144 ->
          _menhir_run_133 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState146 ->
          _menhir_run_133 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState148 ->
          _menhir_run_133 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState152 ->
          _menhir_run_133 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState156 ->
          _menhir_run_133 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState162 ->
          _menhir_run_133 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState164 ->
          _menhir_run_133 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState166 ->
          _menhir_run_133 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState169 ->
          _menhir_run_133 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState188 ->
          _menhir_run_133 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState191 ->
          _menhir_run_133 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState218 ->
          _menhir_run_133 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState219 ->
          _menhir_run_133 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState223 ->
          _menhir_run_133 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState225 ->
          _menhir_run_133 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState229 ->
          _menhir_run_133 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState231 ->
          _menhir_run_133 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState233 ->
          _menhir_run_133 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState236 ->
          _menhir_run_133 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState238 ->
          _menhir_run_133 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState239 ->
          _menhir_run_133 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState243 ->
          _menhir_run_133 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState254 ->
          _menhir_run_133 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState256 ->
          _menhir_run_133 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState258 ->
          _menhir_run_133 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState263 ->
          _menhir_run_133 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState264 ->
          _menhir_run_133 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState271 ->
          _menhir_run_133 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState274 ->
          _menhir_run_133 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState278 ->
          _menhir_run_133 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState280 ->
          _menhir_run_133 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState282 ->
          _menhir_run_133 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState292 ->
          _menhir_run_133 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState294 ->
          _menhir_run_133 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState297 ->
          _menhir_run_133 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState300 ->
          _menhir_run_133 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState302 ->
          _menhir_run_133 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState306 ->
          _menhir_run_133 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState313 ->
          _menhir_run_133 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | _ ->
          _menhir_fail ()
  
  and _menhir_run_131 : type  ttv_stack. (ttv_stack, _menhir_box_program) _menhir_cell1_not_expr -> _ -> _ -> _ -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _v _tok ->
      let MenhirCell1_not_expr (_menhir_stack, _menhir_s, e, _startpos_e_) = _menhir_stack in
      let a = _v in
      let _v = _menhir_action_005 _startpos_e_ a e in
      _menhir_goto_and_expr _menhir_stack _menhir_lexbuf _menhir_lexer _startpos_e_ _v _menhir_s _tok
  
  and _menhir_run_133 : type  ttv_stack. ttv_stack -> _ -> _ -> _ -> _ -> (ttv_stack, _menhir_box_program) _menhir_state -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok ->
      match (_tok : MenhirBasics.token) with
      | OR ->
          let _menhir_stack = MenhirCell1_and_expr (_menhir_stack, _menhir_s, _v, _startpos) in
          let _menhir_s = MenhirState134 in
          let _tok = _menhir_lexer _menhir_lexbuf in
          (match (_tok : MenhirBasics.token) with
          | TRUE ->
              _menhir_run_034 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | SUPER ->
              _menhir_run_035 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | STRING _v ->
              _menhir_run_037 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | SELF ->
              _menhir_run_038 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | PLUS ->
              _menhir_run_039 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | NULL ->
              _menhir_run_040 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | NOT ->
              _menhir_run_044 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | NONE ->
              _menhir_run_041 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | MINUS ->
              _menhir_run_042 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | LPAREN ->
              _menhir_run_043 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | LBRACK ->
              _menhir_run_045 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | LBRACE ->
              _menhir_run_046 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | INT _v ->
              _menhir_run_056 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | INC ->
              _menhir_run_057 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | IDENT _v ->
              _menhir_run_058 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | FLOAT _v ->
              _menhir_run_059 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | FALSE ->
              _menhir_run_060 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | DEC ->
              _menhir_run_061 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | BITNOT ->
              _menhir_run_062 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | _ ->
              _eRR ())
      | ADD_ASSIGN | AS | ASSERT | ASSIGN | BITNOT | BREAK | COLON | COMMA | CONTINUE | DARROW | DEC | DEF | DEL | DIV_ASSIGN | ELSE | ENUM | FALSE | FLOAT _ | FLOORDIV_ASSIGN | FOR | FROM | GLOBAL | IDENT _ | IF | IMPORT | IN | INC | INT _ | LAMBDA | LBRACE | LBRACK | LPAREN | MINUS | MOD_ASSIGN | MUL_ASSIGN | NONE | NONLOCAL | NOT | NULL | PASS | PLUS | POW_ASSIGN | RAISE | RBRACE | RBRACK | RETURN | RPAREN | SELF | SEMI | STRING _ | STRUCT | SUB_ASSIGN | SUPER | TRUE | TRY | WHILE | WITH | YIELD ->
          let (_startpos_e_, e) = (_startpos, _v) in
          let _v = _menhir_action_095 e in
          _menhir_goto_or_expr _menhir_stack _menhir_lexbuf _menhir_lexer _startpos_e_ _v _menhir_s _tok
      | _ ->
          _menhir_fail ()
  
  and _menhir_goto_or_expr : type  ttv_stack. ttv_stack -> _ -> _ -> _ -> _ -> (ttv_stack, _menhir_box_program) _menhir_state -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok ->
      match _menhir_s with
      | MenhirState033 ->
          _menhir_run_104 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState036 ->
          _menhir_run_104 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState043 ->
          _menhir_run_104 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState045 ->
          _menhir_run_104 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState046 ->
          _menhir_run_104 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState055 ->
          _menhir_run_104 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState066 ->
          _menhir_run_104 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState068 ->
          _menhir_run_104 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState105 ->
          _menhir_run_104 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState136 ->
          _menhir_run_104 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState138 ->
          _menhir_run_104 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState140 ->
          _menhir_run_104 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState142 ->
          _menhir_run_104 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState144 ->
          _menhir_run_104 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState146 ->
          _menhir_run_104 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState148 ->
          _menhir_run_104 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState152 ->
          _menhir_run_104 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState156 ->
          _menhir_run_104 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState162 ->
          _menhir_run_104 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState164 ->
          _menhir_run_104 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState166 ->
          _menhir_run_104 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState169 ->
          _menhir_run_104 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState188 ->
          _menhir_run_104 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState191 ->
          _menhir_run_104 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState218 ->
          _menhir_run_104 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState219 ->
          _menhir_run_104 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState223 ->
          _menhir_run_104 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState225 ->
          _menhir_run_104 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState229 ->
          _menhir_run_104 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState231 ->
          _menhir_run_104 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState233 ->
          _menhir_run_104 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState236 ->
          _menhir_run_104 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState238 ->
          _menhir_run_104 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState239 ->
          _menhir_run_104 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState243 ->
          _menhir_run_104 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState254 ->
          _menhir_run_104 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState256 ->
          _menhir_run_104 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState258 ->
          _menhir_run_104 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState263 ->
          _menhir_run_104 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState264 ->
          _menhir_run_104 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState271 ->
          _menhir_run_104 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState274 ->
          _menhir_run_104 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState278 ->
          _menhir_run_104 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState280 ->
          _menhir_run_104 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState282 ->
          _menhir_run_104 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState292 ->
          _menhir_run_104 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState294 ->
          _menhir_run_104 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState297 ->
          _menhir_run_104 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState300 ->
          _menhir_run_104 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState302 ->
          _menhir_run_104 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState306 ->
          _menhir_run_104 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState313 ->
          _menhir_run_104 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState134 ->
          _menhir_run_135 _menhir_stack _menhir_lexbuf _menhir_lexer _v _tok
      | _ ->
          _menhir_fail ()
  
  and _menhir_run_104 : type  ttv_stack. ttv_stack -> _ -> _ -> _ -> _ -> (ttv_stack, _menhir_box_program) _menhir_state -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok ->
      match (_tok : MenhirBasics.token) with
      | SUB_ASSIGN ->
          let _menhir_stack = MenhirCell1_or_expr (_menhir_stack, _menhir_s, _v, _startpos) in
          let _menhir_s = MenhirState105 in
          let _tok = _menhir_lexer _menhir_lexbuf in
          (match (_tok : MenhirBasics.token) with
          | TRUE ->
              _menhir_run_034 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | SUPER ->
              _menhir_run_035 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | STRING _v ->
              _menhir_run_037 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | SELF ->
              _menhir_run_038 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | PLUS ->
              _menhir_run_039 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | NULL ->
              _menhir_run_040 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | NOT ->
              _menhir_run_044 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | NONE ->
              _menhir_run_041 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | MINUS ->
              _menhir_run_042 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | LPAREN ->
              _menhir_run_043 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | LBRACK ->
              _menhir_run_045 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | LBRACE ->
              _menhir_run_046 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | INT _v ->
              _menhir_run_056 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | INC ->
              _menhir_run_057 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | IDENT _v ->
              _menhir_run_058 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | FLOAT _v ->
              _menhir_run_059 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | FALSE ->
              _menhir_run_060 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | DEC ->
              _menhir_run_061 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | BITNOT ->
              _menhir_run_062 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | _ ->
              _eRR ())
      | POW_ASSIGN ->
          let _menhir_stack = MenhirCell1_or_expr (_menhir_stack, _menhir_s, _v, _startpos) in
          let _menhir_s = MenhirState136 in
          let _tok = _menhir_lexer _menhir_lexbuf in
          (match (_tok : MenhirBasics.token) with
          | TRUE ->
              _menhir_run_034 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | SUPER ->
              _menhir_run_035 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | STRING _v ->
              _menhir_run_037 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | SELF ->
              _menhir_run_038 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | PLUS ->
              _menhir_run_039 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | NULL ->
              _menhir_run_040 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | NOT ->
              _menhir_run_044 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | NONE ->
              _menhir_run_041 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | MINUS ->
              _menhir_run_042 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | LPAREN ->
              _menhir_run_043 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | LBRACK ->
              _menhir_run_045 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | LBRACE ->
              _menhir_run_046 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | INT _v ->
              _menhir_run_056 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | INC ->
              _menhir_run_057 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | IDENT _v ->
              _menhir_run_058 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | FLOAT _v ->
              _menhir_run_059 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | FALSE ->
              _menhir_run_060 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | DEC ->
              _menhir_run_061 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | BITNOT ->
              _menhir_run_062 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | _ ->
              _eRR ())
      | MUL_ASSIGN ->
          let _menhir_stack = MenhirCell1_or_expr (_menhir_stack, _menhir_s, _v, _startpos) in
          let _menhir_s = MenhirState138 in
          let _tok = _menhir_lexer _menhir_lexbuf in
          (match (_tok : MenhirBasics.token) with
          | TRUE ->
              _menhir_run_034 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | SUPER ->
              _menhir_run_035 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | STRING _v ->
              _menhir_run_037 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | SELF ->
              _menhir_run_038 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | PLUS ->
              _menhir_run_039 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | NULL ->
              _menhir_run_040 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | NOT ->
              _menhir_run_044 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | NONE ->
              _menhir_run_041 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | MINUS ->
              _menhir_run_042 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | LPAREN ->
              _menhir_run_043 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | LBRACK ->
              _menhir_run_045 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | LBRACE ->
              _menhir_run_046 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | INT _v ->
              _menhir_run_056 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | INC ->
              _menhir_run_057 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | IDENT _v ->
              _menhir_run_058 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | FLOAT _v ->
              _menhir_run_059 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | FALSE ->
              _menhir_run_060 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | DEC ->
              _menhir_run_061 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | BITNOT ->
              _menhir_run_062 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | _ ->
              _eRR ())
      | MOD_ASSIGN ->
          let _menhir_stack = MenhirCell1_or_expr (_menhir_stack, _menhir_s, _v, _startpos) in
          let _menhir_s = MenhirState140 in
          let _tok = _menhir_lexer _menhir_lexbuf in
          (match (_tok : MenhirBasics.token) with
          | TRUE ->
              _menhir_run_034 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | SUPER ->
              _menhir_run_035 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | STRING _v ->
              _menhir_run_037 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | SELF ->
              _menhir_run_038 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | PLUS ->
              _menhir_run_039 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | NULL ->
              _menhir_run_040 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | NOT ->
              _menhir_run_044 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | NONE ->
              _menhir_run_041 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | MINUS ->
              _menhir_run_042 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | LPAREN ->
              _menhir_run_043 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | LBRACK ->
              _menhir_run_045 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | LBRACE ->
              _menhir_run_046 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | INT _v ->
              _menhir_run_056 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | INC ->
              _menhir_run_057 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | IDENT _v ->
              _menhir_run_058 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | FLOAT _v ->
              _menhir_run_059 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | FALSE ->
              _menhir_run_060 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | DEC ->
              _menhir_run_061 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | BITNOT ->
              _menhir_run_062 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | _ ->
              _eRR ())
      | FLOORDIV_ASSIGN ->
          let _menhir_stack = MenhirCell1_or_expr (_menhir_stack, _menhir_s, _v, _startpos) in
          let _menhir_s = MenhirState142 in
          let _tok = _menhir_lexer _menhir_lexbuf in
          (match (_tok : MenhirBasics.token) with
          | TRUE ->
              _menhir_run_034 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | SUPER ->
              _menhir_run_035 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | STRING _v ->
              _menhir_run_037 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | SELF ->
              _menhir_run_038 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | PLUS ->
              _menhir_run_039 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | NULL ->
              _menhir_run_040 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | NOT ->
              _menhir_run_044 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | NONE ->
              _menhir_run_041 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | MINUS ->
              _menhir_run_042 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | LPAREN ->
              _menhir_run_043 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | LBRACK ->
              _menhir_run_045 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | LBRACE ->
              _menhir_run_046 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | INT _v ->
              _menhir_run_056 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | INC ->
              _menhir_run_057 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | IDENT _v ->
              _menhir_run_058 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | FLOAT _v ->
              _menhir_run_059 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | FALSE ->
              _menhir_run_060 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | DEC ->
              _menhir_run_061 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | BITNOT ->
              _menhir_run_062 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | _ ->
              _eRR ())
      | DIV_ASSIGN ->
          let _menhir_stack = MenhirCell1_or_expr (_menhir_stack, _menhir_s, _v, _startpos) in
          let _menhir_s = MenhirState144 in
          let _tok = _menhir_lexer _menhir_lexbuf in
          (match (_tok : MenhirBasics.token) with
          | TRUE ->
              _menhir_run_034 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | SUPER ->
              _menhir_run_035 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | STRING _v ->
              _menhir_run_037 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | SELF ->
              _menhir_run_038 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | PLUS ->
              _menhir_run_039 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | NULL ->
              _menhir_run_040 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | NOT ->
              _menhir_run_044 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | NONE ->
              _menhir_run_041 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | MINUS ->
              _menhir_run_042 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | LPAREN ->
              _menhir_run_043 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | LBRACK ->
              _menhir_run_045 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | LBRACE ->
              _menhir_run_046 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | INT _v ->
              _menhir_run_056 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | INC ->
              _menhir_run_057 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | IDENT _v ->
              _menhir_run_058 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | FLOAT _v ->
              _menhir_run_059 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | FALSE ->
              _menhir_run_060 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | DEC ->
              _menhir_run_061 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | BITNOT ->
              _menhir_run_062 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | _ ->
              _eRR ())
      | ASSIGN ->
          let _menhir_stack = MenhirCell1_or_expr (_menhir_stack, _menhir_s, _v, _startpos) in
          let _menhir_s = MenhirState146 in
          let _tok = _menhir_lexer _menhir_lexbuf in
          (match (_tok : MenhirBasics.token) with
          | TRUE ->
              _menhir_run_034 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | SUPER ->
              _menhir_run_035 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | STRING _v ->
              _menhir_run_037 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | SELF ->
              _menhir_run_038 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | PLUS ->
              _menhir_run_039 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | NULL ->
              _menhir_run_040 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | NOT ->
              _menhir_run_044 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | NONE ->
              _menhir_run_041 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | MINUS ->
              _menhir_run_042 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | LPAREN ->
              _menhir_run_043 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | LBRACK ->
              _menhir_run_045 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | LBRACE ->
              _menhir_run_046 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | INT _v ->
              _menhir_run_056 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | INC ->
              _menhir_run_057 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | IDENT _v ->
              _menhir_run_058 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | FLOAT _v ->
              _menhir_run_059 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | FALSE ->
              _menhir_run_060 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | DEC ->
              _menhir_run_061 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | BITNOT ->
              _menhir_run_062 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | _ ->
              _eRR ())
      | ADD_ASSIGN ->
          let _menhir_stack = MenhirCell1_or_expr (_menhir_stack, _menhir_s, _v, _startpos) in
          let _menhir_s = MenhirState148 in
          let _tok = _menhir_lexer _menhir_lexbuf in
          (match (_tok : MenhirBasics.token) with
          | TRUE ->
              _menhir_run_034 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | SUPER ->
              _menhir_run_035 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | STRING _v ->
              _menhir_run_037 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | SELF ->
              _menhir_run_038 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | PLUS ->
              _menhir_run_039 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | NULL ->
              _menhir_run_040 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | NOT ->
              _menhir_run_044 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | NONE ->
              _menhir_run_041 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | MINUS ->
              _menhir_run_042 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | LPAREN ->
              _menhir_run_043 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | LBRACK ->
              _menhir_run_045 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | LBRACE ->
              _menhir_run_046 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | INT _v ->
              _menhir_run_056 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | INC ->
              _menhir_run_057 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | IDENT _v ->
              _menhir_run_058 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | FLOAT _v ->
              _menhir_run_059 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | FALSE ->
              _menhir_run_060 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | DEC ->
              _menhir_run_061 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | BITNOT ->
              _menhir_run_062 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | _ ->
              _eRR ())
      | AS | ASSERT | BITNOT | BREAK | COLON | COMMA | CONTINUE | DARROW | DEC | DEF | DEL | ELSE | ENUM | FALSE | FLOAT _ | FOR | FROM | GLOBAL | IDENT _ | IF | IMPORT | IN | INC | INT _ | LAMBDA | LBRACE | LBRACK | LPAREN | MINUS | NONE | NONLOCAL | NOT | NULL | PASS | PLUS | RAISE | RBRACE | RBRACK | RETURN | RPAREN | SELF | SEMI | STRING _ | STRUCT | SUPER | TRUE | TRY | WHILE | WITH | YIELD ->
          let (_startpos_e_, e) = (_startpos, _v) in
          let _v = _menhir_action_006 e in
          _menhir_goto_assign_expr _menhir_stack _menhir_lexbuf _menhir_lexer _startpos_e_ _v _menhir_s _tok
      | _ ->
          _menhir_fail ()
  
  and _menhir_goto_assign_expr : type  ttv_stack. ttv_stack -> _ -> _ -> _ -> _ -> (ttv_stack, _menhir_box_program) _menhir_state -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok ->
      match _menhir_s with
      | MenhirState105 ->
          _menhir_run_132 _menhir_stack _menhir_lexbuf _menhir_lexer _v _tok
      | MenhirState136 ->
          _menhir_run_137 _menhir_stack _menhir_lexbuf _menhir_lexer _v _tok
      | MenhirState138 ->
          _menhir_run_139 _menhir_stack _menhir_lexbuf _menhir_lexer _v _tok
      | MenhirState140 ->
          _menhir_run_141 _menhir_stack _menhir_lexbuf _menhir_lexer _v _tok
      | MenhirState142 ->
          _menhir_run_143 _menhir_stack _menhir_lexbuf _menhir_lexer _v _tok
      | MenhirState144 ->
          _menhir_run_145 _menhir_stack _menhir_lexbuf _menhir_lexer _v _tok
      | MenhirState146 ->
          _menhir_run_147 _menhir_stack _menhir_lexbuf _menhir_lexer _v _tok
      | MenhirState148 ->
          _menhir_run_149 _menhir_stack _menhir_lexbuf _menhir_lexer _v _tok
      | MenhirState033 ->
          _menhir_run_150 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState036 ->
          _menhir_run_150 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState043 ->
          _menhir_run_150 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState045 ->
          _menhir_run_150 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState046 ->
          _menhir_run_150 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState055 ->
          _menhir_run_150 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState066 ->
          _menhir_run_150 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState068 ->
          _menhir_run_150 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState152 ->
          _menhir_run_150 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState156 ->
          _menhir_run_150 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState162 ->
          _menhir_run_150 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState164 ->
          _menhir_run_150 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState166 ->
          _menhir_run_150 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState169 ->
          _menhir_run_150 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState188 ->
          _menhir_run_150 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState191 ->
          _menhir_run_150 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState218 ->
          _menhir_run_150 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState219 ->
          _menhir_run_150 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState223 ->
          _menhir_run_150 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState225 ->
          _menhir_run_150 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState229 ->
          _menhir_run_150 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState231 ->
          _menhir_run_150 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState233 ->
          _menhir_run_150 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState236 ->
          _menhir_run_150 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState238 ->
          _menhir_run_150 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState239 ->
          _menhir_run_150 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState243 ->
          _menhir_run_150 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState254 ->
          _menhir_run_150 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState256 ->
          _menhir_run_150 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState258 ->
          _menhir_run_150 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState263 ->
          _menhir_run_150 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState264 ->
          _menhir_run_150 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState271 ->
          _menhir_run_150 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState274 ->
          _menhir_run_150 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState278 ->
          _menhir_run_150 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState280 ->
          _menhir_run_150 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState282 ->
          _menhir_run_150 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState292 ->
          _menhir_run_150 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState294 ->
          _menhir_run_150 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState297 ->
          _menhir_run_150 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState300 ->
          _menhir_run_150 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState302 ->
          _menhir_run_150 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState306 ->
          _menhir_run_150 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState313 ->
          _menhir_run_150 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | _ ->
          _menhir_fail ()
  
  and _menhir_run_132 : type  ttv_stack. (ttv_stack, _menhir_box_program) _menhir_cell1_or_expr -> _ -> _ -> _ -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _v _tok ->
      let MenhirCell1_or_expr (_menhir_stack, _menhir_s, e, _startpos_e_) = _menhir_stack in
      let a = _v in
      let _v = _menhir_action_009 _startpos_e_ a e in
      _menhir_goto_assign_expr _menhir_stack _menhir_lexbuf _menhir_lexer _startpos_e_ _v _menhir_s _tok
  
  and _menhir_run_137 : type  ttv_stack. (ttv_stack, _menhir_box_program) _menhir_cell1_or_expr -> _ -> _ -> _ -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _v _tok ->
      let MenhirCell1_or_expr (_menhir_stack, _menhir_s, e, _startpos_e_) = _menhir_stack in
      let a = _v in
      let _v = _menhir_action_013 _startpos_e_ a e in
      _menhir_goto_assign_expr _menhir_stack _menhir_lexbuf _menhir_lexer _startpos_e_ _v _menhir_s _tok
  
  and _menhir_run_139 : type  ttv_stack. (ttv_stack, _menhir_box_program) _menhir_cell1_or_expr -> _ -> _ -> _ -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _v _tok ->
      let MenhirCell1_or_expr (_menhir_stack, _menhir_s, e, _startpos_e_) = _menhir_stack in
      let a = _v in
      let _v = _menhir_action_010 _startpos_e_ a e in
      _menhir_goto_assign_expr _menhir_stack _menhir_lexbuf _menhir_lexer _startpos_e_ _v _menhir_s _tok
  
  and _menhir_run_141 : type  ttv_stack. (ttv_stack, _menhir_box_program) _menhir_cell1_or_expr -> _ -> _ -> _ -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _v _tok ->
      let MenhirCell1_or_expr (_menhir_stack, _menhir_s, e, _startpos_e_) = _menhir_stack in
      let a = _v in
      let _v = _menhir_action_012 _startpos_e_ a e in
      _menhir_goto_assign_expr _menhir_stack _menhir_lexbuf _menhir_lexer _startpos_e_ _v _menhir_s _tok
  
  and _menhir_run_143 : type  ttv_stack. (ttv_stack, _menhir_box_program) _menhir_cell1_or_expr -> _ -> _ -> _ -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _v _tok ->
      let MenhirCell1_or_expr (_menhir_stack, _menhir_s, e, _startpos_e_) = _menhir_stack in
      let a = _v in
      let _v = _menhir_action_014 _startpos_e_ a e in
      _menhir_goto_assign_expr _menhir_stack _menhir_lexbuf _menhir_lexer _startpos_e_ _v _menhir_s _tok
  
  and _menhir_run_145 : type  ttv_stack. (ttv_stack, _menhir_box_program) _menhir_cell1_or_expr -> _ -> _ -> _ -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _v _tok ->
      let MenhirCell1_or_expr (_menhir_stack, _menhir_s, e, _startpos_e_) = _menhir_stack in
      let a = _v in
      let _v = _menhir_action_011 _startpos_e_ a e in
      _menhir_goto_assign_expr _menhir_stack _menhir_lexbuf _menhir_lexer _startpos_e_ _v _menhir_s _tok
  
  and _menhir_run_147 : type  ttv_stack. (ttv_stack, _menhir_box_program) _menhir_cell1_or_expr -> _ -> _ -> _ -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _v _tok ->
      let MenhirCell1_or_expr (_menhir_stack, _menhir_s, e, _startpos_e_) = _menhir_stack in
      let a = _v in
      let _v = _menhir_action_007 _startpos_e_ a e in
      _menhir_goto_assign_expr _menhir_stack _menhir_lexbuf _menhir_lexer _startpos_e_ _v _menhir_s _tok
  
  and _menhir_run_149 : type  ttv_stack. (ttv_stack, _menhir_box_program) _menhir_cell1_or_expr -> _ -> _ -> _ -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _v _tok ->
      let MenhirCell1_or_expr (_menhir_stack, _menhir_s, e, _startpos_e_) = _menhir_stack in
      let a = _v in
      let _v = _menhir_action_008 _startpos_e_ a e in
      _menhir_goto_assign_expr _menhir_stack _menhir_lexbuf _menhir_lexer _startpos_e_ _v _menhir_s _tok
  
  and _menhir_run_150 : type  ttv_stack. ttv_stack -> _ -> _ -> _ -> _ -> (ttv_stack, _menhir_box_program) _menhir_state -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok ->
      let (_startpos_e_, e) = (_startpos, _v) in
      let _v = _menhir_action_174 e in
      _menhir_goto_walrus_expr _menhir_stack _menhir_lexbuf _menhir_lexer _startpos_e_ _v _menhir_s _tok
  
  and _menhir_goto_walrus_expr : type  ttv_stack. ttv_stack -> _ -> _ -> _ -> _ -> (ttv_stack, _menhir_box_program) _menhir_state -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok ->
      match _menhir_s with
      | MenhirState068 ->
          _menhir_run_069 _menhir_stack _menhir_lexbuf _menhir_lexer _v _tok
      | MenhirState033 ->
          _menhir_run_151 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState036 ->
          _menhir_run_151 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState043 ->
          _menhir_run_151 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState045 ->
          _menhir_run_151 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState046 ->
          _menhir_run_151 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState055 ->
          _menhir_run_151 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState066 ->
          _menhir_run_151 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState152 ->
          _menhir_run_151 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState156 ->
          _menhir_run_151 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState162 ->
          _menhir_run_151 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState164 ->
          _menhir_run_151 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState166 ->
          _menhir_run_151 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState169 ->
          _menhir_run_151 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState188 ->
          _menhir_run_151 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState191 ->
          _menhir_run_151 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState218 ->
          _menhir_run_151 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState219 ->
          _menhir_run_151 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState223 ->
          _menhir_run_151 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState225 ->
          _menhir_run_151 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState229 ->
          _menhir_run_151 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState231 ->
          _menhir_run_151 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState233 ->
          _menhir_run_151 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState236 ->
          _menhir_run_151 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState238 ->
          _menhir_run_151 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState239 ->
          _menhir_run_151 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState243 ->
          _menhir_run_151 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState254 ->
          _menhir_run_151 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState256 ->
          _menhir_run_151 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState258 ->
          _menhir_run_151 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState263 ->
          _menhir_run_151 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState264 ->
          _menhir_run_151 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState271 ->
          _menhir_run_151 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState274 ->
          _menhir_run_151 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState278 ->
          _menhir_run_151 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState280 ->
          _menhir_run_151 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState282 ->
          _menhir_run_151 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState292 ->
          _menhir_run_151 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState294 ->
          _menhir_run_151 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState297 ->
          _menhir_run_151 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState300 ->
          _menhir_run_151 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState302 ->
          _menhir_run_151 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState306 ->
          _menhir_run_151 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState313 ->
          _menhir_run_151 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | _ ->
          _menhir_fail ()
  
  and _menhir_run_069 : type  ttv_stack. (ttv_stack, _menhir_box_program) _menhir_cell1_IDENT -> _ -> _ -> _ -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _v _tok ->
      let MenhirCell1_IDENT (_menhir_stack, _menhir_s, n, _startpos_n_) = _menhir_stack in
      let e = _v in
      let _v = _menhir_action_173 _startpos_n_ e n in
      _menhir_goto_walrus_expr _menhir_stack _menhir_lexbuf _menhir_lexer _startpos_n_ _v _menhir_s _tok
  
  and _menhir_run_151 : type  ttv_stack. ttv_stack -> _ -> _ -> _ -> _ -> (ttv_stack, _menhir_box_program) _menhir_state -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok ->
      match (_tok : MenhirBasics.token) with
      | IF ->
          let _menhir_stack = MenhirCell1_walrus_expr (_menhir_stack, _menhir_s, _v, _startpos) in
          let _startpos = _menhir_lexbuf.Lexing.lex_start_p in
          let _menhir_stack = MenhirCell0_IF (_menhir_stack, _startpos) in
          let _menhir_s = MenhirState152 in
          let _tok = _menhir_lexer _menhir_lexbuf in
          (match (_tok : MenhirBasics.token) with
          | TRUE ->
              _menhir_run_034 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | SUPER ->
              _menhir_run_035 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | STRING _v ->
              _menhir_run_037 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | SELF ->
              _menhir_run_038 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | PLUS ->
              _menhir_run_039 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | NULL ->
              _menhir_run_040 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | NOT ->
              _menhir_run_044 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | NONE ->
              _menhir_run_041 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | MINUS ->
              _menhir_run_042 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | LPAREN ->
              _menhir_run_043 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | LBRACK ->
              _menhir_run_045 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | LBRACE ->
              _menhir_run_046 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | LAMBDA ->
              _menhir_run_047 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | INT _v ->
              _menhir_run_056 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | INC ->
              _menhir_run_057 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | IDENT _v ->
              _menhir_run_067 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | FLOAT _v ->
              _menhir_run_059 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | FALSE ->
              _menhir_run_060 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | DEC ->
              _menhir_run_061 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | BITNOT ->
              _menhir_run_062 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | _ ->
              _eRR ())
      | AS | ASSERT | BITNOT | BREAK | COLON | COMMA | CONTINUE | DARROW | DEC | DEF | DEL | ELSE | ENUM | FALSE | FLOAT _ | FOR | FROM | GLOBAL | IDENT _ | IMPORT | IN | INC | INT _ | LAMBDA | LBRACE | LBRACK | LPAREN | MINUS | NONE | NONLOCAL | NOT | NULL | PASS | PLUS | RAISE | RBRACE | RBRACK | RETURN | RPAREN | SELF | SEMI | STRING _ | STRUCT | SUPER | TRUE | TRY | WHILE | WITH | YIELD ->
          let (_startpos_e_, e) = (_startpos, _v) in
          let _v = _menhir_action_052 e in
          _menhir_goto_if_expr _menhir_stack _menhir_lexbuf _menhir_lexer _startpos_e_ _v _menhir_s _tok
      | _ ->
          _menhir_fail ()
  
  and _menhir_goto_if_expr : type  ttv_stack. ttv_stack -> _ -> _ -> _ -> _ -> (ttv_stack, _menhir_box_program) _menhir_state -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok ->
      match _menhir_s with
      | MenhirState033 ->
          _menhir_run_154 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState036 ->
          _menhir_run_154 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState043 ->
          _menhir_run_154 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState045 ->
          _menhir_run_154 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState046 ->
          _menhir_run_154 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState055 ->
          _menhir_run_154 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState066 ->
          _menhir_run_154 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState152 ->
          _menhir_run_154 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState162 ->
          _menhir_run_154 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState164 ->
          _menhir_run_154 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState166 ->
          _menhir_run_154 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState169 ->
          _menhir_run_154 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState188 ->
          _menhir_run_154 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState191 ->
          _menhir_run_154 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState218 ->
          _menhir_run_154 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState219 ->
          _menhir_run_154 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState223 ->
          _menhir_run_154 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState225 ->
          _menhir_run_154 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState229 ->
          _menhir_run_154 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState231 ->
          _menhir_run_154 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState233 ->
          _menhir_run_154 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState236 ->
          _menhir_run_154 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState238 ->
          _menhir_run_154 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState239 ->
          _menhir_run_154 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState243 ->
          _menhir_run_154 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState254 ->
          _menhir_run_154 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState256 ->
          _menhir_run_154 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState258 ->
          _menhir_run_154 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState263 ->
          _menhir_run_154 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState264 ->
          _menhir_run_154 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState271 ->
          _menhir_run_154 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState274 ->
          _menhir_run_154 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState278 ->
          _menhir_run_154 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState280 ->
          _menhir_run_154 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState282 ->
          _menhir_run_154 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState292 ->
          _menhir_run_154 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState294 ->
          _menhir_run_154 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState297 ->
          _menhir_run_154 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState300 ->
          _menhir_run_154 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState302 ->
          _menhir_run_154 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState306 ->
          _menhir_run_154 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState313 ->
          _menhir_run_154 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState156 ->
          _menhir_run_157 _menhir_stack _menhir_lexbuf _menhir_lexer _v _tok
      | _ ->
          _menhir_fail ()
  
  and _menhir_run_154 : type  ttv_stack. ttv_stack -> _ -> _ -> _ -> _ -> (ttv_stack, _menhir_box_program) _menhir_state -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok ->
      let (_startpos_e_, e) = (_startpos, _v) in
      let _v = _menhir_action_044 e in
      _menhir_goto_expr _menhir_stack _menhir_lexbuf _menhir_lexer _startpos_e_ _v _menhir_s _tok
  
  and _menhir_goto_expr : type  ttv_stack. ttv_stack -> _ -> _ -> _ -> _ -> (ttv_stack, _menhir_box_program) _menhir_state -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok ->
      match _menhir_s with
      | MenhirState152 ->
          _menhir_run_155 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState036 ->
          _menhir_run_161 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState045 ->
          _menhir_run_161 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState066 ->
          _menhir_run_161 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState162 ->
          _menhir_run_161 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState166 ->
          _menhir_run_172 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s _tok
      | MenhirState169 ->
          _menhir_run_172 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s _tok
      | MenhirState164 ->
          _menhir_run_173 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s _tok
      | MenhirState055 ->
          _menhir_run_181 _menhir_stack _menhir_lexbuf _menhir_lexer _v _tok
      | MenhirState046 ->
          _menhir_run_187 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState188 ->
          _menhir_run_189 _menhir_stack _menhir_lexbuf _menhir_lexer _v _tok
      | MenhirState191 ->
          _menhir_run_193 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState043 ->
          _menhir_run_197 _menhir_stack _menhir_lexbuf _menhir_lexer _v _tok
      | MenhirState033 ->
          _menhir_run_203 _menhir_stack _menhir_lexbuf _menhir_lexer _v _tok
      | MenhirState219 ->
          _menhir_run_221 _menhir_stack _menhir_lexbuf _menhir_lexer _v _tok
      | MenhirState223 ->
          _menhir_run_227 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState225 ->
          _menhir_run_227 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState231 ->
          _menhir_run_232 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState236 ->
          _menhir_run_242 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s _tok
      | MenhirState239 ->
          _menhir_run_242 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s _tok
      | MenhirState243 ->
          _menhir_run_242 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s _tok
      | MenhirState256 ->
          _menhir_run_257 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState264 ->
          _menhir_run_265 _menhir_stack _menhir_lexbuf _menhir_lexer _v _tok
      | MenhirState271 ->
          _menhir_run_272 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState274 ->
          _menhir_run_275 _menhir_stack _menhir_lexbuf _menhir_lexer _v _tok
      | MenhirState278 ->
          _menhir_run_279 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState280 ->
          _menhir_run_281 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState218 ->
          _menhir_run_286 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState229 ->
          _menhir_run_286 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState233 ->
          _menhir_run_286 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState238 ->
          _menhir_run_286 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState258 ->
          _menhir_run_286 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState282 ->
          _menhir_run_286 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState294 ->
          _menhir_run_286 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState297 ->
          _menhir_run_286 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState302 ->
          _menhir_run_286 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState306 ->
          _menhir_run_286 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState313 ->
          _menhir_run_286 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState263 ->
          _menhir_run_291 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState292 ->
          _menhir_run_293 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState300 ->
          _menhir_run_301 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | MenhirState254 ->
          _menhir_run_310 _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok
      | _ ->
          _menhir_fail ()
  
  and _menhir_run_155 : type  ttv_stack. ((ttv_stack, _menhir_box_program) _menhir_cell1_walrus_expr _menhir_cell0_IF as 'stack) -> _ -> _ -> _ -> _ -> ('stack, _menhir_box_program) _menhir_state -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok ->
      let _menhir_stack = MenhirCell1_expr (_menhir_stack, _menhir_s, _v, _startpos) in
      match (_tok : MenhirBasics.token) with
      | ELSE ->
          let _menhir_s = MenhirState156 in
          let _tok = _menhir_lexer _menhir_lexbuf in
          (match (_tok : MenhirBasics.token) with
          | TRUE ->
              _menhir_run_034 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | SUPER ->
              _menhir_run_035 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | STRING _v ->
              _menhir_run_037 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | SELF ->
              _menhir_run_038 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | PLUS ->
              _menhir_run_039 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | NULL ->
              _menhir_run_040 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | NOT ->
              _menhir_run_044 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | NONE ->
              _menhir_run_041 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | MINUS ->
              _menhir_run_042 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | LPAREN ->
              _menhir_run_043 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | LBRACK ->
              _menhir_run_045 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | LBRACE ->
              _menhir_run_046 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | INT _v ->
              _menhir_run_056 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | INC ->
              _menhir_run_057 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | IDENT _v ->
              _menhir_run_067 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | FLOAT _v ->
              _menhir_run_059 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | FALSE ->
              _menhir_run_060 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | DEC ->
              _menhir_run_061 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | BITNOT ->
              _menhir_run_062 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | _ ->
              _eRR ())
      | _ ->
          _eRR ()
  
  and _menhir_run_161 : type  ttv_stack. ttv_stack -> _ -> _ -> _ -> _ -> (ttv_stack, _menhir_box_program) _menhir_state -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok ->
      match (_tok : MenhirBasics.token) with
      | COMMA ->
          let _menhir_stack = MenhirCell1_expr (_menhir_stack, _menhir_s, _v, _startpos) in
          _menhir_run_162 _menhir_stack _menhir_lexbuf _menhir_lexer
      | RBRACE | RBRACK | RPAREN ->
          let x = _v in
          let _v = _menhir_action_129 x in
          _menhir_goto_separated_nonempty_list_COMMA_expr_ _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s _tok
      | _ ->
          _eRR ()
  
  and _menhir_run_162 : type  ttv_stack. (ttv_stack, _menhir_box_program) _menhir_cell1_expr -> _ -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer ->
      let _menhir_s = MenhirState162 in
      let _tok = _menhir_lexer _menhir_lexbuf in
      match (_tok : MenhirBasics.token) with
      | TRUE ->
          _menhir_run_034 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | SUPER ->
          _menhir_run_035 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | STRING _v ->
          _menhir_run_037 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
      | SELF ->
          _menhir_run_038 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | PLUS ->
          _menhir_run_039 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | NULL ->
          _menhir_run_040 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | NOT ->
          _menhir_run_044 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | NONE ->
          _menhir_run_041 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | MINUS ->
          _menhir_run_042 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | LPAREN ->
          _menhir_run_043 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | LBRACK ->
          _menhir_run_045 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | LBRACE ->
          _menhir_run_046 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | LAMBDA ->
          _menhir_run_047 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | INT _v ->
          _menhir_run_056 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
      | INC ->
          _menhir_run_057 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | IDENT _v ->
          _menhir_run_067 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
      | FLOAT _v ->
          _menhir_run_059 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
      | FALSE ->
          _menhir_run_060 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | DEC ->
          _menhir_run_061 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | BITNOT ->
          _menhir_run_062 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | _ ->
          _eRR ()
  
  and _menhir_goto_separated_nonempty_list_COMMA_expr_ : type  ttv_stack. ttv_stack -> _ -> _ -> _ -> (ttv_stack, _menhir_box_program) _menhir_state -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s _tok ->
      match _menhir_s with
      | MenhirState036 ->
          _menhir_run_158 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s _tok
      | MenhirState045 ->
          _menhir_run_158 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s _tok
      | MenhirState066 ->
          _menhir_run_158 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s _tok
      | MenhirState162 ->
          _menhir_run_163 _menhir_stack _menhir_lexbuf _menhir_lexer _v _tok
      | MenhirState046 ->
          _menhir_run_182 _menhir_stack _menhir_lexbuf _menhir_lexer _v _tok
      | MenhirState254 ->
          _menhir_run_182 _menhir_stack _menhir_lexbuf _menhir_lexer _v _tok
      | _ ->
          _menhir_fail ()
  
  and _menhir_run_158 : type  ttv_stack. ttv_stack -> _ -> _ -> _ -> (ttv_stack, _menhir_box_program) _menhir_state -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s _tok ->
      let x = _v in
      let _v = _menhir_action_076 x in
      _menhir_goto_loption_separated_nonempty_list_COMMA_expr__ _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s _tok
  
  and _menhir_goto_loption_separated_nonempty_list_COMMA_expr__ : type  ttv_stack. ttv_stack -> _ -> _ -> _ -> (ttv_stack, _menhir_box_program) _menhir_state -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s _tok ->
      match _menhir_s with
      | MenhirState066 ->
          _menhir_run_159 _menhir_stack _menhir_lexbuf _menhir_lexer _v _tok
      | MenhirState045 ->
          _menhir_run_194 _menhir_stack _menhir_lexbuf _menhir_lexer _v _tok
      | MenhirState036 ->
          _menhir_run_201 _menhir_stack _menhir_lexbuf _menhir_lexer _v _tok
      | _ ->
          _menhir_fail ()
  
  and _menhir_run_163 : type  ttv_stack. (ttv_stack, _menhir_box_program) _menhir_cell1_expr -> _ -> _ -> _ -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _v _tok ->
      let MenhirCell1_expr (_menhir_stack, _menhir_s, x, _) = _menhir_stack in
      let xs = _v in
      let _v = _menhir_action_130 x xs in
      _menhir_goto_separated_nonempty_list_COMMA_expr_ _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s _tok
  
  and _menhir_run_182 : type  ttv_stack. (ttv_stack, _menhir_box_program) _menhir_cell1_LBRACE -> _ -> _ -> _ -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _v _tok ->
      match (_tok : MenhirBasics.token) with
      | RBRACE ->
          let _tok = _menhir_lexer _menhir_lexbuf in
          let MenhirCell1_LBRACE (_menhir_stack, _menhir_s, _startpos__1_) = _menhir_stack in
          let items = _v in
          let _v = _menhir_action_121 _startpos__1_ items in
          _menhir_goto_primary_expr _menhir_stack _menhir_lexbuf _menhir_lexer _startpos__1_ _v _menhir_s _tok
      | _ ->
          _eRR ()
  
  and _menhir_run_172 : type  ttv_stack. ((ttv_stack, _menhir_box_program) _menhir_cell1_expr_option as 'stack) -> _ -> _ -> _ -> ('stack, _menhir_box_program) _menhir_state -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s _tok ->
      let e = _v in
      let _v = _menhir_action_046 e in
      _menhir_goto_expr_option _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s _tok
  
  and _menhir_goto_expr_option : type  ttv_stack. ttv_stack -> _ -> _ -> _ -> (ttv_stack, _menhir_box_program) _menhir_state -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s _tok ->
      match _menhir_s with
      | MenhirState164 ->
          _menhir_run_165 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s _tok
      | MenhirState166 ->
          _menhir_run_167 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s _tok
      | MenhirState169 ->
          _menhir_run_170 _menhir_stack _menhir_lexbuf _menhir_lexer _v _tok
      | _ ->
          _menhir_fail ()
  
  and _menhir_run_173 : type  ttv_stack. ((ttv_stack, _menhir_box_program) _menhir_cell1_postfix_expr _menhir_cell0_LBRACK as 'stack) -> _ -> _ -> _ -> ('stack, _menhir_box_program) _menhir_state -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s _tok ->
      match (_tok : MenhirBasics.token) with
      | RBRACK ->
          let _tok = _menhir_lexer _menhir_lexbuf in
          let MenhirCell0_LBRACK (_menhir_stack, _) = _menhir_stack in
          let MenhirCell1_postfix_expr (_menhir_stack, _menhir_s, e, _startpos_e_) = _menhir_stack in
          let i = _v in
          let _v = _menhir_action_101 _startpos_e_ e i in
          _menhir_goto_postfix_expr _menhir_stack _menhir_lexbuf _menhir_lexer _startpos_e_ _v _menhir_s _tok
      | COLON ->
          let e = _v in
          let _v = _menhir_action_046 e in
          _menhir_goto_expr_option _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s _tok
      | _ ->
          _eRR ()
  
  and _menhir_run_181 : type  ttv_stack. ((ttv_stack, _menhir_box_program) _menhir_cell1_LAMBDA, _menhir_box_program) _menhir_cell1_loption_separated_nonempty_list_COMMA_param__ -> _ -> _ -> _ -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _v _tok ->
      let MenhirCell1_loption_separated_nonempty_list_COMMA_param__ (_menhir_stack, _, xs) = _menhir_stack in
      let MenhirCell1_LAMBDA (_menhir_stack, _menhir_s, _startpos__1_) = _menhir_stack in
      let e = _v in
      let _v = _menhir_action_062 _startpos__1_ e xs in
      let _startpos = _startpos__1_ in
      let (_startpos_e_, e) = (_startpos, _v) in
      let _v = _menhir_action_045 e in
      _menhir_goto_expr _menhir_stack _menhir_lexbuf _menhir_lexer _startpos_e_ _v _menhir_s _tok
  
  and _menhir_run_187 : type  ttv_stack. ((ttv_stack, _menhir_box_program) _menhir_cell1_LBRACE as 'stack) -> _ -> _ -> _ -> _ -> ('stack, _menhir_box_program) _menhir_state -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok ->
      match (_tok : MenhirBasics.token) with
      | COMMA ->
          let _menhir_stack = MenhirCell1_expr (_menhir_stack, _menhir_s, _v, _startpos) in
          _menhir_run_162 _menhir_stack _menhir_lexbuf _menhir_lexer
      | COLON ->
          let _menhir_stack = MenhirCell1_expr (_menhir_stack, _menhir_s, _v, _startpos) in
          _menhir_run_188 _menhir_stack _menhir_lexbuf _menhir_lexer
      | RBRACE ->
          let x = _v in
          let _v = _menhir_action_129 x in
          _menhir_goto_separated_nonempty_list_COMMA_expr_ _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s _tok
      | _ ->
          _eRR ()
  
  and _menhir_run_188 : type  ttv_stack. (ttv_stack, _menhir_box_program) _menhir_cell1_expr -> _ -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer ->
      let _menhir_s = MenhirState188 in
      let _tok = _menhir_lexer _menhir_lexbuf in
      match (_tok : MenhirBasics.token) with
      | TRUE ->
          _menhir_run_034 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | SUPER ->
          _menhir_run_035 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | STRING _v ->
          _menhir_run_037 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
      | SELF ->
          _menhir_run_038 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | PLUS ->
          _menhir_run_039 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | NULL ->
          _menhir_run_040 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | NOT ->
          _menhir_run_044 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | NONE ->
          _menhir_run_041 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | MINUS ->
          _menhir_run_042 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | LPAREN ->
          _menhir_run_043 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | LBRACK ->
          _menhir_run_045 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | LBRACE ->
          _menhir_run_046 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | LAMBDA ->
          _menhir_run_047 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | INT _v ->
          _menhir_run_056 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
      | INC ->
          _menhir_run_057 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | IDENT _v ->
          _menhir_run_067 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
      | FLOAT _v ->
          _menhir_run_059 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
      | FALSE ->
          _menhir_run_060 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | DEC ->
          _menhir_run_061 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | BITNOT ->
          _menhir_run_062 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | _ ->
          _eRR ()
  
  and _menhir_run_189 : type  ttv_stack. (ttv_stack, _menhir_box_program) _menhir_cell1_expr -> _ -> _ -> _ -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _v _tok ->
      let MenhirCell1_expr (_menhir_stack, _menhir_s, k, _) = _menhir_stack in
      let v = _v in
      let _v = _menhir_action_033 k v in
      match (_tok : MenhirBasics.token) with
      | COMMA ->
          let _menhir_stack = MenhirCell1_dict_item (_menhir_stack, _menhir_s, _v) in
          let _menhir_s = MenhirState191 in
          let _tok = _menhir_lexer _menhir_lexbuf in
          (match (_tok : MenhirBasics.token) with
          | TRUE ->
              _menhir_run_034 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | SUPER ->
              _menhir_run_035 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | STRING _v ->
              _menhir_run_037 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | SELF ->
              _menhir_run_038 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | PLUS ->
              _menhir_run_039 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | NULL ->
              _menhir_run_040 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | NOT ->
              _menhir_run_044 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | NONE ->
              _menhir_run_041 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | MINUS ->
              _menhir_run_042 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | LPAREN ->
              _menhir_run_043 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | LBRACK ->
              _menhir_run_045 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | LBRACE ->
              _menhir_run_046 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | LAMBDA ->
              _menhir_run_047 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | INT _v ->
              _menhir_run_056 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | INC ->
              _menhir_run_057 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | IDENT _v ->
              _menhir_run_067 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | FLOAT _v ->
              _menhir_run_059 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | FALSE ->
              _menhir_run_060 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | DEC ->
              _menhir_run_061 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | BITNOT ->
              _menhir_run_062 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | _ ->
              _eRR ())
      | RBRACE ->
          let x = _v in
          let _v = _menhir_action_125 x in
          _menhir_goto_separated_nonempty_list_COMMA_dict_item_ _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
      | _ ->
          _eRR ()
  
  and _menhir_goto_separated_nonempty_list_COMMA_dict_item_ : type  ttv_stack. ttv_stack -> _ -> _ -> _ -> (ttv_stack, _menhir_box_program) _menhir_state -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s ->
      match _menhir_s with
      | MenhirState046 ->
          _menhir_run_184 _menhir_stack _menhir_lexbuf _menhir_lexer _v
      | MenhirState254 ->
          _menhir_run_184 _menhir_stack _menhir_lexbuf _menhir_lexer _v
      | MenhirState191 ->
          _menhir_run_192 _menhir_stack _menhir_lexbuf _menhir_lexer _v
      | _ ->
          _menhir_fail ()
  
  and _menhir_run_184 : type  ttv_stack. (ttv_stack, _menhir_box_program) _menhir_cell1_LBRACE -> _ -> _ -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _v ->
      let x = _v in
      let _v = _menhir_action_072 x in
      _menhir_goto_loption_separated_nonempty_list_COMMA_dict_item__ _menhir_stack _menhir_lexbuf _menhir_lexer _v
  
  and _menhir_run_192 : type  ttv_stack. (ttv_stack, _menhir_box_program) _menhir_cell1_dict_item -> _ -> _ -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _v ->
      let MenhirCell1_dict_item (_menhir_stack, _menhir_s, x) = _menhir_stack in
      let xs = _v in
      let _v = _menhir_action_126 x xs in
      _menhir_goto_separated_nonempty_list_COMMA_dict_item_ _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
  
  and _menhir_run_193 : type  ttv_stack. ((ttv_stack, _menhir_box_program) _menhir_cell1_dict_item as 'stack) -> _ -> _ -> _ -> _ -> ('stack, _menhir_box_program) _menhir_state -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok ->
      let _menhir_stack = MenhirCell1_expr (_menhir_stack, _menhir_s, _v, _startpos) in
      match (_tok : MenhirBasics.token) with
      | COLON ->
          _menhir_run_188 _menhir_stack _menhir_lexbuf _menhir_lexer
      | _ ->
          _eRR ()
  
  and _menhir_run_197 : type  ttv_stack. (ttv_stack, _menhir_box_program) _menhir_cell1_LPAREN -> _ -> _ -> _ -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _v _tok ->
      match (_tok : MenhirBasics.token) with
      | RPAREN ->
          let _tok = _menhir_lexer _menhir_lexbuf in
          let MenhirCell1_LPAREN (_menhir_stack, _menhir_s, _startpos__1_) = _menhir_stack in
          let e = _v in
          let _v = _menhir_action_118 e in
          _menhir_goto_primary_expr _menhir_stack _menhir_lexbuf _menhir_lexer _startpos__1_ _v _menhir_s _tok
      | _ ->
          _eRR ()
  
  and _menhir_run_203 : type  ttv_stack. ((ttv_stack, _menhir_box_program) _menhir_cell1_IDENT, _menhir_box_program) _menhir_cell1_ASSIGN -> _ -> _ -> _ -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _v _tok ->
      let MenhirCell1_ASSIGN (_menhir_stack, _menhir_s) = _menhir_stack in
      let x = _v in
      let _v = _menhir_action_094 x in
      _menhir_goto_option_preceded_ASSIGN_expr__ _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s _tok
  
  and _menhir_goto_option_preceded_ASSIGN_expr__ : type  ttv_stack. ((ttv_stack, _menhir_box_program) _menhir_cell1_IDENT as 'stack) -> _ -> _ -> _ -> ('stack, _menhir_box_program) _menhir_state -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s _tok ->
      match _menhir_s with
      | MenhirState048 ->
          _menhir_run_049 _menhir_stack _menhir_lexbuf _menhir_lexer _v _tok
      | MenhirState032 ->
          _menhir_run_204 _menhir_stack _menhir_lexbuf _menhir_lexer _v _tok
      | _ ->
          _menhir_fail ()
  
  and _menhir_run_221 : type  ttv_stack. (ttv_stack, _menhir_box_program) _menhir_cell1_YIELD -> _ -> _ -> _ -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _v _tok ->
      match (_tok : MenhirBasics.token) with
      | SEMI ->
          let _tok = _menhir_lexer _menhir_lexbuf in
          let MenhirCell1_YIELD (_menhir_stack, _menhir_s, _startpos__1_) = _menhir_stack in
          let e = _v in
          let _v = _menhir_action_157 _startpos__1_ e in
          _menhir_goto_stmt _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s _tok
      | _ ->
          _eRR ()
  
  and _menhir_run_227 : type  ttv_stack. ttv_stack -> _ -> _ -> _ -> _ -> (ttv_stack, _menhir_box_program) _menhir_state -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok ->
      let _menhir_stack = MenhirCell1_expr (_menhir_stack, _menhir_s, _v, _startpos) in
      match (_tok : MenhirBasics.token) with
      | AS ->
          _menhir_run_014 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState227
      | ASSERT | BITNOT | BREAK | COMMA | CONTINUE | DEC | DEF | DEL | ENUM | FALSE | FLOAT _ | FOR | FROM | GLOBAL | IDENT _ | IF | IMPORT | INC | INT _ | LAMBDA | LBRACE | LBRACK | LPAREN | MINUS | NONE | NONLOCAL | NOT | NULL | PASS | PLUS | RAISE | RETURN | SELF | STRING _ | STRUCT | SUPER | TRUE | TRY | WHILE | WITH | YIELD ->
          let _v_0 = _menhir_action_091 () in
          _menhir_run_228 _menhir_stack _menhir_lexbuf _menhir_lexer _v_0 _tok
      | _ ->
          _eRR ()
  
  and _menhir_run_232 : type  ttv_stack. ((ttv_stack, _menhir_box_program) _menhir_cell1_WHILE _menhir_cell0_LPAREN as 'stack) -> _ -> _ -> _ -> _ -> ('stack, _menhir_box_program) _menhir_state -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok ->
      let _menhir_stack = MenhirCell1_expr (_menhir_stack, _menhir_s, _v, _startpos) in
      match (_tok : MenhirBasics.token) with
      | RPAREN ->
          let _menhir_s = MenhirState233 in
          let _tok = _menhir_lexer _menhir_lexbuf in
          (match (_tok : MenhirBasics.token) with
          | YIELD ->
              _menhir_run_219 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | WITH ->
              _menhir_run_223 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | WHILE ->
              _menhir_run_230 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | TRY ->
              _menhir_run_234 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | TRUE ->
              _menhir_run_034 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | SUPER ->
              _menhir_run_035 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | STRUCT ->
              _menhir_run_001 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | STRING _v ->
              _menhir_run_037 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | SELF ->
              _menhir_run_038 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | RETURN ->
              _menhir_run_239 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | RAISE ->
              _menhir_run_243 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | PLUS ->
              _menhir_run_039 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | PASS ->
              _menhir_run_246 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | NULL ->
              _menhir_run_040 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | NOT ->
              _menhir_run_044 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | NONLOCAL ->
              _menhir_run_248 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | NONE ->
              _menhir_run_041 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | MINUS ->
              _menhir_run_042 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | LPAREN ->
              _menhir_run_043 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | LBRACK ->
              _menhir_run_045 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | LBRACE ->
              _menhir_run_254 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | LAMBDA ->
              _menhir_run_047 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | INT _v ->
              _menhir_run_056 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | INC ->
              _menhir_run_057 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | IMPORT ->
              _menhir_run_012 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | IF ->
              _menhir_run_255 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | IDENT _v ->
              _menhir_run_067 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | GLOBAL ->
              _menhir_run_259 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | FROM ->
              _menhir_run_018 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | FOR ->
              _menhir_run_262 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | FLOAT _v ->
              _menhir_run_059 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | FALSE ->
              _menhir_run_060 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | ENUM ->
              _menhir_run_028 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | DEL ->
              _menhir_run_264 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | DEF ->
              _menhir_run_212 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | DEC ->
              _menhir_run_061 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | CONTINUE ->
              _menhir_run_267 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | BREAK ->
              _menhir_run_269 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | BITNOT ->
              _menhir_run_062 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | ASSERT ->
              _menhir_run_271 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | _ ->
              _eRR ())
      | _ ->
          _eRR ()
  
  and _menhir_run_242 : type  ttv_stack. ttv_stack -> _ -> _ -> _ -> (ttv_stack, _menhir_box_program) _menhir_state -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s _tok ->
      let x = _v in
      let _v = _menhir_action_088 x in
      _menhir_goto_option_expr_ _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s _tok
  
  and _menhir_goto_option_expr_ : type  ttv_stack. ttv_stack -> _ -> _ -> _ -> (ttv_stack, _menhir_box_program) _menhir_state -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s _tok ->
      match _menhir_s with
      | MenhirState236 ->
          _menhir_run_237 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s _tok
      | MenhirState239 ->
          _menhir_run_240 _menhir_stack _menhir_lexbuf _menhir_lexer _v _tok
      | MenhirState243 ->
          _menhir_run_244 _menhir_stack _menhir_lexbuf _menhir_lexer _v _tok
      | _ ->
          _menhir_fail ()
  
  and _menhir_run_257 : type  ttv_stack. ((ttv_stack, _menhir_box_program) _menhir_cell1_IF _menhir_cell0_LPAREN as 'stack) -> _ -> _ -> _ -> _ -> ('stack, _menhir_box_program) _menhir_state -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok ->
      let _menhir_stack = MenhirCell1_expr (_menhir_stack, _menhir_s, _v, _startpos) in
      match (_tok : MenhirBasics.token) with
      | RPAREN ->
          let _menhir_s = MenhirState258 in
          let _tok = _menhir_lexer _menhir_lexbuf in
          (match (_tok : MenhirBasics.token) with
          | YIELD ->
              _menhir_run_219 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | WITH ->
              _menhir_run_223 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | WHILE ->
              _menhir_run_230 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | TRY ->
              _menhir_run_234 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | TRUE ->
              _menhir_run_034 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | SUPER ->
              _menhir_run_035 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | STRUCT ->
              _menhir_run_001 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | STRING _v ->
              _menhir_run_037 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | SELF ->
              _menhir_run_038 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | RETURN ->
              _menhir_run_239 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | RAISE ->
              _menhir_run_243 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | PLUS ->
              _menhir_run_039 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | PASS ->
              _menhir_run_246 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | NULL ->
              _menhir_run_040 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | NOT ->
              _menhir_run_044 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | NONLOCAL ->
              _menhir_run_248 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | NONE ->
              _menhir_run_041 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | MINUS ->
              _menhir_run_042 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | LPAREN ->
              _menhir_run_043 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | LBRACK ->
              _menhir_run_045 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | LBRACE ->
              _menhir_run_254 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | LAMBDA ->
              _menhir_run_047 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | INT _v ->
              _menhir_run_056 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | INC ->
              _menhir_run_057 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | IMPORT ->
              _menhir_run_012 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | IF ->
              _menhir_run_255 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | IDENT _v ->
              _menhir_run_067 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | GLOBAL ->
              _menhir_run_259 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | FROM ->
              _menhir_run_018 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | FOR ->
              _menhir_run_262 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | FLOAT _v ->
              _menhir_run_059 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | FALSE ->
              _menhir_run_060 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | ENUM ->
              _menhir_run_028 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | DEL ->
              _menhir_run_264 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | DEF ->
              _menhir_run_212 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | DEC ->
              _menhir_run_061 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | CONTINUE ->
              _menhir_run_267 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | BREAK ->
              _menhir_run_269 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | BITNOT ->
              _menhir_run_062 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | ASSERT ->
              _menhir_run_271 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | _ ->
              _eRR ())
      | _ ->
          _eRR ()
  
  and _menhir_run_265 : type  ttv_stack. (ttv_stack, _menhir_box_program) _menhir_cell1_DEL -> _ -> _ -> _ -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _v _tok ->
      match (_tok : MenhirBasics.token) with
      | SEMI ->
          let _tok = _menhir_lexer _menhir_lexbuf in
          let MenhirCell1_DEL (_menhir_stack, _menhir_s, _startpos__1_) = _menhir_stack in
          let e = _v in
          let _v = _menhir_action_153 _startpos__1_ e in
          _menhir_goto_stmt _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s _tok
      | _ ->
          _eRR ()
  
  and _menhir_run_272 : type  ttv_stack. ((ttv_stack, _menhir_box_program) _menhir_cell1_ASSERT as 'stack) -> _ -> _ -> _ -> _ -> ('stack, _menhir_box_program) _menhir_state -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok ->
      match (_tok : MenhirBasics.token) with
      | SEMI ->
          let _tok = _menhir_lexer _menhir_lexbuf in
          let MenhirCell1_ASSERT (_menhir_stack, _menhir_s, _startpos__1_) = _menhir_stack in
          let c = _v in
          let _v = _menhir_action_154 _startpos__1_ c in
          _menhir_goto_stmt _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s _tok
      | COMMA ->
          let _menhir_stack = MenhirCell1_expr (_menhir_stack, _menhir_s, _v, _startpos) in
          let _menhir_s = MenhirState274 in
          let _tok = _menhir_lexer _menhir_lexbuf in
          (match (_tok : MenhirBasics.token) with
          | TRUE ->
              _menhir_run_034 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | SUPER ->
              _menhir_run_035 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | STRING _v ->
              _menhir_run_037 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | SELF ->
              _menhir_run_038 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | PLUS ->
              _menhir_run_039 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | NULL ->
              _menhir_run_040 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | NOT ->
              _menhir_run_044 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | NONE ->
              _menhir_run_041 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | MINUS ->
              _menhir_run_042 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | LPAREN ->
              _menhir_run_043 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | LBRACK ->
              _menhir_run_045 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | LBRACE ->
              _menhir_run_046 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | LAMBDA ->
              _menhir_run_047 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | INT _v ->
              _menhir_run_056 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | INC ->
              _menhir_run_057 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | IDENT _v ->
              _menhir_run_067 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | FLOAT _v ->
              _menhir_run_059 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | FALSE ->
              _menhir_run_060 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | DEC ->
              _menhir_run_061 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | BITNOT ->
              _menhir_run_062 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | _ ->
              _eRR ())
      | _ ->
          _eRR ()
  
  and _menhir_run_275 : type  ttv_stack. ((ttv_stack, _menhir_box_program) _menhir_cell1_ASSERT, _menhir_box_program) _menhir_cell1_expr -> _ -> _ -> _ -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _v _tok ->
      match (_tok : MenhirBasics.token) with
      | SEMI ->
          let _tok = _menhir_lexer _menhir_lexbuf in
          let MenhirCell1_expr (_menhir_stack, _, c, _) = _menhir_stack in
          let MenhirCell1_ASSERT (_menhir_stack, _menhir_s, _startpos__1_) = _menhir_stack in
          let m = _v in
          let _v = _menhir_action_155 _startpos__1_ c m in
          _menhir_goto_stmt _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s _tok
      | _ ->
          _eRR ()
  
  and _menhir_run_279 : type  ttv_stack. (((ttv_stack, _menhir_box_program) _menhir_cell1_FOR _menhir_cell0_LPAREN, _menhir_box_program) _menhir_cell1_stmt as 'stack) -> _ -> _ -> _ -> _ -> ('stack, _menhir_box_program) _menhir_state -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok ->
      let _menhir_stack = MenhirCell1_expr (_menhir_stack, _menhir_s, _v, _startpos) in
      match (_tok : MenhirBasics.token) with
      | SEMI ->
          let _menhir_s = MenhirState280 in
          let _tok = _menhir_lexer _menhir_lexbuf in
          (match (_tok : MenhirBasics.token) with
          | TRUE ->
              _menhir_run_034 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | SUPER ->
              _menhir_run_035 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | STRING _v ->
              _menhir_run_037 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | SELF ->
              _menhir_run_038 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | PLUS ->
              _menhir_run_039 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | NULL ->
              _menhir_run_040 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | NOT ->
              _menhir_run_044 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | NONE ->
              _menhir_run_041 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | MINUS ->
              _menhir_run_042 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | LPAREN ->
              _menhir_run_043 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | LBRACK ->
              _menhir_run_045 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | LBRACE ->
              _menhir_run_046 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | LAMBDA ->
              _menhir_run_047 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | INT _v ->
              _menhir_run_056 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | INC ->
              _menhir_run_057 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | IDENT _v ->
              _menhir_run_067 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | FLOAT _v ->
              _menhir_run_059 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | FALSE ->
              _menhir_run_060 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | DEC ->
              _menhir_run_061 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | BITNOT ->
              _menhir_run_062 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | _ ->
              _eRR ())
      | _ ->
          _eRR ()
  
  and _menhir_run_281 : type  ttv_stack. ((((ttv_stack, _menhir_box_program) _menhir_cell1_FOR _menhir_cell0_LPAREN, _menhir_box_program) _menhir_cell1_stmt, _menhir_box_program) _menhir_cell1_expr as 'stack) -> _ -> _ -> _ -> _ -> ('stack, _menhir_box_program) _menhir_state -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok ->
      let _menhir_stack = MenhirCell1_expr (_menhir_stack, _menhir_s, _v, _startpos) in
      match (_tok : MenhirBasics.token) with
      | RPAREN ->
          let _menhir_s = MenhirState282 in
          let _tok = _menhir_lexer _menhir_lexbuf in
          (match (_tok : MenhirBasics.token) with
          | YIELD ->
              _menhir_run_219 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | WITH ->
              _menhir_run_223 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | WHILE ->
              _menhir_run_230 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | TRY ->
              _menhir_run_234 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | TRUE ->
              _menhir_run_034 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | SUPER ->
              _menhir_run_035 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | STRUCT ->
              _menhir_run_001 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | STRING _v ->
              _menhir_run_037 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | SELF ->
              _menhir_run_038 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | RETURN ->
              _menhir_run_239 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | RAISE ->
              _menhir_run_243 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | PLUS ->
              _menhir_run_039 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | PASS ->
              _menhir_run_246 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | NULL ->
              _menhir_run_040 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | NOT ->
              _menhir_run_044 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | NONLOCAL ->
              _menhir_run_248 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | NONE ->
              _menhir_run_041 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | MINUS ->
              _menhir_run_042 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | LPAREN ->
              _menhir_run_043 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | LBRACK ->
              _menhir_run_045 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | LBRACE ->
              _menhir_run_254 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | LAMBDA ->
              _menhir_run_047 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | INT _v ->
              _menhir_run_056 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | INC ->
              _menhir_run_057 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | IMPORT ->
              _menhir_run_012 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | IF ->
              _menhir_run_255 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | IDENT _v ->
              _menhir_run_067 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | GLOBAL ->
              _menhir_run_259 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | FROM ->
              _menhir_run_018 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | FOR ->
              _menhir_run_262 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | FLOAT _v ->
              _menhir_run_059 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | FALSE ->
              _menhir_run_060 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | ENUM ->
              _menhir_run_028 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | DEL ->
              _menhir_run_264 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | DEF ->
              _menhir_run_212 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | DEC ->
              _menhir_run_061 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | CONTINUE ->
              _menhir_run_267 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | BREAK ->
              _menhir_run_269 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | BITNOT ->
              _menhir_run_062 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | ASSERT ->
              _menhir_run_271 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | _ ->
              _eRR ())
      | _ ->
          _eRR ()
  
  and _menhir_run_286 : type  ttv_stack. ttv_stack -> _ -> _ -> _ -> _ -> (ttv_stack, _menhir_box_program) _menhir_state -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok ->
      let _menhir_stack = MenhirCell1_expr (_menhir_stack, _menhir_s, _v, _startpos) in
      match (_tok : MenhirBasics.token) with
      | SEMI ->
          _menhir_run_287 _menhir_stack _menhir_lexbuf _menhir_lexer
      | _ ->
          _eRR ()
  
  and _menhir_run_287 : type  ttv_stack. (ttv_stack, _menhir_box_program) _menhir_cell1_expr -> _ -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer ->
      let _tok = _menhir_lexer _menhir_lexbuf in
      let MenhirCell1_expr (_menhir_stack, _menhir_s, e, _startpos_e_) = _menhir_stack in
      let _v = _menhir_action_159 _startpos_e_ e in
      _menhir_goto_stmt _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s _tok
  
  and _menhir_run_291 : type  ttv_stack. ((ttv_stack, _menhir_box_program) _menhir_cell1_FOR _menhir_cell0_LPAREN as 'stack) -> _ -> _ -> _ -> _ -> ('stack, _menhir_box_program) _menhir_state -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok ->
      let _menhir_stack = MenhirCell1_expr (_menhir_stack, _menhir_s, _v, _startpos) in
      match (_tok : MenhirBasics.token) with
      | SEMI ->
          _menhir_run_287 _menhir_stack _menhir_lexbuf _menhir_lexer
      | _ ->
          _eRR ()
  
  and _menhir_run_293 : type  ttv_stack. (((ttv_stack, _menhir_box_program) _menhir_cell1_FOR _menhir_cell0_LPAREN, _menhir_box_program) _menhir_cell1_expr as 'stack) -> _ -> _ -> _ -> _ -> ('stack, _menhir_box_program) _menhir_state -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok ->
      let _menhir_stack = MenhirCell1_expr (_menhir_stack, _menhir_s, _v, _startpos) in
      match (_tok : MenhirBasics.token) with
      | RPAREN ->
          let _menhir_s = MenhirState294 in
          let _tok = _menhir_lexer _menhir_lexbuf in
          (match (_tok : MenhirBasics.token) with
          | YIELD ->
              _menhir_run_219 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | WITH ->
              _menhir_run_223 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | WHILE ->
              _menhir_run_230 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | TRY ->
              _menhir_run_234 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | TRUE ->
              _menhir_run_034 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | SUPER ->
              _menhir_run_035 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | STRUCT ->
              _menhir_run_001 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | STRING _v ->
              _menhir_run_037 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | SELF ->
              _menhir_run_038 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | RETURN ->
              _menhir_run_239 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | RAISE ->
              _menhir_run_243 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | PLUS ->
              _menhir_run_039 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | PASS ->
              _menhir_run_246 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | NULL ->
              _menhir_run_040 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | NOT ->
              _menhir_run_044 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | NONLOCAL ->
              _menhir_run_248 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | NONE ->
              _menhir_run_041 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | MINUS ->
              _menhir_run_042 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | LPAREN ->
              _menhir_run_043 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | LBRACK ->
              _menhir_run_045 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | LBRACE ->
              _menhir_run_254 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | LAMBDA ->
              _menhir_run_047 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | INT _v ->
              _menhir_run_056 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | INC ->
              _menhir_run_057 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | IMPORT ->
              _menhir_run_012 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | IF ->
              _menhir_run_255 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | IDENT _v ->
              _menhir_run_067 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | GLOBAL ->
              _menhir_run_259 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | FROM ->
              _menhir_run_018 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | FOR ->
              _menhir_run_262 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | FLOAT _v ->
              _menhir_run_059 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | FALSE ->
              _menhir_run_060 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | ENUM ->
              _menhir_run_028 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | DEL ->
              _menhir_run_264 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | DEF ->
              _menhir_run_212 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | DEC ->
              _menhir_run_061 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | CONTINUE ->
              _menhir_run_267 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | BREAK ->
              _menhir_run_269 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | BITNOT ->
              _menhir_run_062 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | ASSERT ->
              _menhir_run_271 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | _ ->
              _eRR ())
      | _ ->
          _eRR ()
  
  and _menhir_run_301 : type  ttv_stack. ((((ttv_stack _menhir_cell0_LPAREN, _menhir_box_program) _menhir_cell1_expr, _menhir_box_program) _menhir_cell1_stmt, _menhir_box_program) _menhir_cell1_ELIF _menhir_cell0_LPAREN as 'stack) -> _ -> _ -> _ -> _ -> ('stack, _menhir_box_program) _menhir_state -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok ->
      let _menhir_stack = MenhirCell1_expr (_menhir_stack, _menhir_s, _v, _startpos) in
      match (_tok : MenhirBasics.token) with
      | RPAREN ->
          let _menhir_s = MenhirState302 in
          let _tok = _menhir_lexer _menhir_lexbuf in
          (match (_tok : MenhirBasics.token) with
          | YIELD ->
              _menhir_run_219 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | WITH ->
              _menhir_run_223 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | WHILE ->
              _menhir_run_230 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | TRY ->
              _menhir_run_234 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | TRUE ->
              _menhir_run_034 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | SUPER ->
              _menhir_run_035 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | STRUCT ->
              _menhir_run_001 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | STRING _v ->
              _menhir_run_037 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | SELF ->
              _menhir_run_038 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | RETURN ->
              _menhir_run_239 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | RAISE ->
              _menhir_run_243 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | PLUS ->
              _menhir_run_039 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | PASS ->
              _menhir_run_246 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | NULL ->
              _menhir_run_040 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | NOT ->
              _menhir_run_044 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | NONLOCAL ->
              _menhir_run_248 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | NONE ->
              _menhir_run_041 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | MINUS ->
              _menhir_run_042 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | LPAREN ->
              _menhir_run_043 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | LBRACK ->
              _menhir_run_045 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | LBRACE ->
              _menhir_run_254 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | LAMBDA ->
              _menhir_run_047 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | INT _v ->
              _menhir_run_056 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | INC ->
              _menhir_run_057 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | IMPORT ->
              _menhir_run_012 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | IF ->
              _menhir_run_255 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | IDENT _v ->
              _menhir_run_067 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | GLOBAL ->
              _menhir_run_259 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | FROM ->
              _menhir_run_018 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | FOR ->
              _menhir_run_262 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | FLOAT _v ->
              _menhir_run_059 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | FALSE ->
              _menhir_run_060 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | ENUM ->
              _menhir_run_028 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | DEL ->
              _menhir_run_264 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | DEF ->
              _menhir_run_212 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | DEC ->
              _menhir_run_061 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | CONTINUE ->
              _menhir_run_267 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | BREAK ->
              _menhir_run_269 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | BITNOT ->
              _menhir_run_062 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | ASSERT ->
              _menhir_run_271 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | _ ->
              _eRR ())
      | _ ->
          _eRR ()
  
  and _menhir_run_310 : type  ttv_stack. ((ttv_stack, _menhir_box_program) _menhir_cell1_LBRACE as 'stack) -> _ -> _ -> _ -> _ -> ('stack, _menhir_box_program) _menhir_state -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _startpos _v _menhir_s _tok ->
      match (_tok : MenhirBasics.token) with
      | SEMI ->
          let _menhir_stack = MenhirCell1_expr (_menhir_stack, _menhir_s, _v, _startpos) in
          _menhir_run_287 _menhir_stack _menhir_lexbuf _menhir_lexer
      | COMMA ->
          let _menhir_stack = MenhirCell1_expr (_menhir_stack, _menhir_s, _v, _startpos) in
          _menhir_run_162 _menhir_stack _menhir_lexbuf _menhir_lexer
      | COLON ->
          let _menhir_stack = MenhirCell1_expr (_menhir_stack, _menhir_s, _v, _startpos) in
          _menhir_run_188 _menhir_stack _menhir_lexbuf _menhir_lexer
      | RBRACE ->
          let x = _v in
          let _v = _menhir_action_129 x in
          _menhir_goto_separated_nonempty_list_COMMA_expr_ _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s _tok
      | _ ->
          _eRR ()
  
  and _menhir_run_157 : type  ttv_stack. ((ttv_stack, _menhir_box_program) _menhir_cell1_walrus_expr _menhir_cell0_IF, _menhir_box_program) _menhir_cell1_expr -> _ -> _ -> _ -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _v _tok ->
      let MenhirCell1_expr (_menhir_stack, _, c, _) = _menhir_stack in
      let MenhirCell0_IF (_menhir_stack, _) = _menhir_stack in
      let MenhirCell1_walrus_expr (_menhir_stack, _menhir_s, e, _startpos_e_) = _menhir_stack in
      let a = _v in
      let _v = _menhir_action_051 _startpos_e_ a c e in
      _menhir_goto_if_expr _menhir_stack _menhir_lexbuf _menhir_lexer _startpos_e_ _v _menhir_s _tok
  
  and _menhir_run_135 : type  ttv_stack. (ttv_stack, _menhir_box_program) _menhir_cell1_and_expr -> _ -> _ -> _ -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _v _tok ->
      let MenhirCell1_and_expr (_menhir_stack, _menhir_s, e, _startpos_e_) = _menhir_stack in
      let a = _v in
      let _v = _menhir_action_096 _startpos_e_ a e in
      _menhir_goto_or_expr _menhir_stack _menhir_lexbuf _menhir_lexer _startpos_e_ _v _menhir_s _tok
  
  and _menhir_run_196 : type  ttv_stack. (ttv_stack, _menhir_box_program) _menhir_cell1_NOT -> _ -> _ -> _ -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _v _tok ->
      let MenhirCell1_NOT (_menhir_stack, _menhir_s, _startpos__1_) = _menhir_stack in
      let e = _v in
      let _v = _menhir_action_086 _startpos__1_ e in
      _menhir_goto_not_expr _menhir_stack _menhir_lexbuf _menhir_lexer _startpos__1_ _v _menhir_s _tok
  
  and _menhir_run_112 : type  ttv_stack. ((ttv_stack, _menhir_box_program) _menhir_cell1_in_expr, _menhir_box_program) _menhir_cell1_NOT -> _ -> _ -> _ -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _v _tok ->
      let MenhirCell1_NOT (_menhir_stack, _, _) = _menhir_stack in
      let MenhirCell1_in_expr (_menhir_stack, _menhir_s, e, _startpos_e_) = _menhir_stack in
      let a = _v in
      let _v = _menhir_action_061 _startpos_e_ a e in
      _menhir_goto_is_expr _menhir_stack _menhir_lexbuf _menhir_lexer _startpos_e_ _v _menhir_s _tok
  
  and _menhir_run_130 : type  ttv_stack. (ttv_stack, _menhir_box_program) _menhir_cell1_in_expr -> _ -> _ -> _ -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _v _tok ->
      let MenhirCell1_in_expr (_menhir_stack, _menhir_s, e, _startpos_e_) = _menhir_stack in
      let a = _v in
      let _v = _menhir_action_060 _startpos_e_ a e in
      _menhir_goto_is_expr _menhir_stack _menhir_lexbuf _menhir_lexer _startpos_e_ _v _menhir_s _tok
  
  and _menhir_run_116 : type  ttv_stack. (ttv_stack, _menhir_box_program) _menhir_cell1_eq_expr _menhir_cell0_NOT -> _ -> _ -> _ -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _v _tok ->
      let MenhirCell0_NOT (_menhir_stack, _) = _menhir_stack in
      let MenhirCell1_eq_expr (_menhir_stack, _menhir_s, e, _startpos_e_) = _menhir_stack in
      let a = _v in
      let _v = _menhir_action_058 _startpos_e_ a e in
      _menhir_goto_in_expr _menhir_stack _menhir_lexbuf _menhir_lexer _startpos_e_ _v _menhir_s _tok
  
  and _menhir_run_129 : type  ttv_stack. (ttv_stack, _menhir_box_program) _menhir_cell1_eq_expr -> _ -> _ -> _ -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _v _tok ->
      let MenhirCell1_eq_expr (_menhir_stack, _menhir_s, e, _startpos_e_) = _menhir_stack in
      let a = _v in
      let _v = _menhir_action_057 _startpos_e_ a e in
      _menhir_goto_in_expr _menhir_stack _menhir_lexbuf _menhir_lexer _startpos_e_ _v _menhir_s _tok
  
  and _menhir_run_125 : type  ttv_stack. (ttv_stack, _menhir_box_program) _menhir_cell1_bit_or_expr -> _ -> _ -> _ -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _v _tok ->
      let MenhirCell1_bit_or_expr (_menhir_stack, _menhir_s, e, _startpos_e_) = _menhir_stack in
      let a = _v in
      let _v = _menhir_action_042 _startpos_e_ a e in
      _menhir_goto_eq_expr _menhir_stack _menhir_lexbuf _menhir_lexer _startpos_e_ _v _menhir_s _tok
  
  and _menhir_run_127 : type  ttv_stack. (ttv_stack, _menhir_box_program) _menhir_cell1_bit_or_expr -> _ -> _ -> _ -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _v _tok ->
      let MenhirCell1_bit_or_expr (_menhir_stack, _menhir_s, e, _startpos_e_) = _menhir_stack in
      let a = _v in
      let _v = _menhir_action_041 _startpos_e_ a e in
      _menhir_goto_eq_expr _menhir_stack _menhir_lexbuf _menhir_lexer _startpos_e_ _v _menhir_s _tok
  
  and _menhir_run_122 : type  ttv_stack. (ttv_stack, _menhir_box_program) _menhir_cell1_bit_and_expr -> _ -> _ -> _ -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _v _tok ->
      let MenhirCell1_bit_and_expr (_menhir_stack, _menhir_s, e, _startpos_e_) = _menhir_stack in
      let a = _v in
      let _v = _menhir_action_020 _startpos_e_ a e in
      _menhir_goto_bit_xor_expr _menhir_stack _menhir_lexbuf _menhir_lexer _startpos_e_ _v _menhir_s _tok
  
  and _menhir_run_091 : type  ttv_stack. (ttv_stack, _menhir_box_program) _menhir_cell1_cmp_expr -> _ -> _ -> _ -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _v _tok ->
      let MenhirCell1_cmp_expr (_menhir_stack, _menhir_s, e, _startpos_e_) = _menhir_stack in
      let a = _v in
      let _v = _menhir_action_139 _startpos_e_ a e in
      _menhir_goto_shift_expr _menhir_stack _menhir_lexbuf _menhir_lexer _startpos_e_ _v _menhir_s _tok
  
  and _menhir_run_102 : type  ttv_stack. (ttv_stack, _menhir_box_program) _menhir_cell1_cmp_expr -> _ -> _ -> _ -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _v _tok ->
      let MenhirCell1_cmp_expr (_menhir_stack, _menhir_s, e, _startpos_e_) = _menhir_stack in
      let a = _v in
      let _v = _menhir_action_138 _startpos_e_ a e in
      _menhir_goto_shift_expr _menhir_stack _menhir_lexbuf _menhir_lexer _startpos_e_ _v _menhir_s _tok
  
  and _menhir_run_094 : type  ttv_stack. (ttv_stack, _menhir_box_program) _menhir_cell1_add_expr -> _ -> _ -> _ -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _v _tok ->
      let MenhirCell1_add_expr (_menhir_stack, _menhir_s, e, _startpos_e_) = _menhir_stack in
      let a = _v in
      let _v = _menhir_action_024 _startpos_e_ a e in
      _menhir_goto_cmp_expr _menhir_stack _menhir_lexbuf _menhir_lexer _startpos_e_ _v _menhir_s _tok
  
  and _menhir_run_096 : type  ttv_stack. (ttv_stack, _menhir_box_program) _menhir_cell1_add_expr -> _ -> _ -> _ -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _v _tok ->
      let MenhirCell1_add_expr (_menhir_stack, _menhir_s, e, _startpos_e_) = _menhir_stack in
      let a = _v in
      let _v = _menhir_action_026 _startpos_e_ a e in
      _menhir_goto_cmp_expr _menhir_stack _menhir_lexbuf _menhir_lexer _startpos_e_ _v _menhir_s _tok
  
  and _menhir_run_098 : type  ttv_stack. (ttv_stack, _menhir_box_program) _menhir_cell1_add_expr -> _ -> _ -> _ -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _v _tok ->
      let MenhirCell1_add_expr (_menhir_stack, _menhir_s, e, _startpos_e_) = _menhir_stack in
      let a = _v in
      let _v = _menhir_action_025 _startpos_e_ a e in
      _menhir_goto_cmp_expr _menhir_stack _menhir_lexbuf _menhir_lexer _startpos_e_ _v _menhir_s _tok
  
  and _menhir_run_100 : type  ttv_stack. (ttv_stack, _menhir_box_program) _menhir_cell1_add_expr -> _ -> _ -> _ -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _v _tok ->
      let MenhirCell1_add_expr (_menhir_stack, _menhir_s, e, _startpos_e_) = _menhir_stack in
      let a = _v in
      let _v = _menhir_action_027 _startpos_e_ a e in
      _menhir_goto_cmp_expr _menhir_stack _menhir_lexbuf _menhir_lexer _startpos_e_ _v _menhir_s _tok
  
  and _menhir_run_179 : type  ttv_stack. (ttv_stack, _menhir_box_program) _menhir_cell1_DEC -> _ -> _ -> _ -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _v _tok ->
      let MenhirCell1_DEC (_menhir_stack, _menhir_s, _startpos__1_) = _menhir_stack in
      let e = _v in
      let _v = _menhir_action_172 _startpos__1_ e in
      _menhir_goto_unary_expr _menhir_stack _menhir_lexbuf _menhir_lexer _startpos__1_ _v _menhir_s _tok
  
  and _menhir_run_180 : type  ttv_stack. (ttv_stack, _menhir_box_program) _menhir_cell1_INC -> _ -> _ -> _ -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _v _tok ->
      let MenhirCell1_INC (_menhir_stack, _menhir_s, _startpos__1_) = _menhir_stack in
      let e = _v in
      let _v = _menhir_action_171 _startpos__1_ e in
      _menhir_goto_unary_expr _menhir_stack _menhir_lexbuf _menhir_lexer _startpos__1_ _v _menhir_s _tok
  
  and _menhir_run_199 : type  ttv_stack. (ttv_stack, _menhir_box_program) _menhir_cell1_MINUS -> _ -> _ -> _ -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _v _tok ->
      let MenhirCell1_MINUS (_menhir_stack, _menhir_s, _startpos__1_) = _menhir_stack in
      let e = _v in
      let _v = _menhir_action_168 _startpos__1_ e in
      _menhir_goto_unary_expr _menhir_stack _menhir_lexbuf _menhir_lexer _startpos__1_ _v _menhir_s _tok
  
  and _menhir_run_200 : type  ttv_stack. (ttv_stack, _menhir_box_program) _menhir_cell1_PLUS -> _ -> _ -> _ -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _v _tok ->
      let MenhirCell1_PLUS (_menhir_stack, _menhir_s, _startpos__1_) = _menhir_stack in
      let e = _v in
      let _v = _menhir_action_170 _startpos__1_ e in
      _menhir_goto_unary_expr _menhir_stack _menhir_lexbuf _menhir_lexer _startpos__1_ _v _menhir_s _tok
  
  and _menhir_run_283 : type  ttv_stack. ((((ttv_stack, _menhir_box_program) _menhir_cell1_FOR _menhir_cell0_LPAREN, _menhir_box_program) _menhir_cell1_stmt, _menhir_box_program) _menhir_cell1_expr, _menhir_box_program) _menhir_cell1_expr -> _ -> _ -> _ -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _v _tok ->
      let MenhirCell1_expr (_menhir_stack, _, s, _) = _menhir_stack in
      let MenhirCell1_expr (_menhir_stack, _, c, _) = _menhir_stack in
      let MenhirCell1_stmt (_menhir_stack, _, i) = _menhir_stack in
      let MenhirCell0_LPAREN (_menhir_stack, _) = _menhir_stack in
      let MenhirCell1_FOR (_menhir_stack, _menhir_s, _startpos__1_) = _menhir_stack in
      let b = _v in
      let _v = _menhir_action_143 _startpos__1_ b c i s in
      _menhir_goto_stmt _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s _tok
  
  and _menhir_run_295 : type  ttv_stack. (((ttv_stack, _menhir_box_program) _menhir_cell1_FOR _menhir_cell0_LPAREN, _menhir_box_program) _menhir_cell1_expr, _menhir_box_program) _menhir_cell1_expr -> _ -> _ -> _ -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _v _tok ->
      let MenhirCell1_expr (_menhir_stack, _, it, _) = _menhir_stack in
      let MenhirCell1_expr (_menhir_stack, _, v, _) = _menhir_stack in
      let MenhirCell0_LPAREN (_menhir_stack, _) = _menhir_stack in
      let MenhirCell1_FOR (_menhir_stack, _menhir_s, _startpos__1_) = _menhir_stack in
      let b = _v in
      let _v = _menhir_action_144 _startpos__1_ b it v in
      _menhir_goto_stmt _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s _tok
  
  and _menhir_run_296 : type  ttv_stack. (((ttv_stack, _menhir_box_program) _menhir_cell1_IF _menhir_cell0_LPAREN, _menhir_box_program) _menhir_cell1_expr as 'stack) -> _ -> _ -> _ -> ('stack, _menhir_box_program) _menhir_state -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s _tok ->
      let _menhir_stack = MenhirCell1_stmt (_menhir_stack, _menhir_s, _v) in
      match (_tok : MenhirBasics.token) with
      | ELSE ->
          _menhir_run_297 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState296
      | ELIF ->
          _menhir_run_299 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState296
      | ASSERT | BITNOT | BREAK | CONTINUE | DEC | DEF | DEL | ENUM | EXCEPT | FALSE | FINALLY | FLOAT _ | FOR | FROM | GLOBAL | IDENT _ | IF | IMPORT | INC | INT _ | LAMBDA | LBRACE | LBRACK | LPAREN | MINUS | NONE | NONLOCAL | NOT | NULL | PASS | PLUS | RAISE | RBRACE | RETURN | SELF | STRING _ | STRUCT | SUPER | TRUE | TRY | WHILE | WITH | YIELD ->
          let _v_0 = _menhir_action_036 () in
          _menhir_run_305 _menhir_stack _menhir_lexbuf _menhir_lexer _v_0 _tok
      | _ ->
          _eRR ()
  
  and _menhir_run_297 : type  ttv_stack. (((ttv_stack _menhir_cell0_LPAREN, _menhir_box_program) _menhir_cell1_expr, _menhir_box_program) _menhir_cell1_stmt as 'stack) -> _ -> _ -> ('stack, _menhir_box_program) _menhir_state -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s ->
      let _menhir_stack = MenhirCell1_ELSE (_menhir_stack, _menhir_s) in
      let _menhir_s = MenhirState297 in
      let _tok = _menhir_lexer _menhir_lexbuf in
      match (_tok : MenhirBasics.token) with
      | YIELD ->
          _menhir_run_219 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | WITH ->
          _menhir_run_223 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | WHILE ->
          _menhir_run_230 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | TRY ->
          _menhir_run_234 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | TRUE ->
          _menhir_run_034 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | SUPER ->
          _menhir_run_035 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | STRUCT ->
          _menhir_run_001 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | STRING _v ->
          _menhir_run_037 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
      | SELF ->
          _menhir_run_038 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | RETURN ->
          _menhir_run_239 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | RAISE ->
          _menhir_run_243 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | PLUS ->
          _menhir_run_039 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | PASS ->
          _menhir_run_246 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | NULL ->
          _menhir_run_040 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | NOT ->
          _menhir_run_044 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | NONLOCAL ->
          _menhir_run_248 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | NONE ->
          _menhir_run_041 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | MINUS ->
          _menhir_run_042 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | LPAREN ->
          _menhir_run_043 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | LBRACK ->
          _menhir_run_045 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | LBRACE ->
          _menhir_run_254 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | LAMBDA ->
          _menhir_run_047 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | INT _v ->
          _menhir_run_056 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
      | INC ->
          _menhir_run_057 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | IMPORT ->
          _menhir_run_012 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | IF ->
          _menhir_run_255 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | IDENT _v ->
          _menhir_run_067 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
      | GLOBAL ->
          _menhir_run_259 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | FROM ->
          _menhir_run_018 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | FOR ->
          _menhir_run_262 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | FLOAT _v ->
          _menhir_run_059 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
      | FALSE ->
          _menhir_run_060 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | ENUM ->
          _menhir_run_028 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | DEL ->
          _menhir_run_264 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | DEF ->
          _menhir_run_212 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | DEC ->
          _menhir_run_061 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | CONTINUE ->
          _menhir_run_267 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | BREAK ->
          _menhir_run_269 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | BITNOT ->
          _menhir_run_062 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | ASSERT ->
          _menhir_run_271 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
      | _ ->
          _eRR ()
  
  and _menhir_run_299 : type  ttv_stack. (((ttv_stack _menhir_cell0_LPAREN, _menhir_box_program) _menhir_cell1_expr, _menhir_box_program) _menhir_cell1_stmt as 'stack) -> _ -> _ -> ('stack, _menhir_box_program) _menhir_state -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s ->
      let _startpos = _menhir_lexbuf.Lexing.lex_start_p in
      let _menhir_stack = MenhirCell1_ELIF (_menhir_stack, _menhir_s, _startpos) in
      let _tok = _menhir_lexer _menhir_lexbuf in
      match (_tok : MenhirBasics.token) with
      | LPAREN ->
          let _startpos = _menhir_lexbuf.Lexing.lex_start_p in
          let _menhir_stack = MenhirCell0_LPAREN (_menhir_stack, _startpos) in
          let _menhir_s = MenhirState300 in
          let _tok = _menhir_lexer _menhir_lexbuf in
          (match (_tok : MenhirBasics.token) with
          | TRUE ->
              _menhir_run_034 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | SUPER ->
              _menhir_run_035 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | STRING _v ->
              _menhir_run_037 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | SELF ->
              _menhir_run_038 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | PLUS ->
              _menhir_run_039 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | NULL ->
              _menhir_run_040 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | NOT ->
              _menhir_run_044 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | NONE ->
              _menhir_run_041 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | MINUS ->
              _menhir_run_042 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | LPAREN ->
              _menhir_run_043 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | LBRACK ->
              _menhir_run_045 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | LBRACE ->
              _menhir_run_046 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | LAMBDA ->
              _menhir_run_047 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | INT _v ->
              _menhir_run_056 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | INC ->
              _menhir_run_057 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | IDENT _v ->
              _menhir_run_067 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | FLOAT _v ->
              _menhir_run_059 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
          | FALSE ->
              _menhir_run_060 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | DEC ->
              _menhir_run_061 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | BITNOT ->
              _menhir_run_062 _menhir_stack _menhir_lexbuf _menhir_lexer _menhir_s
          | _ ->
              _eRR ())
      | _ ->
          _eRR ()
  
  and _menhir_run_305 : type  ttv_stack. (((ttv_stack, _menhir_box_program) _menhir_cell1_IF _menhir_cell0_LPAREN, _menhir_box_program) _menhir_cell1_expr, _menhir_box_program) _menhir_cell1_stmt -> _ -> _ -> _ -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _v _tok ->
      let MenhirCell1_stmt (_menhir_stack, _, t) = _menhir_stack in
      let MenhirCell1_expr (_menhir_stack, _, c, _) = _menhir_stack in
      let MenhirCell0_LPAREN (_menhir_stack, _) = _menhir_stack in
      let MenhirCell1_IF (_menhir_stack, _menhir_s, _startpos__1_) = _menhir_stack in
      let e = _v in
      let _v = _menhir_action_141 _startpos__1_ c e t in
      _menhir_goto_stmt _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s _tok
  
  and _menhir_run_298 : type  ttv_stack. (((ttv_stack _menhir_cell0_LPAREN, _menhir_box_program) _menhir_cell1_expr, _menhir_box_program) _menhir_cell1_stmt, _menhir_box_program) _menhir_cell1_ELSE -> _ -> _ -> _ -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _v _tok ->
      let MenhirCell1_ELSE (_menhir_stack, _menhir_s) = _menhir_stack in
      let b = _v in
      let _v = _menhir_action_035 b in
      _menhir_goto_elif_else _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s _tok
  
  and _menhir_goto_elif_else : type  ttv_stack. (((ttv_stack _menhir_cell0_LPAREN, _menhir_box_program) _menhir_cell1_expr, _menhir_box_program) _menhir_cell1_stmt as 'stack) -> _ -> _ -> _ -> ('stack, _menhir_box_program) _menhir_state -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s _tok ->
      match _menhir_s with
      | MenhirState303 ->
          _menhir_run_304 _menhir_stack _menhir_lexbuf _menhir_lexer _v _tok
      | MenhirState296 ->
          _menhir_run_305 _menhir_stack _menhir_lexbuf _menhir_lexer _v _tok
      | _ ->
          _menhir_fail ()
  
  and _menhir_run_304 : type  ttv_stack. (((((ttv_stack _menhir_cell0_LPAREN, _menhir_box_program) _menhir_cell1_expr, _menhir_box_program) _menhir_cell1_stmt, _menhir_box_program) _menhir_cell1_ELIF _menhir_cell0_LPAREN, _menhir_box_program) _menhir_cell1_expr, _menhir_box_program) _menhir_cell1_stmt -> _ -> _ -> _ -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _v _tok ->
      let MenhirCell1_stmt (_menhir_stack, _, t) = _menhir_stack in
      let MenhirCell1_expr (_menhir_stack, _, c, _) = _menhir_stack in
      let MenhirCell0_LPAREN (_menhir_stack, _) = _menhir_stack in
      let MenhirCell1_ELIF (_menhir_stack, _menhir_s, _startpos__1_) = _menhir_stack in
      let e = _v in
      let _v = _menhir_action_034 _startpos__1_ c e t in
      _menhir_goto_elif_else _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s _tok
  
  and _menhir_run_303 : type  ttv_stack. (((((ttv_stack _menhir_cell0_LPAREN, _menhir_box_program) _menhir_cell1_expr, _menhir_box_program) _menhir_cell1_stmt, _menhir_box_program) _menhir_cell1_ELIF _menhir_cell0_LPAREN, _menhir_box_program) _menhir_cell1_expr as 'stack) -> _ -> _ -> _ -> ('stack, _menhir_box_program) _menhir_state -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s _tok ->
      let _menhir_stack = MenhirCell1_stmt (_menhir_stack, _menhir_s, _v) in
      match (_tok : MenhirBasics.token) with
      | ELSE ->
          _menhir_run_297 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState303
      | ELIF ->
          _menhir_run_299 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState303
      | ASSERT | BITNOT | BREAK | CONTINUE | DEC | DEF | DEL | ENUM | EXCEPT | FALSE | FINALLY | FLOAT _ | FOR | FROM | GLOBAL | IDENT _ | IF | IMPORT | INC | INT _ | LAMBDA | LBRACE | LBRACK | LPAREN | MINUS | NONE | NONLOCAL | NOT | NULL | PASS | PLUS | RAISE | RBRACE | RETURN | SELF | STRING _ | STRUCT | SUPER | TRUE | TRY | WHILE | WITH | YIELD ->
          let _v_0 = _menhir_action_036 () in
          _menhir_run_304 _menhir_stack _menhir_lexbuf _menhir_lexer _v_0 _tok
      | _ ->
          _eRR ()
  
  and _menhir_run_306 : type  ttv_stack. ttv_stack -> _ -> _ -> _ -> (ttv_stack, _menhir_box_program) _menhir_state -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s _tok ->
      let _menhir_stack = MenhirCell1_stmt (_menhir_stack, _menhir_s, _v) in
      match (_tok : MenhirBasics.token) with
      | YIELD ->
          _menhir_run_219 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState306
      | WITH ->
          _menhir_run_223 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState306
      | WHILE ->
          _menhir_run_230 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState306
      | TRY ->
          _menhir_run_234 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState306
      | TRUE ->
          _menhir_run_034 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState306
      | SUPER ->
          _menhir_run_035 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState306
      | STRUCT ->
          _menhir_run_001 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState306
      | STRING _v_0 ->
          _menhir_run_037 _menhir_stack _menhir_lexbuf _menhir_lexer _v_0 MenhirState306
      | SELF ->
          _menhir_run_038 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState306
      | RETURN ->
          _menhir_run_239 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState306
      | RAISE ->
          _menhir_run_243 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState306
      | PLUS ->
          _menhir_run_039 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState306
      | PASS ->
          _menhir_run_246 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState306
      | NULL ->
          _menhir_run_040 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState306
      | NOT ->
          _menhir_run_044 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState306
      | NONLOCAL ->
          _menhir_run_248 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState306
      | NONE ->
          _menhir_run_041 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState306
      | MINUS ->
          _menhir_run_042 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState306
      | LPAREN ->
          _menhir_run_043 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState306
      | LBRACK ->
          _menhir_run_045 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState306
      | LBRACE ->
          _menhir_run_254 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState306
      | LAMBDA ->
          _menhir_run_047 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState306
      | INT _v_1 ->
          _menhir_run_056 _menhir_stack _menhir_lexbuf _menhir_lexer _v_1 MenhirState306
      | INC ->
          _menhir_run_057 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState306
      | IMPORT ->
          _menhir_run_012 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState306
      | IF ->
          _menhir_run_255 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState306
      | IDENT _v_2 ->
          _menhir_run_067 _menhir_stack _menhir_lexbuf _menhir_lexer _v_2 MenhirState306
      | GLOBAL ->
          _menhir_run_259 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState306
      | FROM ->
          _menhir_run_018 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState306
      | FOR ->
          _menhir_run_262 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState306
      | FLOAT _v_3 ->
          _menhir_run_059 _menhir_stack _menhir_lexbuf _menhir_lexer _v_3 MenhirState306
      | FALSE ->
          _menhir_run_060 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState306
      | ENUM ->
          _menhir_run_028 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState306
      | DEL ->
          _menhir_run_264 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState306
      | DEF ->
          _menhir_run_212 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState306
      | DEC ->
          _menhir_run_061 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState306
      | CONTINUE ->
          _menhir_run_267 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState306
      | BREAK ->
          _menhir_run_269 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState306
      | BITNOT ->
          _menhir_run_062 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState306
      | ASSERT ->
          _menhir_run_271 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState306
      | RBRACE ->
          let _v_4 = _menhir_action_067 () in
          _menhir_run_307 _menhir_stack _menhir_lexbuf _menhir_lexer _v_4
      | _ ->
          _eRR ()
  
  and _menhir_run_307 : type  ttv_stack. (ttv_stack, _menhir_box_program) _menhir_cell1_stmt -> _ -> _ -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _v ->
      let MenhirCell1_stmt (_menhir_stack, _menhir_s, x) = _menhir_stack in
      let xs = _v in
      let _v = _menhir_action_068 x xs in
      _menhir_goto_list_stmt_ _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s
  
  and _menhir_goto_list_stmt_ : type  ttv_stack. ttv_stack -> _ -> _ -> _ -> (ttv_stack, _menhir_box_program) _menhir_state -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s ->
      match _menhir_s with
      | MenhirState306 ->
          _menhir_run_307 _menhir_stack _menhir_lexbuf _menhir_lexer _v
      | MenhirState218 ->
          _menhir_run_308 _menhir_stack _menhir_lexbuf _menhir_lexer _v
      | MenhirState254 ->
          _menhir_run_308 _menhir_stack _menhir_lexbuf _menhir_lexer _v
      | _ ->
          _menhir_fail ()
  
  and _menhir_run_311 : type  ttv_stack. (((ttv_stack, _menhir_box_program) _menhir_cell1_EXCEPT, _menhir_box_program) _menhir_cell1_option_expr_, _menhir_box_program) _menhir_cell1_option_preceded_AS_IDENT__ -> _ -> _ -> _ -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _v _tok ->
      let MenhirCell1_option_preceded_AS_IDENT__ (_menhir_stack, _, a) = _menhir_stack in
      let MenhirCell1_option_expr_ (_menhir_stack, _, e) = _menhir_stack in
      let MenhirCell1_EXCEPT (_menhir_stack, _menhir_s) = _menhir_stack in
      let b = _v in
      let _v = _menhir_action_043 a b e in
      let _menhir_stack = MenhirCell1_except_clause (_menhir_stack, _menhir_s, _v) in
      match (_tok : MenhirBasics.token) with
      | EXCEPT ->
          _menhir_run_236 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState317
      | ASSERT | BITNOT | BREAK | CONTINUE | DEC | DEF | DEL | ELIF | ELSE | ENUM | FALSE | FINALLY | FLOAT _ | FOR | FROM | GLOBAL | IDENT _ | IF | IMPORT | INC | INT _ | LAMBDA | LBRACE | LBRACK | LPAREN | MINUS | NONE | NONLOCAL | NOT | NULL | PASS | PLUS | RAISE | RBRACE | RETURN | SELF | STRING _ | STRUCT | SUPER | TRUE | TRY | WHILE | WITH | YIELD ->
          let _v_0 = _menhir_action_063 () in
          _menhir_run_318 _menhir_stack _menhir_lexbuf _menhir_lexer _v_0 _tok
      | _ ->
          _eRR ()
  
  and _menhir_run_318 : type  ttv_stack. (ttv_stack, _menhir_box_program) _menhir_cell1_except_clause -> _ -> _ -> _ -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _v _tok ->
      let MenhirCell1_except_clause (_menhir_stack, _menhir_s, x) = _menhir_stack in
      let xs = _v in
      let _v = _menhir_action_064 x xs in
      _menhir_goto_list_except_clause_ _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s _tok
  
  and _menhir_goto_list_except_clause_ : type  ttv_stack. ttv_stack -> _ -> _ -> _ -> (ttv_stack, _menhir_box_program) _menhir_state -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s _tok ->
      match _menhir_s with
      | MenhirState235 ->
          _menhir_run_312 _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s _tok
      | MenhirState317 ->
          _menhir_run_318 _menhir_stack _menhir_lexbuf _menhir_lexer _v _tok
      | _ ->
          _menhir_fail ()
  
  and _menhir_run_314 : type  ttv_stack. (((ttv_stack, _menhir_box_program) _menhir_cell1_TRY, _menhir_box_program) _menhir_cell1_block, _menhir_box_program) _menhir_cell1_list_except_clause_ -> _ -> _ -> _ -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _v _tok ->
      let b = _v in
      let _v = _menhir_action_048 b in
      let x = _v in
      let _v = _menhir_action_090 x in
      _menhir_goto_option_finally_clause_ _menhir_stack _menhir_lexbuf _menhir_lexer _v _tok
  
  and _menhir_run_319 : type  ttv_stack. ((ttv_stack, _menhir_box_program) _menhir_cell1_WHILE _menhir_cell0_LPAREN, _menhir_box_program) _menhir_cell1_expr -> _ -> _ -> _ -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _v _tok ->
      let MenhirCell1_expr (_menhir_stack, _, c, _) = _menhir_stack in
      let MenhirCell0_LPAREN (_menhir_stack, _) = _menhir_stack in
      let MenhirCell1_WHILE (_menhir_stack, _menhir_s, _startpos__1_) = _menhir_stack in
      let b = _v in
      let _v = _menhir_action_142 _startpos__1_ b c in
      _menhir_goto_stmt _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s _tok
  
  and _menhir_run_320 : type  ttv_stack. ((ttv_stack, _menhir_box_program) _menhir_cell1_WITH, _menhir_box_program) _menhir_cell1_separated_nonempty_list_COMMA_with_item_ -> _ -> _ -> _ -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _v _tok ->
      let MenhirCell1_separated_nonempty_list_COMMA_with_item_ (_menhir_stack, _, es) = _menhir_stack in
      let MenhirCell1_WITH (_menhir_stack, _menhir_s, _startpos__1_) = _menhir_stack in
      let b = _v in
      let _v = _menhir_action_150 _startpos__1_ b es in
      _menhir_goto_stmt _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s _tok
  
  and _menhir_run_329 : type  ttv_stack. ttv_stack -> _ -> _ -> _ -> (ttv_stack, _menhir_box_program) _menhir_state -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s _tok ->
      let d = _v in
      let _v = _menhir_action_164 d in
      _menhir_goto_top_decl _menhir_stack _menhir_lexbuf _menhir_lexer _v _menhir_s _tok
  
  let _menhir_run_000 : type  ttv_stack. ttv_stack -> _ -> _ -> _menhir_box_program =
    fun _menhir_stack _menhir_lexbuf _menhir_lexer ->
      let _tok = _menhir_lexer _menhir_lexbuf in
      match (_tok : MenhirBasics.token) with
      | STRUCT ->
          _menhir_run_001 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState000
      | IMPORT ->
          _menhir_run_012 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState000
      | FROM ->
          _menhir_run_018 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState000
      | ENUM ->
          _menhir_run_028 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState000
      | DEF ->
          _menhir_run_212 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState000
      | CLASS ->
          _menhir_run_322 _menhir_stack _menhir_lexbuf _menhir_lexer MenhirState000
      | EOF ->
          let _v = _menhir_action_069 () in
          _menhir_run_336 _menhir_stack _v
      | _ ->
          _eRR ()
  
end

let program =
  fun _menhir_lexer _menhir_lexbuf ->
    let _menhir_stack = () in
    let MenhirBox_program v = _menhir_run_000 _menhir_stack _menhir_lexbuf _menhir_lexer in
    v
