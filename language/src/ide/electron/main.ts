import { app, BrowserWindow, dialog, ipcMain, Menu } from "electron";
import * as fs from "fs";
import * as path from "path";
import { spawn, ChildProcess } from "child_process";
import * as readline from "readline";
import chokidar from "chokidar";
import type { FSWatcher } from "chokidar";
import type {
  DocsEntry,
  LspCompletionItem,
  ProjectFileInfo,
  ProjectInfo,
  WatchEvent,
} from "../src/types";

let mainWindow: BrowserWindow | null = null;
let workspaceRoot = "";
let watcher: FSWatcher | null = null;
let child: ChildProcess | null = null;

/* Shell henuz spawn edilmemisken terminale yazilan girisler burada birikir;
 * spawn tamamlaninca (50ms sonra) stdin'e yazilir. Boylece kullanicinin
 * ilk yazdiklari asla kaybolmaz. */
let pendingTerminalInput: string[] = [];

function flushPendingTerminalInput() {
  if (!child || !child.stdin || child.stdin.destroyed) return;
  for (const chunk of pendingTerminalInput) child.stdin.write(chunk);
  pendingTerminalInput = [];
}

/* ---- GCL dil sunucusu (language/src/lsp/gcl_lsp.c -> gcl-lsp.exe) ----
 * NDJSON over stdio. Her istek bir "id" tasir; yanitlar id ile eslesir.
 * gcl-lsp.exe, workspace'i indexler ve import cozumlemesini GCL KURALI ile
 * yapar (kardes .py, yan yana). */
let lspProc: ChildProcess | null = null;
let lspSeq = 0;
const lspPending = new Map<number, (r: unknown) => void>();

function lspPath(): string {
  const exeDir = path.dirname(process.execPath);
  const exe = process.platform === "win32" ? "gcl-lsp.exe" : "gcl-lsp";
  const cands = [
    /* paketli calisma: GnuChanIDE.exe -> build/windows -> yaninda gcl-lsp.exe */
    path.join(exeDir, "..", exe),
    path.join(exeDir, exe),
    /* dev calisma: cwd = language/src/ide iken build/windows/gcl-lsp.exe */
    path.join(process.cwd(), exe),
    path.join(process.cwd(), "..", "build", "windows", exe),
    path.join(process.cwd(), "..", "..", "build", "windows", exe),
    path.join(process.cwd(), "language", "build", "windows", exe),
    /* workspace'ten yukari yuru: language/build/windows ve build/windows */
    path.join(workspaceRoot, exe),
  ];
  {
    let dir = workspaceRoot;
    for (let up = 0; up < 6 && dir; up++) {
      cands.push(
        path.join(dir, "language", "build", "windows", exe),
        path.join(dir, "build", "windows", exe),
        path.join(dir, "language", "build", "gnuLinux", exe),
        path.join(dir, "gcl-lsp.exe"),
      );
      dir = path.dirname(dir);
    }
  }
  const seen = new Set<string>();
  for (const c of cands) {
    if (seen.has(c)) continue;
    seen.add(c);
    try {
      if (fs.statSync(c).isFile()) return c;
    } catch {
      /* not found */
    }
  }
  return "";
}

/* LSP sureci hazir degilse spawn et, stdout'u dinle VE workspace'i indexle.
 * initialize, spawn'dan hemen sonra gonderilir — boylece ilk completion
 * istegi geldiginde workspace hazir olur (didChange akisi da root'suz
 * kalmaz). */
async function lspEnsure(): Promise<void> {
  if (lspProc && !lspProc.stdin?.destroyed) return;
  const exe = lspPath();
  if (!exe) throw new Error("gcl-lsp not found");
  lspPending.clear();
  lspSeq = 0;
  lspProc = spawn(exe, [], { stdio: ["pipe", "pipe", "pipe"] });
  const rl = readline.createInterface({ input: lspProc.stdout! });
  rl.on("line", (line) => {
    try {
      const msg = JSON.parse(line) as { id?: number | string; result?: unknown };
      const idNum = typeof msg.id === "number" ? msg.id : Number(msg.id);
      const fn = lspPending.get(idNum);
      if (fn) {
        lspPending.delete(idNum);
        fn(msg);
      }
    } catch {
      /* malformed satir: yoksay */
    }
  });
  lspProc.on("exit", () => {
    lspProc = null;
    lspPending.clear();
  });
  /* workspace set edilmisse index'i hemen kur */
  if (workspaceRoot) {
    try {
      await lspSend(
        `{"id":"$ID$","method":"initialize","params":{"root":${JSON.stringify(
          workspaceRoot,
        )}}}\n`,
      );
    } catch {
      /* spawn basarisiz: bir sonraki istekte yeniden dene */
    }
  }
}

function lspSend(line: string): Promise<unknown> {
  return new Promise((resolve, reject) => {
    if (!lspProc || !lspProc.stdin || lspProc.stdin.destroyed) {
      reject(new Error("LSP not running"));
      return;
    }
    const id = ++lspSeq;
    lspPending.set(id, resolve);
    lspProc.stdin.write(line.replace('"$ID$"', String(id)));
  });
}

/* Workspace indexle (workspaceRoot degistiginde cagrilir). */
async function lspInit(): Promise<unknown> {
  await lspEnsure();
  return lspSend(
    `{"id":"$ID$","method":"initialize","params":{"root":${JSON.stringify(
      workspaceRoot,
    )}}}\n`,
  );
}

/* Tamamlama: textDocument/completion -> LspCompletionItem[] */
async function lspComplete(
  file: string,
  line: number,
  col: number,
  text: string,
): Promise<LspCompletionItem[]> {
  await lspEnsure();
  return lspSend(
    `{"id":"$ID$","method":"textDocument/completion","params":{` +
      `"file":${JSON.stringify(file)},` +
      `"line":${line},"col":${col},` +
      `"text":${JSON.stringify(text)}}}\n`,
  ).then((r) => {
    const res = r as { result?: unknown };
    return (res?.result as LspCompletionItem[]) || [];
  });
}

/* Dosya kaydedildi -> workspace yeniden indexlenir. */
async function lspDidChange(file: string): Promise<unknown> {
  await lspEnsure();
  return lspSend(
    `{"id":"$ID$","method":"textDocument/didChange","params":{` +
      `"file":${JSON.stringify(file)}}}\n`,
  );
}

/* Directories hidden from the Explorer: embed runtime junk and noise.
 * pyLibrary (python embed runtime: thousands of files), luaLibrary
 * (helpers), the Python runtime's Lib/ + site-packages/ were shown as
 * useless "empty" / enormous trees. */
const HIDDEN_DIRS = new Set([
  "pyLibrary",
  "luaLibrary",
  "Lib",
  "site-packages",
  "bin",
  "__pycache__",
  ".git",
  "node_modules",
]);

/* ---- GCL runner ---- */
function findGcl(): string {
  const exeDir = path.dirname(process.execPath);
  const cwd = process.cwd();
  const here = __dirname; /* dev: language/src/ide/dist-electron */
  const exe = process.platform === "win32" ? "gcl.exe" : "gcl";
  const isWinExe = process.platform === "win32";
  const candidates = [
    /* paketli calismada: exeDir = build/windows/GnuChanIDE_JUNKS
     * -> bir ust (..) = build/windows = gcl.exe'nin yani */
    path.join(exeDir, "..", exe),
    path.join(exeDir, exe),
    /* launcher cwd'yi build/windows'a set ediyor -> cwd dogrudan gcl.exe */
    path.join(cwd, exe),
    /* DEV: bu dosyadan yukari -> language/build/windows/gcl.exe
     * dist-electron (language/src/ide/dist-electron) -> language */
    path.join(here, "..", "..", "build", "windows", exe),
    path.join(here, "..", "..", "build", "gnuLinux", exe),
    /* DEV: cwd'den yukari -> <repo>/language/build/windows/gcl.exe */
    path.join(cwd, "language", "build", "windows", exe),
    path.join(cwd, "build", "windows", exe),
    path.join(cwd, "..", "build", "windows", exe),
    path.join(cwd, "..", "language", "build", "windows", exe),
    path.join(cwd, "..", "..", "language", "build", "windows", exe),
    /* workspace'ten yukari yuruyerek language/build/windows/gcl.exe ve
     * build/windows/gcl.exe'yi dene (proje disina cikan surecler icin) */
    path.join(workspaceRoot, exe),
  ];
  /* workspace root + repo root icin yukari yuru (ikisi de aranir) */
  for (const base of [workspaceRoot, "d:/GnuchanOS", "d:/GnuchanOS/language"]) {
    if (!base) continue;
    let dir = base;
    for (let up = 0; up < 6 && dir; up++) {
      candidates.push(
        path.join(dir, "language", "build", "windows", exe),
        path.join(dir, "build", "windows", exe),
        path.join(dir, "gcl.exe"),
        path.join(dir, "language", "build", "gnuLinux", "gcl"),
      );
      dir = path.dirname(dir);
    }
  }
  const seen = new Set<string>();
  for (const c of candidates) {
    if (seen.has(c)) continue;
    seen.add(c);
    try {
      if (fs.statSync(c).isFile()) return c;
    } catch {
      /* not found */
    }
  }
  return "";
}

function findBuildRoot(): string {
  const gcl = findGcl();
  if (gcl) return path.dirname(gcl);
  const exeDir = path.dirname(process.execPath);
  const cwd = process.cwd();
  const cands = [
    path.join(exeDir, "..", ".."),
    path.join(exeDir),
    path.join(cwd, "language", "build", "windows"),
    path.join(cwd, "build", "windows"),
    path.join("language", "build", "windows"),
    path.join("language", "build", "gnuLinux"),
  ];
  for (const c of cands) {
    try {
      if (fs.statSync(path.join(c, "Library", "Lua", "lua.doc")).isFile()) return c;
    } catch {
      /* not here */
    }
  }
  for (const c of cands) {
    try {
      if (fs.statSync(path.join(c, "Library")).isDirectory()) return c;
    } catch {
      /* not found */
    }
  }
  return "";
}

/* Proje acilirken eksik Library dosyalarini (doc/ref/wrapper) build
 * Library'sinden otomatik tamamlar. Elle kopyalama GEREKMEZ — eski
 * projeler (ww gibi) dahil DOCS paneli asla bos kalmaz. Var olan
 * dosyalarin uzerine YAZILMAZ. */
function ensureProjectDocs(dir: string) {
  try {
    const build = findBuildRoot();
    if (!build || !dir) return;
    const buildLib = path.join(build, "Library");
    const langs = ["Lua", "Python", "bridge"];
    for (const lang of langs) {
      const srcDir = path.join(buildLib, lang);
      let files: string[];
      try {
        if (!fs.statSync(srcDir).isDirectory()) continue;
        files = fs.readdirSync(srcDir);
      } catch {
        continue;
      }
      const dstRoot = path.join(dir, "Library", lang);
      const dstLib =
        lang === "Lua"
          ? path.join(dstRoot, "luaLibrary")
          : lang === "Python"
            ? path.join(dstRoot, "pyLibrary")
            : null;
      for (const name of files) {
        const full = path.join(srcDir, name);
        let isFile: boolean;
        try {
          isFile = fs.statSync(full).isFile();
        } catch {
          continue;
        }
        if (!isFile) continue;
        const isRef = name.endsWith(".gcReference");
        const isDoc = name.endsWith(".doc");
        const isWrapper =
          lang === "Lua"
            ? name.endsWith(".lua")
            : lang === "Python"
              ? name.endsWith(".py")
              : false;
        if (lang === "bridge") {
          if (isRef) {
            const dst = path.join(dstRoot, name);
            if (!fs.existsSync(dst)) {
              fs.mkdirSync(dstRoot, { recursive: true });
              fs.copyFileSync(full, dst);
            }
          }
          continue;
        }
        if (isRef || isDoc) {
          const dst = path.join(dstRoot, name);
          if (!fs.existsSync(dst)) {
            fs.mkdirSync(dstRoot, { recursive: true });
            fs.copyFileSync(full, dst);
          }
          if (isDoc && dstLib) {
            const dstIn = path.join(dstLib, name);
            if (!fs.existsSync(dstIn)) {
              fs.mkdirSync(dstLib, { recursive: true });
              fs.copyFileSync(full, dstIn);
            }
          }
          continue;
        }
        if (isWrapper && dstLib) {
          if (lang === "Python") {
            /* pyRaylib.py'nin cagrildigi yer Library/Python/ KOKUDUR
             * (build yerlesimi, pyraylib_py_COPY). pyLibrary/ embed
             * runtime'dir. Root'a kopyala ki DOCS + python embed onu gorsun. */
            const dstRootF = path.join(dstRoot, name);
            if (!fs.existsSync(dstRootF)) {
              fs.mkdirSync(dstRoot, { recursive: true });
              fs.copyFileSync(full, dstRootF);
            }
          } else {
            const dstIn = path.join(dstLib, name);
            if (!fs.existsSync(dstIn)) {
              fs.mkdirSync(dstLib, { recursive: true });
              fs.copyFileSync(full, dstIn);
            }
          }
        }
      }
    }
  } catch {
    /* dokuman tamamlama basarisizsa sessiz gec */
  }
}

/* Proje metadata okuma: Project.gcDATA JSON'u veya null. */
async function readProjectInfo(dir: string): Promise<ProjectInfo | null> {
  try {
    const raw = await fs.promises.readFile(
      path.join(dir, "Project.gcDATA"),
      "utf-8",
    );
    return JSON.parse(raw) as ProjectInfo;
  } catch {
    return null;
  }
}

/* Export icin izlenen script/native dosya tipleri. Embed runtime govdeleri
 * (pyLibrary/Lua Lib) HARIC tutulur — onlar paketin kendisidir. */
const SCRIPT_EXTS = [
  ".gcsf", ".gclib", ".gcl", ".lua", ".py", ".dll", ".so", ".gcdl",
];

const SCAN_SKIP_DIRS = new Set([
  "node_modules", ".git", "dist", "build", "__pycache__",
  "pyLibrary", "luaLibrary", "Lib", "site-packages", "bin",
]);

/* Eksik embed alanlarini varsayilana tamamlar ve proje kokunu tarayarak
 * info dump olusturur: tum .gcsf/.gclib/.lua/.py/.dll/.so dosyalarinin
 * yolu, turu, runtime'i, import adi + kullanici klasor listesi. */
function normalizeProjectInfo(
  info: ProjectInfo | null,
  dir: string,
): ProjectInfo | null {
  if (!info) return null;
  return {
    name: info.name || path.basename(dir),
    developer: info.developer ?? "",
    useLua: !!info.useLua,
    usePython: !!info.usePython,
    version: info.version || "0.1.0",
    createdAt: info.createdAt ?? "",
    updatedAt: info.updatedAt ?? "",
    files: info.files ?? [],
    dirs: info.dirs ?? [],
  };
}

/* Dosya uzantisindan embed runtime'i bulur (export bundle + info dump icin). */
function runtimeOfExt(ext: string): ProjectFileInfo["runtime"] {
  if (ext === ".lua") return "lua";
  if (ext === ".py") return "python";
  if (ext === ".dll" || ext === ".so" || ext === ".gcdl") return "native";
  return "gcl"; /* .gcsf / .gclib / .gcl */
}

/* Proje kokunu dolaşir; script/native dosyalari ve kullanici klasorlerini
 * "info dump" olarak listeler.
 *   import adi kurali:
 *     src/ alti : src/pyFiles/test.py -> "pyFiles.test" (src/ on eki atilir)
 *     kok       : kokteki test.py     -> "test"          (dosya adi)
 *     src disi  : Library/bridge/bridge.gcDL -> "bridge" (dosya adi)
 * gcdl_loader nativeleri de dosya adiyla bulur (bridge, lua_raylib). */
async function scanProjectFiles(
  root: string,
): Promise<{ files: ProjectFileInfo[]; dirs: string[] }> {
  const files: ProjectFileInfo[] = [];
  const dirs = new Set<string>();
  const walk = async (dir: string) => {
    let entries;
    try {
      entries = await fs.promises.readdir(dir, { withFileTypes: true });
    } catch {
      return;
    }
    for (const e of entries) {
      if (SCAN_SKIP_DIRS.has(e.name)) continue;
      const full = path.join(dir, e.name);
      if (e.isDirectory()) {
        await walk(full);
      } else {
        const extFrom = path.extname(e.name).toLowerCase();
        if (!SCRIPT_EXTS.includes(extFrom)) continue;
        const rel = path.relative(root, full).split(path.sep).join("/");
        const dirRel = rel.includes("/")
          ? rel.slice(0, rel.lastIndexOf("/"))
          : "";
        const base = e.name.slice(0, e.name.length - path.extname(e.name).length);
        const isSrcChild = dirRel === "src" || dirRel.startsWith("src/");
        let importName: string;
        if (isSrcChild) {
          const rest = dirRel === "src" ? "" : dirRel.slice(4);
          importName = (rest ? rest + "." : "") + base;
        } else {
          importName = base;
        }
        /* dosyanin klasorunu kullanici klasoru olarak kaydet (kok disinda) */
        if (dirRel) dirs.add(dirRel);
        files.push({
          path: rel,
          name: e.name,
          ext: extFrom,
          dir: dirRel,
          importName,
          kind: (extFrom === ".gcdl" ? "gcdl" : extFrom.slice(1)) as ProjectFileInfo["kind"],
          runtime: runtimeOfExt(extFrom),
        });
      }
    }
  };
  await walk(root);
  files.sort((a, b) => a.path.localeCompare(b.path));
  const sortedDirs = [...dirs].sort((a, b) => a.localeCompare(b));
  return { files, dirs: sortedDirs };
}

/* Info'ya taranmis dosya haritasini + klasor listesini baglar (info dump). */
async function withScannedFiles(info: ProjectInfo, dir: string): Promise<ProjectInfo> {
  let files: ProjectFileInfo[] = [];
  let dirs: string[] = [];
  try {
    const scanned = await scanProjectFiles(dir);
    files = scanned.files;
    dirs = scanned.dirs;
  } catch {
    files = [];
    dirs = [];
  }
  return { ...info, files, dirs };
}

/* Project.gcDATA'daki info dump'i diske gore yeniler: Meta okunur, disk
 * taze taranir (dosya konumu + import adi + runtime + klasor listesi) ve
 * guncel (updatedAt) olarak geri yazilir. Proje acilisinda, kayitta ve
 * watcher olaylarinda cagrilir — dosyalar eklense/silinse bile dump her
 * zaman diski yansitir. Yazilamazsa sessiz gecer (okuma tarafi tekrar
 * taranir). */
async function refreshProjectFileMap(dir: string): Promise<void> {
  if (!dir) return;
  const info = await readProjectInfo(dir);
  if (!info) return;
  const scanned = await withScannedFiles(info, dir);
  await fs.promises.writeFile(
    path.join(dir, "Project.gcDATA"),
    JSON.stringify({ ...scanned, updatedAt: new Date().toISOString() }, null, 2) + "\n",
    "utf-8",
  );
}

/* Watcher olaylarini debounce eder: ardisik ekle/sil patlamalarinda (bir
 * klasor tasinirken 10+ dosya) her seferinde disk taramak yerine 400ms
 * sonra TEK tarama yapar. */
let refreshTimer: NodeJS.Timeout | null = null;
function scheduleProjectRefresh(dir: string) {
  if (refreshTimer) clearTimeout(refreshTimer);
  refreshTimer = setTimeout(() => {
    refreshTimer = null;
    refreshProjectFileMap(dir).catch(() => {
      /* yazilamazsa sessiz gec */
    });
  }, 400);
}

async function collectFiles(root: string, exts: string[]): Promise<string[]> {
  const out: string[] = [];
  const walk = async (dir: string) => {
    let entries;
    try {
      entries = await fs.promises.readdir(dir, { withFileTypes: true });
    } catch {
      return;
    }
    for (const e of entries) {
      if (["node_modules", ".git", "dist", "build"].includes(e.name)) continue;
      const full = path.join(dir, e.name);
      if (e.isDirectory()) await walk(full);
      else if (exts.includes(path.extname(e.name).toLowerCase())) out.push(full);
    }
  };
  await walk(root);
  return out;
}

async function copyRel(src: string, dst: string) {
  await fs.promises.mkdir(path.dirname(dst), { recursive: true });
  await fs.promises.copyFile(src, dst);
}

/* Project Library template: copy wrapper + .gcReference + .doc files from
 * the build Library into the new project so luaLibrary/pyLibrary are never
 * empty and everything is readable inside the IDE. */
function templateLibrary(dir: string) {
  const build = findBuildRoot();
  if (!build) return;
  const buildLib = path.join(build, "Library");
  const langs = ["Lua", "Python", "bridge"];
  for (const lang of langs) {
    const srcDir = path.join(buildLib, lang);
    let files: string[];
    try {
      if (!fs.statSync(srcDir).isDirectory()) continue;
      files = fs.readdirSync(srcDir);
    } catch {
      continue;
    }
    const dstRoot = path.join(dir, "Library", lang);
    fs.mkdirSync(dstRoot, { recursive: true });
    const dstLib =
      lang === "Lua"
        ? path.join(dstRoot, "luaLibrary")
        : lang === "Python"
          ? path.join(dstRoot, "pyLibrary")
          : null;
    if (dstLib) fs.mkdirSync(dstLib, { recursive: true });

    for (const name of files) {
      const full = path.join(srcDir, name);
      let isFile: boolean;
      try {
        isFile = fs.statSync(full).isFile();
      } catch {
        continue;
      }
      if (!isFile) continue;
      const isRef = name.endsWith(".gcReference");
      const isDoc = name.endsWith(".doc");
      const isWrapper =
        lang === "Lua"
          ? name.endsWith(".lua")
          : lang === "Python"
            ? name.endsWith(".py")
            : false;
      if (lang === "bridge") {
        if (isRef) fs.copyFileSync(full, path.join(dstRoot, name));
        continue;
      }
      if (isRef || isDoc) {
        fs.copyFileSync(full, path.join(dstRoot, name));
        if (isDoc && dstLib) fs.copyFileSync(full, path.join(dstLib, name));
        continue;
      }
      if (isWrapper && dstLib) {
        if (lang === "Python") {
          /* pyRaylib.py cagrildigi yer Library/Python/ kokudur; pyLibrary/
           * embed runtime'dir. DOCS + edit icin root'a kopyala. */
          fs.copyFileSync(full, path.join(dstRoot, name));
        } else {
          fs.copyFileSync(full, path.join(dstLib, name));
        }
      }
    }
  }
}

async function buildBundles(root: string, name: string, target: string) {
  const safe = name.replace(/[^A-Za-z0-9_\- ]+/g, "").trim() || "GCLProject";
  const files = await collectFiles(root, [
    ".lua", ".py", ".gcsf", ".gclib", ".gcReference",
  ]);
  const byLang: Record<string, [string, string][]> = { py: [], lua: [] };
  for (const f of files) {
    const rel = path.relative(root, f);
    try {
      const content = await fs.promises.readFile(f, "utf-8");
      if (rel.endsWith(".py")) byLang.py.push([rel, content]);
      else if (rel.endsWith(".lua")) byLang.lua.push([rel, content]);
      else if (rel.endsWith(".gcsf") || rel.endsWith(".gclib"))
        byLang.py.push([rel, content]);
    } catch {
      /* binary - skip */
    }
  }
  const writeBundle = async (ext: string, entries: [string, string][]) => {
    if (entries.length === 0) return;
    const lines = [
      `---- GCL Bundle: ${safe} ----`,
      `#project ${safe}`,
    ];
    for (const [rel, content] of entries) {
      lines.push(`--- ${rel}`);
      lines.push(content);
    }
    await fs.promises.writeFile(
      path.join(target, `${safe}.${ext}`),
      lines.join("\n"),
      "utf-8",
    );
  };
  await writeBundle("pyBundle", byLang.py);
  await writeBundle("luaBundle", byLang.lua);
}

/* gcl/embed ciktisindaki ANSI renk/VT kodlarini temizle: Output paneli düz
 * metindir, ham ESC dizileri "[38;2;160;59;255m" gibi cop gozukur. */
function stripAnsi(s: string): string {
  // eslint-disable-next-line no-control-regex
  return s.replace(/\u001b\[[0-9;]*m/g, "");
}

function send(channel: string, payload: unknown) {
  if (mainWindow && !mainWindow.isDestroyed())
    mainWindow.webContents.send(channel, payload);
}

/* Shell ciktisini terminale gonderirken satir sonlarini normalize et:
 * gcl/cmd Windows'ta CRLF ("\r\n") basar. xterm'de tek basina "\r"
 * imleci satir basina tasir ve mevcut satiri (gcl> promptu dahil) uzerine
 * yazar — prompt "silinir". Hepsi "\n"e indirgenirse convertEol:true
 * satirlari dogru yonetir, cift satir boslugu da olusmaz. */
function sendOutput(s: string) {
  send("output:line", s.replace(/\r\n/g, "\n").replace(/\r/g, "\n"));
}

function runGcl(args: string[], cwd: string, label: string) {
  const gcl = findGcl();
  if (!gcl) {
    send("output:line", `[GnuChanIDE] gcl not found (${label})\n`);
    return;
  }
  send("output:line", `[GnuChanIDE] $ ${gcl} ${args.join(" ")}\n`);
  child?.kill();
  child = spawn(gcl, args, { cwd });
  child.stdout?.on("data", (d) => sendOutput(stripAnsi(d.toString())));
  child.stderr?.on("data", (d) => sendOutput(stripAnsi(d.toString())));
  child.on("close", (code) => {
    send("output:line", `[GnuChanIDE] ${label} exit code: ${code ?? "?"}\n`);
    child = null;
  });
}

/* Sistem kabugunu baslat: Windows'ta cmd.exe, Linux'ta $SHELL (veya bash).
 * GCL shell yokken terminalin bos kalmamasi icin fallback'tir. */
function spawnSystemShell() {
  const isWin = process.platform === "win32";
  const shell = isWin
    ? process.env.ComSpec || "cmd.exe"
    : process.env.SHELL || "/bin/bash";
  child?.kill();
  child = spawn(shell, [], { cwd: workspaceRoot || process.cwd() });
  child.stdout?.on("data", (d) => sendOutput(stripAnsi(d.toString())));
  child.stderr?.on("data", (d) => sendOutput(stripAnsi(d.toString())));
  child.on("close", (code) => {
    send(
      "output:line",
      `[GnuChanIDE] ${isWin ? "cmd" : "shell"} closed (code ${code ?? "?"})\n`,
    );
    child = null;
  });
  send(
    "output:line",
    isWin
      ? "[GnuChanIDE] system shell started (cmd.exe)\n"
      : "[GnuChanIDE] system shell started\n",
  );
}

/* GCL shell'i baslat. gcl.exe bulunamazsa (veya hemen cikarsa) terminal bos
 * kalmaz: sistem kabugu (cmd/bash) devreye girer. */
function startGclShell() {
  const gcl = findGcl();
  if (!gcl) {
    send(
      "output:line",
      "[GnuChanIDE] gcl not found — starting system shell instead\n",
    );
    spawnSystemShell();
    return;
  }
  child?.kill();
  child = spawn(gcl, [], { cwd: workspaceRoot });
  child.stdout?.on("data", (d) => sendOutput(stripAnsi(d.toString())));
  child.stderr?.on("data", (d) => sendOutput(stripAnsi(d.toString())));
  child.on("close", (code) => {
    send("output:line", `[GnuChanIDE] gcl shell closed (code ${code ?? "?"})\n`);
    child = null;
  });
  send("output:line", "[GnuChanIDE] gcl shell started\n");
  /* Spawn tamamlandi: bekleyen komutlari stdin'e yaz (ilk harfler kaybolmaz). */
  setTimeout(flushPendingTerminalInput, 50);
}

/* DOCS paneli icin Library taramasi: Explorer'in gizledigi embed runtime
 * klasorlerini (luaLibrary/pyLibrary) BILEREK gecer ve .doc / .gcReference /
 * wrapper (.py/.lua) dosyalarini toplar. Boylece build Library'sinden gelen
 * pyRaylib.py gibi wrapper'lar her zaman DOCS listesinde gorunur.
 *
 * KATEGORI + TEKIL: ayni dosya hem Library/Lua/ kokunde hem de
 * Library/Lua/luaLibrary/ altinda durdugundan (templateLibrary/her
 * proje acilisinda kopyalanir) iki kez listeleniyordu. Burada:
 *  - her kayit lang (lua/python/bridge) + group ("Lua"/"Python"/"Bridge")
 *    kategorisine ayrilir,
 *  - ayni (basename + kind) yalnizca BIR kez gosterilir (kok onceli). */
function listDocs(root: string): DocsEntry[] {
  const out: DocsEntry[] = [];
  const seen = new Set<string>();

  const langOfDir = (rel: string): DocsEntry["lang"] => {
    const head = rel.split("/")[0]?.toLowerCase() ?? "";
    if (head === "lua") return "lua";
    if (head === "python") return "python";
    if (head === "bridge") return "bridge";
    return "other";
  };

  const groupOfLang = (lang: DocsEntry["lang"]): string => {
    switch (lang) {
      case "lua": return "Lua";
      case "python": return "Python";
      case "bridge": return "Bridge";
      default: return "Other";
    }
  };

  /* Dosya ADINDAN dil tespiti: proje Library'sinin KOKUNDE duran kopyalar
   * (Library/lua.doc, Library/pyRaylib.py gibi) icin — klasor basligi
   * olmadigindan langOfDir "other" derdi. Ad uyumu sayesinde bunlar dogru
   * dil grubuna duser ve ayni (kind,name) dedupe onlari eler. */
  const langOfName = (n: string): DocsEntry["lang"] => {
    const lower = n.toLowerCase();
    if (lower.startsWith("lua")) return "lua";
    if (lower.startsWith("py") || lower.startsWith("python")) return "python";
    if (lower.startsWith("bridge")) return "bridge";
    return "other";
  };

  const addFile = (full: string, display: string, rel: string) => {
    const n = path.basename(full);
    let kind: DocsEntry["kind"] | null = null;
    if (n.endsWith(".doc")) kind = "doc";
    else if (n.endsWith(".gcReference")) kind = "ref";
    else if (n.endsWith(".py") || n.endsWith(".lua")) kind = "lib";
    if (!kind) return;

    /* TEKILLIK ANAHTARI ARTIK DILDEN BAGIMSIZ: ayni (tür, dosya-adi)
     * nerede bulunursa bulunsun yalnizca BIR kez listelenir. "Other"
     * tekrarlarinin kaynagi buydu — Library koku + Library/Lua kopyalari
     * farkli lang oldugu icin ayri kayitlardi. Ilk karsilasilan konum
     * kazanir (siralama klasordur -> dil klasoru once islenir). */
    const dedupeKey = `${kind}|${n}`;
    if (seen.has(dedupeKey)) return;
    seen.add(dedupeKey);

    /* lang: once klasor basligi; kok dosyasiysa (klasor yok) ADINDAN. */
    const head = rel.split("/")[0]?.toLowerCase() ?? "";
    const lang: DocsEntry["lang"] =
      head === "lua" || head === "python" || head === "bridge"
        ? head
        : langOfName(n);
    out.push({
      path: full,
      name: n,
      kind,
      lang,
      group: groupOfLang(lang),
    });
  };

  const walk = (dir: string, prefix: string) => {
    let items;
    try {
      items = fs.readdirSync(dir, { withFileTypes: true });
    } catch {
      return;
    }
    items.sort((a, b) => a.name.localeCompare(b.name));
    for (const it of items) {
      if (it.name === ".git" || it.name === "node_modules") continue;
      const full = path.join(dir, it.name);
      const rel = prefix ? `${prefix}/${it.name}` : it.name;
      if (it.isDirectory()) {
        /* pyLibrary/Lib/site-packages gibi devasa runtime govdesine girme;
         * wrapper'lar module kokunde durur. */
        if (it.name === "Lib" || it.name === "site-packages") continue;
        walk(full, rel);
      } else {
        addFile(full, rel, rel);
      }
    }
  };
  if (root) walk(root, "");
  return out;
}

function listTree(dir: string): Record<string, unknown>[] {
  const entries: { name: string; path: string; dir: boolean }[] = [];
  try {
    const items = fs.readdirSync(dir, { withFileTypes: true });
    items.sort((a, b) => {
      if (a.isDirectory() !== b.isDirectory()) return a.isDirectory() ? -1 : 1;
      return a.name.localeCompare(b.name);
    });
    for (const it of items) {
      /* hide embed runtime junk from the Explorer */
      if (it.isDirectory() && HIDDEN_DIRS.has(it.name)) continue;
      entries.push({
        name: it.name,
        path: path.join(dir, it.name),
        dir: it.isDirectory(),
      });
    }
  } catch {
    /* unreadable dir */
  }
  return entries;
}

function attachWatcher() {
  watcher?.close();
  watcher = null;
  if (!workspaceRoot) return;
  /* src/ altindaki dosya olusturma/silme (Exporer yenilemesi) dahil tum
   * degisiklikleri izle. depth:0 iken src/test.py olusturulunca Explorer
   * HICBIR event almiyordu ("test.py gozukmuyor" hatasi). Devasa embed
   * runtime govdeleri (pyLibrary/binlerce dosya) yok sayilir; zaten
   * Explorer'da gizli. */
  watcher = chokidar.watch(workspaceRoot, {
    ignored: [
      /node_modules/,
      /\.git/,
      /dist/,
      /build/,
      /__pycache__/,
      /[\\/]pyLibrary[\\/]/,
      /[\\/]luaLibrary[\\/]/,
      /[\\/]Lib[\\/]/,
      /[\\/]site-packages[\\/]/,
    ],
    ignoreInitial: true,
  });
  watcher.on("all", (event: string, filePath: string) => {
    const ev: WatchEvent = { event, path: filePath };
    send("workspace:event", ev);
    /* Script/native dosya eklenip silindiginde Project.gcDATA'daki info
     * dump (files + import adlari + dirs) debounce'lu yenilenir. Boylece
     * dosya haritasi her zaman diskle esit kalir. */
    if (
      (event === "add" || event === "unlink") &&
      SCRIPT_EXTS.includes(path.extname(filePath).toLowerCase())
    ) {
      scheduleProjectRefresh(workspaceRoot);
    }
    /* LSP workspace'ini CANLI tut: dosya ekle/sil/kaydet oldugunda
     * gcl-lsp.exe'ye textDocument/didChange gonderilir ve LSP diski
     * yeniden indexler. IDE acikken eklenen yeni klasor + .py dosyalari
     * boylece otomatik tamamlamaya girer — "yeni script gorunmuyor"
     * sorununun kok kaynagi bu eksikti. */
    if (
      SCRIPT_EXTS.includes(path.extname(filePath).toLowerCase()) &&
      (event === "add" || event === "unlink" || event === "change")
    ) {
      lspDidChange(filePath).catch(() => {
        /* gcl-lsp yoksa sessiz gec */
      });
    }
  });
}

/* Pencere ikonu (title bar + taskbar): asagidaki sirayla aranir.
 * 1. electron-builder extraResources -> resources/logo.ico (paketli calisma)
 * 2. repo kokundeki assets/logo.ico   (dev calisma)
 * Yol yoksa undefined doner; Windows icin exe'ye gomulu ikon kullanilir. */
function resolveWindowIcon(): string | undefined {
  /* Title bar / taskbar ikonu: once paketli runtime'da electron-builder'in
   * extraResources ile kopyaladigi resources/ altina, sonra repo kokundeki
   * assets/ altina bakar. Hicbiri yoksa undefined doner (Electron'un
   * varsayilan "elektron" logosu gorunur) — bu yuzden logo.ico disinda
   * logo.png / icon.png (kullanici avatari) da denenir. */
  const cands = [
    /* paketli calisma: resources/logo.ico (extraResources) */
    path.join(process.resourcesPath, "logo.ico"),
    path.join(process.resourcesPath, "logo.png"),
    path.join(process.resourcesPath, "icon.png"),
    /* dev calisma: repo kokundeki assets/ (dist-electron -> 4 ust) */
    path.join(__dirname, "..", "..", "..", "..", "assets", "logo.ico"),
    path.join(__dirname, "..", "..", "..", "..", "assets", "icon.png"),
    path.join(__dirname, "..", "..", "..", "..", "assets", "logo.png"),
  ];
  const seen = new Set<string>();
  for (const c of cands) {
    if (seen.has(c)) continue;
    seen.add(c);
    try {
      if (fs.statSync(c).isFile()) return c;
    } catch {
      /* not there */
    }
  }
  return undefined;
}

function createWindow() {
  mainWindow = new BrowserWindow({
    width: 1280,
    height: 800,
    title: "GnuChanIDE",
    autoHideMenuBar: true,
    /* GnuchanOS tek ikonu: assets/logo.ico (title bar + taskbar) */
    icon: resolveWindowIcon(),
    webPreferences: {
      preload: path.join(__dirname, "preload.cjs"),
      contextIsolation: true,
      nodeIntegration: false,
    },
  });
  mainWindow.removeMenu();
  /* Clear the disk cache so a freshly packaged renderer is always used
   * instead of a stale dist/index.html from a previous build. */
  mainWindow.webContents.session.clearCache().catch(() => {
    /* ignore */
  });

  if (process.env.VITE_DEV_SERVER_URL) {
    mainWindow.loadURL(process.env.VITE_DEV_SERVER_URL);
  } else {
    mainWindow.loadFile(path.join(__dirname, "..", "dist", "index.html"), {
      query: { v: Date.now().toString() },
    });
  }
  mainWindow.webContents.on("did-finish-load", () => {
    send("workspace:root", workspaceRoot);
  });
}

function setupIpc() {
  ipcMain.handle("fs:read", async (_e, file: string) => {
    return fs.promises.readFile(file, "utf-8");
  });
  ipcMain.handle("fs:write", async (_e, file: string, content: string) => {
    await fs.promises.writeFile(file, content, "utf-8");
  });
  ipcMain.handle("fs:exists", async (_e, p: string) => {
    try {
      return fs.statSync(p).isFile();
    } catch {
      return false;
    }
  });
  /* ---- fs/dir CRUD (Explorer context menu) ----
   * Dosya/klasor olusturmak yalnizca projenin src/ altinda serbest. */
  const isInSrc = (p: string) => {
    if (!workspaceRoot) return false;
    const norm = path.normalize(p);
    const root = path.normalize(workspaceRoot);
    const src = path.join(root, "src");
    return norm.startsWith(src + path.sep) || norm === src;
  };
  ipcMain.handle("fs:createFile", async (_e, file: string, content?: string) => {
    if (!isInSrc(file)) {
      return { ok: false, message: "Only create files inside the project src/" };
    }
    await fs.promises.mkdir(path.dirname(file), { recursive: true });
    await fs.promises.writeFile(file, content ?? "", "utf-8");
    return { ok: true };
  });
  ipcMain.handle("fs:createDir", async (_e, dir: string) => {
    if (!isInSrc(dir)) {
      return { ok: false, message: "Only create folders inside the project src/" };
    }
    await fs.promises.mkdir(dir, { recursive: true });
    return { ok: true };
  });
  ipcMain.handle("fs:delete", async (_e, p: string) => {
    const st = await fs.promises.stat(p);
    if (st.isDirectory())
      await fs.promises.rm(p, { recursive: true, force: true });
    else await fs.promises.unlink(p);
  });
  ipcMain.handle("fs:copy", async (_e, src: string, dst: string) => {
    await copyRel(src, dst);
  });
  ipcMain.handle("workspace:set", async (_e, dir: string) => {
    workspaceRoot = dir;
    /* eksik dokumanlari otomatik tamamla (lua.doc/py.doc) */
    ensureProjectDocs(dir);
    /* Proje acilisinda disk taranir: tum script/native dosyalarin
     * konumu + import adi + runtime bilgisi (info dump) Project.gcDATA'ya
     * yazilir. Boylece "test.py nerede, nasil import edilir" her zaman
     * guncel. */
    await refreshProjectFileMap(dir).catch(() => {
      /* yazilamazsa sessiz gec (read tarafi tekrar taranir) */
    });
    attachWatcher();
    /* Eski LSP index'ini duser, yeni root ile yeniden baslatir. */
    lspProc?.kill();
    lspProc = null;
    lspPending.clear();
    lspInit().catch(() => {
      /* gcl-lsp yoksa completion fallback'e duser */
    });
    /* Push the new root to the renderer immediately so the UI switches
     * right after a project is created (fixes "nothing happens"). */
    send("workspace:root", workspaceRoot);
    return workspaceRoot;
  });
  ipcMain.handle("dir:tree", async (_e, dir: string) => {
    if (!dir) return [];
    return listTree(dir);
  });
  /* DOCS paneli: Library'yi gizli klasorler dahil tara (wrapper'lar gorunsun) */
  ipcMain.handle("docs:list", async (_e, dir: string) => {
    if (!dir) return [];
    return listDocs(path.join(dir, "Library"));
  });
  /* LSP disinda hicbir statik tamamlama kaynagi yoktur; tamamlama
   * yalnizca gcl-lsp.exe'den gelir. */
  ipcMain.handle("gcl:find", async () => findGcl());

  /* ---- GCL dil sunucusu (gcl-lsp.exe) ---- */
  ipcMain.handle("lsp:init", async () => {
    try {
      await lspInit();
      return { ok: true };
    } catch (err) {
      return { ok: false, message: (err as Error).message };
    }
  });
  ipcMain.handle(
    "lsp:complete",
    async (_e, file: string, line: number, col: number, text: string) => {
      try {
        return await lspComplete(file, line, col, text);
      } catch {
        return [];
      }
    },
  );
  ipcMain.handle("lsp:didChange", async (_e, file: string) => {
    try {
      await lspDidChange(file);
    } catch {
      /* gcl-lsp yoksa sessiz gec */
    }
  });
  ipcMain.handle("terminal:write", (_e, data: string) => {
    /* Terminal acildiginda duz yazmaya baslar baslamaz shell'i hazirla:
     * GCL (veya yoksa sistem) kabugu yoksa o an baslatilir. Spawn henuz
     * hazir degilse giris KUYRUKTA bekler; spawn biter bitmez topluca
     * stdin'e yazilir — ilk harfler asla kaybolmaz.
     *
     * SIRALAMA KORUMASI: flush (50ms) tamamlanana kadar tum girisler
     * kuyrukta tutulur; kuyruk bos degilken direkt stdin'e yazilmaz.
     * Aksi halde 1. harf kuyrukta beklerken 2. harf direkt gider ve
     * "ba" seklinde ters siralanabilir. */
    if (
      !child ||
      !child.stdin ||
      child.stdin.destroyed ||
      pendingTerminalInput.length > 0
    ) {
      if (pendingTerminalInput.length === 0) startGclShell();
      pendingTerminalInput.push(data);
      return;
    }
    child.stdin.write(data);
  });
  ipcMain.handle("shell:start", () => startGclShell());
  ipcMain.handle("dialog:openFile", async () => {
    const r = await dialog.showOpenDialog(mainWindow!, {
      title: "Open file",
      properties: ["openFile"],
      filters: [
        {
          name: "GCL / Lua / Python",
          extensions: ["gcsf", "gclib", "gcl", "lua", "py"],
        },
      ],
    });
    return r.canceled || r.filePaths.length === 0 ? null : r.filePaths[0];
  });
  /* ---- workspace: herhangi bir klasoru workspace olarak ac ---- */
  ipcMain.handle("dialog:openFolder", async () => {
    const r = await dialog.showOpenDialog(mainWindow!, {
      title: "Open folder",
      properties: ["openDirectory", "createDirectory"],
    });
    return r.canceled || r.filePaths.length === 0 ? null : r.filePaths[0];
  });
  ipcMain.handle("app:quit", () => {
    app.quit();
  });
  ipcMain.handle("run:file", async (_e, file: string) => {
    const ext = path.extname(file).toLowerCase();
    const cwd = path.dirname(file);
    if (ext === ".lua") runGcl(["-luarun", file], cwd, "lua");
    else if (ext === ".py") runGcl(["-pyrun", file], cwd, "python");
    else if (ext === ".gcsf" || ext === ".gclib" || ext === ".gcl")
      runGcl(["-run", file], cwd, "gcl");
    else send("output:line", `[GnuChanIDE] No runner for (.${ext})\n`);
  });

  /* Calisan gcl/embed surecini durdurur (Run/Build & Run'un yanindaki
   * Stop butonu). child zaten runGcl'de eski sureci olduruyor; bu handler
   * kullanici istedigi anda ayni kill'i tetikler. */
  ipcMain.handle("stop:run", () => {
    if (!child) {
      send("output:line", "[GnuChanIDE] No running process\n");
      return;
    }
    const pid = child.pid;
    child.kill();
    send(
      "output:line",
      `[GnuChanIDE] process stopped (pid ${pid ?? "?"})\n`,
    );
  });

  ipcMain.handle(
    "build:file",
    async (_e, file: string, mode: "build" | "buildRun") => {
      const ext = path.extname(file).toLowerCase();
      const cwd = path.dirname(file);
      if (ext === ".gcsf" || ext === ".gclib" || ext === ".gcl") {
        if (mode === "build") runGcl([file], cwd, "gcl build");
        else runGcl([file, "-o", path.join(cwd, "out"), "-run"], cwd, "gcl build+run");
      } else if (ext === ".lua") {
        runGcl(["-luarun", file], cwd, "lua embed");
      } else if (ext === ".py") {
        runGcl(["-pyrun", file], cwd, "python embed");
      } else {
        send("output:line", `[GnuChanIDE] No build for (.${ext})\n`);
      }
    },
  );

  /* ---- GCL project system (Project.gcDATA) ---- */
  /* "Open Project": kullanici bir KLASOR (proje kokune) veya dogrudan
   * Project.gcDATA dosyasini secer. Klasor secilirse icindeki
   * Project.gcDATA otomatik okunur; dosya secilirse parent'i klasor
   * kabul edilir. Eski projeler eksik alanlarla (createdAt gerekmez)
   * normalize edilerek doner — info null olsa bile workspace yine acilir. */
  ipcMain.handle("project:selectProjectData", async () => {
    const r = await dialog.showOpenDialog(mainWindow!, {
      title: "Open project folder (Project.gcDATA auto-detected)",
      properties: ["openDirectory", "createDirectory", "openFile"],
    });
    if (r.canceled || r.filePaths.length === 0) return null;
    const picked = r.filePaths[0];
    let st: fs.Stats;
    try {
      st = await fs.promises.stat(picked);
    } catch {
      return null;
    }
    const dir = st.isFile() ? path.dirname(picked) : picked;
    let info: ProjectInfo | null = normalizeProjectInfo(
      await readProjectInfo(dir),
      dir,
    );
    return { dir, info };
  });

  ipcMain.handle(
    "project:create",
    async (_e, dir: string, info: ProjectInfo) => {
      try {
        await fs.promises.mkdir(dir, { recursive: true });

        const metaPath = path.join(dir, "Project.gcDATA");
        if (fs.existsSync(metaPath)) {
          return {
            ok: false,
            message: "Project.gcDATA already exists in this folder",
          };
        }
        const now = new Date().toISOString();
        const meta: ProjectInfo = normalizeProjectInfo(info, dir) ?? info;
        meta.createdAt = now;
        meta.updatedAt = now;
        await fs.promises.writeFile(
          metaPath,
          JSON.stringify(meta, null, 2) + "\n",
          "utf-8",
        );
        /* skeleton dirs */
        await fs.promises.mkdir(path.join(dir, "src"), { recursive: true });
        await fs.promises.mkdir(
          path.join(dir, "Library", "Lua", "luaLibrary"),
          { recursive: true },
        );
        await fs.promises.mkdir(
          path.join(dir, "Library", "Python", "pyLibrary"),
          { recursive: true },
        );
        /* Library template: refs + wrappers + docs (never empty) */
        templateLibrary(dir);
        /* skeleton main file */
        const mainEntry = path.join(dir, "src", "main.gcsf");
        if (!fs.existsSync(mainEntry)) {
          await fs.promises.writeFile(
            mainEntry,
            `#// ${info.name}\n#// ${info.developer}\n\nint main(void) {\n    printf("Hello GCL!\\n");\n    return 0;\n}\n`,
            "utf-8",
          );
        }
        return { ok: true, message: `Project created: ${info.name}` };
      } catch (err) {
        return {
          ok: false,
          message: `Could not create project: ${(err as Error).message}`,
        };
      }
    },
  );

  /* Proje acildiginda src/ icindeki script dosyalarini dondurur (main.gcsf
   * dahil). App.tsx bunlari acik tab olarak geri yukler — "acik kalan
   * scriptler gene acilir". */
  ipcMain.handle("project:srcFiles", async (_e, dir: string) => {
    if (!dir) return [];
    return collectFiles(path.join(dir, "src"), [
      ".gcsf", ".gclib", ".gcl", ".lua", ".py",
    ]);
  });

  ipcMain.handle("project:read", async (_e, dir: string) => {
    return normalizeProjectInfo(await readProjectInfo(dir), dir);
  });

  ipcMain.handle(
    "project:write",
    async (_e, dir: string, info: ProjectInfo) => {
      try {
        /* Renderer'dan gelen 'files' STALE OLABILIR (kullanici diskin
         * icindeki dosyalari IDE disinda degistirmis olabilir). Kayit
         * oncesi disk YENIDEN taranir; info dump (dosya konumu + import
         * adi + runtime + klasor listesi) her zaman guncel diski yansitir. */
        const meta: ProjectInfo = {
          ...info,
          files: info.files ?? [],
        };
        await refreshProjectFileMap(dir);
        return {
          ok: true,
          message: `Project updated (${meta.files?.length ?? 0} files in map)`,
        };
      } catch (err) {
        return {
          ok: false,
          message: `Could not save: ${(err as Error).message}`,
        };
      }
    },
  );

  ipcMain.handle(
    "project:export",
    async (_e, sourceDir: string, info: ProjectInfo) => {
      try {
        const r = await dialog.showOpenDialog(mainWindow!, {
          title: "Choose export target",
          properties: ["openDirectory", "createDirectory"],
        });
        if (r.canceled || r.filePaths.length === 0) {
          return { ok: false, target: "", message: "Cancelled" } as const;
        }
        const targetRoot = path.join(
          r.filePaths[0],
          info.name.replace(/[^A-Za-z0-9_\- ]+/g, "").trim() || "GCLProject",
        );
        await fs.promises.mkdir(targetRoot, { recursive: true });

        const ignore = new Set([
          "node_modules",
          ".git",
          "dist",
          "build",
        ]);
        const copyDir = async (from: string, to: string) => {
          const entries = await fs.promises.readdir(from, {
            withFileTypes: true,
          });
          for (const e of entries) {
            if (ignore.has(e.name)) continue;
            const s = path.join(from, e.name);
            const d = path.join(to, e.name);
            if (e.isDirectory()) {
              await fs.promises.mkdir(d, { recursive: true });
              await copyDir(s, d);
            } else {
              await fs.promises.copyFile(s, d);
            }
          }
        };
        await copyDir(sourceDir, targetRoot);

        /* Export manifesti = embed'lere INFO DUMP. Lua/Python embed sistemleri
         * ayri bir butundur; gelen proje hangi dosyanin nerede oldugunu ve
         * nasil import edilecegini bu dosyadan ogrenir:
         *   - project.files : tum .gcsf/.gclib/.lua/.py/.dll/.so dosyalari
         *                     (goreli yol + importName + kind + runtime)
         *   - importMap    : yol -> import adi (hizli arama)
         *   - dirs         : .gcBundle icine gomulecek kullanici klasorleri
         *   - embedRuntime : hangi embed runtime'lari pakette tasinir */
        const scanned = await withScannedFiles(info, sourceDir);
        const importMap: Record<string, string> = {};
        for (const f of scanned.files ?? []) importMap[f.path] = f.importName;
        const manifest = {
          project: scanned,
          exportedAt: new Date().toISOString(),
          importMap,
          dirs: scanned.dirs ?? [],
          embedRuntime: {
            lua: info.useLua ? "Library/Lua/luaLibrary" : null,
            python: info.usePython ? "Library/Python/pyLibrary" : null,
          },
          note:
            "GCL project export. project.files (path/importName/kind/runtime) " +
            "+ dirs, lua/python embed'lerinin dosyalari nerede bulacagini " +
            "ve nasil import edecegini gosterir.",
        };
        await fs.promises.writeFile(
          path.join(targetRoot, "EXPORT_INFO.json"),
          JSON.stringify(manifest, null, 2) + "\n",
          "utf-8",
        );

        await buildBundles(sourceDir, info.name, targetRoot);

        return {
          ok: true,
          target: targetRoot,
          message: `Export done: ${targetRoot}`,
        } as const;
      } catch (err) {
        return {
          ok: false,
          target: "",
          message: `Export failed: ${(err as Error).message}`,
        } as const;
      }
    },
  );
}

app.whenReady().then(() => {
  Menu.setApplicationMenu(null);
  createWindow();
  setupIpc();
  attachWatcher();
  app.on("activate", () => {
    if (BrowserWindow.getAllWindows().length === 0) createWindow();
  });
});

app.on("window-all-closed", () => {
  child?.kill();
  app.quit();
});
