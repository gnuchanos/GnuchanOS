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

export default function ProjectModal({ mode, initial, onClose, onSubmit, onBrowse }: Props) {
  const [info, setInfo] = useState<ProjectInfo>(initial ?? emptyInfo);
  const [projectPath, setProjectPath] = useState("");
  const [busy, setBusy] = useState(false);
  const [error, setError] = useState("");

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
      await onSubmit(
        { ...info, name: info.name.trim() },
        mode === "create" ? projectPath.trim() : undefined,
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
            <label className="modal-label">
              Project Path
              <div className="modal-path-row">
                <input
                  className="modal-input"
                  value={projectPath}
                  onChange={(e) => setProjectPath(e.target.value)}
                  placeholder="Pick a folder or type a path"
                />
                {onBrowse && (
                  <button
                    className="menu-btn"
                    type="button"
                    onClick={async () => {
                      const dir = await onBrowse();
                      if (dir) setProjectPath(dir);
                    }}
                    title="Choose folder"
                  >
                    <FolderOpen size={13} /> Browse…
                  </button>
                )}
              </div>
            </label>
          )}

          <label className="modal-label">
            Project Name
            <input
              className="modal-input"
              value={info.name}
              onChange={(e) =>
                setInfo((p) => ({ ...p, name: e.target.value }))
              }
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

          <div className="modal-check-row">
            <label className="modal-check">
              <input
                type="checkbox"
                checked={info.useLua}
                onChange={(e) =>
                  setInfo((p) => ({ ...p, useLua: e.target.checked }))
                }
              />
              Uses Lua
            </label>
            <label className="modal-check">
              <input
                type="checkbox"
                checked={info.usePython}
                onChange={(e) =>
                  setInfo((p) => ({ ...p, usePython: e.target.checked }))
                }
              />
              Uses Python
            </label>
          </div>

          {error && <div className="modal-error">{error}</div>}

          <p className="modal-hint">
            {mode === "create"
              ? "Project details are written to Project.gcDATA in the given path and used on export."
              : mode === "save"
                ? "Project metadata (name, developer, language flags) is written to Project.gcDATA."
                : "The project is copied to the target folder with an embedded EXPORT_INFO.json manifest."}
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
