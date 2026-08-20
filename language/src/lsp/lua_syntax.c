/* lua_syntax.c — Lua dilinin kendi kelimeleri (keywords + globals).
 * GCL -luarun embed kuraliyla uyumlu: Lua 5.4 kelimeleri.
 * prefix: "pri" -> print, "ta" -> table gibi filtreleme. */
#include <stdio.h>
#include <string.h>

#include "lua_syntax.h"

typedef struct {
  const char *label;
  const char *kind;
  const char *detail;
} LuaEntry;

static const LuaEntry g_lua[] = {
    /* keywords */
    {"and", "keyword", "keyword"},
    {"break", "keyword", "keyword"},
    {"do", "keyword", "keyword"},
    {"else", "keyword", "keyword"},
    {"elseif", "keyword", "keyword"},
    {"end", "keyword", "keyword"},
    {"false", "keyword", "false"},
    {"for", "keyword", "for (init, limit, step) do ... end"},
    {"function", "keyword", "function"},
    {"goto", "keyword", "keyword"},
    {"if", "keyword", "if cond then ... end"},
    {"in", "keyword", "keyword"},
    {"local", "keyword", "local name = value"},
    {"nil", "keyword", "nil"},
    {"not", "keyword", "not"},
    {"or", "keyword", "keyword"},
    {"repeat", "keyword", "repeat ... until cond"},
    {"return", "keyword", "return value"},
    {"then", "keyword", "keyword"},
    {"true", "keyword", "true"},
    {"until", "keyword", "until cond"},
    {"while", "keyword", "while cond do ... end"},

    /* globals */
    {"print", "fn", "print(...)"},
    {"require", "fn", "require(\"module\")"},
    {"dofile", "fn", "dofile(filename)"},
    {"load", "fn", "load(chunk)"},
    {"loadfile", "fn", "loadfile(filename)"},
    {"loadstring", "fn", "loadstring(chunk)"},
    {"error", "fn", "error(msg)"},
    {"assert", "fn", "assert(cond)"},
    {"pairs", "fn", "pairs(t)"},
    {"ipairs", "fn", "ipairs(t)"},
    {"next", "fn", "next(t)"},
    {"type", "fn", "type(v)"},
    {"tostring", "fn", "tostring(v)"},
    {"tonumber", "fn", "tonumber(v)"},
    {"select", "fn", "select(index, ...)"},
    {"rawequal", "fn", "rawequal(a, b)"},
    {"rawget", "fn", "rawget(t, k)"},
    {"rawset", "fn", "rawset(t, k, v)"},
    {"rawlen", "fn", "rawlen(v)"},
    {"setmetatable", "fn", "setmetatable(t, mt)"},
    {"getmetatable", "fn", "getmetatable(t)"},
    {"collectgarbage", "fn", "collectgarbage([opt])"},
    {"pcall", "fn", "pcall(f, ...)"},
    {"xpcall", "fn", "xpcall(f, msgh, ...)"},

    /* coroutine */
    {"coroutine", "module", "coroutine library"},
    {"coroutine.create", "fn", "coroutine.create(f)"},
    {"coroutine.resume", "fn", "coroutine.resume(co, ...)"},
    {"coroutine.yield", "fn", "coroutine.yield(...)"},
    {"coroutine.running", "fn", "coroutine.running()"},
    {"coroutine.status", "fn", "coroutine.status(co)"},
    {"coroutine.wrap", "fn", "coroutine.wrap(f)"},
    {"coroutine.isyieldable", "fn", "coroutine.isyieldable()"},

    /* string */
    {"string", "module", "string library"},
    {"string.format", "fn", "string.format(fmt, ...)"},
    {"string.gsub", "fn", "string.gsub(s, pat, repl)"},
    {"string.len", "fn", "string.len(s)"},
    {"string.lower", "fn", "string.lower(s)"},
    {"string.upper", "fn", "string.upper(s)"},
    {"string.sub", "fn", "string.sub(s, i[, j])"},
    {"string.rep", "fn", "string.rep(s, n)"},
    {"string.reverse", "fn", "string.reverse(s)"},
    {"string.byte", "fn", "string.byte(s[, i])"},
    {"string.char", "fn", "string.char(...)"},
    {"string.find", "fn", "string.find(s, pat[, init])"},
    {"string.match", "fn", "string.match(s, pat)"},
    {"string.gmatch", "fn", "string.gmatch(s, pat)"},

    /* table */
    {"table", "module", "table library"},
    {"table.concat", "fn", "table.concat(t[, sep])"},
    {"table.insert", "fn", "table.insert(t, [pos,] v)"},
    {"table.remove", "fn", "table.remove(t[, pos])"},
    {"table.sort", "fn", "table.sort(t[, comp])"},
    {"table.unpack", "fn", "table.unpack(t[, i[, j]])"},
    {"table.pack", "fn", "table.pack(...)"},
    {"table.move", "fn", "table.move(a1, f, e, t[, a2])"},

    /* math */
    {"math", "module", "math library"},
    {"math.abs", "fn", "math.abs(x)"},
    {"math.acos", "fn", "math.acos(x)"},
    {"math.asin", "fn", "math.asin(x)"},
    {"math.atan", "fn", "math.atan(y[, x])"},
    {"math.ceil", "fn", "math.ceil(x)"},
    {"math.cos", "fn", "math.cos(x)"},
    {"math.deg", "fn", "math.deg(x)"},
    {"math.exp", "fn", "math.exp(x)"},
    {"math.floor", "fn", "math.floor(x)"},
    {"math.fmod", "fn", "math.fmod(x, y)"},
    {"math.huge", "const", "math.huge"},
    {"math.log", "fn", "math.log(x)"},
    {"math.max", "fn", "math.max(x, ...)"},
    {"math.min", "fn", "math.min(x, ...)"},
    {"math.modf", "fn", "math.modf(x)"},
    {"math.pi", "const", "math.pi"},
    {"math.pow", "fn", "math.pow(x, y)"},
    {"math.rad", "fn", "math.rad(x)"},
    {"math.random", "fn", "math.random([m[, n]])"},
    {"math.randomseed", "fn", "math.randomseed(x)"},
    {"math.sin", "fn", "math.sin(x)"},
    {"math.sqrt", "fn", "math.sqrt(x)"},
    {"math.tan", "fn", "math.tan(x)"},

    /* io */
    {"io", "module", "io library"},
    {"io.open", "fn", "io.open(filename[, mode])"},
    {"io.close", "fn", "io.close([file])"},
    {"io.read", "fn", "io.read(...)"},
    {"io.write", "fn", "io.write(...)"},
    {"io.lines", "fn", "io.lines([filename])"},
    {"io.input", "fn", "io.input([file])"},
    {"io.output", "fn", "io.output([file])"},

    /* os */
    {"os", "module", "os library"},
    {"os.clock", "fn", "os.clock()"},
    {"os.date", "fn", "os.date([format[, t]])"},
    {"os.time", "fn", "os.time([t])"},
    {"os.exit", "fn", "os.exit([code])"},
    {"os.getenv", "fn", "os.getenv(name)"},
    {"os.setlocale", "fn", "os.setlocale(locale)"},
    {"os.remove", "fn", "os.remove(filename)"},
    {"os.rename", "fn", "os.rename(old, new)"},
    {"os.tmpname", "fn", "os.tmpname()"},

    /* utf8 */
    {"utf8", "module", "utf8 library"},
    {"utf8.char", "fn", "utf8.char(...)"},
    {"utf8.charpattern", "const", "utf8.charpattern"},
    {"utf8.codepoint", "fn", "utf8.codepoint(s[, i[, j]])"},
    {"utf8.codes", "fn", "utf8.codes(s)"},
    {"utf8.len", "fn", "utf8.len(s)"},
    {"utf8.offset", "fn", "utf8.offset(s, n[, i])"},

    /* debug */
    {"debug", "module", "debug library"},
    {"debug.traceback", "fn", "debug.traceback(...)"},
    {"debug.getinfo", "fn", "debug.getinfo(...)"},

    /* constants of the emitter */
    {"_G", "const", "global table"},
    {"_VERSION", "const", "_VERSION"},
    {"arg", "const", "arg table"},
};

static int prefix_match(const char *label, const char *prefix) {
  size_t n = strlen(prefix);
  return strncmp(label, prefix, n) == 0;
}

int lua_syntax_count(const char *prefix) {
  int n = 0;
  size_t pn = prefix ? strlen(prefix) : 0;
  size_t i;
  for (i = 0; i < sizeof g_lua / sizeof g_lua[0]; i++) {
    if (prefix_match(g_lua[i].label, prefix ? prefix : "")) {
      (void)pn;
      n++;
    }
  }
  return n;
}

void lua_syntax_at(int idx, const char *prefix, char *label, size_t label_cap,
                   char *kind, size_t kind_cap, char *detail, size_t detail_cap) {
  int n = 0;
  size_t i;
  if (label_cap > 0) label[0] = '\0';
  if (kind_cap > 0) kind[0] = '\0';
  if (detail_cap > 0) detail[0] = '\0';
  for (i = 0; i < sizeof g_lua / sizeof g_lua[0]; i++) {
    if (prefix_match(g_lua[i].label, prefix ? prefix : "")) {
      if (n == idx) {
        if (label_cap > 0) snprintf(label, label_cap, "%s", g_lua[i].label);
        if (kind_cap > 0) snprintf(kind, kind_cap, "%s", g_lua[i].kind);
        if (detail_cap > 0) snprintf(detail, detail_cap, "%s", g_lua[i].detail);
        return;
      }
      n++;
    }
  }
}
