/*
 * completions.ts — GCL dil sunucusu (gcl-lsp.exe) tarafindan uretilen
 * tamamlama itemlarinin tipi + donusturuculeri.
 *
 * LSP (language/src/lsp/gcl_lsp.c -> gcl-lsp.exe) workspace'i indexler ve
 * import cozumlemesini GCL KURALI ile yapar: "import ossuruk" yazildiysa ve
 * ossuruk.py yan yana duruyorsa LSP, ossuruk.py icindeki HER sembolü
 * (konst, klas, fonksiyon) otomatik tamamlamaya verir. Dosya kaydedilince
 * (didChange) workspace yeniden indexlenir — "zamber ekledim görünmüyor"
 * senaryosu ortadan kalkar.
 */

/* Popup item'i: kind, AutocompletePopup icindeki kindIcon tarafindan
 * kullanilir (fn/fonksiyon/modül/konst/klas). */
export interface CompletionItem {
  label: string;
  kind: "fn" | "function" | "class" | "type" | "const" | "module" | "keyword";
  detail: string;
}

/* LSP item'ini (language/src/lsp) DOM popup item'ina cevirir. */
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

/* LSP CIKMASI/HIC BASLAMAMASI DURUMUNDA Ctrl+Space FALLBACK'I.
 * gcl-lsp.exe spawn edilemezse veya yanit veremezse tamamlama penceresi
 * tamamen bos kalmayip yine de Python'un kendi dilini onerir (bozuk ICU
 * imlec senaryosu dahil popup her zaman bir sey gosterir). LSP calisirken
 * bu liste KULLANILMAZ — LSP zaten python_syntax.c'den ayni kelimeleri
 * dondurur; tekil gozetmen caktirmaz. */
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
];

/* placeholderFallback: LSP yokken Ctrl+Space'e basinca prefix ile eslesen
 * Python keyword + builtin listesini dondurur. */
export function pythonFallback(prefix: string): CompletionItem[] {
  const p = prefix.toLowerCase();
  const out: CompletionItem[] = [];
  for (const w of PY_KEYWORDS) {
    if (w.toLowerCase().startsWith(p))
      out.push({ label: w, kind: "keyword", detail: "keyword" });
  }
  for (const w of PY_BUILTINS) {
    if (w.toLowerCase().startsWith(p))
      out.push({ label: w, kind: "fn", detail: `built-in ${w}` });
  }
  return out;
}
