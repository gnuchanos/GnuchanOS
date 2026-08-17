import { useState } from "react";
import { FolderOpen, X } from "lucide-react";
import type { ProjectInfo } from "../types";

interface Props {
  mode: "create" | "save" | "export";
  initial?: ProjectInfo;
  onClose: () => void;
  onSubmit: (info: ProjectInfo, projectPath?: string) => Promise<void>;
  /* Yeni proje: klasor secmek icin Browse (dialog) — path elle yazilmaz */
  onBrowse?: () => Promise<string | null>;
}

const emptyInfo: ProjectInfo = {
  name: "",
  developer: "",
  useLua: false,
  usePython: false,
  version: "0.1.0",
  createdAt: "",
  updatedAt: "",
};

/* Web tarafinda path.join yok: basit goreli join (Windows/Linux uyumlu). */
function joinPath(a: string, b: string): string {
  const sep = a.includes("\\") ? "\\" : "/";
  return a.replace(/[\\/]+$/, "") + sep + b;
}

/* Klasor adi olarak guvenli proje adi (dosya sistemi uyumlu). */
function safeProjectName(name: string): string {
  const cleaned = name.trim().replace(/[^A-Za-z0-9_\- ]+/g, "").trim();
  return cleaned || "GCLProject";
}

export default function ProjectModal({ mode, initial, onClose, onSubmit, onBrowse }: Props) {
  const [info, setInfo] = useState<ProjectInfo>(initial ?? emptyInfo);
  const [projectPath, setProjectPath] = useState("");
  const [busy, setBusy] = useState(false);
  const [error, setError] = useState("");
  /* Proje kendi klasorunde olusur: parent/<name>/ (varsayilan acik). */
  const [subfolder, setSubfolder] = useState(true);
  const [fullCreatePath, setFullCreatePath] = useState("");
  const updatePath = (p: string) => {
    setProjectPath(p);
    setFullCreatePath(
      p.trim() && subfolder
        ? joinPath(p.trim(), safeProjectName(info.name || "GCLProject"))
        : p.trim(),
    );
  };
  const updateName = (fn: (i: ProjectInfo) => ProjectInfo) => {
    setInfo((prev) => {
      const next = fn(prev);
      setFullCreatePath(
        projectPath.trim() && subfolder
          ? joinPath(projectPath.trim(), safeProjectName(next.name || "GCLProject"))
          : projectPath.trim(),
      );
      return next;
    });
  };

  const handleSubmit = async () => {
    if (!info.name.trim()) {
      setError("Project name cannot be empty");
      return;
    }
    if (mode === "create" && !projectPath.trim()) {
      setError("Project path cannot be empty (e.g. D:\\projects\\my_game)");
      return;
    }
    setBusy(true);
    setError("");
    try {
      /* Dosya haritasi (files/dirs) ASLA elle girilmez: main.ts proje
       * acilisinda, kayitta ve dosya ekle/sil olaylarinda diski otomatik
       * tarar ve Project.gcDATA'yi gunceller. Buradaki payload yalnizca
       * kullanici alanlarini (name/developer/version/useLua/usePython)
       * tasir. */
      const payload: ProjectInfo = {
        ...info,
        name: info.name.trim(),
      };
      await onSubmit(
        payload,
        mode === "create"
          ? (subfolder
              ? joinPath(projectPath.trim(), safeProjectName(payload.name || "GCLProject"))
              : projectPath.trim())
          : undefined,
      );
      onClose();
    } catch (e) {
      setError((e as Error).message);
    } finally {
      setBusy(false);
    }
  };

  return (
    <div className="modal-overlay">
      <div className="modal">
        <div className="modal-header">
          <span>
            {mode === "create"
              ? "New GCL Project"
              : mode === "save"
                ? "Project Info (Save Project)"
                : "Export Project"}
          </span>
          <button className="icon-btn" onClick={onClose} title="Close">
            <X size={14} />
          </button>
        </div>

        <div className="modal-body">
          {mode === "create" && (
            <>
              {/* Konum secimi: elle yol yazilmaz — Browse ile klasor secilir.
               * Secilen yol kartin icinde gozukur; secim yoksa placeholder. */}
              <div className="modal-label">
                <span className="folder-picker-label">
                  Project Location (parent folder)
                </span>
                <button
                  type="button"
                  className={`folder-picker ${projectPath.trim() ? "has-path" : ""}`}
                  onClick={async () => {
                    if (!onBrowse) return;
                    const dir = await onBrowse();
                    if (dir) updatePath(dir);
                  }}
                  title="Choose parent folder"
                >
                  <FolderOpen size={16} className="folder-picker-icon" />
                  <span className="folder-picker-path">
                    {projectPath.trim()
                      ? projectPath.trim()
                      : "Choose a folder…"}
                  </span>
                  <span className="folder-picker-btn">Browse…</span>
                </button>
              </div>

              {/* Otomatik klasor: proje parent/<name>/ icinde olusur */}
              <label className="modal-label project-folder-row">
                Create inside a subfolder
                <label className="modal-check">
                  <input
                    type="checkbox"
                    checked={subfolder}
                    onChange={(e) => {
                      setSubfolder(e.target.checked);
                      updatePath(projectPath);
                    }}
                  />
                  {safeProjectName(info.name || "GCLProject")}/
                </label>
                {projectPath.trim() && subfolder && (
                  <code className="project-path-preview">{fullCreatePath}</code>
                )}
              </label>
            </>
          )}

          <label className="modal-label">
            Project Name
            <input
              className="modal-input"
              value={info.name}
              onChange={(e) => updateName((p) => ({ ...p, name: e.target.value }))}
              placeholder="my_first_project"
            />
          </label>

          <label className="modal-label">
            Developer
            <input
              className="modal-input"
              value={info.developer}
              onChange={(e) =>
                setInfo((p) => ({ ...p, developer: e.target.value }))
              }
              placeholder="Gnuchan"
            />
          </label>

          <label className="modal-label">
            Version
            <input
              className="modal-input"
              value={info.version}
              onChange={(e) =>
                setInfo((p) => ({ ...p, version: e.target.value }))
              }
              placeholder="0.1.0"
            />
          </label>

          {/* Embed dilleri: kart toggles — export'ta bu dillerin runtime'lari
           * gomulur. Script/klasor konumu buradan GIRILMEZ; disk otomatik
           * taranir ve Project.gcDATA'ya yazilir (info dump). */}
          <div className="embed-grid">
            <div className={`embed-card ${info.useLua ? "active" : ""}`}>
              <button
                type="button"
                className="embed-card-head"
                onClick={() => setInfo((p) => ({ ...p, useLua: !p.useLua }))}
              >
                <span className="embed-logo lua">LUA</span>
                <span className="embed-card-title">Lua Embed</span>
                <span className="embed-toggle">{info.useLua ? "ON" : "OFF"}</span>
              </button>
              {info.useLua ? (
                <p className="embed-off-hint">
                  Lua runtime embedded. .lua files are auto-scanned (import map).
                </p>
              ) : (
                <p className="embed-off-hint">
                  Lua runtime and scripts are NOT embedded.
                </p>
              )}
            </div>

            <div className={`embed-card ${info.usePython ? "active" : ""}`}>
              <button
                type="button"
                className="embed-card-head"
                onClick={() =>
                  setInfo((p) => ({ ...p, usePython: !p.usePython }))
                }
              >
                <span className="embed-logo py">PY</span>
                <span className="embed-card-title">Python Embed</span>
                <span className="embed-toggle">
                  {info.usePython ? "ON" : "OFF"}
                </span>
              </button>
              {info.usePython ? (
                <p className="embed-off-hint">
                  Python runtime embedded. .py files are auto-scanned (import map).
                </p>
              ) : (
                <p className="embed-off-hint">
                  Python runtime and scripts are NOT embedded.
                </p>
              )}
            </div>
          </div>

          {/* Canli proje topolojisi: export'ta Lua/Python embed'ine gonderilen
           * info dump — dosya haritasi (konum + import adi + runtime) ve
           * gomulenler. Klasorler (pyFiles vb.) .gcBundle icinde korunur. */}
          <div className="project-schema">
            <div className="project-schema-title">PROJECT TOPOLOGY</div>
            <div className="project-schema-line">
              <span className="schema-dir">📁 {safeProjectName(info.name || "GCLProject")}</span>
            </div>
            <div className="project-schema-line">
              <span className="schema-dir">├─ 📄 Project.gcDATA</span>
              <span className="schema-note">(auto-scanned file map)</span>
            </div>
            <div className="project-schema-line">
              <span className="schema-dir">├─ 📁 src</span>
              <span className="schema-note">
                auto-scanned → src/pyFiles/test.py = "pyFiles.test"
              </span>
            </div>
            <div className="project-schema-line">
              <span className="schema-dir">└─ 📁 Library</span>
            </div>
            {info.useLua && (
              <div className="project-schema-line">
                <span className="schema-dir">   ├─  Lua/luaLibrary</span>
                <span className="schema-note">Lua runtime (embedded)</span>
              </div>
            )}
            {info.usePython && (
              <div className="project-schema-line">
                <span className="schema-dir">   └─ 📁 Python/pyLibrary</span>
                <span className="schema-note">Python runtime (embedded)</span>
              </div>
            )}
          </div>

          {error && <div className="modal-error">{error}</div>}

          <p className="modal-hint">
            {mode === "create"
              ? "Project.gcDATA is auto-written: on open/save the disk is scanned and every script/native file's location + import name is recorded. No manual dirs."
              : mode === "save"
                ? "Project metadata is written to Project.gcDATA; the file map is re-scanned from disk automatically (no manual dirs needed)."
                : "The project is copied to the target folder with an EXPORT_INFO.json manifest (file map + import names + dirs included)."}
          </p>
        </div>

        <div className="modal-footer">
          <button className="menu-btn" onClick={onClose} disabled={busy}>
            Cancel
          </button>
          <button className="menu-btn run" onClick={handleSubmit} disabled={busy}>
            {busy
              ? "…"
              : mode === "create"
                ? "Create"
                : mode === "save"
                  ? "Save"
                  : "Export"}
          </button>
        </div>
      </div>
    </div>
  );
}
