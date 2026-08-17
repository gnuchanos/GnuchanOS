import { useEffect, useRef } from "react";
import { Terminal } from "xterm";
import { FitAddon } from "xterm-addon-fit";
import "xterm/css/xterm.css";
import type { Palette } from "../ideSettings";

interface Props {
  palette: Palette;
  fontSize: number;
  fontFamily: string;
}

/* GCL shell komutlari + dosya/dizin tamamlama icin temel kelime havuzu.
 * fish benzeri: "he" + TAB -> "help"; "p" + TAB -> "pwd"; "ec" -> "echo" */
const GCL_COMMANDS = [
  "help",
  "version",
  "ls",
  "pwd",
  "cd",
  "echo",
  "clear",
  "exit",
];

/* GERCEK xterm.js terminali — todo.md'deki "xterm.js entegrasyonu" eksigini
 * kapatir. Paket ve addon'lar kuruluydu ama hic kullanilmiyordu.
 *
 * - gcl shell stdout/stderr'i `output:line` IPC kanalindan gelir (main.ts),
 *   burada dogrudan terminale yazilir (formatlanmadan). Her abone HAM ciktiyi
 *   alir; App'in "[id]" oneklemesi yalnizca kendi Output state'indе.
 * - Klavye girisleri `terminalWrite` ile gcl child stdin'ine aktarilir. */
export default function TerminalPanel({ palette, fontSize, fontFamily }: Props) {
  const containerRef = useRef<HTMLDivElement | null>(null);
  const termRef = useRef<Terminal | null>(null);
  /* SATIR MODELI: kullanici girdisi karakter karakter stdin'e GITMEZ; burada
   * birikir (fish gibi), Enter'a basinca satir + "\n" tek parca gonderilir.
   * Boylece backspace/Ctrl+C/tab shell'e karismaz; gercek satir duzenleme
   * mumkun olur (onceki tasarim her tusu gonderip gcl'nin fgets satirina
   * \x7f/\x03 gibi karakterlerin karismasina yol aciyordu). */
  const pendingLineRef = useRef("");
  /* Prompt metni: gcl> — coklu Tab adayinda satiri yeniden cizmemiz gerekir.
   * (gcl shell icin sabittir; cmd fallback'inde yalnizca gorsel varsayim.) */
  const PROMPT = "gcl> ";

  /* terminal kurulumu: bir kez */
  useEffect(() => {
    if (!containerRef.current) return;

    const term = new Terminal({
      fontFamily,
      fontSize,
      cursorBlink: true,
      convertEol: true,
      scrollback: 2000,
      theme: {
        background: palette.editorBg,
        foreground: palette.fg0,
        cursor: palette.editorCursor,
        selectionBackground: palette.editorSelection,
      },
    });
    const fit = new FitAddon();
    term.loadAddon(fit);
    term.open(containerRef.current);
    fit.fit();
    termRef.current = term;

    /* gcl ciktisini terminale yaz (ham chunk) */
    const offOutput = window.ide.onOutput((chunk) => {
      term.write(chunk);
    });
    /* SATIR MODELI KLVYE YONETIMI:
     * PIPE modunda surecler yazilan karakterleri ekrana geri yazmaz (gcl
     * fgets echo yok, cmd redirect echo yok), bu yuzden xterm girilen her
     * karakteri KENDISI yansitir ("local echo").
     *
     * Onemli fark: kullanici satiri karakter karakter gcl stdin'ine GITMEZ.
     * pendingLineRef'te birikir; yalnizca Enter'a basildiginda satir + "\n"
     * tek parca gonderilir. Boylece backspace ("\x7f"), Ctrl+C ("\x03") ve
     * Tab gibi satir duzenleme karakterleri asla gcl'nin fgets satirina
     * karismaz ("helpx\x7f" gibi kirli komutlar olusmaz). */
    const offData = term.onData((data) => {
      if (data === "\x7f" || data === "\b") {
        /* backspace: yalnizca satir bos degilse son karakteri sil */
        if (pendingLineRef.current.length > 0) {
          pendingLineRef.current = pendingLineRef.current.slice(0, -1);
          term.write("\b \b");
        }
        return;
      }
      if (data === "\x03") {
        /* Ctrl+C: giris satirini iptal et, taze prompt ciz */
        pendingLineRef.current = "";
        term.write("^C\r\n" + PROMPT);
        return;
      }
      if (data === "\r") {
        /* Enter: bekleyen satiri tam isle. xterm '\r' verir ama pipe'taki
         * shell'ler (gcl fgets, cmd redirect) satir sonu olarak '\n' ister. */
        const line = pendingLineRef.current;
        pendingLineRef.current = "";
        term.write("\r\n");
        if (line.length > 0) window.ide.terminalWrite(line + "\n");
        else window.ide.terminalWrite("\n"); /* bos satir da shell'e gider */
        return;
      }
      if (data === "\t") {
        /* FISH BENZERI TAB TAMAMLAMA:
         * - bekleyen satirin son kelimesini al
         * - GCL_COMMANDS icinde BIR eslesme varsa kalan kismi tamamla
         *   (ekrana yaz + satira ekle; shell'e Enter'da gider)
         * - birden fazla eslesme varsa adaylari alt satira listele ve
         *   prompt + satiri yeniden ciz (komut degismez)
         * - eslesme yoksa Tab'i yut */
        const word =
          pendingLineRef.current.match(/([A-Za-z_][A-Za-z0-9_]*)$/)?.[1] ?? "";
        const cands = GCL_COMMANDS.filter(
          (c) => c.startsWith(word) && c !== word,
        );
        if (cands.length === 1) {
          const rest = cands[0].slice(word.length);
          pendingLineRef.current += rest;
          term.write(rest);
          return;
        }
        if (cands.length > 1) {
          term.write("\r\n" + cands.join("  ") + "\r\n" + PROMPT + pendingLineRef.current);
          return;
        }
        return;
      }
      if (data === "\x1b") {
        /* ESC: yut (ok tuslari vb. burada islenmez) */
        return;
      }
      /* yazdirilabilir tek karakter (paste onData'ya tek parca gelebilir):
       * satira ekle + local echo yap */
      pendingLineRef.current += data;
      term.write(data);
    });

    /* container boyutu degisince fit */
    const ro = new ResizeObserver(() => {
      try {
        fit.fit();
      } catch {
        /* not attached yet */
      }
    });
    if (containerRef.current) ro.observe(containerRef.current);

    /* ---- GERCEK INTERAKTIF SHELL ----
     * Terminal sekmesi acildigi anda shell otomatik baslar: kullanici
     * butona basmak zorunda kalmaz. gcl.exe yoksa main.ts sistem kabuguna
     * (cmd/bash) devreder. 250ms gecikme, ilk "gcl shell started" ciktisinin
     * xterm'e yazilmasi icin guvenli bir pencere saglar. */
    const autoStart = setTimeout(() => {
      window.ide.startShell();
      term.focus();
    }, 250);

    return () => {
      clearTimeout(autoStart);
      offOutput();
      offData.dispose();
      ro.disconnect();
      term.dispose();
      termRef.current = null;
    };
    /* palette/font degisimi terminali yeniden kurmadan guncelle */
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, []);

  /* tema + font degisimlerini canli uygula */
  useEffect(() => {
    const term = termRef.current;
    if (!term) return;
    term.options.theme = {
      background: palette.editorBg,
      foreground: palette.fg0,
      cursor: palette.editorCursor,
      selectionBackground: palette.editorSelection,
    };
    term.options.fontFamily = fontFamily;
    term.options.fontSize = fontSize;
  }, [palette, fontSize, fontFamily]);

  return (
    <div className="terminal-panel" onClick={() => termRef.current?.focus()}>
      {/* Toolbar yok: shell mount'ta 250ms sonra otomatik baslar, terminal
       * tam yukseklik kaplar (buton + hint kaldirildi). */}
      <div className="terminal-host" ref={containerRef} />
    </div>
  );
}
