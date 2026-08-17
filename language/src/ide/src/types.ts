export interface FsEntry {
  name: string;
  path: string;
  dir: boolean;
}

/* DOCS panelinde listelenen dokuman girisleri (build Library'den gelen
 * .doc / .gcReference / wrapper dosyalari + kullanici Library'si).
 * lang: dil grubu (Lua / Python / bridge). group: kategori basligi. */
export interface DocsEntry {
  path: string;
  name: string;
  kind: "doc" | "ref" | "lib";
  lang: "lua" | "python" | "bridge" | "other";
  group: string;
}

export interface WatchEvent {
  event: string;
  path: string;
}

/* embed kutuphanesi taramasi sonucu: dosya basina defin listesi.
 * params: fonksiyon imzasi icin parantez ici parametre listesi
 * (Python: def f(a, b=1) -> params="a, b=1"). */
export interface EmbedDef {
  name: string;
  params: string;
}

export interface EmbedFileDefs {
  file: string;
  defs: EmbedDef[];
}

export interface EmbedScanResult {
  python: EmbedFileDefs[];
  lua: EmbedFileDefs[];
}

/* ---- GCL dil sunucusu (language/src/lsp) ---- */

/* LSP tek satirlik JSON (NDJSON) satir istekleridir; Elektron, gcl-lsp.exe'yi
 * spawn edip bu kanallarla konusur. */
export interface LspCompletionItem {
  label: string;
  kind: "fn" | "class" | "const" | "module";
  detail: string;
}

/* ---- GCL proje sistemi (Project.gcDATA) ---- */

export interface ProjectInfo {
  name: string;
  developer: string;
  useLua: boolean;
  usePython: boolean;
  version: string;
  createdAt: string;
  updatedAt: string;
}

export interface ExportResult {
  ok: boolean;
  target: string;
  message: string;
}

/* Dosya/klasor CRUD islemlerinden donebilecek sonuc (src disinda izinsiz
 * create denemesi gibi hatalar mesaj olarak doner, uygulama cokmez). */
export interface CmdResult {
  ok: boolean;
  message: string;
}

export interface IdeApi {
  readFile: (file: string) => Promise<string>;
  writeFile: (file: string, content: string) => Promise<void>;
  fileExists: (p: string) => Promise<boolean>;
  createFile: (file: string, content?: string) => Promise<CmdResult | void>;
  createDir: (dir: string) => Promise<CmdResult | void>;
  deleteFs: (p: string) => Promise<void>;
  copyFile: (src: string, dst: string) => Promise<void>;
  setWorkspace: (dir: string) => Promise<string>;
  dirTree: (dir: string) => Promise<FsEntry[]>;
  docsList: (dir: string) => Promise<DocsEntry[]>;
  findGcl: () => Promise<string>;

  /* GCL dil sunucusu (gcl-lsp.exe) */
  lspInit: () => Promise<{ ok: boolean; message?: string }>;
  lspComplete: (
    file: string,
    line: number,
    col: number,
    text: string,
  ) => Promise<LspCompletionItem[]>;
  lspDidChange: (file: string) => Promise<void>;
  terminalWrite: (data: string) => Promise<void>;
  startShell: () => Promise<void>;
  runFile: (file: string) => Promise<void>;
  buildFile: (file: string, mode: "build" | "buildRun") => Promise<void>;

  /* dosya açma / uygulama */
  openFileDialog: () => Promise<string | null>;
  openFolderDialog: () => Promise<string | null>;
  quit: () => void;

  /* proje sistemi */
  selectProjectData: () => Promise<{
    dir: string;
    info: ProjectInfo | null;
  } | null>;
  createProject: (
    dir: string,
    info: ProjectInfo,
  ) => Promise<{ ok: boolean; message: string }>;
  readProject: (dir: string) => Promise<ProjectInfo | null>;
  writeProject: (
    dir: string,
    info: ProjectInfo,
  ) => Promise<{ ok: boolean; message: string }>;
  exportProject: (
    sourceDir: string,
    info: ProjectInfo,
  ) => Promise<ExportResult>;

  onOutput: (cb: (line: string) => void) => () => void;
  onWorkspaceRoot: (cb: (root: string) => void) => () => void;
  onWorkspaceEvent: (cb: (ev: WatchEvent) => void) => () => void;
}
