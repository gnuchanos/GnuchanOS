{
(* ================================================================
   lexer.mll — OCamlLex lexer for GCL-SH
   ================================================================ *)
open Parser
}

let digit = ['0'-'9']
let hex_digit = ['0'-'9' 'a'-'f' 'A'-'F']
let oct_digit = ['0'-'7']
let bin_digit = ['0'-'1']
let ident_start = ['a'-'z' 'A'-'Z' '_']
let ident_cont = ident_start | digit
let whitespace = [' ' '\t' '\r']

rule token = parse
  | whitespace+  { token lexbuf }
  | '\n'         { Lexing.new_line lexbuf; token lexbuf }

  (* Comments *)
  | '#' [^'\n']* { token lexbuf }
  | "/*"         { comment lexbuf; token lexbuf }

  (* Punctuation *)
  | '('          { LPAREN }
  | ')'          { RPAREN }
  | '{'          { LBRACE }
  | '}'          { RBRACE }
  | '['          { LBRACK }
  | ']'          { RBRACK }
  | ';'          { SEMI }
  | ','          { COMMA }
  | '.'          { DOT }
  | ':'          { COLON }
  | "->"         { ARROW }
  | "=>"         { DARROW }

  (* Multi-char operators *)
  | "**="        { POW_ASSIGN }
  | "**"         { POW }
  | "//="        { FLOORDIV_ASSIGN }
  | "//"         { FLOORDIV }
  | "<<="        { LSHIFT_ASSIGN }
  | ">>="        { RSHIFT_ASSIGN }
  | "<<"         { LSHIFT }
  | ">>"         { RSHIFT }
  | "++"         { INC }
  | "--"         { DEC }
  | "=="         { EQEQ }
  | "!="         { NEQ }
  | "<="         { LE }
  | ">="         { GE }
  | "+="         { ADD_ASSIGN }
  | "-="         { SUB_ASSIGN }
  | "*="         { MUL_ASSIGN }
  | "/="         { DIV_ASSIGN }
  | "%="         { MOD_ASSIGN }
  | "&="         { BITAND_ASSIGN }
  | "|="         { BITOR_ASSIGN }
  | "^="         { BITXOR_ASSIGN }
  | ":="         { WALRUS }

  (* Single-char operators *)
  | "+"          { PLUS }
  | "-"          { MINUS }
  | "*"          { STAR }
  | "/"          { SLASH }
  | "%"          { PERCENT }
  | "&"          { BITAND }
  | "|"          { BITOR }
  | "^"          { BITXOR }
  | "~"          { BITNOT }
  | "<"          { LT }
  | ">"          { GT }
  | "="          { ASSIGN }

  (* Numbers *)
  | '0' ('x'|'X') hex_digit+ as s     { INT (int_of_string s) }
  | '0' ('o'|'O') oct_digit+ as s     { INT (int_of_string s) }
  | '0' ('b'|'B') bin_digit+ as s     { INT (int_of_string s) }
  | digit+ '.' digit+ (['e' 'E'] ['+' '-']? digit+)? as s
                                       { FLOAT (float_of_string s) }
  | digit+ (['e' 'E'] ['+' '-']? digit+) as s
                                       { FLOAT (float_of_string s) }
  | digit+ as s                        { INT (int_of_string s) }
  | digit+ '.'? digit* ['j' 'J'] as s  { let s' = String.sub s 0 (String.length s - 1) in
                                          FLOAT (float_of_string s') }

  (* Strings - triple before single to avoid conflict *)
  | "\"\"\""     { STRING (read_triple_string (Buffer.create 32) lexbuf) }
  | '"'          { STRING (read_string (Buffer.create 32) lexbuf) }
  | '\''         { STRING (read_string_single (Buffer.create 32) lexbuf) }

  (* Identifiers and keywords *)
  | ident_start ident_cont* as s {
      match s with
      | "if"        -> IF      | "elif"      -> ELIF    | "else"      -> ELSE
      | "while"     -> WHILE   | "for"       -> FOR     | "in"        -> IN
      | "return"    -> RETURN  | "break"     -> BREAK   | "continue"  -> CONTINUE
      | "def"       -> DEF     | "class"     -> CLASS
      | "struct"    -> STRUCT  | "enum"      -> ENUM    | "pass"      -> PASS
      | "true"      -> TRUE    | "false"     -> FALSE   | "null"      -> NULL
      | "none"      -> NONE    | "and"       -> AND     | "or"        -> OR
      | "not"       -> NOT     | "try"       -> TRY     | "except"    -> EXCEPT
      | "finally"   -> FINALLY | "raise"     -> RAISE   | "with"      -> WITH
      | "yield"     -> YIELD   | "lambda"    -> LAMBDA
      | "match"     -> MATCH   | "case"      -> CASE
      | "global"    -> GLOBAL  | "nonlocal"  -> NONLOCAL
      | "del"       -> DEL     | "assert"    -> ASSERT
      | "import"    -> IMPORT  | "from"      -> FROM    | "as"        -> AS
      | "is"        -> IS      | "self"      -> SELF    | "super"     -> SUPER
      | "True"      -> TRUE    | "False"     -> FALSE   | "None"      -> NONE
      | _           -> IDENT s
    }

  | eof          { EOF }
  | _            {
      let p = Lexing.lexeme_start_p lexbuf in
      Error.add_error (Error.unexpected_char (Lexing.lexeme_char lexbuf 0)
        {file=p.pos_fname; line=p.pos_lnum; col=p.pos_cnum - p.pos_bol + 1});
      token lexbuf
    }

and comment = parse
  | "*/"         { () }
  | eof          { () }
  | '\n'         { Lexing.new_line lexbuf; comment lexbuf }
  | _            { comment lexbuf }

and read_string buf = parse
  | '"'          { Buffer.contents buf }
  | '\\' '"'     { Buffer.add_char buf '"'; read_string buf lexbuf }
  | '\\' 'n'     { Buffer.add_char buf '\n'; read_string buf lexbuf }
  | '\\' 't'     { Buffer.add_char buf '\t'; read_string buf lexbuf }
  | '\\' 'r'     { Buffer.add_char buf '\r'; read_string buf lexbuf }
  | '\\' '\\'    { Buffer.add_char buf '\\'; read_string buf lexbuf }
  | '\\' '0'     { Buffer.add_char buf (Char.chr 0); read_string buf lexbuf }
  | '\\' 'x' hex_digit hex_digit
      { let ch = int_of_string ("0x" ^ Lexing.lexeme lexbuf) in
        Buffer.add_char buf (Char.chr ch); read_string buf lexbuf }
  | '\\' digit digit digit
      { let ch = int_of_string ("0o" ^ Lexing.lexeme lexbuf) in
        Buffer.add_char buf (Char.chr ch); read_string buf lexbuf }
  | '\\' _       { Buffer.add_char buf (Lexing.lexeme_char lexbuf 0);
                   read_string buf lexbuf }
  | '\n'         { Lexing.new_line lexbuf; read_string buf lexbuf }
  | eof          { let p = Lexing.lexeme_start_p lexbuf in
                   Error.add_error (Error.unterminated_string
                     {file=p.pos_fname; line=p.pos_lnum; col=p.pos_cnum - p.pos_bol + 1});
                   Buffer.contents buf }
  | _            { Buffer.add_char buf (Lexing.lexeme_char lexbuf 0);
                   read_string buf lexbuf }

and read_string_single buf = parse
  | '\''         { Buffer.contents buf }
  | '\\' '\''    { Buffer.add_char buf '\''; read_string_single buf lexbuf }
  | '\\' '\\'    { Buffer.add_char buf '\\'; read_string_single buf lexbuf }
  | '\\' _       { Buffer.add_char buf (Lexing.lexeme_char lexbuf 0);
                   read_string_single buf lexbuf }
  | '\n'         { Lexing.new_line lexbuf; read_string_single buf lexbuf }
  | eof          { let p = Lexing.lexeme_start_p lexbuf in
                   Error.add_error (Error.unterminated_string
                     {file=p.pos_fname; line=p.pos_lnum; col=p.pos_cnum - p.pos_bol + 1});
                   Buffer.contents buf }
  | _            { Buffer.add_char buf (Lexing.lexeme_char lexbuf 0);
                   read_string_single buf lexbuf }

and read_triple_string buf = parse
  | "\"\"\""     { Buffer.contents buf }
  | "\"\""       { Buffer.add_string buf "\"\""; read_triple_string buf lexbuf }
  | '"'          { Buffer.add_char buf '"'; read_triple_string buf lexbuf }
  | '\\' '"'     { Buffer.add_char buf '"'; read_triple_string buf lexbuf }
  | '\\' 'n'     { Buffer.add_char buf '\n'; read_triple_string buf lexbuf }
  | '\\' 't'     { Buffer.add_char buf '\t'; read_triple_string buf lexbuf }
  | '\\' '\\'    { Buffer.add_char buf '\\'; read_triple_string buf lexbuf }
  | '\\' _       { Buffer.add_char buf (Lexing.lexeme_char lexbuf 0);
                   read_triple_string buf lexbuf }
  | '\n'         { Lexing.new_line lexbuf; Buffer.add_char buf '\n'; read_triple_string buf lexbuf }
  | eof          { let p = Lexing.lexeme_start_p lexbuf in
                   Error.add_error (Error.unterminated_string
                     {file=p.pos_fname; line=p.pos_lnum; col=p.pos_cnum - p.pos_bol + 1});
                   Buffer.contents buf }
  | _            { Buffer.add_char buf (Lexing.lexeme_char lexbuf 0);
                   read_triple_string buf lexbuf }
