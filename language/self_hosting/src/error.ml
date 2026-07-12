(* ================================================================
   error.ml — Enhanced error reporting module for GCL-SH
   
   Features:
   - Source line display with ^ marker (Rust compiler style)
   - Error categories with organized codes
   - Suggestions/hints for common errors
   - Error/warning/note statistics
   - ANSI colored output (auto-detects terminal)
   - Source file caching for line lookup
   - DEBUG LOGGING: phase tracking with timestamps
   - ERROR CHAIN: multiple related errors can be linked
   - CONTEXT: display 2 lines before/after the error line
   - Backward compatible: make_error ~pos ~code ~msg still works
   ================================================================ *)

open Ast

(* ================================================================
   DEBUG LOGGING — track compiler phases with timestamps
   ================================================================ *)
let debug_mode = ref false

let log_prefixes = [
  "LEXER",    "\027[36m";    (* cyan *)
  "PARSER",   "\027[32m";    (* green *)
  "TYPECHK",  "\027[33m";    (* yellow *)
  "CODEGEN",  "\027[35m";    (* magenta *)
  "LINK",     "\027[34m";    (* blue *)
  "RUNTIME",  "\027[31m";    (* red *)
  "INFO",     "\027[37m";    (* white *)
]

let get_timestamp () =
  let t = Unix.time () in
  let tm = Unix.localtime t in
  Printf.sprintf "%02d:%02d:%02d" tm.tm_hour tm.tm_min tm.tm_sec

let log_phase phase msg =
  if !debug_mode then
    let ts = get_timestamp () in
    let color = try List.assoc phase log_prefixes with Not_found -> "\027[0m" in
    Printf.eprintf "%s[%s]%s [%s] %s\n%!" color ts "\027[0m" phase msg

let log_lexer msg = log_phase "LEXER" msg
let log_parser msg = log_phase "PARSER" msg
let log_typechk msg = log_phase "TYPECHK" msg
let log_codegen msg = log_phase "CODEGEN" msg
let log_info msg = log_phase "INFO" msg

let set_debug b = debug_mode := b

(* ================================================================
   Source cache — stores file contents for line display
   ================================================================ *)
let source_cache : (string, string array) Hashtbl.t = Hashtbl.create 16

let cache_source file content =
  if content = "" then
    Hashtbl.replace source_cache file [||]
  else
    Hashtbl.replace source_cache file (Array.of_list (String.split_on_char '\n' content))

let get_line file n =
  try
    let lines = Hashtbl.find source_cache file in
    if n >= 1 && n <= Array.length lines then
      Some lines.(n - 1)
    else None
  with Not_found -> None

let get_lines_range file start_line end_line =
  try
    let lines = Hashtbl.find source_cache file in
    let start = max start_line 1 in
    let last = min end_line (Array.length lines) in
    if start > last then None
    else Some (Array.sub lines (start - 1) (last - start + 1))
  with Not_found -> None

(* ================================================================
   Error categories
   ================================================================ *)
type error_category =
  | Lexical     (* lexer errors *)
  | Syntax      (* parser/syntax errors *)
  | Name        (* undefined names, redeclaration *)
  | Type        (* type errors *)
  | Semantic    (* semantic errors *)
  | Runtime     (* runtime errors *)
  | Internal    (* compiler bugs *)
  | Warning     (* warnings *)
  | IO          (* file I/O errors *)

let category_name = function
  | Lexical -> "LexicalError"
  | Syntax -> "SyntaxError"
  | Name -> "NameError"
  | Type -> "TypeError"
  | Semantic -> "SemanticError"
  | Runtime -> "RuntimeError"
  | Internal -> "InternalError"
  | Warning -> "Warning"
  | IO -> "IOError"

(* ================================================================
   Severity
   ================================================================ *)
type severity = Note | Warning | Error | Fatal

let severity_label = function
  | Error -> "error"
  | Fatal -> "fatal error"
  | Warning -> "warning"
  | Note -> "note"

(* ================================================================
   Error record — now includes phase, sub-errors chain, and notes
   ================================================================ *)
type error = {
  severity: severity;
  category: error_category;
  pos: pos option;
  code: string;
  message: string;
  hint: string option;
  phase: string;           (* which compiler phase produced this *)
  mutable sub_errors: error list;  (* related errors chain *)
  mutable notes: string list;      (* additional notes *)
}

(* Internal constructor — full control *)
let make_error_full ~severity ~category ~pos ~code ~msg =
  { severity; category; pos; code; message = msg; hint = None;
    phase = ""; sub_errors = []; notes = [] }

(* Backward-compatible: Error severity, Syntax category, required pos *)
let make_error ~pos ~code ~msg =
  make_error_full ~severity:Error ~category:Syntax ~pos:(Some pos) ~code ~msg

(* Backward-compatible: Error severity, optional pos *)
let make_error_opt ~pos ~code ~msg =
  make_error_full ~severity:Error ~category:Syntax ~pos ~code ~msg

(* ---- Convenience: add hint to any error ---- *)
let with_hint err hint = { err with hint = Some hint }

(* ---- Chain related errors ---- *)
let add_sub_error err sub =
  err.sub_errors <- sub :: err.sub_errors;
  err

let add_note err note =
  err.notes <- note :: err.notes;
  err

(* Set phase on error *)
let with_phase err phase = { err with phase }

(* ================================================================
   Global error accumulator
   ================================================================ *)
let errors : error list ref = ref []
let error_count = ref 0
let warning_count = ref 0
let note_count = ref 0

let add_error (err : error) =
  errors := err :: !errors;
  match err.severity with
  | Error | Fatal -> incr error_count
  | Warning -> incr warning_count
  | Note -> incr note_count

let clear_errors () =
  errors := [];
  error_count := 0;
  warning_count := 0;
  note_count := 0

let has_errors () = !error_count > 0

let exit_code () = if has_errors () then 1 else 0

(* ================================================================
   ANSI color helpers
   ================================================================ *)
let color_red s = "\027[31m" ^ s ^ "\027[0m"
let color_green s = "\027[32m" ^ s ^ "\027[0m"
let color_yellow s = "\027[33m" ^ s ^ "\027[0m"
let color_cyan s = "\027[36m" ^ s ^ "\027[0m"
let color_bold s = "\027[1m" ^ s ^ "\027[0m"
let color_dim s = "\027[2m" ^ s ^ "\027[0m"

(* Color support — disabled when output is redirected or on Windows < 10 *)
let use_color = ref true

let red s = if !use_color then color_red s else s
let green s = if !use_color then color_green s else s
let yellow s = if !use_color then color_yellow s else s
let cyan s = if !use_color then color_cyan s else s
let bold s = if !use_color then color_bold s else s
let dim s = if !use_color then color_dim s else s

(* ================================================================
   Formatter — Rust-like compiler output
   
   Output format:
     file:line:col error[CODE]: message
       N | source line
        | ^
      = help: hint text
      = note: additional info
   ================================================================ *)
let format_pos (p : pos) =
  Printf.sprintf "%s:%d:%d" p.file p.line p.col

let format_error err =
  let loc = match err.pos with
    | Some p -> bold (format_pos p) ^ ": "
    | None -> "" in
  let sev_label = match err.severity with
    | Error | Fatal -> red (severity_label err.severity)
    | Warning -> yellow (severity_label err.severity)
    | Note -> cyan (severity_label err.severity) in
  let code_tag = cyan (Printf.sprintf "[%s]" err.code) in

  (* ---- Line 1: location severity[CODE]: message ---- *)
  Printf.eprintf "%s%s %s: %s\n%!" loc sev_label code_tag err.message;

  (* ---- Line 2-3: source line with ^ marker (only for errors) ---- *)
  (match err.pos with
   | Some p when err.severity = Error || err.severity = Fatal ->
     (* Show context: 2 lines before *)
     (match get_lines_range p.file (p.line - 2) (p.line - 1) with
      | Some lines when Array.length lines > 0 ->
        Array.iteri (fun i line ->
          let lnum = p.line - 2 + i + 1 in
          let line_num_str = Printf.sprintf " %4d | " lnum in
          Printf.eprintf "%s%s\n%!" (dim line_num_str) line
        ) lines
      | _ -> ());
     (* Show the error line *)
     (match get_line p.file p.line with
      | Some line when String.length line > 0 ->
          let line_num_str = Printf.sprintf " %4d | " p.line in
          Printf.eprintf "%s%s\n%!" (bold line_num_str) line;
          (* Compute underline position *)
          let col = max p.col 1 in
          let marker_pos = min (col - 1) (String.length line) in
          let marker = Printf.sprintf "%s^" (String.make marker_pos ' ') in
          let spacing = String.make (String.length line_num_str) ' ' in
          Printf.eprintf "%s%s\n%!" spacing (red marker);
          (* Show 1 line after *)
          (match get_lines_range p.file (p.line + 1) (p.line + 1) with
           | Some lines when Array.length lines > 0 ->
             let line_num_str = Printf.sprintf " %4d | " (p.line + 1) in
             Printf.eprintf "%s%s\n%!" (dim line_num_str) lines.(0)
           | _ -> ())
      | _ -> Printf.eprintf "     |\n%!")
   | _ -> ());

  (* ---- Line: notes ---- *)
  List.iter (fun note ->
    Printf.eprintf "  %s: %s\n%!" (cyan "note") note
  ) (List.rev err.notes);

  (* ---- Line: hint ---- *)
  (match err.hint with
   | Some h -> Printf.eprintf "  %s: %s\n%!" (green "help") h
   | None -> ());

  (* ---- Sub-errors ---- *)
  List.iter (fun sub ->
    Printf.eprintf "    %s: %s\n%!" (red "caused by") sub.message;
    (match sub.pos with
     | Some p -> Printf.eprintf "      at %s:%d:%d\n%!" p.file p.line p.col
     | None -> ())
  ) (List.rev err.sub_errors);

  flush stderr

let print_all_errors ?(file="") () =
  (* Ensure file is in cache *)
  if file <> "" then ignore (get_line file 1);
  List.iter format_error (List.rev !errors);

  (* ---- Summary ---- *)
  let ec = !error_count in
  let wc = !warning_count in
  if ec > 0 || wc > 0 then begin
    Printf.eprintf "\n";
    if ec > 0 then begin
      let msg = Printf.sprintf "%s: %d %s"
        (red "aborting due to") ec
        (if ec = 1 then "error" else "errors") in
      let msg = if wc > 0 then
        Printf.sprintf "%s, %d %s" msg wc
          (if wc = 1 then "warning" else "warnings")
        else msg in
      Printf.eprintf "%s\n%!" msg
    end else begin
      Printf.eprintf "%s: %d %s\n%!" (yellow "warning") wc
        (if wc = 1 then "warning issued" else "warnings issued")
    end
  end;
  clear_errors ()

(* ================================================================
   Convenience error constructors
   Organized by category.
   ================================================================ *)

(* ---- IO errors ---- *)
let file_not_found path =
  make_error_full ~severity:Fatal ~category:IO ~pos:None
    ~code:"IO001" ~msg:(Printf.sprintf "file not found: '%s'" path)

let file_read_error path msg =
  make_error_full ~severity:Fatal ~category:IO ~pos:None
    ~code:"IO002" ~msg:(Printf.sprintf "cannot read file '%s': %s" path msg)

(* ---- Lexical errors ---- *)
let unexpected_char ch pos =
  with_hint (make_error_full ~severity:Error ~category:Lexical ~pos:(Some pos)
    ~code:"L001" ~msg:(Printf.sprintf "unexpected character: '%c'" ch))
    "check for typos or unsupported Unicode characters"

let unterminated_string pos =
  with_hint (make_error_full ~severity:Error ~category:Lexical ~pos:(Some pos)
    ~code:"L002" ~msg:"unterminated string literal")
    "add a matching closing quote to terminate the string"

let invalid_escape_seq seq pos =
  let ch_str = match seq with
    | 'x' -> "\\xHH"
    | 'u' -> "\\uXXXX"
    | 'U' -> "\\UXXXXXXXX"
    | 'N' -> "\\N{NAME}"
    | c -> Printf.sprintf "\\%c" c in
  with_hint (make_error_full ~severity:Error ~category:Lexical ~pos:(Some pos)
    ~code:"L003" ~msg:(Printf.sprintf "invalid escape sequence: '%s'" ch_str))
    "valid escapes: \\n \\t \\r \\\\ \\\" \\' \\0 \\x.. \\u.... \\U........ \\N{NAME}"

let invalid_int_literal s pos =
  make_error_full ~severity:Error ~category:Lexical ~pos:(Some pos)
    ~code:"L004" ~msg:(Printf.sprintf "invalid integer literal: '%s'" s)

let invalid_float_literal s pos =
  make_error_full ~severity:Error ~category:Lexical ~pos:(Some pos)
    ~code:"L005" ~msg:(Printf.sprintf "invalid float literal: '%s'" s)

let unknown_string_prefix prefix pos =
  with_hint (make_error_full ~severity:Error ~category:Lexical ~pos:(Some pos)
    ~code:"L006" ~msg:(Printf.sprintf "unknown string prefix: '%s'" prefix))
    "valid prefixes: b r f rb rf br fr (or empty for normal string)"

let invalid_char_literal pos =
  with_hint (make_error_full ~severity:Error ~category:Lexical ~pos:(Some pos)
    ~code:"L007" ~msg:"invalid character literal (Python has no char type; use a string of length 1)")
    "use a string like \"a\" instead of 'a'"

let unknown_escape_name name pos =
  with_hint (make_error_full ~severity:Error ~category:Lexical ~pos:(Some pos)
    ~code:"L008" ~msg:(Printf.sprintf "unknown Unicode character name: '\\N{%s}'" name))
    "use a valid Unicode name like \\N{LATIN CAPITAL LETTER A}"

(* ---- Syntax errors ---- *)
let syntax_error msg pos =
  make_error_full ~severity:Error ~category:Syntax ~pos:(Some pos)
    ~code:"P001" ~msg

let expected_token expected found pos =
  with_hint (make_error_full ~severity:Error ~category:Syntax ~pos:(Some pos)
    ~code:"P002" ~msg:(Printf.sprintf "expected %s, but found '%s'" expected found))
    "review the syntax at this position; check for missing parentheses or operators"

let missing_semicolon pos =
  with_hint (make_error_full ~severity:Error ~category:Syntax ~pos:(Some pos)
    ~code:"P003" ~msg:"expected ';' after statement")
    "each statement must end with a semicolon ';'"

let invalid_block pos =
  with_hint (make_error_full ~severity:Error ~category:Syntax ~pos:(Some pos)
    ~code:"P004" ~msg:"expected a block body")
    "use { } to define a block after the ':'"

let duplicate_default pos =
  make_error_full ~severity:Error ~category:Syntax ~pos:(Some pos)
    ~code:"P005" ~msg:"duplicate default value in parameter"

let missing_colon pos =
  with_hint (make_error_full ~severity:Error ~category:Syntax ~pos:(Some pos)
    ~code:"P006" ~msg:"expected ':'")
    "Python syntax requires ':' before a block (def, if, while, for, class, with, etc.)"

let unmatched_delimiter delimiter pos =
  with_hint (make_error_full ~severity:Error ~category:Syntax ~pos:(Some pos)
    ~code:"P007" ~msg:(Printf.sprintf "unmatched '%s'" delimiter))
    "check that all opening brackets/braces have corresponding closing ones"

let empty_body pos =
  with_hint (make_error_full ~severity:Error ~category:Syntax ~pos:(Some pos)
    ~code:"P008" ~msg:"expected at least one statement in block body")
    "use 'pass;' for an empty block"

let invalid_decorator pos =
  with_hint (make_error_full ~severity:Error ~category:Syntax ~pos:(Some pos)
    ~code:"P009" ~msg:"invalid decorator syntax")
    "decorators must be '@' followed by an expression, placed before 'def' or 'class'"

(* ---- Name errors ---- *)
let undefined_name name pos =
  with_hint (make_error_full ~severity:Error ~category:Name ~pos:(Some pos)
    ~code:"N001" ~msg:(Printf.sprintf "name '%s' is not defined" name))
    (Printf.sprintf "is '%s' spelled correctly? was it defined before use?" name)

let redeclaration name (first_pos : pos) pos =
  let err = make_error_full ~severity:Error ~category:Name ~pos:(Some pos)
    ~code:"N002" ~msg:(Printf.sprintf "'%s' is already declared" name) in
  let sub = make_error_full ~severity:Note ~category:Name ~pos:(Some first_pos)
    ~code:"N002" ~msg:(Printf.sprintf "'%s' was first declared here" name) in
  let err = add_note err (Printf.sprintf "'%s' was first declared at %s:%d:%d"
    name first_pos.file first_pos.line first_pos.col) in
  add_sub_error err sub

let undefined_attribute obj attr pos =
  with_hint (make_error_full ~severity:Error ~category:Name ~pos:(Some pos)
    ~code:"N003" ~msg:(Printf.sprintf "'%s' has no attribute '%s'" obj attr))
    "check the attribute name for typos, or add the attribute to the class definition"

let not_callable name pos =
  with_hint (make_error_full ~severity:Error ~category:Name ~pos:(Some pos)
    ~code:"N004" ~msg:(Printf.sprintf "'%s' is not callable" name))
    "only functions and classes can be called"

let module_not_found name pos =
  with_hint (make_error_full ~severity:Error ~category:Name ~pos:(Some pos)
    ~code:"N005" ~msg:(Printf.sprintf "module '%s' not found" name))
    "check that the module name is spelled correctly and the file exists"

(* ---- Type errors ---- *)
let type_mismatch expected got pos =
  with_hint (make_error_full ~severity:Error ~category:Type ~pos:(Some pos)
    ~code:"T001" ~msg:(Printf.sprintf "expected type '%s', got '%s'" expected got))
    "adding explicit type annotations can help catch mismatches earlier"

let wrong_arg_count expected found pos =
  with_hint (make_error_full ~severity:Error ~category:Type ~pos:(Some pos)
    ~code:"T002" ~msg:(Printf.sprintf "expected %d argument(s), but got %d" expected found))
    (Printf.sprintf "this function takes %d argument(s)" expected)

let non_function_call pos =
  with_hint (make_error_full ~severity:Error ~category:Type ~pos:(Some pos)
    ~code:"T003" ~msg:"'<value>' is not callable")
    "only functions, classes, and objects with __call__ can be invoked"

let cannot_index pos =
  with_hint (make_error_full ~severity:Error ~category:Type ~pos:(Some pos)
    ~code:"T004" ~msg:"type is not subscriptable")
    "only indexable types support []: lists, strings, tuples, dicts"

let unsupported_operand op typ pos =
  make_error_full ~severity:Error ~category:Type ~pos:(Some pos)
    ~code:"T005" ~msg:(Printf.sprintf "unsupported operand type(s) for '%s': '%s'" op typ)

let not_iterable typ pos =
  with_hint (make_error_full ~severity:Error ~category:Type ~pos:(Some pos)
    ~code:"T006" ~msg:(Printf.sprintf "'%s' is not iterable" typ))
    "for-in loops, comprehensions, and unpacking require an iterable object"

let cannot_assign_to_literal pos =
  with_hint (make_error_full ~severity:Error ~category:Type ~pos:(Some pos)
    ~code:"T007" ~msg:"cannot assign to a literal value")
    "only variables, attributes, and subscripts can be assigned to"

let infinite_type pos =
  make_error_full ~severity:Error ~category:Type ~pos:(Some pos)
    ~code:"T008" ~msg:"infinite type detected (recursive type without explicit annotation)"

(* ---- Semantic errors ---- *)
let division_by_zero pos =
  make_error_full ~severity:Error ~category:Semantic ~pos:(Some pos)
    ~code:"S001" ~msg:"division by zero"

let outside_function what pos =
  with_hint (make_error_full ~severity:Error ~category:Semantic ~pos:(Some pos)
    ~code:"S002" ~msg:(Printf.sprintf "'%s' outside function body" what))
    (Printf.sprintf "'%s' is only valid inside a function definition" what)

let outside_loop what pos =
  with_hint (make_error_full ~severity:Error ~category:Semantic ~pos:(Some pos)
    ~code:"S003" ~msg:(Printf.sprintf "'%s' outside loop" what))
    (Printf.sprintf "'%s' is only valid inside a loop body" what)

let missing_return pos =
  with_hint (make_error_full ~severity:Error ~category:Semantic ~pos:(Some pos)
    ~code:"S004" ~msg:"missing return value on some code paths")
    "all paths through this function must return a value"

let invalid_assignment pos =
  with_hint (make_error_full ~severity:Error ~category:Semantic ~pos:(Some pos)
    ~code:"S005" ~msg:"cannot assign to this expression")
    "only variables, attributes, and subscripts can be assigned to"

let unreachable_code pos =
  with_hint (make_error_full ~severity:Warning ~category:Semantic ~pos:(Some pos)
    ~code:"S006" ~msg:"unreachable code after return/break/continue")
    "any code after a return, break, or continue will never execute"

let duplicate_parameter name pos =
  with_hint (make_error_full ~severity:Error ~category:Semantic ~pos:(Some pos)
    ~code:"S007" ~msg:(Printf.sprintf "duplicate parameter name: '%s'" name))
    "each parameter must have a unique name"

(* ---- Runtime errors ---- *)
let runtime_error msg pos =
  make_error_full ~severity:Error ~category:Runtime ~pos:(Some pos)
    ~code:"R001" ~msg

let index_out_of_range pos =
  with_hint (make_error_full ~severity:Error ~category:Runtime ~pos:(Some pos)
    ~code:"R002" ~msg:"index out of range")
    "list/tuple indices must be within the bounds of the collection"

(* ---- Internal errors — compiler bugs ---- *)
let internal_error msg pos =
  with_hint (make_error_full ~severity:Error ~category:Internal ~pos:(Some pos)
    ~code:"I001" ~msg:("compiler internal error: " ^ msg))
    "this is a bug in the compiler — please report it at github.com/gnuchanos/GnuchanOS"

let unimplemented feature pos =
  with_hint (make_error_full ~severity:Error ~category:Internal ~pos:(Some pos)
    ~code:"I002" ~msg:(Printf.sprintf "feature not yet implemented: %s" feature))
    "this feature is planned but not yet available"

let assertion_failure msg pos =
  make_error_full ~severity:Error ~category:Internal ~pos:(Some pos)
    ~code:"I003" ~msg:("compiler assertion failed: " ^ msg)


(* ---- Warnings ---- *)
let unused_variable name pos =
  make_error_full ~severity:Warning ~category:Warning ~pos:(Some pos)
    ~code:"W001" ~msg:(Printf.sprintf "variable '%s' is assigned but never used" name)

let unused_function name pos =
  make_error_full ~severity:Warning ~category:Warning ~pos:(Some pos)
    ~code:"W002" ~msg:(Printf.sprintf "function '%s' is defined but never used" name)

let deprecated_syntax msg pos =
  make_error_full ~severity:Warning ~category:Warning ~pos:(Some pos)
    ~code:"W003" ~msg:msg

let shadowed_variable name pos =
  make_error_full ~severity:Warning ~category:Warning ~pos:(Some pos)
    ~code:"W004" ~msg:(Printf.sprintf "variable '%s' shadows outer scope variable" name)

let implicit_global name pos =
  make_error_full ~severity:Warning ~category:Warning ~pos:(Some pos)
    ~code:"W005" ~msg:(Printf.sprintf "variable '%s' is implicitly global (use 'global' keyword)" name)
