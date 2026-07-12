(* ================================================================
   codegen.ml — C code generator for GCL-SH (Python-like)
   Auto-declares variables on first assignment.
   ================================================================ *)

open Ast

type t = {
  buf:    Buffer.t;
  mutable indent: int;
  file:   string;
  mutable vars:  (string, unit) Hashtbl.t;  (* declared C variables *)
}

let create file =
  { buf = Buffer.create 4096; indent = 0; file;
    vars = Hashtbl.create 64 }

let emit cg fmt =
  Printf.ksprintf (fun s -> Buffer.add_string cg.buf s) fmt

let emit_line cg fmt =
  for _ = 0 to cg.indent do Buffer.add_string cg.buf "  " done;
  Printf.ksprintf (fun s -> Buffer.add_string cg.buf s; Buffer.add_char cg.buf '\n') fmt

let rec typ_to_c t =
  let tr = match t with
    | TUnit -> "void" | TBool -> "int" | TInt -> "int" | TFloat -> "double"
    | TString -> "char*" | TBytes -> "char*"
    | TComplex -> "double complex"
    | TFun (a, r) ->
        let args = String.concat ", " (List.map (fun _ -> "int") a) in
        Printf.sprintf "%s (%s)" (typ_to_c r) args
    | TArray e -> Printf.sprintf "%s*" (typ_to_c e)
    | TStruct n -> "struct " ^ n | TEnum _ -> "int"
    | TNamed n -> n
    | TVar _ | TLink _ -> "int"
    | TSelf -> "void*"
    | TAny -> "void*"
    | TTuple _ -> "void*"
    | TDict _ -> "void*"
    | TSet _ -> "void*"
    | TClass (n, _) -> "struct " ^ n ^ "*"
    | TOptional t -> typ_to_c t
  in tr

(* Check if an expr_node is an assignment to a simple identifier *)
let is_var_assign : expr_node -> string option = function
  | EBinary ((Assign|AddAssign|SubAssign|MulAssign|DivAssign|ModAssign),
             { e_node = EIdent name; _ }, _) -> Some name
  | _ -> None

let rec gen_expr cg (expr : expr) = match expr.e_node with
  | ELiteral (LInt n) -> emit cg "%d" n
  | ELiteral (LFloat f) -> emit cg "%g" f
  | ELiteral (LString s) -> emit cg "\"%s\"" (String.escaped s)
  | ELiteral (LBool true) -> emit cg "1"
  | ELiteral (LBool false) -> emit cg "0"
  | ELiteral (LNull | LBytes _ | LComplex _) -> emit cg "0"
  | EIdent name -> emit cg "%s" name
  | EBinary (Add, l, r) -> binop cg l " + " r
  | EBinary (Sub, l, r) -> binop cg l " - " r
  | EBinary (Mul, l, r) -> binop cg l " * " r
  | EBinary (Div, l, r) -> binop cg l " / " r
  | EBinary (Mod, l, r) -> binop cg l " %% " r
  | EBinary (Pow, l, r) -> binop cg l " * " r  (* approximate: pow not in C *)
  | EBinary (FloorDiv, l, r) ->
      emit_raw cg "((int)("; gen_expr cg l; emit_raw cg " / "; gen_expr cg r; emit_raw cg "))"
  | EBinary (Eq, l, r)  -> binop cg l " == " r
  | EBinary (Ne, l, r)  -> binop cg l " != " r
  | EBinary (Lt, l, r)  -> binop cg l " < " r
  | EBinary (Le, l, r)  -> binop cg l " <= " r
  | EBinary (Gt, l, r)  -> binop cg l " > " r
  | EBinary (Ge, l, r)  -> binop cg l " >= " r
  | EBinary (And, l, r) -> binop cg l " && " r
  | EBinary (Or, l, r)  -> binop cg l " || " r
  | EBinary (BitOr, l, r) -> binop cg l " | " r
  | EBinary (BitAnd, l, r) -> binop cg l " & " r
  | EBinary (BitXor, l, r) -> binop cg l " ^ " r
  | EBinary (LShift, l, r) -> binop cg l " << " r
  | EBinary (RShift, l, r) -> binop cg l " >> " r
  | EBinary (Is, l, r)  -> binop cg l " == " r
  | EBinary (IsNot, l, r) -> binop cg l " != " r
  | EBinary (In, l, r) -> binop cg l " /* in */ " r
  | EBinary (NotIn, l, r) -> binop cg l " /* not in */ " r
  | EBinary (Assign, l, r) ->
      (match l.e_node with
       | EIdent name when not (Hashtbl.mem cg.vars name) ->
           let rt = match r.e_typ with Some t -> t | None -> TInt in
           let ct = typ_to_c rt in
           emit cg "%s %s" ct name;
           emit_raw cg " = ";
           gen_expr cg r;
           Hashtbl.add cg.vars name ()
       | _ ->
           gen_expr cg l; emit_raw cg " = "; gen_expr cg r)
  | EBinary (AddAssign, l, r) -> gen_expr cg l; emit_raw cg " += "; gen_expr cg r
  | EBinary (SubAssign, l, r) -> gen_expr cg l; emit_raw cg " -= "; gen_expr cg r
  | EBinary (MulAssign, l, r) -> gen_expr cg l; emit_raw cg " *= "; gen_expr cg r
  | EBinary (DivAssign, l, r) -> gen_expr cg l; emit_raw cg " /= "; gen_expr cg r
  | EBinary (ModAssign, l, r) -> gen_expr cg l; emit_raw cg " %%= "; gen_expr cg r
  | EBinary (PowAssign, l, r) -> gen_expr cg l; emit_raw cg " = pow("; gen_expr cg l; emit_raw cg ", "; gen_expr cg r; emit_raw cg ")"
  | EBinary (FloorDivAssign, l, r) ->
      gen_expr cg l; emit_raw cg " = (int)("; gen_expr cg l; emit_raw cg " / "; gen_expr cg r; emit_raw cg ")"
  | EBinary (BitOrAssign, l, r) -> gen_expr cg l; emit_raw cg " |= "; gen_expr cg r
  | EBinary (BitAndAssign, l, r) -> gen_expr cg l; emit_raw cg " &= "; gen_expr cg r
  | EBinary (BitXorAssign, l, r) -> gen_expr cg l; emit_raw cg " ^= "; gen_expr cg r
  | EBinary (LShiftAssign, l, r) -> gen_expr cg l; emit_raw cg " <<= "; gen_expr cg r
  | EBinary (RShiftAssign, l, r) -> gen_expr cg l; emit_raw cg " >>= "; gen_expr cg r
  | EBinary (Seq, l, r) -> gen_expr cg l; emit_raw cg "; "; gen_expr cg r
  | EUnary (Neg, o) -> emit_raw cg "-"; gen_expr cg o
  | EUnary (Not, o) -> emit_raw cg "!"; gen_expr cg o
  | EUnary (BitNot, o) -> emit_raw cg "~"; gen_expr cg o
  | EUnary (Plus, o) -> emit_raw cg "+"; gen_expr cg o
  | EUnary (PreInc, o) -> emit_raw cg "++"; gen_expr cg o
  | EUnary (PreDec, o) -> emit_raw cg "--"; gen_expr cg o
  | EUnary (PostInc, o) -> gen_expr cg o; emit_raw cg "++"
  | EUnary (PostDec, o) -> gen_expr cg o; emit_raw cg "--"
  | ETernary (c, t, e) ->
      emit_raw cg "("; gen_expr cg c;
      emit_raw cg " ? "; gen_expr cg t;
      emit_raw cg " : "; gen_expr cg e;
      emit_raw cg ")"
  | ECall (callee, args) ->
      gen_expr cg callee; emit_raw cg "(";
      let first = ref true in
      List.iter (fun a ->
        if !first then first := false else emit_raw cg ", ";
        gen_expr cg a) args;
      emit_raw cg ")"
  | EIndex (arr, idx) -> gen_expr cg arr; emit_raw cg "["; gen_expr cg idx; emit_raw cg "]"
  | ESlice (i1, i2, i3) ->
      emit_raw cg "/* slice: ["; 
      (match i1 with Some e -> gen_expr cg e | None -> emit_raw cg "");
      emit_raw cg ":";
      (match i2 with Some e -> gen_expr cg e | None -> emit_raw cg "");
      emit_raw cg ":";
      (match i3 with Some e -> gen_expr cg e | None -> emit_raw cg "");
      emit_raw cg "] */ 0"
  | EList items ->
      emit_raw cg "{";
      let first = ref true in
      List.iter (fun i ->
        if !first then first := false else emit_raw cg ", ";
        gen_expr cg i) items;
      emit_raw cg "}"
  | ETuple items ->
      emit_raw cg "{";
      let first = ref true in
      List.iter (fun i ->
        if !first then first := false else emit_raw cg ", ";
        gen_expr cg i) items;
      emit_raw cg "}"
  | EDict items ->
      emit_raw cg "/* dict */ {";
      let first = ref true in
      List.iter (fun (k, v) ->
        if !first then first := false else emit_raw cg ", ";
        gen_expr cg k; emit_raw cg ", "; gen_expr cg v) items;
      emit_raw cg "}"
  | ESet items ->
      emit_raw cg "{";
      let first = ref true in
      List.iter (fun i ->
        if !first then first := false else emit_raw cg ", ";
        gen_expr cg i) items;
      emit_raw cg "}"
  | EMember (obj, name) -> gen_expr cg obj; emit_raw cg "."; emit_raw cg name
  | EWalrus (n, e) ->
      let rt = match e.e_typ with Some t -> t | None -> TInt in
      let ct = typ_to_c rt in
      emit cg "(%s %s = " ct n;
      gen_expr cg e;
      emit cg ", %s" n;
      emit_raw cg ")"
  | ELambda (_ps, _body) ->
      emit_raw cg "/* lambda */ 0"
  | EListComp (e, _) ->
      emit_raw cg "/* list comp */ "; gen_expr cg e
  | ESetComp (e, _) ->
      emit_raw cg "/* set comp */ "; gen_expr cg e
  | EDictComp (k, v, _) ->
      emit_raw cg "/* dict comp */ "; gen_expr cg k; emit_raw cg " : "; gen_expr cg v
  | EGenerator (e, _) ->
      emit_raw cg "/* generator */ "; gen_expr cg e
and binop cg l o r = emit_raw cg "("; gen_expr cg l; emit_raw cg o; gen_expr cg r; emit_raw cg ")"
and emit_raw cg s = Buffer.add_string cg.buf s

let rec gen_stmt cg stmt = match stmt.s_node with
  | SBlock stmts ->
      emit_line cg "{";
      cg.indent <- cg.indent + 1;
      List.iter (gen_stmt cg) stmts;
      cg.indent <- cg.indent - 1;
      emit_line cg "}"
  | SExpr e ->
      (match is_var_assign e.e_node with
       | Some name when not (Hashtbl.mem cg.vars name) ->
           let rhs = match e.e_node with
             | EBinary (_, _, r) -> r | _ -> assert false in
           let rt = match rhs.e_typ with Some t -> t | None -> TInt in
           let ct = typ_to_c rt in
           emit cg "%s %s" ct name;
           emit_raw cg " = ";
           gen_expr cg rhs;
           Hashtbl.add cg.vars name ();
           emit_line cg ";"
       | _ ->
           gen_expr cg e; emit_line cg ";")
  | SIf (c, t, e) ->
      emit cg "if ("; gen_expr cg c; emit_line cg ")";
      gen_stmt cg t;
      (match e with Some e -> emit_line cg "else"; gen_stmt cg e | None -> ())
  | SWhile (c, b) ->
      emit cg "while ("; gen_expr cg c; emit_line cg ")";
      gen_stmt cg b
  | SFor (init, cond, step, body) ->
      emit cg "for ("; gen_stmt cg init;
      emit cg " "; gen_expr cg cond;
      emit cg "; "; gen_expr cg step;
      emit_line cg ")";
      gen_stmt cg body
  | SForIn (v, it, body) ->
      (* Simple: iterate as if it's an array with known length *)
      gen_expr cg it; emit_line cg ";";
      emit_line cg "{";
      cg.indent <- cg.indent + 1;
      emit_line cg "int _len_ = sizeof(/* TODO */)/sizeof(/* TODO */);";
      emit_line cg "for (int _i_ = 0; _i_ < _len_; _i_++) {";
      cg.indent <- cg.indent + 1;
      gen_expr cg v; emit_raw cg " = "; gen_expr cg it; emit_raw cg "[_i_]"; emit_line cg ";";
      gen_stmt cg body;
      cg.indent <- cg.indent - 1;
      emit_line cg "}";
      cg.indent <- cg.indent - 1;
      emit_line cg "}"
  | SReturn None -> emit_line cg "return;"
  | SReturn (Some e) -> emit cg "return "; gen_expr cg e; emit_line cg ";"
  | SBreak -> emit_line cg "break;"
  | SContinue -> emit_line cg "continue;"
  | SDelete e -> gen_expr cg e; emit_line cg " = 0; /* delete */"
  | SAssert (c, m) ->
      emit_raw cg "if (!("; gen_expr cg c; emit_line cg ")) {";
      cg.indent <- cg.indent + 1;
      emit_line cg "fprintf(stderr, \"Assertion failed\\n\");";
      (match m with Some e -> emit_raw cg "fprintf(stderr, \""; gen_expr cg e; emit_line cg "\");" | None -> ());
      emit_line cg "exit(1);";
      cg.indent <- cg.indent - 1;
      emit_line cg "}"
  | SRaise e ->
      emit_raw cg "/* raise */ ";
      (match e with Some e -> gen_expr cg e | None -> ());
      emit_line cg ";"
  | SGlobal ns ->
      List.iter (fun n -> emit_line cg "/* global %s */" n) ns
  | SNonlocal ns ->
      List.iter (fun n -> emit_line cg "/* nonlocal %s */" n) ns
  | STry (body, excepts, fin) ->
      emit_line cg "{ /* try */";
      cg.indent <- cg.indent + 1;
      gen_stmt cg body;
      List.iter (fun (eopt, _, b) ->
        emit_raw cg "/* except "; (match eopt with Some e -> gen_expr cg e | None -> emit_raw cg "all"); emit_line cg " */";
        gen_stmt cg b
      ) excepts;
      (match fin with Some f -> emit_line cg "/* finally */"; gen_stmt cg f | None -> ());
      cg.indent <- cg.indent - 1;
      emit_line cg "}"
  | SWith (_items, body) ->
      emit_line cg "{ /* with */";
      cg.indent <- cg.indent + 1;
      gen_stmt cg body;
      cg.indent <- cg.indent - 1;
      emit_line cg "}"
  | SYield None -> emit_line cg "/* yield */"
  | SYield (Some e) -> emit_raw cg "/* yield */ "; gen_expr cg e; emit_line cg ";"
  | SYieldFrom e -> emit_raw cg "/* yield from */ "; gen_expr cg e; emit_line cg ";"
  | SMatch (e, cases) ->
      emit_raw cg "/* match */ "; gen_expr cg e; emit_line cg ";";
      List.iter (fun c ->
        emit_raw cg "/* case */ "; gen_expr cg c.mc_pattern; emit_line cg ":";
        gen_stmt cg c.mc_body
      ) cases
  | SDecorated (decs, d) ->
      List.iter (fun dec -> emit_raw cg "/* @"; gen_expr cg dec; emit_line cg " */") decs;
      gen_decl cg d
  | SDecl d -> gen_decl cg d

and gen_decl cg d = match d.d_node with
  | DVarDecl (name, init) ->
      let typ = match d.d_typ with Some t -> typ_to_c t | None -> "int" in
      emit cg "%s %s" typ name;
      (match init with Some e -> emit cg " = "; gen_expr cg e | None -> emit cg " = 0");
      emit_line cg ";";
      Hashtbl.add cg.vars name ()
  | DVarDeclAnnot (name, _, init) ->
      let typ = match d.d_typ with Some t -> typ_to_c t | None -> "int" in
      emit cg "%s %s" typ name;
      (match init with Some e -> emit cg " = "; gen_expr cg e | None -> emit cg " = 0");
      emit_line cg ";";
      Hashtbl.add cg.vars name ()
  | DFuncDef (name, params, _ret_type, body) ->
      emit cg "int %s(" name;
      let first = ref true in
      List.iter (fun p ->
        if !first then first := false else emit cg ", ";
        let pt = match p.p_typ with Some t -> typ_to_c t | None -> "int" in
        emit cg "%s %s" pt p.p_name) params;
      emit_line cg ")";
      gen_stmt cg body
  | DExternFunc (name, params) ->
      emit cg "extern int %s(" name;
      let first = ref true in
      List.iter (fun p ->
        if !first then first := false else emit cg ", ";
        let pt = match p.p_typ with Some t -> typ_to_c t | None -> "int" in
        emit cg "%s" pt) params;
      emit_line cg ");"
  | DStructDecl (name, members) ->
      emit_line cg "struct %s {" name; cg.indent <- cg.indent + 1;
      Option.iter (List.iter (gen_decl cg)) members;
      cg.indent <- cg.indent - 1; emit_line cg "};"
  | DEnumDecl (name, items) ->
      emit cg "enum %s {" name;
      let first = ref true in
      List.iter (fun (n, v) ->
        if !first then first := false else emit cg ", ";
        emit cg "%s" n;
        match v with Some e -> emit cg " = "; gen_expr cg e | None -> ()) items;
      emit_line cg "};"
  | DClassDef (name, _bases, _body) ->
      emit_line cg "struct %s {" name; cg.indent <- cg.indent + 1;
      emit_line cg "/* class %s */" name;
      cg.indent <- cg.indent - 1; emit_line cg "};";
      Hashtbl.add cg.vars name ()
  | DImport (m, alias) ->
      let name = match alias with Some a -> a | None -> m in
      emit_line cg "/* import %s as %s */" m name
  | DFromImport (m, items) ->
      List.iter (fun (n, alias) ->
        let name = match alias with Some a -> a | None -> n in
        emit_line cg "/* from %s import %s as %s */" m n name
      ) items
  | DTypeAlias (name, _te) ->
      emit_line cg "/* type %s = ... */" name

let gen_program (prog : Ast.program) =
  let cg = create prog.file in
  emit_line cg "/* Generated by GCL self-hosting compiler */";
  emit_line cg "#include <stdio.h>";
  emit_line cg "#include <stdlib.h>";
  emit_line cg "#include <string.h>";
  emit_line cg "";
  List.iter (gen_decl cg) prog.decls;
  Buffer.contents cg.buf
