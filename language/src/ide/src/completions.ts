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
