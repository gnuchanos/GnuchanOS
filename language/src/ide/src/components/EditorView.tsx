import { useCallback, useEffect, useRef, useState } from "react";
import Editor, { OnMount } from "@monaco-editor/react";
import { monaco } from "../monacoSetup";
import type { editor as MonacoEditor } from "monaco-editor";
import type { Palette } from "../ideSettings";
import { fromLsp, pythonFallback, type CompletionItem } from "../completions";
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

/* gcl-theme tanimi (onMount + useEffect icin). Native suggest kullanilmasa
 * da editor bg/fg/token renkleri buradan gelir. */
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
      let items = lspItems.map((it) => fromLsp(it.kind, it.label, it.detail));
      if (items.length === 0) {
        /* LSP yanit vermezse (gcl-lsp.exe yok / IPC asili / henuz index
         * yok) popup YINE DE acilir: yerel Python kelime havuzuna dusulur.
         * Eskiden fallback yalnizca Ctrl+Space'te (force) calisiyordu;
         * normal yazimda LSP gecikirse popup aninda kapanip "hicbir sey
         * acilmiyor" hissi veriyordu. Artik her durumda ya LSP ya fallback
         * mutlaka item uretir. */
        items = pythonFallback(prefix);
        if (items.length === 0) {
          setPopupBoth(null);
          return;
        }
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
    [file.path, setPopupBoth, computePos],
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

      {/* Fonksiyon parametre gostergesi: AYRI pencere (.ac-sig-popup),
       * imlecin biraz USTUNE konumlanir ve "func(" icinde (")" / yeni satir
       * / Esc olmadigi surece) kalici durur. Tamamlama listesinden
       * tamamen bagimsizdir. */}
      {popup?.signature && (
        <div
          className="ac-sig-popup"
          style={{ top: popup.top - 64, left: Math.max(8, popup.left - 8) }}
        >
          <span className="ac-sig-label">params</span>
          <code className="ac-sig-code">{popup.signature}</code>
        </div>
      )}
      {popup && (
        <AutocompletePopup
          items={popup.items}
          index={popup.index}
          top={popup.top}
          left={popup.left}
          rowHeight={Math.round(fontSize * 1.6)}
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
