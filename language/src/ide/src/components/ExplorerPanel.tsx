import { useEffect, useState, type MouseEvent } from "react";
import { FilePlus, FolderInput, FolderTree, RefreshCw, Trash2 } from "lucide-react";
import type { FsEntry } from "../types";

interface Props {
  root: string;
  onOpen: (path: string) => void;
  refreshKey: number;
}

/* Context menu durumu: hangi yolda hangi islem */
interface Ctx {
  x: number;
  y: number;
  path: string;
  isDir: boolean;
}

export default function ExplorerPanel({ root, onOpen, refreshKey }: Props) {
  const [entries, setEntries] = useState<FsEntry[] | null>(null);
  const [dirs, setDirs] = useState<Record<string, FsEntry[]>>({});
  const [ctx, setCtx] = useState<Ctx | null>(null);
  const [prompt, setPrompt] = useState<{
    label: string;
    baseDir: string;
    isDir: boolean;
    placeholder: string;
  } | null>(null);
  const [promptValue, setPromptValue] = useState("");
  const [promptError, setPromptError] = useState("");

  /* sag tik menusu: disari tiklayinca veya Esc ile kapan */
  useEffect(() => {
    if (!ctx) return;
    const onDown = (e: globalThis.MouseEvent) => {
      const target = e.target as HTMLElement | null;
      if (target && target.closest(".ctx-menu")) return;
      setCtx(null);
    };
    const onKey = (e: KeyboardEvent) => {
      if (e.key === "Escape") setCtx(null);
    };
    window.addEventListener("mousedown", onDown);
    window.addEventListener("keydown", onKey);
    return () => {
      window.removeEventListener("mousedown", onDown);
      window.removeEventListener("keydown", onKey);
    };
  }, [ctx]);

  const load = async () => {
    if (!root) return;
    const top = await window.ide.dirTree(root);
    setEntries(top);
  };

  /* root degisince butun agac sifirlanir; ama refreshKey (chokidar event)
   * geldiginde SADECE top-level yenilenir — acik dizinler kapali kalir.
   * Boylece disaridan yapilan degisiklikte kullanici agaci kaybetmez. */
  useEffect(() => {
    load();
    setDirs({});
    /* root degisince sifirla */
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [root]);

  useEffect(() => {
    if (!root) return;
    /* refreshKey her degistiginde top listeyi tazele (acik dizinleri koru) */
    load();
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [refreshKey]);

  const toggleDir = async (path: string) => {
    if (dirs[path]) {
      const next = { ...dirs };
      delete next[path];
      setDirs(next);
    } else {
      const items = await window.ide.dirTree(path);
      setDirs((d) => ({ ...d, [path]: items }));
    }
  };

  const ensureOpen = async (path: string) => {
    if (!dirs[path]) {
      const items = await window.ide.dirTree(path);
      setDirs((d) => ({ ...d, [path]: items }));
    }
  };

  const closeMenu = () => {
    setCtx(null);
  };

  const onCtx = (e: MouseEvent, path: string, isDir: boolean) => {
    e.preventDefault();
    e.stopPropagation();
    setCtx({ x: e.clientX, y: e.clientY, path, isDir });
    if (isDir) ensureOpen(path);
  };

  const doCreate = async () => {
    if (!prompt) return;
    const name = promptValue.trim();
    if (!name) return;
    const target = joinPath(prompt.baseDir, name);
    let res;
    try {
      if (prompt.isDir) res = await window.ide.createDir(target);
      else res = await window.ide.createFile(target);
    } catch {
      setPromptError("Create failed.");
      return;
    }
    if (res && res.ok === false) {
      setPromptError(res.message ?? "Create failed.");
      return;
    }
    await refreshAround(prompt.baseDir);
    setPrompt(null);
    setPromptValue("");
    setPromptError("");
  };

  /* islem sonrasi ilgili dizini yeniden oku */
  const refreshAround = async (dirPath: string) => {
    const items = await window.ide.dirTree(dirPath);
    setDirs((d) => ({ ...d, [dirPath]: items }));
    if (dirPath === root) setEntries(items);
  };

  const doDelete = async (p: string, parent: string) => {
    try {
      await window.ide.deleteFs(p);
      await refreshAround(parent);
      closeMenu();
    } catch {
      closeMenu();
    }
  };

  const doCopy = async (p: string, parent: string) => {
    const name = p.split(/[\\/]/).pop() ?? "file";
    const dst = joinPath(parent, `copy_${name}`);
    try {
      await window.ide.copyFile(p, dst);
      await refreshAround(parent);
      closeMenu();
    } catch {
      closeMenu();
    }
  };

  const openPrompt = (baseDir: string, isDir: boolean) => {
    setPrompt({
      label: isDir ? "New Folder" : "New File",
      baseDir,
      isDir,
      placeholder: isDir ? "folder_name" : "file.gcsf",
    });
    setPromptValue("");
    setPromptError("");
    closeMenu();
  };

  if (!entries) return <div className="panel-empty">...</div>;

  return (
    <div className="explorer" onContextMenu={(e) => e.preventDefault()}>
      <div className="panel-header">
        <span>
          <FolderTree size={14} /> EXPLORER
        </span>
        <div className="panel-actions">
          <button className="icon-btn" onClick={() => openPrompt(root, false)} title="New File">
            <FilePlus size={13} />
          </button>
          <button className="icon-btn" onClick={() => openPrompt(root, true)} title="New Folder">
            <FolderInput size={13} />
          </button>
          <button className="icon-btn" onClick={() => load()} title="Refresh">
            <RefreshCw size={13} />
          </button>
        </div>
      </div>

      {entries.length === 0 && (
        <div className="panel-empty">Folder is empty.</div>
      )}

      <div className="tree">
        {entries
          .filter((e) => e.dir || !isDocHidden(e.name))
          .map((e) => (
            <DirNode
              key={e.path}
              entry={e}
              openDirs={dirs}
              onToggle={toggleDir}
              onOpen={onOpen}
              onCtx={onCtx}
            />
          ))}
      </div>

      {/* context menu */}
      {ctx && (
        <div
          className="ctx-menu"
          style={{ left: ctx.x, top: ctx.y }}
          onClick={(e) => e.stopPropagation()}
        >
          <button
            className="ctx-item"
            onClick={() => openPrompt(ctx.isDir ? ctx.path : parentOf(ctx.path), false)}
          >
            <FilePlus size={12} /> New File
          </button>
          <button
            className="ctx-item"
            onClick={() => openPrompt(ctx.isDir ? ctx.path : parentOf(ctx.path), true)}
          >
            <FolderInput size={12} /> New Folder
          </button>
          <button
            className="ctx-item"
            onClick={() => doCopy(ctx.path, ctx.isDir ? parentOf(ctx.path) : parentOf(ctx.path))}
          >
            <FolderTree size={12} /> Copy
          </button>
          <button
            className="ctx-item danger"
            onClick={() => doDelete(ctx.path, ctx.isDir ? parentOf(ctx.path) : parentOf(ctx.path))}
          >
            <Trash2 size={12} /> Delete
          </button>
        </div>
      )}

      {/* new name prompt */}
      {prompt && (
        <div className="prompt-overlay" onClick={() => setPrompt(null)}>
          <div className="prompt" onClick={(e) => e.stopPropagation()}>
            <div className="prompt-label">{prompt.label} in {prompt.baseDir.split(/[\\/]/).pop()}</div>
            <input
              className="modal-input"
              autoFocus
              value={promptValue}
              placeholder={prompt.placeholder}
              onChange={(e) => {
                setPromptValue(e.target.value);
                setPromptError("");
              }}
              onKeyDown={(e) => {
                if (e.key === "Enter") doCreate();
                if (e.key === "Escape") setPrompt(null);
              }}
            />
            {promptError && <div className="modal-error">{promptError}</div>}
            <div className="prompt-actions">
              <button className="menu-btn" onClick={() => setPrompt(null)}>
                Cancel
              </button>
              <button className="menu-btn run" onClick={() => doCreate()}>
                Create
              </button>
            </div>
          </div>
        </div>
      )}
    </div>
  );
}

/* path yardimcilari */
function joinPath(dir: string, name: string): string {
  const sep = dir.includes("\\") ? "\\" : "/";
  return dir.replace(/[\\/]+$/, "") + sep + name;
}

function parentOf(p: string): string {
  const i = Math.max(p.lastIndexOf("\\"), p.lastIndexOf("/"));
  return i > 0 ? p.slice(0, i) : p;
}

/* .doc / .gcReference dosyalari Explorer'da gorunmez (DOCS sekmesinde) */
function isDocHidden(name: string): boolean {
  const n = name.toLowerCase();
  return n.endsWith(".doc") || n.endsWith(".gcreference");
}

/* arama icin: boş dizinleri göstermek üzere DirNode rec */
function DirNode({
  entry,
  openDirs,
  onToggle,
  onOpen,
  onCtx,
}: {
  entry: FsEntry;
  openDirs: Record<string, FsEntry[]>;
  onToggle: (path: string) => void;
  onOpen: (path: string) => void;
  onCtx: (e: MouseEvent, path: string, isDir: boolean) => void;
}) {
  const children = openDirs[entry.path];
  const isOpen = !!children;

  if (entry.dir) {
    return (
      <div className="tree-node">
        <div
          className="tree-row"
          onClick={() => onToggle(entry.path)}
          onContextMenu={(e) => onCtx(e, entry.path, true)}
        >
          <span className={`tree-caret ${isOpen ? "open" : ""}`}>▶</span>
          <span className="tree-dir">📁</span>
          <span className="tree-name">{entry.name}</span>
        </div>
        {isOpen && (
          <div className="tree-children">
            {children
              .filter((c) => c.dir || !isDocHidden(c.name))
              .map((c) => (
                <DirNode
                  key={c.path}
                  entry={c}
                  openDirs={openDirs}
                  onToggle={onToggle}
                  onOpen={onOpen}
                  onCtx={onCtx}
                />
              ))}
          </div>
        )}
      </div>
    );
  }

  return (
    <div className="tree-node">
      <div
        className="tree-row file"
        onClick={() => onOpen(entry.path)}
        onContextMenu={(e) => onCtx(e, entry.path, false)}
      >
        <span className="tree-caret hidden">·</span>
        <span className="tree-file">📄</span>
        <span className="tree-name">{entry.name}</span>
      </div>
    </div>
  );
}
