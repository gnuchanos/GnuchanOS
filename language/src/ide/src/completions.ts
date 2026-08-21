/*
 * completions.ts — GCL dil sunucusu (gcl-lsp.exe) tarafından üretilen
 * tamamlama itemlarının tipi + dönüştürücüleri + LSP yokken/yanıt vermezken
 * kullanılan YEREL dil tabloları (fallback).
 *
 * Yerel tablolar LSP'deki gerçek kelime listelerinin birebir karşılığıdır:
 *   - GCL      -> gcl_syntax.c    (gcl_doc.md sözdizimi)
 *   - Lua      -> lua_syntax.c    (Lua 5.4 keywords + globals + std libs)
 *   - Python   -> python_syntax.c (keywords + builtins + exception sınıfları)
 *
 * LSP çalışırken bu tablolar KULLANILMAZ — gcl-lsp.exe aynı kelimeleri
 * zaten döndürür. LSP spawn edilemezse / IPC asılı kalırsa / index yoksa
 * popup asla boş kalmaması için bu havuzlara düşülür (gcl-lsp_lua.md ve
 * gcl-lsp_python.md'deki "Asla boş kalma" ilkesi).
 */

/* Popup item'i: kind, AutocompletePopup içindeki kindIcon tarafından
 * kullanılır (fn/fonksiyon/modül/const/class). */
export interface CompletionItem {
  label: string;
  kind: "fn" | "function" | "class" | "type" | "const" | "module" | "keyword";
  detail: string;
}

/* LSP item'ini (language/src/lsp) DOM popup item'ına çevirir. */
export function fromLsp(
  kind: string,
  label: string,
  detail: string,
): CompletionItem {
  let k: CompletionItem["kind"];
  switch (kind) {
    case "fn":
    case "function":
      k = "fn";
      break;
    case "class":
    case "type":
      k = "class";
      break;
    case "const":
      k = "const";
      break;
    case "module":
      k = "module";
      break;
    default:
      k = "keyword";
      break;
  }
  return { label, kind: k, detail };
}

/* ------------------------------------------------------------------ */
/* GCL — gcl_syntax.c + gcl_doc.md                                     */
/* ------------------------------------------------------------------ */

interface LangEntry {
  label: string;
  kind: CompletionItem["kind"];
  detail: string;
}

const GCL_WORDS: LangEntry[] = [
  /* keywords */
  { label: "if", kind: "keyword", detail: "if (...) { ... } else if ..." },
  { label: "else", kind: "keyword", detail: "else { ... }" },
  { label: "elif", kind: "keyword", detail: "elif () { ... }" },
  { label: "while", kind: "keyword", detail: "while (...) { ... }" },
  { label: "do", kind: "keyword", detail: "do { ... } while(...)" },
  { label: "for", kind: "keyword", detail: "for (int i = 0; i < n; i++)" },
  { label: "switch", kind: "keyword", detail: "switch () { case: default: }" },
  { label: "case", kind: "keyword", detail: "case:" },
  { label: "break", kind: "keyword", detail: "break;" },
  { label: "continue", kind: "keyword", detail: "continue;" },
  { label: "return", kind: "keyword", detail: "return value;" },
  { label: "public", kind: "keyword", detail: "public type id = value;" },
  { label: "private", kind: "keyword", detail: "private type id = value;" },
  { label: "const", kind: "keyword", detail: "const type id = value;" },
  { label: "inline", kind: "keyword", detail: "inline identifier;" },
  { label: "global", kind: "keyword", detail: "global identifier;" },
  { label: "typedef", kind: "keyword", detail: "typedef ... ; / typedef != notEquals;" },
  { label: "struct", kind: "keyword", detail: "struct Name { ... };" },
  { label: "class", kind: "keyword", detail: "class Name() { void m() {} }" },
  { label: "enum", kind: "keyword", detail: "enum Name { A, B, C };" },
  { label: "tuple", kind: "keyword", detail: "tuple t = ('a', 10, 300.0);" },
  { label: "dict", kind: "keyword", detail: "dict d = { gcChar key : value };" },
  { label: "main", kind: "fn", detail: "int main(int argc, char *argv[])" },

  /* preprocessor */
  { label: "#include", kind: "keyword", detail: '#include "script.gcsf" veya <script.gcsf>' },
  { label: "#lib", kind: "keyword", detail: '#lib "kutuphane.gclib" veya <kutuphane.gclib>' },
  { label: "#extern", kind: "keyword", detail: '#extern "raylib.dll" veya <raylib.so>' },
  { label: "#register", kind: "keyword", detail: "#register void InitWindow(int w, int h, const char *t);" },
  { label: "#define", kind: "keyword", detail: "#define NAME value" },
  { label: "#undef", kind: "keyword", detail: "#undef NAME" },
  { label: "#warning", kind: "keyword", detail: '#warning "sari metin"' },
  { label: "#error", kind: "keyword", detail: '#error "kirmizi metin"' },
  { label: "#debug", kind: "keyword", detail: '#debug "mavi metin"' },
  { label: "#ifdef", kind: "keyword", detail: "#ifdef NAME" },
  { label: "#ifndef", kind: "keyword", detail: "#ifndef NAME" },
  { label: "#if", kind: "keyword", detail: "#if windows" },
  { label: "#elif", kind: "keyword", detail: "#elif linux" },
  { label: "#else", kind: "keyword", detail: "#else" },
  { label: "#endif", kind: "keyword", detail: "#endif" },

  /* GCL types (gcl standard) */
  { label: "int8", kind: "type", detail: "int8" },
  { label: "int16", kind: "type", detail: "int16" },
  { label: "int32", kind: "type", detail: "int32" },
  { label: "int64", kind: "type", detail: "int64" },
  { label: "int128", kind: "type", detail: "int128" },
  { label: "uint8", kind: "type", detail: "uint8" },
  { label: "uint16", kind: "type", detail: "uint16" },
  { label: "uint32", kind: "type", detail: "uint32" },
  { label: "uint64", kind: "type", detail: "uint64" },
  { label: "uint128", kind: "type", detail: "uint128" },
  { label: "float16", kind: "type", detail: "float16" },
  { label: "float32", kind: "type", detail: "float32" },
  { label: "float64", kind: "type", detail: "float64" },
  { label: "float128", kind: "type", detail: "float128" },
  { label: "gcChar", kind: "type", detail: "UTF-8 char/string (char != gcChar)" },
  { label: "bool", kind: "type", detail: "bool" },
  { label: "char", kind: "type", detail: "vanilla 1 byte char" },
  { label: "short", kind: "type", detail: "short" },
  { label: "int", kind: "type", detail: "int" },
  { label: "float", kind: "type", detail: "float" },
  { label: "double", kind: "type", detail: "double" },
  { label: "long", kind: "type", detail: "long" },
  { label: "long long", kind: "type", detail: "long long" },
  { label: "unsigned", kind: "type", detail: "unsigned" },
  { label: "void", kind: "type", detail: "void" },

  /* built-ins */
  { label: "printf", kind: "fn", detail: 'printf("format %d %s %f", args...)' },
  { label: "scanf", kind: "fn", detail: 'scanf("%type", text) — safe read' },
  { label: "malloc", kind: "fn", detail: "malloc(reserve=count) — fixed int* list" },
  { label: "gcMalloc", kind: "fn", detail: "gcMalloc(reserve=count, extra=n) — auto-grow" },
  { label: "free", kind: "fn", detail: "free(ptr) — frees and sets null" },
  { label: "sizeof", kind: "fn", detail: "sizeof(type) veya sizeof(variable)" },
];

/* prefix ile eşleşen GCL kelimelerini döndürür. "#" profiksinde yalnızca
 * direktifler döner (gclFallback zaten "#" ile filtreler). */
export function gclFallback(prefix: string): CompletionItem[] {
  const p = prefix.toLowerCase();
  const out: CompletionItem[] = [];
  for (const w of GCL_WORDS) {
    if (w.label.toLowerCase().startsWith(p)) out.push(w);
  }
  return out;
}

/* ------------------------------------------------------------------ */
/* Lua 5.4 — lua_syntax.c + gcl-lsp_lua.md                             */
/* ------------------------------------------------------------------ */

const LUA_WORDS: LangEntry[] = [
  /* keywords */
  { label: "and", kind: "keyword", detail: "and" },
  { label: "break", kind: "keyword", detail: "break" },
  { label: "do", kind: "keyword", detail: "do ... end" },
  { label: "else", kind: "keyword", detail: "else" },
  { label: "elseif", kind: "keyword", detail: "elseif cond then" },
  { label: "end", kind: "keyword", detail: "end" },
  { label: "false", kind: "keyword", detail: "false" },
  { label: "for", kind: "keyword", detail: "for i = 1, n do ... end" },
  { label: "function", kind: "keyword", detail: "function ad(...) ... end" },
  { label: "goto", kind: "keyword", detail: "goto label" },
  { label: "if", kind: "keyword", detail: "if cond then ... end" },
  { label: "in", kind: "keyword", detail: "in" },
  { label: "local", kind: "keyword", detail: "local name = value" },
  { label: "nil", kind: "keyword", detail: "nil" },
  { label: "not", kind: "keyword", detail: "not" },
  { label: "or", kind: "keyword", detail: "or" },
  { label: "repeat", kind: "keyword", detail: "repeat ... until cond" },
  { label: "return", kind: "keyword", detail: "return value" },
  { label: "then", kind: "keyword", detail: "then" },
  { label: "true", kind: "keyword", detail: "true" },
  { label: "until", kind: "keyword", detail: "until cond" },
  { label: "while", kind: "keyword", detail: "while cond do ... end" },

  /* globals */
  { label: "print", kind: "fn", detail: "print(...)" },
  { label: "require", kind: "fn", detail: 'require("module")' },
  { label: "dofile", kind: "fn", detail: "dofile(filename)" },
  { label: "load", kind: "fn", detail: "load(chunk)" },
  { label: "loadfile", kind: "fn", detail: "loadfile(filename)" },
  { label: "loadstring", kind: "fn", detail: "loadstring(chunk)" },
  { label: "error", kind: "fn", detail: "error(msg)" },
  { label: "assert", kind: "fn", detail: "assert(cond)" },
  { label: "pairs", kind: "fn", detail: "pairs(t)" },
  { label: "ipairs", kind: "fn", detail: "ipairs(t)" },
  { label: "next", kind: "fn", detail: "next(t)" },
  { label: "type", kind: "fn", detail: "type(v)" },
  { label: "tostring", kind: "fn", detail: "tostring(v)" },
  { label: "tonumber", kind: "fn", detail: "tonumber(v)" },
  { label: "select", kind: "fn", detail: "select(index, ...)" },
  { label: "rawequal", kind: "fn", detail: "rawequal(a, b)" },
  { label: "rawget", kind: "fn", detail: "rawget(t, k)" },
  { label: "rawset", kind: "fn", detail: "rawset(t, k, v)" },
  { label: "rawlen", kind: "fn", detail: "rawlen(v)" },
  { label: "setmetatable", kind: "fn", detail: "setmetatable(t, mt)" },
  { label: "getmetatable", kind: "fn", detail: "getmetatable(t)" },
  { label: "collectgarbage", kind: "fn", detail: "collectgarbage([opt])" },
  { label: "pcall", kind: "fn", detail: "pcall(f, ...)" },
  { label: "xpcall", kind: "fn", detail: "xpcall(f, msgh, ...)" },

  /* std libs: module + members */
  { label: "coroutine", kind: "module", detail: "coroutine library" },
  { label: "coroutine.create", kind: "fn", detail: "coroutine.create(f)" },
  { label: "coroutine.resume", kind: "fn", detail: "coroutine.resume(co, ...)" },
  { label: "coroutine.yield", kind: "fn", detail: "coroutine.yield(...)" },
  { label: "coroutine.running", kind: "fn", detail: "coroutine.running()" },
  { label: "coroutine.status", kind: "fn", detail: "coroutine.status(co)" },
  { label: "coroutine.wrap", kind: "fn", detail: "coroutine.wrap(f)" },
  { label: "coroutine.isyieldable", kind: "fn", detail: "coroutine.isyieldable()" },
  { label: "string", kind: "module", detail: "string library" },
  { label: "string.format", kind: "fn", detail: "string.format(fmt, ...)" },
  { label: "string.gsub", kind: "fn", detail: "string.gsub(s, pat, repl)" },
  { label: "string.len", kind: "fn", detail: "string.len(s)" },
  { label: "string.lower", kind: "fn", detail: "string.lower(s)" },
  { label: "string.upper", kind: "fn", detail: "string.upper(s)" },
  { label: "string.sub", kind: "fn", detail: "string.sub(s, i[, j])" },
  { label: "string.rep", kind: "fn", detail: "string.rep(s, n)" },
  { label: "string.reverse", kind: "fn", detail: "string.reverse(s)" },
  { label: "string.byte", kind: "fn", detail: "string.byte(s[, i])" },
  { label: "string.char", kind: "fn", detail: "string.char(...)" },
  { label: "string.find", kind: "fn", detail: "string.find(s, pat[, init])" },
  { label: "string.match", kind: "fn", detail: "string.match(s, pat)" },
  { label: "string.gmatch", kind: "fn", detail: "string.gmatch(s, pat)" },
  { label: "table", kind: "module", detail: "table library" },
  { label: "table.concat", kind: "fn", detail: "table.concat(t[, sep])" },
  { label: "table.insert", kind: "fn", detail: "table.insert(t, [pos,] v)" },
  { label: "table.remove", kind: "fn", detail: "table.remove(t[, pos])" },
  { label: "table.sort", kind: "fn", detail: "table.sort(t[, comp])" },
  { label: "table.unpack", kind: "fn", detail: "table.unpack(t[, i[, j]])" },
  { label: "table.pack", kind: "fn", detail: "table.pack(...)" },
  { label: "table.move", kind: "fn", detail: "table.move(a1, f, e, t[, a2])" },
  { label: "math", kind: "module", detail: "math library" },
  { label: "math.abs", kind: "fn", detail: "math.abs(x)" },
  { label: "math.acos", kind: "fn", detail: "math.acos(x)" },
  { label: "math.asin", kind: "fn", detail: "math.asin(x)" },
  { label: "math.atan", kind: "fn", detail: "math.atan(y[, x])" },
  { label: "math.ceil", kind: "fn", detail: "math.ceil(x)" },
  { label: "math.cos", kind: "fn", detail: "math.cos(x)" },
  { label: "math.deg", kind: "fn", detail: "math.deg(x)" },
  { label: "math.exp", kind: "fn", detail: "math.exp(x)" },
  { label: "math.floor", kind: "fn", detail: "math.floor(x)" },
  { label: "math.fmod", kind: "fn", detail: "math.fmod(x, y)" },
  { label: "math.huge", kind: "const", detail: "math.huge" },
  { label: "math.log", kind: "fn", detail: "math.log(x)" },
  { label: "math.max", kind: "fn", detail: "math.max(x, ...)" },
  { label: "math.min", kind: "fn", detail: "math.min(x, ...)" },
  { label: "math.modf", kind: "fn", detail: "math.modf(x)" },
  { label: "math.pi", kind: "const", detail: "math.pi" },
  { label: "math.pow", kind: "fn", detail: "math.pow(x, y)" },
  { label: "math.rad", kind: "fn", detail: "math.rad(x)" },
  { label: "math.random", kind: "fn", detail: "math.random([m[, n]])" },
  { label: "math.randomseed", kind: "fn", detail: "math.randomseed(x)" },
  { label: "math.sin", kind: "fn", detail: "math.sin(x)" },
  { label: "math.sqrt", kind: "fn", detail: "math.sqrt(x)" },
  { label: "math.tan", kind: "fn", detail: "math.tan(x)" },
  { label: "io", kind: "module", detail: "io library" },
  { label: "io.open", kind: "fn", detail: "io.open(filename[, mode])" },
  { label: "io.close", kind: "fn", detail: "io.close([file])" },
  { label: "io.read", kind: "fn", detail: "io.read(...)" },
  { label: "io.write", kind: "fn", detail: "io.write(...)" },
  { label: "io.lines", kind: "fn", detail: "io.lines([filename])" },
  { label: "io.input", kind: "fn", detail: "io.input([file])" },
  { label: "io.output", kind: "fn", detail: "io.output([file])" },
  { label: "os", kind: "module", detail: "os library" },
  { label: "os.clock", kind: "fn", detail: "os.clock()" },
  { label: "os.date", kind: "fn", detail: "os.date([format[, t]])" },
  { label: "os.time", kind: "fn", detail: "os.time([t])" },
  { label: "os.exit", kind: "fn", detail: "os.exit([code])" },
  { label: "os.getenv", kind: "fn", detail: "os.getenv(name)" },
  { label: "os.setlocale", kind: "fn", detail: "os.setlocale(locale)" },
  { label: "os.remove", kind: "fn", detail: "os.remove(filename)" },
  { label: "os.rename", kind: "fn", detail: "os.rename(old, new)" },
  { label: "os.tmpname", kind: "fn", detail: "os.tmpname()" },
  { label: "utf8", kind: "module", detail: "utf8 library" },
  { label: "utf8.char", kind: "fn", detail: "utf8.char(...)" },
  { label: "utf8.charpattern", kind: "const", detail: "utf8.charpattern" },
  { label: "utf8.codepoint", kind: "fn", detail: "utf8.codepoint(s[, i[, j]])" },
  { label: "utf8.codes", kind: "fn", detail: "utf8.codes(s)" },
  { label: "utf8.len", kind: "fn", detail: "utf8.len(s)" },
  { label: "utf8.offset", kind: "fn", detail: "utf8.offset(s, n[, i])" },
  { label: "debug", kind: "module", detail: "debug library" },
  { label: "debug.traceback", kind: "fn", detail: "debug.traceback(...)" },
  { label: "debug.getinfo", kind: "fn", detail: "debug.getinfo(...)" },

  /* Lua constants */
  { label: "_G", kind: "const", detail: "global table" },
  { label: "_VERSION", kind: "const", detail: "_VERSION" },
  { label: "arg", kind: "const", detail: "arg table" },
];

export function luaFallback(prefix: string): CompletionItem[] {
  const p = prefix.toLowerCase();
  const out: CompletionItem[] = [];
  for (const w of LUA_WORDS) {
    if (w.label.toLowerCase().startsWith(p)) out.push(w);
  }
  return out;
}

/* ------------------------------------------------------------------ */
/* Python — python_syntax.c + gcl-lsp_python.md                        */
/* ------------------------------------------------------------------ */

const PY_KEYWORDS: string[] = [
  "False", "None", "True", "and", "as", "assert", "async", "await",
  "break", "class", "continue", "def", "del", "elif", "else", "except",
  "finally", "for", "from", "global", "if", "import", "in", "is",
  "lambda", "nonlocal", "not", "or", "pass", "raise", "return", "try",
  "while", "with", "yield",
];

const PY_BUILTINS: string[] = [
  "abs", "aiter", "all", "anext", "any", "ascii", "bin", "bool",
  "breakpoint", "bytearray", "bytes", "callable", "chr", "classmethod",
  "compile", "complex", "delattr", "dict", "dir", "divmod", "enumerate",
  "eval", "exec", "filter", "float", "format", "frozenset", "getattr",
  "globals", "hasattr", "hash", "help", "hex", "id", "input", "int",
  "isinstance", "issubclass", "iter", "len", "list", "locals", "map",
  "max", "memoryview", "min", "next", "object", "oct", "open", "ord",
  "pow", "print", "property", "range", "repr", "reversed", "round",
  "set", "setattr", "slice", "sorted", "staticmethod", "str", "sum",
  "super", "tuple", "type", "vars", "zip",
  /* exception + warning sınıfları (python_syntax.c'deki tam liste) */
  "ArithmeticError", "AssertionError", "AttributeError", "BaseException",
  "BaseExceptionGroup", "BlockingIOError", "BrokenPipeError", "BufferError",
  "BytesWarning", "ChildProcessError", "ConnectionAbortedError",
  "ConnectionError", "ConnectionRefusedError", "ConnectionResetError",
  "DeprecationWarning", "EOFError", "Ellipsis", "EncodingWarning",
  "EnvironmentError", "Exception", "ExceptionGroup", "FloatingPointError",
  "FutureWarning", "GeneratorExit", "ImportError", "ImportWarning",
  "IndentationError", "IndexError", "InterruptedError", "IsADirectoryError",
  "KeyError", "KeyboardInterrupt", "LookupError", "MemoryError",
  "ModuleNotFoundError", "NotADirectoryError", "NotImplemented",
  "NotImplementedError", "OSError", "OverflowError",
  "PendingDeprecationWarning", "PermissionError", "ProcessLookupError",
  "RecursionError", "ReferenceError", "ResourceWarning", "RuntimeError",
  "RuntimeWarning", "StopAsyncIteration", "StopIteration", "SyntaxError",
  "SyntaxWarning", "SystemError", "SystemExit", "TabError", "TimeoutError",
  "TypeError", "UnboundLocalError", "UnicodeDecodeError",
  "UnicodeEncodeError", "UnicodeError", "UnicodeTranslateError",
  "UnicodeWarning", "UserWarning", "ValueError", "Warning",
  "ZeroDivisionError",
];

export function pythonFallback(prefix: string): CompletionItem[] {
  const p = prefix.toLowerCase();
  const out: CompletionItem[] = [];
  for (const w of PY_KEYWORDS) {
    if (w.toLowerCase().startsWith(p))
      out.push({ label: w, kind: "keyword", detail: "keyword" });
  }
  for (const w of PY_BUILTINS) {
    if (w.toLowerCase().startsWith(p)) {
      const isClass = w[0] >= "A" && w[0] <= "Z";
      out.push({
        label: w,
        kind: isClass ? "class" : "fn",
        detail: isClass ? `built-in ${w}` : `built-in ${w}`,
      });
    }
  }
  return out;
}
