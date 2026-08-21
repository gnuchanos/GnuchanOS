import { useCallback, useEffect, useRef, useState } from "react";
import Editor, { OnMount } from "@monaco-editor/react";
import { monaco } from "../monacoSetup";
import type { editor as MonacoEditor } from "monaco-editor";
import type { Palette } from "../ideSettings";
import {
  fromLsp,
  gclFallback,
  luaFallback,
  pythonFallback,
  type CompletionItem,
} from "../completions";
import type { LspCompletionItem } from "../types";
import AutocompletePopup from "./AutocompletePopup";

export interface OpenFile {
  path: string;
  name: string;
  lang: string; // "lua" | "python" | "gcl" | "doc" | "ref" | "plaintext"
}

interface Props {
  file: OpenFile;
  content: string;
  root: string;
  onContent: (value: string | undefined) => void;
  onSave: () => void;
  onRun: () => void;
  onCursor: (line: number, col: number) => void;
  palette: Palette;
  fontSize: number;
  fontFamily: string;
}

/* Popup durumu: onerilen item'lar + secili satir + imlec hizasi + insert range.
 * signature: "func(" sonrasi gosterilen parametre imzasi (items bos olabilir). */
interface PopupState {
  items: CompletionItem[];
  index: number;
  top: number;
  left: number;
  range: monaco.IRange;
  signature?: string;
}

/* Async popup yenileme icin race guard: eski istek donsa bile yeni bir
 * istek varsa eski sonuc yok sayilir. */
let refreshSeq = 0;

/* LSP istegini 400ms timeout ile sarmalar. gcl-lsp.exe spawn edilemezse
 * veya IPC yaniti asili kalirsa (lspSend promise'i cozumlemez) popup akisi
 * hicbir zaman sorun yasamaz: timeout'ta [] doner, cagiran taraf fallback'e
 * gecer. Bu, "Ctrl+Space basiyorum hicbir sey olmuyor" sikayetinin kok
 * nedeni olan asili-await durumunu kokten kapatir. */
async function lspCompleteSafe(
  filePath: string,
  line: number,
  col: number,
  docText: string,
): Promise<LspCompletionItem[]> {
  const task = window.ide.lspComplete(filePath, line, col, docText);
  const timeout = new Promise<LspCompletionItem[]>((resolve) => {
    setTimeout(() => resolve([]), 400);
  });
  return Promise.race([task, timeout]);
}

/* Dosyadaki #include/#lib direktif adlarini toplar (hem <x> hem "x").
 * Hem include dosyalarini cozmek hem de LSP modul item'larini süzmek
 * icin kullanilir. */
function collectIncludeNames(docText: string): string[] {
  const names: string[] = [];
  const re = /#(?:include|lib)\s*[<"]([^>"]+)[>"]/g;
  let m: RegExpExecArray | null;
  while ((m = re.exec(docText))) {
    const n = m[1].trim();
    if (n && !names.includes(n)) names.push(n);
  }
  return names;
}

/* GCL preprocessor birlestirmesi (gcl_merge_includes): #include/#lib ile
 * cagrilan dosya icerigi BU dosyaya gomulur — yani o dosyadaki semboller
 * de burada gecerlidir. gcl.c dosya adini once dosyanin BULUNDUGU KLASORDE,
 * ardindan proje kokunde arar ve sirasiyla (adi, adi.gcsf, adi.gclib)
 * dener. IDE tamamlamasi da ayni kurali izler: dosyalari okur, iclerinden
 * fonksiyon/makro/konst/class sembollerini cikarir ve listeye ekler — boylece
 * "#include <test_include> yazdim ama oradaki fonksiyonlar cikmiyor" sorunu
 * kokten cozulur. */
async function includedSymbols(
  filePath: string,
  docText: string,
  root: string,
): Promise<CompletionItem[]> {
  /* Arama sirasi GCL ile birebir: once dosyanin dizini, sonra proje koku */
  const dirs: string[] = [];
  const sepIdx = Math.max(
    filePath.lastIndexOf("/"),
    filePath.lastIndexOf("\\"),
  );
  if (sepIdx > 0) dirs.push(filePath.slice(0, sepIdx));
  if (root) dirs.push(root);

  const names = collectIncludeNames(docText);
  if (names.length === 0) return [];

  const exts = ["", ".gcsf", ".gclib"];
  const out: CompletionItem[] = [];
  const seenFiles = new Set<string>();
  const seenLabels = new Set<string>();
  for (const name of names) {
    let resolved = false;
    for (const base of dirs) {
      if (!base) continue;
      for (const ext of exts) {
        const candidate = `${base}/${name}${ext}`;
        if (seenFiles.has(candidate)) continue;
        seenFiles.add(candidate);
        let content: string;
        try {
          content = await window.ide.readFile(candidate);
        } catch {
          continue; /* yok: sonraki uzanti */
        }
        resolved = true;
        for (const s of extractIncludedSymbols(content)) {
          if (!seenLabels.has(s.label)) {
            seenLabels.add(s.label);
            out.push(s);
          }
        }
        break;
      }
      if (resolved) break;
    }
  }
  return out;
}

function extractIncludedSymbols(src: string): CompletionItem[] {
  const items: CompletionItem[] = [];
  const seen = new Set<string>();
  const add = (label: string, kind: CompletionItem["kind"], detail: string) => {
    if (!seen.has(label)) {
      seen.add(label);
      items.push({ label, kind, detail });
    }
  };

  /* #define MAKRO */
  let m: RegExpExecArray | null;
  const defRe = /#define\s+([A-Za-z_]\w*)/g;
  while ((m = defRe.exec(src))) add(m[1], "const", "macro (included)");

  /* GCL fonksiyon tanimi: [nitelikler] [tip] ad(parametreler)...
   * "if (", "for (" gibi ifadeler ad yakalayamadigi icin eslesmez:
   * tip alternatifinden sonra \s+ gerektigi icin "if (" deki "if" tek
   * basina kalmaz; ad kismi yalnizca gercek tanimlarda dolar. */
  const fnRe =
    /(?:(?:public|private|inline|static|const)\s+)*(?:int(?:8|16|32|64|128)?|uint(?:8|16|32|64|128)?|float(?:16|32|64|128)?|char|short|int|long|double|float|bool|void|gcChar|[A-Za-z_]\w*)\s+([A-Za-z_]\w*)\s*\(/g;
  while ((m = fnRe.exec(src))) {
    add(m[1], "fn", "function (included)");
  }

  /* const tip X = deger */
  const constRe =
    /\bconst\s+(?:int(?:8|16|32|64|128)?|uint(?:8|16|32|64|128)?|float(?:16|32|64|128)?|char|bool|gcChar)\s+([A-Za-z_]\w*)\s*=/g;
  while ((m = constRe.exec(src))) add(m[1], "const", "const (included)");

  /* class / struct / enum */
  const typeRe = /\b(?:class|struct|enum)\s+([A-Za-z_]\w*)/g;
  while ((m = typeRe.exec(src))) add(m[1], "class", "type (included)");

  return items;
}

/* Monaco find widget (Ctrl+F arama penceresi) renkleri: tema paletinden
 * turetir. `editorWidget`/`input`/`button`/`list` alanlari arama kutusu,
 * sonuc listesi ve toggle butonlarini (regex, duyarlilik) boyar; aksi
 * halde widget vs-dark varsayilaninda kalip temaya uymazdi. */
function findWidgetColors(palette: Palette): Record<string, string> {
  return {
    "editorWidget.background": palette.editorWidget,
    "editorWidget.border": palette.editorWidgetBorder,
    "editorWidget.foreground": palette.suggestFg,
    "input.background": palette.inputBg,
    "input.foreground": palette.inputFg,
    "input.border": palette.inputBorder,
    "input.placeholderForeground": palette.inputPlaceholder,
    "inputOption.activeBackground": palette.editorWidgetSel,
    "inputOption.activeBorder": palette.focusBorder,
    "inputOption.activeForeground": palette.suggestSelectedFg,
    "focusBorder": palette.focusBorder,
    "button.background": palette.buttonBg,
    "button.foreground": palette.buttonFg,
    "button.hoverBackground": palette.buttonHoverBg,
    "list.hoverBackground": palette.listHoverBg,
    "list.hoverForeground": palette.listHoverFg,
    "list.activeSelectionBackground": palette.listActiveBg,
    "list.activeSelectionForeground": palette.listActiveFg,
    "editor.findMatchBackground": palette.selectionBg,
    "editor.findMatchHighlightBackground": palette.selectionBg,
    "editor.findRangeHighlightBackground": palette.selectionBg,
    "editor.findMatchBorder": palette.acc,
    "editor.findMatchHighlightBorder": palette.acc,
  };
}

/* gcl-theme tanimi (onMount + useEffect icin). Native suggest kullanilmasa
 * da editor bg/fg/token renkleri buradan gelir; find widget renkleri de
 * buraya bindirilir. */
function defineGclTheme(monacoInstance: typeof monaco, palette: Palette) {
  monacoInstance.editor.defineTheme("gcl-theme", {
    base: "vs-dark",
    inherit: true,
    rules: [
      { token: "comment", foreground: palette.fg2 },
      { token: "keyword", foreground: palette.acc },
      { token: "string", foreground: palette.alt },
      { token: "number", foreground: palette.alt },
      { token: "number.float", foreground: palette.alt },
      { token: "preprocessor", foreground: palette.acc, fontStyle: "italic" },
      { token: "type.identifier", foreground: palette.acc },
      { token: "delimiter", foreground: palette.fg2 },
    ],
    colors: {
      "editor.background": palette.editorBg,
      "editor.lineHighlightBackground": palette.editorLine,
      "editorLineNumber.foreground": palette.editorLineNum,
      "editorCursor.foreground": palette.editorCursor,
      "editor.selectionBackground": palette.editorSelection,
      ...findWidgetColors(palette),
    },
  });
  monacoInstance.editor.setTheme("gcl-theme");
}

export default function EditorView({
  file,
  content,
  root,
  onContent,
  onSave,
  onRun,
  onCursor,
  palette,
  fontSize,
  fontFamily,
}: Props) {
  const editorRef = useRef<MonacoEditor.IStandaloneCodeEditor | null>(null);
  const wrapRef = useRef<HTMLDivElement | null>(null);

  const [popup, setPopup] = useState<PopupState | null>(null);
  const popupRef = useRef<PopupState | null>(null);

  /* state + ref'i birlikte guncelle: keydown handle'i onMount'ta bir kez
   * kuruldugu icin ref uzerinden guncel durumu okur. */
  const setPopupBoth = useCallback((p: PopupState | null) => {
    popupRef.current = p;
    setPopup(p);
  }, []);

  const applyItem = useCallback((p: PopupState) => {
    const editor = editorRef.current;
    const item = p.items[p.index];
    if (!editor || !item) return;
    editor.executeEdits("ide-autocomplete", [
      {
        range: p.range,
        text: item.label,
        forceMoveMarkers: true,
      },
    ]);
    editor.setPosition({
      lineNumber: p.range.startLineNumber,
      column: p.range.startColumn + item.label.length,
    });
    editor.focus();
  }, []);

  const computePos = useCallback((editor: MonacoEditor.IStandaloneCodeEditor, pos: monaco.Position) => {
    const wrap = wrapRef.current;
    const dom = editor.getDomNode();
    const coords = editor.getScrolledVisiblePosition(pos);
    if (!wrap || !dom || !coords) return null;
    const wrapRect = wrap.getBoundingClientRect();
    const domRect = dom.getBoundingClientRect();
    return {
      top: domRect.top - wrapRect.top + coords.top + coords.height + 4,
      left: domRect.left - wrapRect.left + coords.left,
    };
  }, []);

  /* Popup'i guncelle: imlec + prefix + member + parantez imzasi + pozisyon.
   * force=true (Ctrl+Space) prefix bos olsa bile tum modulleri listeler. */
  const doRefresh = useCallback(
    async (force = false) => {
      const editor = editorRef.current;
      const model = editor?.getModel();
      const pos = editor?.getPosition();
      if (!editor || !model || !pos) {
        setPopupBoth(null);
        return;
      }

      const lineText = model.getLineContent(pos.lineNumber);
      const beforeCursor = lineText.slice(0, pos.column - 1);

      const seq = ++refreshSeq;
      const docText = model.getValue();

      /* 1) PARAMS PENCERESI: "(" acildigi anda ve icinde kalindigi surece
       *    (henuz ")" yazilmadi) fonksiyon imzasi USTTE serit, tamamlama
       *    itemlari ALTTTA birlikte gosterilir. "InitWindow(scre" yazarken
       *    hem "InitWindow(width, height, title)" sabit gorunur hem
       *    screenWidth onerilir. Satir disina cikilmadikca kapanmaz. */
      const callMatch = beforeCursor.match(
        /(?:([A-Za-z_]\w*)\.)?([A-Za-z_]\w*)\s*\(\s*([^)]*)$/,
      );
      if (callMatch) {
        const func = callMatch[2];
        /* Parantez ICI: "InitWindow(screenWidth, h" -> inside. Insert yalnizca
         * imlecin gerisindeki SON KELIMEYI degistirir; virgul + onceki
         * parametreler asla silinmez. */
        const inside = callMatch[3] ?? "";
        const innerPrefix = inside.match(/[A-Za-z_]\w*$/)?.at(0) ?? "";

        const lspCall = await lspCompleteSafe(
          file.path,
          pos.lineNumber,
          pos.column,
          docText,
        );
        if (seq !== refreshSeq) return;
        const sigItem = lspCall.find(
          (it) => it.label === func && it.detail.includes("("),
        );
        /* Virgulden sonra alakasiz oneriler cikmasin: son kelimeye gore filtre. */
        const items = lspCall
          .map((it) => fromLsp(it.kind, it.label, it.detail))
          .filter((it) => !innerPrefix || it.label.startsWith(innerPrefix));

        const xy = computePos(editor, pos);
        if (!xy) {
          setPopupBoth(null);
          return;
        }
        /* "(" acik oldugu surece popup KAPANMAZ (sigItem yoksa da): imza
         * varsa USTTE, liste ALTTA. Esc/yeni satir -> callMatch eslesmez. */
        setPopupBoth({
          items,
          index: 0,
          top: xy.top,
          left: xy.left,
          range: {
            startLineNumber: pos.lineNumber,
            startColumn: pos.column - innerPrefix.length,
            endLineNumber: pos.lineNumber,
            endColumn: pos.column,
          },
          signature: sigItem?.detail,
        });
        return;
      }

      /* 2) member: "mod." veya "mod.pre" (prefix bos olabilir -> "rl." aninda acilir).
       *    Duz yazida (nokta yok) imlecin gerisindeki kelime prefix olur:
       *    "a" yazinca autocomplete acilir — VS Code davranisi. */
      const memberMatch = beforeCursor.match(
        /([A-Za-z_]\w*)\.([A-Za-z0-9_]*)$/,
      );
      const isMember = !!memberMatch;
      const memberModule = memberMatch?.[1] ?? "";
      const prefix = memberMatch
        ? memberMatch[2]
        : (beforeCursor.match(/[A-Za-z_]\w*$/)?.at(0) ?? "");

      /* 3) INCLUDE DIREKTIF MODU: "#include <" / "#lib \"" / "#extern <" yazarken
       *    projedeki script/native dosyalarini oner. GCL preprocessor import adi
       *    uzantisiz dosya adidir (#include <test_include> -> test_include.gcsf);
       *    #extern ise native (.dll/.so/.gcdl) dosya adiyla yuklenir (raylib.dll).
       *    Insert "<" veya "\"" SONRASINDAN baslar — direktif + ayrac korunur. */
      const includeMatch = beforeCursor.match(
        /#(include|lib|extern)\s*[<"]([^<"]*)$/,
      );
      if (includeMatch) {
        const incKind = includeMatch[1];
        const incPrefix = includeMatch[2];
        const incLower = incPrefix.toLowerCase();
        let incItems: CompletionItem[] = [];
        try {
          const info = root ? await window.ide.readProject(root) : null;
          for (const f of info?.files ?? []) {
            const isNative =
              f.ext === ".dll" || f.ext === ".so" || f.ext === ".gcdl";
            if (incKind === "extern") {
              if (!isNative) continue;
              if (!f.name.toLowerCase().startsWith(incLower)) continue;
              incItems.push({
                label: f.name,
                kind: "const",
                detail: "native library",
              });
            } else {
              if (isNative) continue;
              if (!f.importName.toLowerCase().startsWith(incLower)) continue;
              incItems.push({
                label: f.importName,
                kind: "const",
                detail: `${f.ext.slice(1)} module`,
              });
            }
          }
        } catch {
          /* workspace okunamadi: bos liste */
        }
        if (incItems.length === 0) {
          setPopupBoth(null);
          return;
        }
        if (seq !== refreshSeq) return;
        incItems.sort((a, b) => a.label.localeCompare(b.label));
        const incXy = computePos(editor, pos);
        if (!incXy) {
          setPopupBoth(null);
          return;
        }
        setPopupBoth({
          items: incItems,
          index: 0,
          top: incXy.top,
          left: incXy.left,
          range: {
            startLineNumber: pos.lineNumber,
            startColumn: pos.column - incPrefix.length,
            endLineNumber: pos.lineNumber,
            endColumn: pos.column,
          },
        });
        return;
      }

      /* 3.5) LUA REQUIRE MODU: require(" yazarken workspace .lua modülleri
       * onerilir (gcl-lsp_lua.md 5.1/5.3). require("X") -> X.lua,
       * require("P/  -> P klasorunun icindeki .lua dosyalari. Insert
       * tirnaktan SONRASINDAN baslar — require parantezi/tirnak korunur. */
      const requireMatch =
        file.lang === "lua"
          ? beforeCursor.match(/require\s*\(\s*["']([^"']*)$/)
          : null;
      if (requireMatch) {
        const reqPrefix = requireMatch[1];
        const reqLower = reqPrefix.toLowerCase();
        let reqItems: CompletionItem[] = [];
        try {
          const info = root ? await window.ide.readProject(root) : null;
          for (const f of info?.files ?? []) {
            if (f.ext !== ".lua") continue;
            /* importName kuraliyla ayni: src/ on eki atilir; klasor varsa
             * "klasor/modul" adi uretilir (require("testFolder/helloworld")). */
            let candidate = f.name.replace(/\.lua$/i, "");
            if (f.dir && !f.dir.startsWith("src")) {
              candidate = `${f.dir.replace(/^src\//, "")}/${candidate}`;
            } else if (f.dir && f.dir !== "src") {
              candidate = `${f.dir.slice(4)}/${candidate}`;
            }
            if (!candidate.toLowerCase().startsWith(reqLower)) continue;
            reqItems.push({ label: candidate, kind: "module", detail: "lua module" });
          }
        } catch {
          /* workspace okunamadi: bos liste */
        }
        if (reqItems.length === 0) {
          setPopupBoth(null);
          return;
        }
        if (seq !== refreshSeq) return;
        reqItems.sort((a, b) => a.label.localeCompare(b.label));
        const reqXy = computePos(editor, pos);
        if (!reqXy) {
          setPopupBoth(null);
          return;
        }
        setPopupBoth({
          items: reqItems,
          index: 0,
          top: reqXy.top,
          left: reqXy.left,
          range: {
            startLineNumber: pos.lineNumber,
            startColumn: pos.column - reqPrefix.length,
            endLineNumber: pos.lineNumber,
            endColumn: pos.column,
          },
        });
        return;
      }

      /* GCL direktifleri: "#" veya "#in" yazarken #include/#lib/#extern gibi
       * on-islemci kelimeleri oner. "#" prefix olarak alinir, insert araligi
       * da "#"den itibaren surer. */
      const hashPrefix = beforeCursor.match(/#[A-Za-z]*$/)?.at(0) ?? "";
      const hashMode = hashPrefix !== "";

      /* # DIREKTIF MODU: LSP'ye GEREK YOK — headerlar statik GCL kelimeleridir.
       * gcl-lsp "#"i bir harf olarak gormedigi icin prefix'i bos sayip HER SEYI
       * donduruyordu. Burada LSP atlanir ve popup yalnizca "#" ile baslayan
       * direktifleri gosterir (gclFallback zaten "#" ile filtreler). */
      if (hashMode) {
        const hashItems = gclFallback(hashPrefix);
        if (hashItems.length === 0) {
          setPopupBoth(null);
          return;
        }
        const hashXy = computePos(editor, pos);
        if (!hashXy) {
          setPopupBoth(null);
          return;
        }
        setPopupBoth({
          items: hashItems,
          index: 0,
          top: hashXy.top,
          left: hashXy.left,
          range: {
            startLineNumber: pos.lineNumber,
            startColumn: pos.column - hashPrefix.length,
            endLineNumber: pos.lineNumber,
            endColumn: pos.column,
          },
        });
        return;
      }

      /* imlecin onunde tamamlanabilir bir kelime yoksa (bosluk / noktalama
       * sonrasi imlec) popup yalnizca force (Ctrl+Space) ile acilir */
      if (!isMember && prefix === "" && !force) {
        setPopupBoth(null);
        return;
      }

      /* LSP'den tamamlama: gcl-lsp.exe dosyayi (import + member) cozer ve
       * dosya sembollerini prefix'e gore filtreler. Sadece LSP kullanilir;
       * eski JSON kutuphane sistemi (completions.json) devre disi.
       *
       * LSP basarisiz olursa (gcl-lsp.exe bulunamadi, spawn hatasi, zaman
       * asimi vb.) Ctrl+Space YINE DE acilsin: Python'un kendi kelimelerine
       * fallback yapilir — popup asla bos kalip "acilmiyor" hissi vermez. */
      let lspItems: LspCompletionItem[];
      try {
        lspItems = await lspCompleteSafe(
          file.path,
          pos.lineNumber,
          pos.column,
          docText,
        );
      } catch {
        lspItems = [];
      }
      if (seq !== refreshSeq) return; /* daha yeni bir istek geldi */

      /* gcl-lsp #include <x> satirini gorunce x dosyasini MODULE olarak
       * completion'a veriyor — kodun icinden yazarken "test_include" adi
       * cikiyordu. O dosyanin kendisi degil ICERIGI (fonksiyonlar) burada
       * gecerlidir; asagida includedSymbols onu ekliyor. Bu yuzden LSP'den
       * gelen include-dosya module item'lari SÜZÜLUR. */
      const incNames = file.lang === "gcl" ? collectIncludeNames(docText) : [];
      const incNamesLc = new Set(incNames.map((n) => n.toLowerCase()));
      let items = lspItems
        .map((it) => fromLsp(it.kind, it.label, it.detail))
        .filter((it) => {
          if (it.kind !== "module") return true;
          const labelLc = it.label.toLowerCase();
          /* "test_include", "test_include.gcsf", "test_include.gclib",
           * "./test_include" varyantlarini da ele. */
          const base = labelLc.split(".")[0].replace(/^[\\/]+/, "");
          return !incNamesLc.has(base) && !incNamesLc.has(labelLc);
        });
      if (items.length === 0) {
        /* LSP yanit vermezse (gcl-lsp.exe yok / IPC asili / henuz index
         * yok) popup YINE DE acilir: dosya diline gore yerel kelime
         * havuzuna dusulur (GCL -> gclFallback, Lua -> luaFallback,
         * Python -> pythonFallback). Eskiden fallback yalnizca Ctrl+Space'te
         * (force) calisiyordu; normal yazimda LSP gecikirse popup aninda
         * kapanip "hicbir sey acilmiyor" hissi veriyordu. Artik her
         * durumda ya LSP ya fallback mutlaka item uretir. */
        items =
          file.lang === "gcl"
            ? gclFallback(prefix)
            : file.lang === "lua"
              ? luaFallback(prefix)
              : pythonFallback(prefix);
      }

      /* GCL: #include/#lib ile GOMULEN dosyalarin sembolleri de burada
       * gecerlidir (gcl_merge_includes dosya icerigini dogma bir karistirir).
       * LSP bunlari gormese bile (gcl-lsp import cozumlemesi embed dosyalari
       * icin tasarlandi, GCL icindeki #include icerigini sembol listesine
       * katmaz) IDE yine de onlari bulur ve prefix'e gore filtreler. */
      if (file.lang === "gcl") {
        try {
          const inc = await includedSymbols(file.path, docText, root);
          for (const s of inc) {
            if (s.label.toLowerCase().startsWith(prefix.toLowerCase())) {
              items.push(s);
            }
          }
        } catch {
          /* include okunamadi: LSP/fallback sonucu yeterli */
        }
      }
      if (seq !== refreshSeq) return;

      if (items.length === 0) {
        setPopupBoth(null);
        return;
      }

      const xy = computePos(editor, pos);
      if (!xy) {
        setPopupBoth(null);
        return;
      }

      setPopupBoth({
        items,
        index: 0,
        top: xy.top,
        left: xy.left,
        range: {
          startLineNumber: pos.lineNumber,
          startColumn: pos.column - prefix.length,
          endLineNumber: pos.lineNumber,
          endColumn: pos.column,
        },
      });
    },
    [file.path, root, setPopupBoth, computePos],
  );

  /* onMount bir kez calisir; guncel closure'lar ref uzerinden cagrilir. */
  const refreshRef = useRef<(force?: boolean) => void>(() => {});
  const applyRef = useRef<(p: PopupState) => void>(() => {});
  const saveRef = useRef(onSave);
  const runRef = useRef(onRun);
  const cursorRef = useRef(onCursor);
  saveRef.current = onSave;
  runRef.current = onRun;
  cursorRef.current = onCursor;
  refreshRef.current = (force = false) => {
    void doRefresh(force);
  };
  applyRef.current = applyItem;

  const onMount: OnMount = (editor, monacoInstance) => {
    editorRef.current = editor;
    defineGclTheme(monacoInstance, palette);

    /* kisa yollar */
    editor.addCommand(
      monacoInstance.KeyMod.CtrlCmd | monacoInstance.KeyCode.KeyS,
      () => saveRef.current(),
    );
    editor.addCommand(monacoInstance.KeyCode.F5, () => runRef.current());
    /* Ctrl+Space -> zorla tamamlama. DOM capture birincil yol; Monaco
     * addCommand yedek yol (cogu klavyede bu ikisi celismaz, cift acilim
     * doRefresh'in race guard'i sayesinde zararsizdir). */
    editor.addCommand(
      monacoInstance.KeyMod.CtrlCmd | monacoInstance.KeyCode.Space,
      () => refreshRef.current(true),
    );

    /* imlec takibi -> StatusBar + popup yenileme */
    const pos = editor.getPosition();
    if (pos) cursorRef.current(pos.lineNumber, pos.column);
    editor.onDidChangeCursorPosition((e) => {
      cursorRef.current(e.position.lineNumber, e.position.column);
      refreshRef.current();
    });

    /* Popup klavye navigasyonu — DOM seviyesinde capture: Monaco'nun kendi
     * keybinding sistemi (onKeyDown/addCommand) düz tuslarda default
     * davranisi (imlec hareketi) engelleyemez. DOM keydown'i MONACO'DAN
     * ONCE yakalanir (capture:true); preventDefault + stopImmediatePropagation
     * ile tus Monaco'ya hic ulasmadan popup navigasyonu yapilir. */
    const dom = editor.getDomNode();
    const onDomKeyDown = (e: globalThis.KeyboardEvent) => {
      /* Ctrl+Space -> zorla tamamlama. e.code "Space"; bazi klavyelerde
       * / IME durumlarinda e.code "Space" yerine " " gibi gelmez ama
       * e.key " " olabilir — ikisini de dene. */
      const isCtrlSpace =
        e.key === " " && (e.ctrlKey || e.metaKey) &&
        (e.code === "Space" ||
          e.code === "Spacebar" ||
          e.code === "" ||
          e.code === "Numpad0" ||
          !e.code);
      if (isCtrlSpace) {
        e.preventDefault();
        e.stopImmediatePropagation();
        refreshRef.current(true);
        return;
      }

      const p = popupRef.current;
      if (!p) return;

      if (e.key === "ArrowDown" || e.key === "ArrowUp") {
        e.preventDefault();
        e.stopImmediatePropagation();
        if (p.items.length === 0) {
          /* signature modu: popup'i kapat, oklar imlece doner */
          setPopupBoth(null);
          return;
        }
        const dir = e.key === "ArrowDown" ? 1 : -1;
        setPopupBoth({
          ...p,
          index: (p.index + dir + p.items.length) % p.items.length,
        });
      } else if (e.key === "Enter" || e.key === "Tab") {
        e.preventDefault();
        e.stopImmediatePropagation();
        if (p.items.length === 0) {
          setPopupBoth(null);
          return;
        }
        applyRef.current(p);
        setPopupBoth(null);
      } else if (e.key === "Escape") {
        e.preventDefault();
        e.stopImmediatePropagation();
        setPopupBoth(null);
      }
    };
    dom?.addEventListener("keydown", onDomKeyDown, { capture: true });

    editor.onDidBlurEditorText(() => setPopupBoth(null));
  };

  /* icerik her degistiginde popup'i guncelle (yazma sirasinda canli filtre) */
  useEffect(() => {
    refreshRef.current();
  }, [content]);

  /* palette degisince Monaco editor temasini guncelle */
  useEffect(() => {
    monaco.editor.defineTheme("gcl-theme", {
      base: "vs-dark",
      inherit: true,
      rules: [
        { token: "comment", foreground: palette.fg2 },
        { token: "keyword", foreground: palette.acc },
        { token: "string", foreground: palette.alt },
        { token: "number", foreground: palette.alt },
        { token: "number.float", foreground: palette.alt },
        { token: "preprocessor", foreground: palette.acc, fontStyle: "italic" },
        { token: "type.identifier", foreground: palette.acc },
        { token: "delimiter", foreground: palette.fg2 },
      ],
      colors: {
        "editor.background": palette.editorBg,
        "editor.lineHighlightBackground": palette.editorLine,
        "editorLineNumber.foreground": palette.editorLineNum,
        "editorCursor.foreground": palette.editorCursor,
        "editor.selectionBackground": palette.editorSelection,
        ...findWidgetColors(palette),
      },
    });
    monaco.editor.setTheme("gcl-theme");
  }, [palette]);

  return (
    <div className="editor-wrap" ref={wrapRef}>
      <Editor
        path={file.path}
        language={file.lang}
        theme="gcl-theme"
        value={content}
        onChange={(v) => onContent(v)}
        onMount={onMount}
        options={{
          fontSize,
          fontFamily,
          tabSize: 4,
          insertSpaces: true,
          minimap: { enabled: false },
          scrollBeyondLastLine: false,
          automaticLayout: true,
          wordWrap: "off",
          renderLineHighlight: "all",
          cursorBlinking: "smooth",
          mouseWheelZoom: true,
          padding: { top: 6 },
          /* Monaco'nun native suggest'i tamamen kapali — tamamlama bizim
           * DOM popup'imizda (AutocompletePopup). */
          quickSuggestions: false,
          suggestOnTriggerCharacters: false,
          acceptSuggestionOnEnter: "off",
          wordBasedSuggestions: "off",
          parameterHints: { enabled: false },
        }}
      />

      {/* Fonksiyon parametre gostergesi artık AutocompletePopup'in ICINDE,
       * tamamlama listesinin ALTINDA render edilir (ayri pencere degil):
       * ne yazilan satiri ne de listeyi orter. */}
      {popup && (
        <AutocompletePopup
          items={popup.items}
          index={popup.index}
          top={popup.top}
          left={popup.left}
          rowHeight={Math.round(fontSize * 1.6)}
          signature={popup.signature}
          onMouseDown={(i) => {
            const p = popupRef.current;
            if (!p) return;
            applyRef.current({ ...p, index: i });
            setPopupBoth(null);
          }}
        />
      )}
    </div>
  );
}
