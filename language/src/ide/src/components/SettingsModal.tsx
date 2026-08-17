import { useState } from "react";
import { X } from "lucide-react";
import {
  IdeSettings,
  palettesOf,
  THEME_NAMES,
  ThemeName,
} from "../ideSettings";

interface Props {
  initial: IdeSettings;
  onClose: () => void;
  onSave: (s: IdeSettings) => void;
}

/* IDE Settings modal: tema (5 palet — tek rengin tonları), font, text size, UI scale */
export default function SettingsModal({ initial, onClose, onSave }: Props) {
  const [theme, setTheme] = useState<ThemeName>(initial.theme);
  const [fontFamily, setFontFamily] = useState(initial.fontFamily);
  const [fontSize, setFontSize] = useState(initial.fontSize);
  const [textSize, setTextSize] = useState(initial.textSize);
  const [scale, setScale] = useState(initial.scale);

  const palettes = new Map<ThemeName, ReturnType<typeof palettesOf>>(
    THEME_NAMES.map((t) => [t.name, palettesOf({ ...initial, theme: t.name })]),
  );

  const apply = (s: IdeSettings) => {
    onSave(s);
  };

  const handleSave = () => {
    apply({ theme, fontFamily, fontSize, textSize, scale, language: initial.language });
    onClose();
  };

  return (
    <div className="modal-overlay">
      <div className="modal">
        <div className="modal-header">
          <span>IDE Settings</span>
          <button className="icon-btn" onClick={onClose} title="Close">
            <X size={14} />
          </button>
        </div>

        <div className="modal-body">
          <div className="settings-grid">
            {/* Theme */}
            <div className="settings-group full">
              <div className="settings-title">Theme (shades of one color)</div>
              <div className="theme-swatches">
                {THEME_NAMES.map((t) => {
                  const p = palettes.get(t.name)!;
                  return (
                    <button
                      key={t.name}
                      className={`theme-swatch ${theme === t.name ? "active" : ""}`}
                      style={{ background: p.acc }}
                      title={t.label}
                      onClick={() => {
                        setTheme(t.name);
                        apply({ theme: t.name, fontFamily, fontSize, textSize, scale, language: initial.language });
                      }}
                    >
                      {theme === t.name ? "✓" : ""}
                    </button>
                  );
                })}
              </div>
            </div>

            {/* Editor font */}
            <div className="settings-group">
              <div className="settings-title">Editor Font</div>
              <input
                className="modal-input"
                value={fontFamily}
                onChange={(e) => setFontFamily(e.target.value)}
                placeholder="'Cascadia Code', Consolas, monospace"
              />
            </div>

            {/* Editor font size */}
            <div className="settings-group">
              <div className="settings-title">Editor Font Size</div>
              <select
                className="modal-select"
                value={fontSize}
                onChange={(e) => setFontSize(Number(e.target.value))}
              >
                {[10, 11, 12, 13, 14, 16, 18, 20, 24].map((n) => (
                  <option key={n} value={n}>
                    {n}px
                  </option>
                ))}
              </select>
            </div>

            {/* UI text size */}
            <div className="settings-group">
              <div className="settings-title">UI Text Size</div>
              <select
                className="modal-select"
                value={textSize}
                onChange={(e) => setTextSize(Number(e.target.value))}
              >
                {[11, 12, 13, 14, 15, 16].map((n) => (
                  <option key={n} value={n}>
                    {n}px
                  </option>
                ))}
              </select>
            </div>

            {/* UI scale */}
            <div className="settings-group">
              <div className="settings-title">UI Scale</div>
              <select
                className="modal-select"
                value={scale}
                onChange={(e) => setScale(Number(e.target.value))}
              >
                {[0.8, 0.9, 1, 1.1, 1.25, 1.5].map((s) => (
                  <option key={s} value={s}>
                    {Math.round(s * 100)}%
                  </option>
                ))}
              </select>
            </div>
          </div>

          <p className="modal-hint">
            Settings are saved locally and apply instantly. Theme colors are
            shades of one accent color only.
          </p>
        </div>

        <div className="modal-footer">
          <button className="menu-btn" onClick={onClose}>
            Cancel
          </button>
          <button className="menu-btn run" onClick={handleSave}>
            Save
          </button>
        </div>
      </div>
    </div>
  );
}
