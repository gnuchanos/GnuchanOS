(* ================================================================
   typechecker.ml — Type checker for GCL-SH (Python-like)
   Supports implicit variable declaration on first assignment.
   Now with proper position-aware error reporting.
   ================================================================ *)

open Ast

(* ---- Type variable counter ---- *)
let var_counter = ref 0

let fresh_var () =
  let v = !var_counter in
  var_counter := v + 1;
  TVar v

(* ---- Substitution ---- *)
type subst = (int * typ) list

let empty_subst = []

let rec apply t s =
  match t with
  | TVar n ->
      (try apply (List.assoc n s) s
       with Not_found -> t)
  | TFun (args, ret) -> TFun (List.map (fun a -> apply a s) args, apply ret s)
  | TArray e -> TArray (apply e s)
  | TLink {contents = t'} -> apply t' s
  | _ -> t

let occurs n t s =
  let t' = apply t s in
  let rec loop = function
    | TVar m -> m = n
    | TFun (args, ret) -> List.exists loop args || loop ret
    | TArray e -> loop e
    | _ -> false
  in
  loop t'

let extend n t s = (n, t) :: s

(* ---- Type to string ---- *)
let rec typ_to_string t =
  match t with
  | TUnit -> "void" | TBool -> "bool" | TInt -> "int"
  | TFloat -> "float" | TString -> "string" | TBytes -> "bytes"
  | TComplex -> "complex"
  | TFun (a, r) -> Printf.sprintf "(%s) -> %s"
      (String.concat ", " (List.map typ_to_string a)) (typ_to_string r)
  | TArray e -> Printf.sprintf "[]%s" (typ_to_string e)
  | TStruct n -> "struct " ^ n | TEnum n -> "enum " ^ n
  | TNamed n -> n
  | TVar n -> Printf.sprintf "'t%d" n
  | TLink r -> typ_to_string !r
  | TSelf -> "Self"
  | TAny -> "Any"
  | TTuple ts -> Printf.sprintf "(%s)" (String.concat ", " (List.map typ_to_string ts))
  | TDict (k, v) -> Printf.sprintf "{%s: %s}" (typ_to_string k) (typ_to_string v)
  | TSet e -> Printf.sprintf "{%s}" (typ_to_string e)
  | TClass (n, _) -> n
  | TOptional t -> Printf.sprintf "Optional[%s]" (typ_to_string t)

let rec unify a b s pos =
  let a = apply a s in
  let b = apply b s in
  match a, b with
  | TVar n, TVar m when n = m -> s
  | TVar n, _ ->
      if occurs n b s then
        (Error.add_error (Error.infinite_type pos); s)
      else extend n b s
  | _, TVar n ->
      if occurs n a s then
        (Error.add_error (Error.infinite_type pos); s)
      else extend n a s
  | TUnit, TUnit | TBool, TBool | TInt, TInt | TFloat, TFloat
  | TString, TString | TBytes, TBytes | TComplex, TComplex -> s
  | TFun (a1, r1), TFun (a2, r2) ->
      let s' = ref s in
      (try List.iter2 (fun a b -> s' := unify a b !s' pos) a1 a2
       with Invalid_argument _ ->
         Error.add_error (Error.wrong_arg_count
           (List.length a1) (List.length a2) pos));
      unify r1 r2 !s' pos
  | TArray a1, TArray a2 -> unify a1 a2 s pos
  | TNamed n1, TNamed n2 when n1 = n2 -> s
  | TStruct n1, TStruct n2 when n1 = n2 -> s
  | TEnum n1, TEnum n2 when n1 = n2 -> s
  | TSelf, TSelf | TAny, TAny -> s
  | TOptional t1, TOptional t2 -> unify t1 t2 s pos
  | TTuple ts1, TTuple ts2 ->
      if List.length ts1 <> List.length ts2 then
        (Error.add_error (Error.type_mismatch (typ_to_string b) (typ_to_string a) pos); s)
      else
        List.fold_left2 (fun s' t1 t2 -> unify t1 t2 s' pos) s ts1 ts2
  | TDict (k1, v1), TDict (k2, v2) ->
      let s' = unify k1 k2 s pos in
      unify v1 v2 s' pos
  | TSet t1, TSet t2 -> unify t1 t2 s pos
  | TClass (n1, _), TClass (n2, _) when n1 = n2 -> s
  | _ ->
      Error.add_error (Error.type_mismatch (typ_to_string b) (typ_to_string a) pos);
      s

(* ---- Environment ---- *)
type binding = { typ: typ; mutable initialized: bool }
type env = { parent: env option; bindings: (string, binding) Hashtbl.t }

let make_env ?(parent=None) () =
  { parent; bindings = Hashtbl.create 32 }

let rec lookup env name =
  try Some (Hashtbl.find env.bindings name)
  with Not_found -> match env.parent with Some p -> lookup p name | None -> None

let add_binding env name typ =
  Hashtbl.replace env.bindings name { typ; initialized = false }

(* ---- Type checker state ---- *)
type t = {
  mutable env: env;
  file: string;
  mutable subst: subst;
  mutable current_return_type: typ;
  mutable in_loop: bool;
  mutable in_function: bool;
}

let create file =
  { env = make_env (); file; subst = empty_subst;
    current_return_type = TUnit; in_loop = false; in_function = false }

(* ---- Built-in environment ---- *)
let add_builtins chk =
  add_binding chk.env "print" (TFun ([TString], TUnit));
  add_binding chk.env "println" (TFun ([TString], TUnit));
  add_binding chk.env "len" (TFun ([TArray (TVar 0)], TInt));
  add_binding chk.env "exit" (TFun ([TInt], TUnit));
  add_binding chk.env "input" (TFun ([], TString));
  add_binding chk.env "int" (TFun ([TVar 0], TInt));
  add_binding chk.env "str" (TFun ([TVar 0], TString));
  add_binding chk.env "float" (TFun ([TVar 0], TFloat));
  add_binding chk.env "bool" (TFun ([TVar 0], TBool));
  add_binding chk.env "range" (TFun ([TInt; TInt], TArray TInt));
  add_binding chk.env "type" (TFun ([TVar 0], TString))

(* ---- Check whether an expr is a simple assignment to an identifier ---- *)
let is_simple_assign : expr_node -> (string * expr) option = function
  | EBinary (Assign, { e_node = EIdent name; _ }, rhs) -> Some (name, rhs)
  | EBinary ((AddAssign|SubAssign|MulAssign|DivAssign|ModAssign),
             { e_node = EIdent name; _ }, rhs) -> Some (name, rhs)
  | _ -> None

(* ---- Auto-declare: if identifier not found in env, add it with fresh type ---- *)
let auto_declare chk name =
  if lookup chk.env name = None then
    add_binding chk.env name (fresh_var ())

let rec check_expr chk expr =
  let pos = expr.e_pos in
  let t =
    match expr.e_node with
    | ELiteral (LInt _) -> TInt
    | ELiteral (LFloat _) -> TFloat
    | ELiteral (LString _) -> TString
    | ELiteral (LBool _) -> TBool
    | ELiteral (LNull) -> TInt
    | ELiteral (LBytes _) -> TBytes
    | ELiteral (LComplex _) -> TComplex

    | EIdent name ->
        (match lookup chk.env name with
         | Some b -> apply b.typ chk.subst
         | None ->
             Error.add_error (Error.undefined_name name pos);
             TInt)

    | EBinary (b, l, r) ->
        let lt = check_expr chk l in
        let rt = check_expr chk r in
        (match b with
         | Add ->
             let is_str t = match t with TString -> true | _ -> false in
             if is_str (apply lt chk.subst) || is_str (apply rt chk.subst) then
               TString
             else begin
               chk.subst <- unify lt TInt chk.subst pos;
               chk.subst <- unify rt TInt chk.subst pos;
               TInt
             end
         | Sub | Mul | Div | Mod ->
             chk.subst <- unify lt TInt chk.subst pos;
             chk.subst <- unify rt TInt chk.subst pos;
             TInt
         | Pow | FloorDiv ->
             chk.subst <- unify lt TInt chk.subst pos;
             chk.subst <- unify rt TInt chk.subst pos;
             TInt
         | Eq | Ne | Lt | Le | Gt | Ge ->
             chk.subst <- unify lt rt chk.subst pos;
             TBool
         | And | Or ->
             chk.subst <- unify lt TBool chk.subst pos;
             chk.subst <- unify rt TBool chk.subst pos;
             TBool
         | BitOr | BitAnd | BitXor | LShift | RShift ->
             chk.subst <- unify lt TInt chk.subst pos;
             chk.subst <- unify rt TInt chk.subst pos;
             TInt
         | Is | IsNot ->
             chk.subst <- unify lt rt chk.subst pos;
             TBool
         | In | NotIn ->
             chk.subst <- unify lt rt chk.subst pos;
             TBool
         | Assign | AddAssign | SubAssign
         | MulAssign | DivAssign | ModAssign
         | PowAssign | FloorDivAssign
         | BitOrAssign | BitAndAssign | BitXorAssign
         | LShiftAssign | RShiftAssign ->
             (match l.e_node with
              | ELiteral _ ->
                  Error.add_error (Error.cannot_assign_to_literal pos)
              | _ -> ());
             chk.subst <- unify lt rt chk.subst pos;
             lt
         | Seq -> rt)

    | EUnary (op, e) ->
        let et = check_expr chk e in
        (match op with
         | Neg | PreInc | PreDec | PostInc | PostDec ->
             chk.subst <- unify et TInt chk.subst pos;
             et
         | Not ->
             chk.subst <- unify et TBool chk.subst pos;
             TBool
         | BitNot ->
             chk.subst <- unify et TInt chk.subst pos;
             TInt
         | Plus ->
             chk.subst <- unify et TInt chk.subst pos;
             TInt)

    | ETernary (c, t, e) ->
        let ct = check_expr chk c in
        chk.subst <- unify ct TBool chk.subst pos;
        let tt = check_expr chk t in
        let et = check_expr chk e in
        chk.subst <- unify tt et chk.subst pos;
        tt

    | ECall (callee, args) ->
        let ct = check_expr chk callee in
        let at = List.map (check_expr chk) args in
        let rv = fresh_var () in
        chk.subst <- unify ct (TFun (at, rv)) chk.subst pos;
        rv

    | EIndex (arr, idx) ->
        let at = check_expr chk arr in
        let it = check_expr chk idx in
        chk.subst <- unify it TInt chk.subst pos;
        let et = fresh_var () in
        chk.subst <- unify at (TArray et) chk.subst pos;
        et

    | ESlice (i1, i2, i3) ->
        Option.iter (fun i -> ignore (check_expr chk i)) i1;
        Option.iter (fun i -> ignore (check_expr chk i)) i2;
        Option.iter (fun i -> ignore (check_expr chk i)) i3;
        TArray (fresh_var ())

    | EList items ->
        (match items with
         | [] -> TArray (fresh_var ())
         | f :: r ->
             let ft = check_expr chk f in
             List.iter (fun i -> chk.subst <- unify (check_expr chk i) ft chk.subst pos) r;
             TArray ft)

    | ETuple items ->
        let ts = List.map (check_expr chk) items in
        TTuple ts

    | EDict items ->
        (match items with
         | [] -> TDict (fresh_var (), fresh_var ())
         | (kf, vf) :: r ->
             let kt = check_expr chk kf in
             let vt = check_expr chk vf in
             List.iter (fun (k, v) ->
               chk.subst <- unify (check_expr chk k) kt chk.subst pos;
               chk.subst <- unify (check_expr chk v) vt chk.subst pos
             ) r;
             TDict (kt, vt))

    | ESet items ->
        (match items with
         | [] -> TSet (fresh_var ())
         | f :: r ->
             let ft = check_expr chk f in
             List.iter (fun i -> chk.subst <- unify (check_expr chk i) ft chk.subst pos) r;
             TSet ft)

    | EMember (obj, _name) ->
        ignore (check_expr chk obj);
        TInt

    | EWalrus (name, e) ->
        auto_declare chk name;
        let et = check_expr chk e in
        (match lookup chk.env name with
         | Some b -> chk.subst <- unify b.typ et chk.subst pos; et
         | None -> et)

    | ELambda (ps, body) ->
        let param_types = List.map (fun _ -> fresh_var ()) ps in
        let ret_type = fresh_var () in
        let ft = TFun (param_types, ret_type) in
        (* Type-check body in new scope *)
        let old_env = chk.env in
        let func_env = make_env ~parent:(Some old_env) () in
        chk.env <- func_env;
        List.iter2 (fun p pt -> add_binding chk.env p.p_name pt) ps param_types;
        let saved_ret = chk.current_return_type in
        let saved_fn = chk.in_function in
        chk.current_return_type <- ret_type;
        chk.in_function <- true;
        ignore (check_expr chk body);
        chk.in_function <- saved_fn;
        chk.current_return_type <- saved_ret;
        chk.env <- old_env;
        ft

    | EListComp (e, clauses) ->
        let _ = check_comp_clauses chk clauses in
        check_expr chk e

    | ESetComp (e, clauses) ->
        let _ = check_comp_clauses chk clauses in
        check_expr chk e

    | EDictComp (k, v, clauses) ->
        let _ = check_comp_clauses chk clauses in
        ignore (check_expr chk k);
        check_expr chk v

    | EGenerator (e, clauses) ->
        let _ = check_comp_clauses chk clauses in
        check_expr chk e
  in
  expr.e_typ <- Some t;
  t

and check_comp_clauses chk clauses =
  List.iter (fun clause ->
    let _it_typ = check_expr chk clause.cc_iterator in
    ignore (check_expr chk clause.cc_binding);
    List.iter (fun c -> ignore (check_expr chk c)) clause.cc_conditions
  ) clauses

let rec check_stmt chk stmt =
  match stmt.s_node with
  | SBlock stmts ->
      let old_env = chk.env in
      let new_env = make_env ~parent:(Some old_env) () in
      chk.env <- new_env;
      List.iter (check_stmt chk) stmts;
      chk.env <- old_env

  | SExpr e ->
      (match is_simple_assign e.e_node with
       | Some (name, _rhs) ->
           auto_declare chk name;
           ignore (check_expr chk e)
       | None ->
           ignore (check_expr chk e))

  | SIf (c, t, e) ->
      let ct = check_expr chk c in
      chk.subst <- unify ct TBool chk.subst c.e_pos;
      check_stmt chk t;
      Option.iter (check_stmt chk) e

  | SWhile (c, b) ->
      let ct = check_expr chk c in
      chk.subst <- unify ct TBool chk.subst c.e_pos;
      let saved_loop = chk.in_loop in
      chk.in_loop <- true;
      check_stmt chk b;
      chk.in_loop <- saved_loop

  | SFor (init, cond, step, body) ->
      let old_env = chk.env in
      let new_env = make_env ~parent:(Some old_env) () in
      chk.env <- new_env;
      check_stmt chk init;
      let ct = check_expr chk cond in
      chk.subst <- unify ct TBool chk.subst cond.e_pos;
      ignore (check_expr chk step);
      let saved_loop = chk.in_loop in
      chk.in_loop <- true;
      check_stmt chk body;
      chk.in_loop <- saved_loop;
      chk.env <- old_env

  | SForIn (v, it, body) ->
      let old_env = chk.env in
      let new_env = make_env ~parent:(Some old_env) () in
      chk.env <- new_env;
      let _it_typ = check_expr chk it in
      ignore (check_expr chk v);
      let saved_loop = chk.in_loop in
      chk.in_loop <- true;
      check_stmt chk body;
      chk.in_loop <- saved_loop;
      chk.env <- old_env

  | SReturn None ->
      if not chk.in_function then
        Error.add_error (Error.outside_function "return" stmt.s_pos);
      chk.subst <- unify chk.current_return_type TUnit chk.subst stmt.s_pos

  | SReturn (Some e) ->
      if not chk.in_function then
        Error.add_error (Error.outside_function "return" stmt.s_pos);
      let et = check_expr chk e in
      chk.subst <- unify chk.current_return_type et chk.subst e.e_pos

  | SBreak ->
      if not chk.in_loop then
        Error.add_error (Error.outside_loop "break" stmt.s_pos)

  | SContinue ->
      if not chk.in_loop then
        Error.add_error (Error.outside_loop "continue" stmt.s_pos)

  | SDelete e ->
      ignore (check_expr chk e)

  | SAssert (c, m) ->
      let ct = check_expr chk c in
      chk.subst <- unify ct TBool chk.subst c.e_pos;
      Option.iter (fun e -> ignore (check_expr chk e)) m

  | SRaise e ->
      Option.iter (fun e -> ignore (check_expr chk e)) e

  | SGlobal _ -> ()
  | SNonlocal _ -> ()

  | STry (body, excepts, fin) ->
      check_stmt chk body;
      List.iter (fun (eopt, _name, b) ->
        Option.iter (fun e -> ignore (check_expr chk e)) eopt;
        check_stmt chk b
      ) excepts;
      Option.iter (check_stmt chk) fin

  | SWith (items, body) ->
      List.iter (fun e -> ignore (check_expr chk e)) items;
      check_stmt chk body

  | SYield None -> ()
  | SYield (Some e) -> ignore (check_expr chk e)
  | SYieldFrom e -> ignore (check_expr chk e)

  | SMatch (e, cases) ->
      ignore (check_expr chk e);
      List.iter (fun c ->
        ignore (check_expr chk c.mc_pattern);
        Option.iter (fun g -> ignore (check_expr chk g)) c.mc_guard;
        check_stmt chk c.mc_body
      ) cases

  | SDecorated (decs, d) ->
      List.iter (fun e -> ignore (check_expr chk e)) decs;
      ignore (check_decl chk d)

  | SDecl d -> ignore (check_decl chk d)

and check_decl chk d =
  let pos = d.d_pos in
  let t =
    match d.d_node with
    | DVarDecl (name, init) ->
        (match lookup chk.env name with
         | Some _ ->
             Error.add_error (Error.shadowed_variable name pos)
         | None -> ());
        (match init with
         | Some e ->
             let it = check_expr chk e in
             add_binding chk.env name it;
             it
         | None ->
             let tv = fresh_var () in
             add_binding chk.env name tv;
             tv)

    | DVarDeclAnnot (name, _annot, init) ->
        (match lookup chk.env name with
         | Some _ ->
             Error.add_error (Error.shadowed_variable name pos)
         | None -> ());
        (match init with
         | Some e ->
             let it = check_expr chk e in
             add_binding chk.env name it;
             it
         | None ->
             let tv = fresh_var () in
             add_binding chk.env name tv;
             tv)

    | DFuncDef (name, params, _ret_type_opt, body) ->
        (match lookup chk.env name with
         | Some _ ->
             Error.add_error (Error.redeclaration name pos pos)
         | None -> ());
        let old_env = chk.env in
        let func_env = make_env ~parent:(Some old_env) () in
        chk.env <- func_env;

        let body_ret = fresh_var () in
        let param_types = List.map (fun p ->
          match p.p_typ with
          | Some t -> t
          | None -> fresh_var ()
        ) params in
        let ft = TFun (param_types, body_ret) in
        add_binding func_env name ft;

        List.iter2 (fun p pt ->
          (match lookup chk.env p.p_name with
           | Some _ ->
               Error.add_error (Error.duplicate_parameter p.p_name p.p_pos)
           | None -> ());
          add_binding chk.env p.p_name pt;
        ) params param_types;

        let saved_ret = chk.current_return_type in
        let saved_fn = chk.in_function in
        chk.current_return_type <- body_ret;
        chk.in_function <- true;

        check_stmt chk body;

        chk.in_function <- saved_fn;
        chk.current_return_type <- saved_ret;
        chk.env <- old_env;
        ft

    | DExternFunc (name, params) ->
        let param_types = List.map (fun p ->
          match p.p_typ with Some t -> t | None -> TInt
        ) params in
        let ft = TFun (param_types, TInt) in
        add_binding chk.env name ft;
        ft

    | DStructDecl (name, _members) ->
        let st = TStruct name in
        add_binding chk.env name st;
        st

    | DEnumDecl (name, items) ->
        let et = TEnum name in
        add_binding chk.env name et;
        List.iter (fun (en, _) -> add_binding chk.env en TInt) items;
        et

    | DClassDef (name, _bases, _body) ->
        let ct = TClass (name, None) in
        add_binding chk.env name ct;
        ct

    | DImport (m, _alias) ->
        add_binding chk.env m TAny;
        TAny

    | DFromImport (m, items) ->
        add_binding chk.env m TAny;
        List.iter (fun (n, _) -> add_binding chk.env n TAny) items;
        TAny

    | DTypeAlias (name, _te) ->
        add_binding chk.env name TAny;
        TAny
  in
  d.d_typ <- Some t;
  t

let check_program (prog : Ast.program) =
  var_counter := 0;
  Error.log_typechk (Printf.sprintf "checking program: %s" prog.file);
  let chk = create prog.file in
  add_builtins chk;
  List.iter (fun d ->
    ignore (check_decl chk d)
  ) prog.decls;
  Error.log_typechk (Printf.sprintf "type check complete: %d errors"
    !Error.error_count);
  chk
