import { contextBridge, ipcRenderer } from "electron";
import type { DocsEntry, IdeApi, WatchEvent } from "../src/types";

const api: IdeApi = {
  readFile: (file) => ipcRenderer.invoke("fs:read", file),
  writeFile: (file, content) => ipcRenderer.invoke("fs:write", file, content),
  fileExists: (p) => ipcRenderer.invoke("fs:exists", p),
  createFile: (file, content) => ipcRenderer.invoke("fs:createFile", file, content),
  createDir: (dir) => ipcRenderer.invoke("fs:createDir", dir),
  deleteFs: (p) => ipcRenderer.invoke("fs:delete", p),
  copyFile: (src, dst) => ipcRenderer.invoke("fs:copy", src, dst),
  renameFs: (src, dst) => ipcRenderer.invoke("fs:rename", src, dst),
  setWorkspace: (dir) => ipcRenderer.invoke("workspace:set", dir),
  dirTree: (dir) => ipcRenderer.invoke("dir:tree", dir),
  docsList: (dir) => ipcRenderer.invoke("docs:list", dir),
  findGcl: () => ipcRenderer.invoke("gcl:find"),

  /* GCL dil sunucusu (gcl-lsp.exe) */
  lspInit: () => ipcRenderer.invoke("lsp:init"),
  lspComplete: (file, line, col, text) =>
    ipcRenderer.invoke("lsp:complete", file, line, col, text),
  lspDidChange: (file) => ipcRenderer.invoke("lsp:didChange", file),
  terminalWrite: (data) => ipcRenderer.invoke("terminal:write", data),
  startShell: () => ipcRenderer.invoke("shell:start"),
  runFile: (file) => ipcRenderer.invoke("run:file", file),
  stopRun: () => ipcRenderer.invoke("stop:run"),
  buildFile: (file, mode) => ipcRenderer.invoke("build:file", file, mode),

  /* dosya açma / uygulama */
  openFileDialog: () => ipcRenderer.invoke("dialog:openFile"),
  openFolderDialog: () => ipcRenderer.invoke("dialog:openFolder"),
  quit: () => {
    ipcRenderer.invoke("app:quit");
  },

  /* proje sistemi */
  selectProjectData: () => ipcRenderer.invoke("project:selectProjectData"),
  projectSrcFiles: (dir) => ipcRenderer.invoke("project:srcFiles", dir),
  createProject: (dir, info) => ipcRenderer.invoke("project:create", dir, info),
  readProject: (dir) => ipcRenderer.invoke("project:read", dir),
  writeProject: (dir, info) => ipcRenderer.invoke("project:write", dir, info),
  exportProject: (sourceDir, info) =>
    ipcRenderer.invoke("project:export", sourceDir, info),
  onOutput: (cb) => {
    const listener = (_e: unknown, line: string) => cb(line);
    ipcRenderer.on("output:line", listener);
    return () => ipcRenderer.removeListener("output:line", listener);
  },
  onWorkspaceRoot: (cb) => {
    const listener = (_e: unknown, root: string) => cb(root);
    ipcRenderer.on("workspace:root", listener);
    return () => ipcRenderer.removeListener("workspace:root", listener);
  },
  onWorkspaceEvent: (cb) => {
    const listener = (_e: unknown, ev: WatchEvent) => cb(ev);
    ipcRenderer.on("workspace:event", listener);
    return () => ipcRenderer.removeListener("workspace:event", listener);
  },
};

contextBridge.exposeInMainWorld("ide", api);
