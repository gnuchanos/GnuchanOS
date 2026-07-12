(* ================================================================
   main.ml — Main entry point for GCL-SH (Gnuchan Self-Hosting) compiler
   Uses generated OCamlLex lexer + Menhir parser
   Features: debug mode, error logging, phase tracking
   ================================================================ *)

open Ast
open Error

let version = "0.3.0"

let read_file path =
  Error.log_lexer (Printf.sprintf "reading file: %s" path);
  let ic =
    try open_in_bin path
    with Sys_error msg ->
      Error.add_error (Error.file_read_error path msg);
      Error.print_all_errors ();
      exit (Error.exit_code ()) in
  let n = in_channel_length ic in
  let s = Bytes.create n in
  really_input ic s 0 n;
  close_in ic;
  (* Cache source for error display *)
  let content = Bytes.to_string s in
  Error.cache_source path content;
  Error.log_lexer (Printf.sprintf "read %d bytes from %s" n path);
  content

let parse_source source file =
  Error.log_parser (Printf.sprintf "parsing: %s" file);
  let lexbuf = Lexing.from_string source in
  lexbuf.lex_curr_p <- { lexbuf.lex_curr_p with pos_fname = file };
  let prog =
    try Parser.program Lexer.token lexbuf
    with Parser.Error ->
      let p = lexbuf.lex_curr_p in
      let pos = {file=p.pos_fname; line=p.pos_lnum; col=p.pos_cnum - p.pos_bol + 1} in
      Error.add_error (Error.syntax_error "failed to parse input" pos);
      { decls = []; file }
  in
  Error.log_parser (Printf.sprintf "parsed %d declarations" (List.length prog.decls));
  { prog with file }

let compile_to_string source file =
  Error.log_info (Printf.sprintf "=== Compile: %s ===" file);

  (* Phase 1: Parse *)
  let prog = parse_source source file in
  if Error.has_errors () then begin
    Error.print_all_errors ~file ();
    exit (Error.exit_code ())
  end;

  (* Phase 2: Type check *)
  Error.log_typechk "starting type check";
  let _chk = Typechecker.check_program prog in
  if Error.has_errors () then begin
    Error.print_all_errors ~file ();
    exit (Error.exit_code ())
  end;

  (* Phase 3: Code generation *)
  Error.log_codegen "generating C code";
  let c_source = Codegen.gen_program prog in
  Error.log_codegen (Printf.sprintf "generated %d bytes of C code" (String.length c_source));
  Error.log_info "=== Compile complete ===";
  c_source

let compile_to_file source file output_path =
  Error.log_info (Printf.sprintf "compiling %s -> %s" file output_path);
  let c_source = compile_to_string source file in
  let oc = open_out output_path in
  output_string oc c_source;
  close_out oc;
  Error.log_info (Printf.sprintf "wrote %s" output_path)

let token_to_string tok =
  let open Parser in
  match tok with
  | EOF -> "EOF"
  | LPAREN -> "'('" | RPAREN -> "')'" | LBRACE -> "'{'" | RBRACE -> "'}'"
  | LBRACK -> "'['" | RBRACK -> "']'"
  | SEMI -> "';'" | COMMA -> "','" | DOT -> "'.'" | COLON -> "':'" | ARROW -> "'->'" | DARROW -> "'=>'"
  | PLUS -> "'+'" | MINUS -> "'-'" | STAR -> "'*'" | SLASH -> "'/'" | PERCENT -> "'%'"
  | POW -> "'**'" | FLOORDIV -> "'//'"
  | EQEQ -> "'=='" | NEQ -> "'!='" | LT -> "'<'" | GT -> "'>'" | LE -> "'<='" | GE -> "'>='"
  | AND -> "'and'" | OR -> "'or'" | NOT -> "'not'"
  | IS -> "'is'" | IN -> "'in'"
  | BITAND -> "'&'" | BITOR -> "'|'" | BITXOR -> "'^'" | BITNOT -> "'~'"
  | LSHIFT -> "'<<'" | RSHIFT -> "'>>'"
  | ASSIGN -> "'='" | ADD_ASSIGN -> "'+='" | SUB_ASSIGN -> "'-='"
  | MUL_ASSIGN -> "'*='" | DIV_ASSIGN -> "'/='" | MOD_ASSIGN -> "'%='"
  | POW_ASSIGN -> "'**='" | FLOORDIV_ASSIGN -> "'//='"
  | BITAND_ASSIGN -> "'&='" | BITOR_ASSIGN -> "'|='"
  | BITXOR_ASSIGN -> "'^='"
  | LSHIFT_ASSIGN -> "'<<='" | RSHIFT_ASSIGN -> "'>>='"
  | INC -> "'++'" | DEC -> "'--'" | WALRUS -> "':='"
  | IF -> "IF" | ELIF -> "ELIF" | ELSE -> "ELSE"
  | WHILE -> "WHILE" | FOR -> "FOR"
  | RETURN -> "RETURN" | BREAK -> "BREAK" | CONTINUE -> "CONTINUE"
  | DEF -> "DEF" | CLASS -> "CLASS" | STRUCT -> "STRUCT" | ENUM -> "ENUM" | PASS -> "PASS"
  | TRY -> "TRY" | EXCEPT -> "EXCEPT" | FINALLY -> "FINALLY" | RAISE -> "RAISE"
  | WITH -> "WITH" | YIELD -> "YIELD" | LAMBDA -> "LAMBDA"
  | MATCH -> "MATCH" | CASE -> "CASE"
  | GLOBAL -> "GLOBAL" | NONLOCAL -> "NONLOCAL"
  | DEL -> "DEL" | ASSERT -> "ASSERT"
  | IMPORT -> "IMPORT" | FROM -> "FROM" | AS -> "AS"
  | SELF -> "SELF" | SUPER -> "SUPER"
  | TRUE -> "TRUE" | FALSE -> "FALSE" | NULL -> "NULL" | NONE -> "NONE"
  | INT n -> Printf.sprintf "INT(%d)" n
  | FLOAT f -> Printf.sprintf "FLOAT(%g)" f
  | STRING s -> Printf.sprintf "STRING(\"%s\")" (String.escaped s)
  | IDENT s -> Printf.sprintf "IDENT(%s)" s

let run_lex source file =
  Error.log_lexer (Printf.sprintf "lexer dump: %s" file);
  let lexbuf = Lexing.from_string source in
  lexbuf.lex_curr_p <- { lexbuf.lex_curr_p with pos_fname = file };
  let rec loop () =
    let tok = Lexer.token lexbuf in
    let p = lexbuf.lex_curr_p in
    let line = p.pos_lnum in
    let col = p.pos_cnum - p.pos_bol + 1 in
    Printf.printf "%5d:%-3d %s\n" line col (token_to_string tok);
    flush stdout;
    match tok with Parser.EOF -> () | _ -> loop ()
  in
  loop ()

let dump_ast prog =
  let rec dump_expr e =
    match e.e_node with
    | ELiteral l -> (match l with
        | LInt n -> Printf.sprintf "%d" n | LFloat f -> Printf.sprintf "%g" f
        | LString s -> Printf.sprintf "\"%s\"" (String.escaped s)
        | LBool true -> "true" | LBool false -> "false" | LNull -> "null"
        | LBytes s -> Printf.sprintf "b\"%s\"" (String.escaped s)
        | LComplex (r, i) -> Printf.sprintf "%g%+gj" r i)
    | EIdent n -> n
    | EBinary (b, l, r) -> Printf.sprintf "(%s %s %s)" (dump_expr l) (dump_binop b) (dump_expr r)
    | EUnary (op, e) -> Printf.sprintf "(%s%s)" (dump_unop op) (dump_expr e)
    | ECall (f, args) -> Printf.sprintf "%s(%s)" (dump_expr f) (String.concat ", " (List.map dump_expr args))
    | EIndex (a, i) -> Printf.sprintf "%s[%s]" (dump_expr a) (dump_expr i)
    | ESlice (a, b, c) ->
        let s1 = match a with Some e -> dump_expr e | None -> "" in
        let s2 = match b with Some e -> dump_expr e | None -> "" in
        let s3 = match c with Some e -> dump_expr e | None -> "" in
        Printf.sprintf "%s:%s:%s" s1 s2 s3
    | EList items -> Printf.sprintf "[%s]" (String.concat ", " (List.map dump_expr items))
    | ETuple items -> Printf.sprintf "(%s)" (String.concat ", " (List.map dump_expr items))
    | EDict items ->
        let pairs = List.map (fun (k, v) -> Printf.sprintf "%s:%s" (dump_expr k) (dump_expr v)) items in
        Printf.sprintf "{%s}" (String.concat ", " pairs)
    | ESet items -> Printf.sprintf "{%s}" (String.concat ", " (List.map dump_expr items))
    | EMember (o, n) -> Printf.sprintf "%s.%s" (dump_expr o) n
    | ETernary (c, t, e) -> Printf.sprintf "(%s if %s else %s)" (dump_expr t) (dump_expr c) (dump_expr e)
    | EWalrus (n, e) -> Printf.sprintf "(%s := %s)" n (dump_expr e)
    | ELambda (ps, body) ->
        Printf.sprintf "lambda %s: %s" (String.concat ", " (List.map (fun p -> p.p_name) ps)) (dump_expr body)
    | EListComp (e, cs) -> Printf.sprintf "[%s %s]" (dump_expr e) (dump_comp_clauses cs)
    | ESetComp (e, cs) -> Printf.sprintf "{%s %s}" (dump_expr e) (dump_comp_clauses cs)
    | EDictComp (k, v, cs) -> Printf.sprintf "{%s: %s %s}" (dump_expr k) (dump_expr v) (dump_comp_clauses cs)
    | EGenerator (e, cs) -> Printf.sprintf "(%s %s)" (dump_expr e) (dump_comp_clauses cs)
  and dump_binop = function
    | Add -> "+" | Sub -> "-" | Mul -> "*" | Div -> "/" | Mod -> "%" | Pow -> "**" | FloorDiv -> "//"
    | Eq -> "==" | Ne -> "!=" | Lt -> "<" | Le -> "<=" | Gt -> ">" | Ge -> ">="
    | And -> "and" | Or -> "or"
    | BitOr -> "|" | BitAnd -> "&" | BitXor -> "^" | LShift -> "<<" | RShift -> ">>"
    | Is -> "is" | IsNot -> "is not" | In -> "in" | NotIn -> "not in"
    | Assign -> "=" | AddAssign -> "+=" | SubAssign -> "-=" | MulAssign -> "*=" | DivAssign -> "/=" | ModAssign -> "%="
    | PowAssign -> "**=" | FloorDivAssign -> "//="
    | BitOrAssign -> "|=" | BitAndAssign -> "&=" | BitXorAssign -> "^=" | LShiftAssign -> "<<=" | RShiftAssign -> ">>="
    | Seq -> ";"
  and dump_unop = function
    | Neg -> "-" | Not -> "not " | BitNot -> "~" | Plus -> "+"
    | PreInc -> "++" | PreDec -> "--" | PostInc -> "++" | PostDec -> "--"
  and dump_comp_clauses cs =
    String.concat " " (List.map (fun c ->
      let cc = Printf.sprintf "for %s in %s" (dump_expr c.cc_binding) (dump_expr c.cc_iterator) in
      let conds = if c.cc_conditions = [] then "" else
        " " ^ String.concat " " (List.map (fun e -> "if " ^ dump_expr e) c.cc_conditions) in
      cc ^ conds
    ) cs)
  and dump_stmt i s =
    match s.s_node with
    | SBlock stmts ->
        Printf.printf "  [%d] Block (%d stmts)\n" i (List.length stmts)
    | SExpr e -> Printf.printf "  [%d] Expr: %s\n" i (dump_expr e)
    | SIf (c, _, _) -> Printf.printf "  [%d] If: %s\n" i (dump_expr c)
    | SWhile (c, _) -> Printf.printf "  [%d] While: %s\n" i (dump_expr c)
    | SFor (init, c, step, _) -> Printf.printf "  [%d] For(C-style): %s; %s; %s\n" i
        (dump_stmt_as_expr init) (dump_expr c) (dump_expr step)
    | SForIn (v, it, _) -> Printf.printf "  [%d] ForIn: %s in %s\n" i (dump_expr v) (dump_expr it)
    | SReturn None -> Printf.printf "  [%d] Return\n" i
    | SReturn (Some e) -> Printf.printf "  [%d] Return: %s\n" i (dump_expr e)
    | SBreak -> Printf.printf "  [%d] Break\n" i
    | SContinue -> Printf.printf "  [%d] Continue\n" i
    | SDelete e -> Printf.printf "  [%d] Delete: %s\n" i (dump_expr e)
    | SAssert (c, m) -> Printf.printf "  [%d] Assert: %s%s\n" i (dump_expr c)
        (match m with Some e -> Printf.sprintf ", %s" (dump_expr e) | None -> "")
    | SRaise e -> Printf.printf "  [%d] Raise: %s\n" i
        (match e with Some e -> dump_expr e | None -> "")
    | SGlobal ns -> Printf.printf "  [%d] Global: %s\n" i (String.concat ", " ns)
    | SNonlocal ns -> Printf.printf "  [%d] Nonlocal: %s\n" i (String.concat ", " ns)
    | STry _ -> Printf.printf "  [%d] Try\n" i
    | SWith _ -> Printf.printf "  [%d] With\n" i
    | SYield None -> Printf.printf "  [%d] Yield\n" i
    | SYield (Some e) -> Printf.printf "  [%d] Yield: %s\n" i (dump_expr e)
    | SYieldFrom e -> Printf.printf "  [%d] YieldFrom: %s\n" i (dump_expr e)
    | SMatch (e, cases) -> Printf.printf "  [%d] Match: %s (%d cases)\n" i (dump_expr e) (List.length cases)
    | SDecorated (decs, d) -> Printf.printf "  [%d] Decorated (%d decs)\n" i (List.length decs)
    | SDecl d -> dump_decl i d
  and dump_stmt_as_expr s = match s.s_node with
    | SExpr e -> dump_expr e
    | _ -> "..."
  and dump_decl i d =
    match d.d_node with
    | DVarDecl (n, init) -> Printf.printf "  [%d] VarDecl: %s%s\n" i n
        (match init with Some e -> Printf.sprintf " = %s" (dump_expr e) | None -> "")
    | DVarDeclAnnot (n, _, init) -> Printf.printf "  [%d] VarDeclAnnot: %s%s\n" i n
        (match init with Some e -> Printf.sprintf " = %s" (dump_expr e) | None -> "")
    | DFuncDef (n, ps, _, _) -> Printf.printf "  [%d] FuncDef: %s(%s)\n" i n
        (String.concat ", " (List.map (fun p -> p.p_name) ps))
    | DExternFunc (n, ps) -> Printf.printf "  [%d] ExternFunc: %s(%s)\n" i n
        (String.concat ", " (List.map (fun p -> p.p_name) ps))
    | DStructDecl (n, _) -> Printf.printf "  [%d] StructDecl: %s\n" i n
    | DEnumDecl (n, items) -> Printf.printf "  [%d] EnumDecl: %s = {%s}\n" i n
        (String.concat ", " (List.map fst items))
    | DClassDef (n, bases, _) -> Printf.printf "  [%d] ClassDef: %s(%s)\n" i n
        (String.concat ", " (List.map dump_expr bases))
    | DImport (m, alias) -> Printf.printf "  [%d] Import: %s%s\n" i m
        (match alias with Some a -> Printf.sprintf " as %s" a | None -> "")
    | DFromImport (m, items) -> Printf.printf "  [%d] FromImport: %s.%s\n" i m
        (String.concat ", " (List.map (fun (n, a) ->
           match a with Some a -> Printf.sprintf "%s as %s" n a | None -> n) items))
    | DTypeAlias (n, _) -> Printf.printf "  [%d] TypeAlias: %s\n" i n
  in
  Printf.printf "=== AST Dump (%d declarations) ===\n" (List.length prog.decls);
  Printf.printf "File: %s\n" prog.file;
  List.iteri (fun i d -> dump_decl i d) prog.decls;
  Printf.printf "=== End AST ===\n%!"

let print_version () =
  Printf.printf "GCL-SH Self-Hosting Compiler v%s\n" version

let print_usage () =
  print_version ();
  Printf.printf "Usage:\n";
  Printf.printf "  gclc <file.gcl>                    — Lex only (show tokens)\n";
  Printf.printf "  gclc -p <file.gcl>                 — Parse (show AST)\n";
  Printf.printf "  gclc -c <file.gcl>                 — Generate C code (stdout)\n";
  Printf.printf "  gclc -o <output.c> <input.gcl>     — Compile to C file\n";
  Printf.printf "  gclc -t <file.gcl>                 — Type-check only\n";
  Printf.printf "  gclc -d <file.gcl>                 — Debug mode (verbose logs)\n";
  Printf.printf "  gclc -h                            — This help\n"

let parse_file file =
  let source = read_file file in
  let prog = parse_source source file in
  prog

let () =
  let args = List.tl (Array.to_list Sys.argv) in
  (* Check for debug flag anywhere and remove it *)
  let is_debug = List.exists (fun a -> a = "-d" || a = "--debug") args in
  if is_debug then Error.set_debug true;
  let args = List.filter (fun a -> a <> "-d" && a <> "--debug") args in

  match args with
  | [] -> print_usage ()
  | ["-h"] | ["-help"] | ["--help"] -> print_usage ()
  | ["-version"] | ["--version"] -> print_version ()
  | ["-p"; file] | ["--parse"; file] ->
      Error.log_info (Printf.sprintf "mode: parse AST from %s" file);
      let prog = parse_file file in
      if Error.has_errors () then begin
        Error.print_all_errors ();
        exit (Error.exit_code ())
      end;
      dump_ast prog
  | ["-c"; file] | ["--compile"; file] ->
      Error.log_info (Printf.sprintf "mode: compile %s to C (stdout)" file);
      print_string (compile_to_string (read_file file) file)
  | ["-t"; file] | ["--typecheck"; file] ->
      Error.log_info (Printf.sprintf "mode: type-check %s" file);
      let prog = parse_file file in
      if Error.has_errors () then begin
        Error.print_all_errors ();
        exit (Error.exit_code ())
      end;
      let _chk = Typechecker.check_program prog in
      if Error.has_errors () then begin
        Error.print_all_errors ();
        exit (Error.exit_code ())
      end;
      Printf.printf "Type-check OK: %d declarations\n" (List.length prog.decls);
      flush stdout
  | ["-o"; output; input] ->
      Error.log_info (Printf.sprintf "mode: compile %s -> %s" input output);
      compile_to_file (read_file input) input output;
      Printf.eprintf "Success! Compile with: gcc -o %s %s\n"
        (Filename.chop_extension output ^ ".exe") output
  | [file] ->
      if is_debug then begin
        Error.log_info (Printf.sprintf "mode: full compile (debug) %s" file);
        print_string (compile_to_string (read_file file) file)
      end else begin
        Error.log_info (Printf.sprintf "mode: lexer dump %s" file);
        run_lex (read_file file) file
      end
  | _ -> Printf.eprintf "Unknown arguments\n"; print_usage (); exit 1
