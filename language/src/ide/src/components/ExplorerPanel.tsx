import { useEffect, useState, type MouseEvent } from "react";
import {
  Copy,
  FilePlus,
  FolderInput,
  FolderTree,
  Pencil,
  RefreshCw,
  Scissors,
  Clipboard,
  Trash2,
  Lock,
} from "lucide-react";
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
  protected: boolean;
}

/* Inline yeniden adlandirma / olusturma: dosya agacinin icinde gomulu input */
interface InlineEdit {
  kind: "rename" | "createFile" | "createDir";
  path: string; /* rename: mevcut adres; create: hedef klasor */
  value: string;
  error: string;
}

/* Kes (cut) beklemesi: paste'te tasima yapilir */
interface ClipOp {
  mode: "cut" | "copy";
  path: string;
  isDir: boolean;
}

export default function ExplorerPanel({ root, onOpen, refreshKey }: Props) {
  const [entries, setEntries] = useState<FsEntry[] | null>(null);
  const [dirs, setDirs] = useState<Record<string, FsEntry[]>>({});
  const [ctx, setCtx] = useState<Ctx | null>(null);
  const [inlineEdit, setInlineEdit] = useState<InlineEdit | null>(null);
  const [clip, setClip] = useState<ClipOp | null>(null);
  /* Secili oge: ust butonlar hedefe uygulanir (VS Code secim modeli). */
  const [selected, setSelected] = useState<string | null>(null);
  const [busy, setBusy] = useState<string | null>(null); /* islem ipucu */
  /* Surukle-birak: suruklenen ogenin yolu (drop hedefi klasore tasinir) */
  const [dragPath, setDragPath] = useState<string | null>(null);
  const [dropTarget, setDropTarget] = useState<string | null>(null);

  /* sag tik menusu: disari tiklayinca veya Esc ile kapan */
  useEffect(() => {
    if (!ctx) return;
    const onDown = (e: globalThis.MouseEvent) => {
      const target = e.target as HTMLElement | null;
      if (target && target.closest(".ctx-menu")) return;
      setCtx(null);
    };
    const onKey = (e: globalThis.KeyboardEvent) => {
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

  /* root degisince butun agac sifirlanir; refreshKey geldiginde SADECE
   * top-level yenilenir — acik dizinler kapali kalir. */
  useEffect(() => {
    load();
    setDirs({});
    setSelected(null);
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [root]);

  useEffect(() => {
    if (!root) return;
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

  const closeMenu = () => setCtx(null);

  const onCtx = (e: MouseEvent, path: string, isDir: boolean, isProtected: boolean) => {
    e.preventDefault();
    e.stopPropagation();
    setSelected(path);
    setCtx({
      x: e.clientX,
      y: e.clientY,
      path,
      isDir,
      protected: isProtected,
    });
    if (isDir) ensureOpen(path);
  };

  /* islem sonrasi ilgili dizini yeniden oku */
  const refreshAround = async (dirPath: string) => {
    const items = await window.ide.dirTree(dirPath);
    setDirs((d) => ({ ...d, [dirPath]: items }));
    if (dirPath === root) setEntries(items);
  };

  const reportError = (msg: string) => setBusy(msg);

  /* Surukle-birak: suruklenen ogeyi hedef klasorun ALTINA tasir.
   * Korumali kaynak/hedef veya kendine-atma reddedilir. */
  const doDrop = async (targetDir: string) => {
    if (!dragPath) return;
    setDropTarget(null);
    const srcPath = dragPath;
    setDragPath(null);
    /* kendine atma: hedef, suruklenen ogenin kendisi veya icindeki klasor */
    if (pathEq(srcPath, targetDir) || pathStartsWith(targetDir, srcPath)) return;
    const srcIsProtected = allEntries().find((f) => f.path === srcPath)?.protected;
    const dst = joinPath(targetDir, baseName(srcPath));
    if (pathEq(dst, srcPath)) return;
    const res = await window.ide.renameFs(srcPath, dst);
    if (res && res.ok === false) {
      reportError(res.message ?? "Move failed.");
      return;
    }
    await refreshAround(targetDir);
    const srcParent = parentOf(srcPath);
    if (srcParent !== targetDir) await refreshAround(srcParent);
    setSelected(dst);
    if (srcIsProtected) setSelected(null);
  };

  /* --- CRUD --- */
  const doCreate = async () => {
    if (!inlineEdit || inlineEdit.kind === "rename") return;
    const name = inlineEdit.value.trim();
    if (!name) return;
    const dir = inlineEdit.path;
    const target = joinPath(dir, name);
    const isDir = inlineEdit.kind === "createDir";
    const res = isDir
      ? await window.ide.createDir(target)
      : await window.ide.createFile(target);
    if (res && res.ok === false) {
      setInlineEdit((ed) =>
        ed ? { ...ed, error: res.message ?? "Create failed." } : ed,
      );
      return;
    }
    await refreshAround(dir);
    setInlineEdit(null);
    if (!isDir) {
      await ensureOpen(dir);
      onOpen(target);
    }
    setSelected(target);
  };

  const doRename = async () => {
    if (!inlineEdit || inlineEdit.kind !== "rename") return;
    const newName = inlineEdit.value.trim();
    if (!newName) return;
    if (newName === baseName(inlineEdit.path)) {
      setInlineEdit(null);
      return;
    }
    const dst = joinPath(parentOf(inlineEdit.path), newName);
    const res = await window.ide.renameFs(inlineEdit.path, dst);
    if (res && res.ok === false) {
      setInlineEdit((ed) =>
        ed ? { ...ed, error: res.message ?? "Rename failed." } : ed,
      );
      return;
    }
    await refreshAround(parentOf(inlineEdit.path));
    setInlineEdit(null);
    setSelected(dst);
  };

  const doDelete = async (p: string, isDir: boolean) => {
    if (!window.confirm(`Delete ${isDir ? "folder" : "file"} "${baseName(p)}"?`))
      return;
    const parent = parentOf(p);
    const res = await window.ide.deleteFs(p);
    if (res && res.ok === false) {
      reportError(res.message ?? "Delete failed.");
      closeMenu();
      return;
    }
    await refreshAround(parent);
    closeMenu();
    if (selected === p) setSelected(null);
  };

  const doCopyClip = (p: string, isDir: boolean, mode: "cut" | "copy") => {
    setClip({ mode, path: p, isDir });
    closeMenu();
  };

  const doPaste = async (targetDir: string) => {
    if (!clip) return;
    const srcName = baseName(clip.path);
    const dst = joinPath(targetDir, srcName);
    if (pathEq(dst, clip.path)) return; /* kendine paste */
    if (clip.mode === "copy") {
      const res = await window.ide.copyFile(clip.path, dst);
      if (res && res.ok === false) {
        reportError(res.message ?? "Copy failed.");
        return;
      }
    } else {
      const res = await window.ide.renameFs(clip.path, dst);
      if (res && res.ok === false) {
        reportError(res.message ?? "Move failed.");
        return;
      }
    }
    await refreshAround(targetDir);
    if (parentOf(clip.path) !== targetDir) await refreshAround(parentOf(clip.path));
    if (clip.mode === "copy") setClip(null); /* copy birden fazla paste edilebilir */
    setSelected(dst);
  };

  /* --- prompt helpers --- */
  const openRename = (p: string, isDir: boolean) => {
    setInlineEdit({ kind: "rename", path: p, value: baseName(p), error: "" });
    if (isDir) ensureOpen(parentOf(p));
    closeMenu();
  };

  const openCreate = (dir: string, isDir: boolean) => {
    setInlineEdit({
      kind: isDir ? "createDir" : "createFile",
      path: dir,
      value: "",
      error: "",
    });
    closeMenu();
  };

  /* Buton / menu hedef klasoru: secili oge bir klasor ise kendisi,
   * dosya ise parent'i; secim yoksa root. */
  const targetDirOf = (): string => {
    if (!selected) return root;
    if (dirs[selected] || entries?.some((e) => e.path === selected && e.dir))
      return selected;
    return parentOf(selected);
  };

  const isSelectedProtected = (): boolean => {
    if (!selected) return false;
    const flat = allEntries();
    const it = flat.find((f) => f.path === selected);
    return !!it?.protected;
  };

  /* Agactaki tum ogeler (acik dizinler dahil) — secilen koruma tespiti */
  const allEntries = (): FsEntry[] => {
    const out = [...(entries ?? [])];
    for (const k of Object.keys(dirs)) out.push(...dirs[k]);
    return out;
  };

  const closeInline = () => setInlineEdit(null);

  const reloadAll = async () => {
    await load();
    const keys = Object.keys(dirs);
    for (const k of keys) {
      const items = await window.ide.dirTree(k);
      setDirs((d) => ({ ...d, [k]: items }));
    }
  };

  /* Dosya/klasor olusturma hedefi uygun mu? src/ ve diger kok ogelerine
   * olusturma SERBEST; yalnizca Library/ ve icindekilere engellidir
   * (backend de dstInsideProtected ile ayni kurali uygular). */
  const canCreateAt = (dir: string): boolean => {
    if (!root) return false;
    const lib = joinPath(root, "Library");
    return !pathStartsWith(dir, lib);
  };

  /* Context menü hedefi: New File/New Folder disabled durumu icin.
   * src/ gibi korumali kok klasorlerinde olusturma SERBEST, yalnizca
   * Library/ icinde engellidir. */
  const ctxCreateTarget = ctx
    ? ctx.isDir
      ? ctx.path
      : parentOf(ctx.path)
    : root;
  const ctxCreateOk = ctx ? canCreateAt(ctxCreateTarget) : true;

  if (!entries) return <div className="panel-empty">...</div>;

  const activeDir = targetDirOf();
  const selName = selected ? baseName(selected) : null;
  /* Secili oge klasor mu? (acik ya da kapali olabilir) */
  const selIsDir = !!selected &&
    (dirs[selected] !== undefined ||
      entries?.some((e) => e.path === selected && e.dir));
  const selProtected = isSelectedProtected();

  return (
    <div
      className="explorer"
      onContextMenu={(e) => {
        e.preventDefault();
        setCtx({
          x: e.clientX,
          y: e.clientY,
          path: root,
          isDir: true,
          protected: false,
        });
      }}
    >
      <div className="panel-header">
        <span>
          <FolderTree size={14} /> EXPLORER
        </span>
        <div className="panel-actions">
          <button
            className="icon-btn"
            onClick={() => openCreate(activeDir, false)}
            title="New File"
          >
            <FilePlus size={13} />
          </button>
          <button
            className="icon-btn"
            onClick={() => openCreate(activeDir, true)}
            title="New Folder"
          >
            <FolderInput size={13} />
          </button>
          <button className="icon-btn" onClick={reloadAll} title="Refresh">
            <RefreshCw size={13} />
          </button>
          {selected && (
            <>
              <span className="ex-sel-sep" />
              <button
                className="icon-btn"
                onClick={() => openRename(selected, selIsDir)}
                title="Rename (F2)"
                disabled={selProtected}
              >
                <Pencil size={13} />
              </button>
              <button
                className="icon-btn"
                onClick={() => doCopyClip(selected, selIsDir, "cut")}
                title="Cut"
                disabled={selProtected}
              >
                <Scissors size={13} />
              </button>
              <button
                className="icon-btn"
                onClick={() => doCopyClip(selected, selIsDir, "copy")}
                title="Copy"
                disabled={selProtected}
              >
                <Copy size={13} />
              </button>
              {clip && (
                <button
                  className="icon-btn"
                  onClick={() => doPaste(activeDir)}
                  title="Paste"
                >
                  <Clipboard size={13} />
                </button>
              )}
              <button
                className="icon-btn danger"
                onClick={() => doDelete(selected, selIsDir)}
                title="Delete"
                disabled={selProtected}
              >
                <Trash2 size={13} />
              </button>
            </>
          )}
        </div>
      </div>

      {busy && (
        <div className="ex-toast" onClick={() => setBusy(null)}>
          {busy}
        </div>
      )}

      {clip && (
        <div className="ex-selbar">
          <Scissors size={11} />
          <span>
            {clip.mode === "cut" ? "Cut: " : "Copy: "}
            {baseName(clip.path)}
          </span>
          <button onClick={() => setClip(null)} title="Clear clipboard">
            ✕
          </button>
        </div>
      )}

      {/* secim + hedef cubugu */}
      <div className="ex-statusrow">
        <span className="ex-sel-info">
          {selected ? (
            <>
              <b>{selName}</b>
              {selProtected && (
                <span className="ex-prot-badge">
                  <Lock size={10} /> core
                </span>
              )}
            </>
          ) : (
            <span className="ex-sel-empty">No selection — actions target root</span>
          )}
        </span>
        <span className="ex-dir-hint">target: {displayPath(activeDir)}</span>
      </div>

      {entries.length === 0 && <div className="panel-empty">Folder is empty.</div>}

      <div className="tree">
        {entries
          .filter((e) => e.dir || !isDocHidden(e.name))
          .map((e) => (
            <DirNode
              key={e.path}
              entry={e}
              level={0}
              openDirs={dirs}
              selected={selected}
              inlineEdit={inlineEdit}
              dragPath={dragPath}
              dropTarget={dropTarget}
              onSelect={setSelected}
              onToggle={toggleDir}
              onOpen={onOpen}
              onCtx={onCtx}
              onInlineChange={setInlineEdit}
              onInlineCommit={inlineEdit?.kind === "rename" ? doRename : doCreate}
              onInlineCancel={closeInline}
              onDragStart={setDragPath}
              onDragEnd={() => {
                setDragPath(null);
                setDropTarget(null);
              }}
              onDragOver={setDropTarget}
              onDrop={doDrop}
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
            disabled={!ctxCreateOk}
            onClick={() =>
              openCreate(ctx.isDir ? ctx.path : parentOf(ctx.path), false)
            }
          >
            <FilePlus size={12} /> New File...
          </button>
          <button
            className="ctx-item"
            disabled={!ctxCreateOk}
            onClick={() =>
              openCreate(ctx.isDir ? ctx.path : parentOf(ctx.path), true)
            }
          >
            <FolderInput size={12} /> New Folder...
          </button>
          <div className="ctx-sep" />
          <button
            className="ctx-item"
            disabled={ctx.protected}
            onClick={() => openRename(ctx.path, ctx.isDir)}
          >
            <Pencil size={12} /> Rename <span className="ctx-short">F2</span>
          </button>
          <button
            className="ctx-item"
            disabled={ctx.protected}
            onClick={() => doCopyClip(ctx.path, ctx.isDir, "cut")}
          >
            <Scissors size={12} /> Cut
          </button>
          <button
            className="ctx-item"
            disabled={ctx.protected}
            onClick={() => doCopyClip(ctx.path, ctx.isDir, "copy")}
          >
            <Copy size={12} /> Copy
          </button>
          {clip && (
            <button
              className="ctx-item"
              onClick={() =>
                doPaste(ctx.isDir ? ctx.path : parentOf(ctx.path))
              }
            >
              <Clipboard size={12} /> Paste here
            </button>
          )}
          <div className="ctx-sep" />
          <button
            className="ctx-item danger"
            disabled={ctx.protected}
            onClick={() => doDelete(ctx.path, ctx.isDir)}
          >
            <Trash2 size={12} /> Delete
          </button>
          {ctx.protected && (
            <div className="ctx-note">
              <Lock size={10} /> This is a core folder and cannot be modified
            </div>
          )}
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

function baseName(p: string): string {
  const i = Math.max(p.lastIndexOf("\\"), p.lastIndexOf("/"));
  return i >= 0 ? p.slice(i + 1) : p;
}

function pathEq(a: string, b: string): boolean {
  const norm = (s: string) => s.replace(/\\/g, "/").toLowerCase();
  return norm(a) === norm(b);
}

/* `a`, `b`'nin dizini (veya kendisi) mi? Surukle-birakta klasorun kendi
 * icine atilmasini engellemek icin. */
function pathStartsWith(a: string, b: string): boolean {
  const norm = (s: string) => s.replace(/\\/g, "/").replace(/\/+$/, "").toLowerCase();
  return norm(a) === norm(b) || norm(a).startsWith(norm(b) + "/");
}

function displayPath(p: string): string {
  const parts = p.split(/[\\/]/).filter(Boolean);
  if (parts.length <= 3) return parts.join("/");
  return ".../" + parts.slice(-2).join("/");
}

/* .doc / .gcReference dosyalari Explorer'da gorunmez (DOCS sekmesinde) */
function isDocHidden(name: string): boolean {
  const n = name.toLowerCase();
  return n.endsWith(".doc") || n.endsWith(".gcreference");
}

/* Inline edit inputu: rename / create — value degisimi parent state'e
 * onInlineChange ile yansir. Odak: autoFocus + onFocus (rename'de uzanti
 * disini secer, create'te tump metni). */
function InlineInput({
  edit,
  onChange,
  onCommit,
  onCancel,
}: {
  edit: InlineEdit;
  onChange: (ed: InlineEdit) => void;
  onCommit: () => void;
  onCancel: () => void;
}) {
  return (
    <div
      className="tree-row inline-editing"
      onClick={(e) => e.stopPropagation()}
      onContextMenu={(e) => e.stopPropagation()}
    >
      <span className="tree-caret hidden">·</span>
      <span className="tree-file">{edit.kind === "createDir" ? "📁" : "📄"}</span>
      <div className="tree-inline">
        <input
          autoFocus
          className="tree-inline-input"
          value={edit.value}
          placeholder={
            edit.kind === "rename"
              ? "name"
              : edit.kind === "createDir"
                ? "folder name"
                : "file.gcsf"
          }
          onChange={(e) => onChange({ ...edit, value: e.target.value, error: "" })}
          onFocus={(e) => {
            if (edit.kind === "rename") {
              const dot = e.target.value.lastIndexOf(".");
              e.target.setSelectionRange(0, dot > 0 ? dot : e.target.value.length);
            } else {
              e.target.select();
            }
          }}
          onKeyDown={(e) => {
            if (e.key === "Enter") onCommit();
            if (e.key === "Escape") onCancel();
          }}
        />
        {edit.error && <div className="tree-inline-error">{edit.error}</div>}
      </div>
    </div>
  );
}

/* Recursive agac dugumu */
function DirNode({
  entry,
  level,
  openDirs,
  selected,
  inlineEdit,
  dragPath,
  dropTarget,
  onSelect,
  onToggle,
  onOpen,
  onCtx,
  onInlineChange,
  onInlineCommit,
  onInlineCancel,
  onDragStart,
  onDragEnd,
  onDragOver,
  onDrop,
}: {
  entry: FsEntry;
  level: number;
  openDirs: Record<string, FsEntry[]>;
  selected: string | null;
  inlineEdit: InlineEdit | null;
  dragPath: string | null;
  dropTarget: string | null;
  onSelect: (p: string) => void;
  onToggle: (path: string) => void;
  onOpen: (path: string) => void;
  onCtx: (e: MouseEvent, path: string, isDir: boolean, isProtected: boolean) => void;
  onInlineChange: (ed: InlineEdit) => void;
  onInlineCommit: () => void;
  onInlineCancel: () => void;
  onDragStart: (p: string) => void;
  onDragEnd: () => void;
  onDragOver: (p: string | null) => void;
  onDrop: (targetDir: string) => void;
}) {
  const children = openDirs[entry.path];
  const isOpen = !!children;

  /* drag baslat (korumali oge suruklenemez) */
  const handleDragStart = (ev: React.DragEvent) => {
    if (entry.protected) {
      ev.preventDefault();
      return;
    }
    onDragStart(entry.path);
    ev.dataTransfer.effectAllowed = "move";
    ev.dataTransfer.setData("text/plain", entry.path);
  };

  /* drop hedefi: yalnizca klasorler. Korumali klasore drop SERBEST —
   * main.ts hedef Library ise reddeder, src kabul eder. */
  const handleDragOver = (ev: React.DragEvent) => {
    if (!entry.dir || !dragPath) return;
    ev.preventDefault();
    ev.dataTransfer.dropEffect = "move";
    onDragOver(entry.path);
  };

  const handleDrop = (ev: React.DragEvent) => {
    if (!entry.dir || !dragPath) return;
    ev.preventDefault();
    onDrop(entry.path);
  };

  /* inline edit bu node'da mi? */
  const editingHere =
    inlineEdit?.kind === "rename" && inlineEdit.path === entry.path;
  const creatingHere =
    (inlineEdit?.kind === "createFile" || inlineEdit?.kind === "createDir") &&
    inlineEdit.path === (entry.dir ? entry.path : parentOf(entry.path));

  if (entry.dir) {
    return (
      <div className="tree-node">
        <div
          className={
            "tree-row" +
            (selected === entry.path ? " selected" : "") +
            (entry.protected ? " protected" : "") +
            (dropTarget === entry.path ? " drop-target" : "")
          }
          style={{ paddingLeft: 8 + level * 14 }}
          draggable={!entry.protected}
          onClick={(e) => {
            onSelect(entry.path);
            if (e.detail === 2) onToggle(entry.path);
          }}
          onContextMenu={(e) => onCtx(e, entry.path, true, !!entry.protected)}
          onDragStart={handleDragStart}
          onDragEnd={onDragEnd}
          onDragOver={handleDragOver}
          onDrop={handleDrop}
        >
          <span className={`tree-caret ${isOpen ? "open" : ""}`}>▶</span>
          <span className="tree-dir">{entry.name === "src" ? "📦" : "📁"}</span>
          <span className="tree-name">{entry.name}</span>
          {entry.protected && (
            <span
              className="tree-lock"
              title="Core folder — cannot rename, move or delete"
            >
              <Lock size={10} />
            </span>
          )}
        </div>
        {isOpen && (
          <div className="tree-children" onContextMenu={(e) => e.stopPropagation()}>
            {(editingHere || creatingHere) && inlineEdit ? (
              <InlineInput
                edit={inlineEdit}
                onChange={onInlineChange}
                onCommit={onInlineCommit}
                onCancel={onInlineCancel}
              />
            ) : null}
            {children
              .filter((c) => c.dir || !isDocHidden(c.name))
              .map((c) => (
                <DirNode
                  key={c.path}
                  entry={c}
                  level={level + 1}
                  openDirs={openDirs}
                  selected={selected}
                  inlineEdit={inlineEdit}
                  dragPath={dragPath}
                  dropTarget={dropTarget}
                  onSelect={onSelect}
                  onToggle={onToggle}
                  onOpen={onOpen}
                  onCtx={onCtx}
                  onInlineChange={onInlineChange}
                  onInlineCommit={onInlineCommit}
                  onInlineCancel={onInlineCancel}
                  onDragStart={onDragStart}
                  onDragEnd={onDragEnd}
                  onDragOver={onDragOver}
                  onDrop={onDrop}
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
        className={"tree-row file" + (selected === entry.path ? " selected" : "")}
        style={{ paddingLeft: 8 + level * 14 }}
        draggable={!entry.protected}
        onClick={(e) => {
          onSelect(entry.path);
          onOpen(entry.path);
        }}
        onContextMenu={(e) => onCtx(e, entry.path, false, !!entry.protected)}
        onDragStart={handleDragStart}
        onDragEnd={onDragEnd}
      >
        <span className="tree-caret hidden">·</span>
        <span className={`tree-file ${iconClass(entry.name)}`}>
          {fileIcon(entry.name)}
        </span>
        <span className="tree-name">{entry.name}</span>
        {entry.protected && (
          <span className="tree-lock" title="Core file">
            <Lock size={10} />
          </span>
        )}
      </div>
    </div>
  );
}

/* dile gore dosya ikonu */
function fileIcon(name: string): string {
  const n = name.toLowerCase();
  if (n.endsWith(".gcsf") || n.endsWith(".gclib") || n.endsWith(".gcl"))
    return "◆";
  if (n.endsWith(".py")) return "🐍";
  if (n.endsWith(".lua")) return "🟦";
  if (n.endsWith(".gcdata")) return "⚙";
  return "📄";
}

function iconClass(name: string): string {
  const n = name.toLowerCase();
  if (n.endsWith(".gcsf") || n.endsWith(".gclib") || n.endsWith(".gcl"))
    return "ix-gcl";
  if (n.endsWith(".py")) return "ix-py";
  if (n.endsWith(".lua")) return "ix-lua";
  if (n.endsWith(".gcdata")) return "ix-data";
  return "";
}
