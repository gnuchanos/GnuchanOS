(* ================================================================
   ast.ml — Abstract Syntax Tree for SHL (Self-Hosting Language)
   Full Python feature set AST nodes.
   ================================================================ *)

type pos = {
  file: string;
  line: int;
  col: int;
}

type literal =
  | LInt    of int
  | LFloat  of float
  | LString of string
  | LBool   of bool
  | LNull
  | LBytes  of string
  | LComplex of float * float

type binop =
  | Add  | Sub  | Mul  | Div  | Mod | Pow | FloorDiv
  | Eq   | Ne   | Lt   | Le   | Gt   | Ge
  | And  | Or
  | BitOr | BitAnd | BitXor | LShift | RShift
  | Assign | AddAssign | SubAssign | MulAssign | DivAssign | ModAssign
  | PowAssign | FloorDivAssign | BitOrAssign | BitAndAssign | BitXorAssign
  | LShiftAssign | RShiftAssign
  | Is | IsNot | In | NotIn
  | Seq

type unop =
  | Neg | Not | BitNot
  | PreInc | PreDec | PostInc | PostDec
  | Plus

type typ =
  | TUnit
  | TBool
  | TInt
  | TFloat
  | TString
  | TBytes
  | TComplex
  | TFun   of typ list * typ
  | TArray of typ
  | TTuple of typ list
  | TDict  of typ * typ
  | TSet   of typ
  | TStruct of string
  | TEnum   of string
  | TNamed  of string
  | TClass  of string * typ option
  | TSelf
  | TVar   of int
  | TLink  of typ ref
  | TOptional of typ
  | TAny

type type_expr = {
  te_node: type_expr_node;
  te_pos: pos;
}

and type_expr_node =
  | TName of string
  | TApply of type_expr * type_expr list
  | TFunType of type_expr list * type_expr
  | TIndexType of type_expr * type_expr

(* param must be defined before expr since expr uses param list *)
and param = {
  p_name:  string;
  p_typ:   typ option;
  p_default: expr option;
  p_pos:   pos;
}

and comp_clause = {
  cc_binding: expr;
  cc_iterator: expr;
  cc_conditions: expr list;
}

and match_case = {
  mc_pattern: expr;
  mc_guard: expr option;
  mc_body: stmt;
}

and expr = {
  e_node:  expr_node;
  mutable e_typ:   typ option;
  e_pos:   pos;
}

and expr_node =
  | ELiteral    of literal
  | EIdent      of string
  | EBinary     of binop * expr * expr
  | EUnary      of unop * expr
  | ETernary    of expr * expr * expr
  | ECall       of expr * expr list
  | EIndex      of expr * expr
  | ESlice      of expr option * expr option * expr option
  | EList       of expr list
  | ETuple      of expr list
  | EDict       of (expr * expr) list
  | ESet        of expr list
  | EMember     of expr * string
  | EWalrus     of string * expr
  | ELambda     of param list * expr
  | EListComp   of expr * comp_clause list
  | ESetComp    of expr * comp_clause list
  | EDictComp   of expr * expr * comp_clause list
  | EGenerator  of expr * comp_clause list

and stmt = {
  s_node:  stmt_node;
  s_pos:   pos;
}

and stmt_node =
  | SBlock      of stmt list
  | SExpr       of expr
  | SIf         of expr * stmt * stmt option
  | SWhile      of expr * stmt
  | SFor        of stmt * expr * expr * stmt
  | SForIn      of expr * expr * stmt
  | SReturn     of expr option
  | SBreak
  | SContinue
  | SDecl       of decl
  | SDelete     of expr
  | SAssert     of expr * expr option
  | SRaise      of expr option
  | SGlobal     of string list
  | SNonlocal   of string list
  | STry        of stmt * (expr option * string option * stmt) list * stmt option
  | SWith       of expr list * stmt
  | SYield      of expr option
  | SYieldFrom  of expr
  | SMatch      of expr * match_case list
  | SDecorated  of expr list * decl

and decl = {
  d_node:  decl_node;
  mutable d_typ:   typ option;
  d_pos:   pos;
}

and decl_node =
  | DVarDecl        of string * expr option
  | DVarDeclAnnot   of string * type_expr * expr option
  | DFuncDef        of string * param list * type_expr option * stmt
  | DExternFunc     of string * param list
  | DStructDecl     of string * decl list option
  | DEnumDecl       of string * (string * expr option) list
  | DClassDef       of string * expr list * stmt
  | DImport         of string * string option
  | DFromImport     of string * (string * string option) list
  | DTypeAlias      of string * type_expr

type program = {
  decls:   decl list;
  file:    string;
}
