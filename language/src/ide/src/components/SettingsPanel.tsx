import { useState } from "react";
import { RotateCcw, X } from "lucide-react";
import {
  IdeSettings,
  palettesOf,
  Palette,
  THEME_NAMES,
  ThemeName,
  UiLanguage,
} from "../ideSettings";

interface Props {
  initial: IdeSettings;
  onClose: () => void;
  onSave: (s: IdeSettings) => void;
}

const MIN_W = 260;
const MAX_W = 520;
const DEFAULT_W = 320;

/* Renk override'lari kullaniciya KATEGORILER halinde sunulur — her kategorinin
 * altinda "bu neyi degistirir" aciklamasi vardir. Tum alanlar Palette'teki
 * semantik anahtarlarla birebir eslesir ve App.tsx'teki CSS degiskenlerine
 * baglidir. */
const COLOR_GROUPS: {
  title: string;
  desc: string;
  fields: { key: keyof Palette; label: string; hint: string }[];
}[] = [
  {
    title: "General",
    desc: "App-wide surfaces: backgrounds, borders, text tones and the accent color.",
    fields: [
      { key: "bg0", label: "App Background", hint: "darkest app surface" },
      { key: "bg1", label: "Panel Background", hint: "generic panel surface" },
      { key: "bg2", label: "Raised / Hover", hint: "hover + raised surfaces" },
      { key: "bg3", label: "Active Surface", hint: "pressed / active row" },
      { key: "border", label: "Border", hint: "separators and outlines" },
      { key: "fg0", label: "Main Text", hint: "primary text everywhere" },
      { key: "fg1", label: "Secondary Text", hint: "descriptions / details" },
      { key: "fg2", label: "Dim Text", hint: "muted labels" },
      { key: "acc", label: "Accent", hint: "brand color, links, focus" },
      { key: "accDim", label: "Accent Dim", hint: "muted accent fills" },
      { key: "alt", label: "Contrast Accent", hint: "secondary highlight" },
      { key: "iconFg", label: "Icons", hint: "toolbar + tree icons" },
      { key: "focusBorder", label: "Focus Border", hint: "focused inputs" },
      { key: "selectionBg", label: "Selection", hint: "text selection" },
      { key: "scrollbarBg", label: "Scrollbar", hint: "scrollbar thumb" },
      { key: "scrollbarHoverBg", label: "Scrollbar Hover", hint: "scrollbar hover" },
    ],
  },
  {
    title: "Title Bar → Menu Bar",
    desc: "The top strip: window title text and the menu bar behind the File/Project/Build menus.",
    fields: [
      { key: "titleBarBg", label: "Title Bar Bg", hint: "native window title area" },
      { key: "titleBarFg", label: "Title Bar Text", hint: "window title text" },
      { key: "menuBarBg", label: "Menu Bar Bg", hint: "strip behind menus" },
      { key: "menuBarFg", label: "Menu Bar Text", hint: "menu button text" },
    ],
  },
  {
    title: "Activity Bar",
    desc: "The left icon column (Explorer / Docs / Settings buttons) and its badge.",
    fields: [
      { key: "activityBarBg", label: "Background", hint: "icon column surface" },
      { key: "activityBarFg", label: "Active Icon", hint: "selected icon color" },
      { key: "activityBarInactiveFg", label: "Inactive Icon", hint: "unselected icons" },
      { key: "activityBarActiveBg", label: "Active Row Bg", hint: "highlight behind active icon" },
      { key: "activityBarBorder", label: "Border", hint: "right separator line" },
      { key: "activityBarBadgeBg", label: "Badge Bg", hint: "notification dot fill" },
      { key: "activityBarBadgeFg", label: "Badge Text", hint: "notification dot text" },
    ],
  },
  {
    title: "Sidebar / Explorer",
    desc: "The explorer tree, its section header, and the rows you hover or select.",
    fields: [
      { key: "sideBarBg", label: "Background", hint: "sidebar surface" },
      { key: "sideBarFg", label: "Text", hint: "file/folder names" },
      { key: "sideBarBorder", label: "Border", hint: "edge between sidebar and editor" },
      { key: "sideBarSectionHeaderBg", label: "Section Header Bg", hint: "EXPLORER / DOCS strip" },
      { key: "sideBarSectionHeaderFg", label: "Section Header Text", hint: "strip label color" },
      { key: "sideBarTitleFg", label: "Project Name", hint: "project title in menu bar" },
      { key: "listHoverBg", label: "Row Hover Bg", hint: "tree row under mouse" },
      { key: "listHoverFg", label: "Row Hover Text", hint: "text on hover" },
      { key: "listActiveBg", label: "Row Active Bg", hint: "clicked row / nav item" },
      { key: "listActiveFg", label: "Row Active Text", hint: "clicked row text" },
      { key: "listHighlight", label: "Match Highlight", hint: "search match in lists" },
    ],
  },
  {
    title: "Status Bar",
    desc: "The bottom strip with GCL status, file name and Ln/Col.",
    fields: [
      { key: "statusBarBg", label: "Background", hint: "bottom strip surface" },
      { key: "statusBarFg", label: "Text", hint: "status items" },
      { key: "statusBarBorder", label: "Border", hint: "top separator line" },
      { key: "statusBarItemHoverBg", label: "Item Hover Bg", hint: "hover highlight" },
    ],
  },
  {
    title: "Menus",
    desc: "Dropdown and right-click menus (File / Project / Build / context menu).",
    fields: [
      { key: "menuBg", label: "Background", hint: "menu surface" },
      { key: "menuFg", label: "Text", hint: "menu item text" },
      { key: "menuSelectionBg", label: "Selected Row Bg", hint: "highlighted item" },
      { key: "menuSelectionFg", label: "Selected Row Text", hint: "highlighted item text" },
      { key: "menuBorder", label: "Border", hint: "menu outline" },
    ],
  },
  {
    title: "Tabs",
    desc: "The file tab strip above the editor.",
    fields: [
      { key: "tabActiveBg", label: "Active Tab Bg", hint: "current file tab" },
      { key: "tabActiveFg", label: "Active Tab Text", hint: "current file name" },
      { key: "tabActiveBorderTop", label: "Active Top Border", hint: "2px accent line" },
      { key: "tabInactiveBg", label: "Inactive Tab Bg", hint: "other tabs" },
      { key: "tabInactiveFg", label: "Inactive Tab Text", hint: "other tab names" },
      { key: "tabHoverBg", label: "Hover Tab Bg", hint: "tab under mouse" },
      { key: "tabBorder", label: "Tab Border", hint: "separator lines" },
    ],
  },
  {
    title: "Editor",
    desc: "The code editing surface (Monaco): background, text, line numbers, cursor and selection.",
    fields: [
      { key: "editorBg", label: "Background", hint: "editor surface" },
      { key: "editorFg", label: "Text", hint: "editor text" },
      { key: "editorLine", label: "Line Highlight", hint: "current line row" },
      { key: "editorLineNum", label: "Line Numbers", hint: "gutter numbers" },
      { key: "editorCursor", label: "Cursor", hint: "caret color" },
      { key: "editorSelection", label: "Selection Bg", hint: "selected code" },
      { key: "editorSelectionFg", label: "Selection Text", hint: "selected code text" },
    ],
  },
  {
    title: "Autocomplete Window",
    desc: "The suggestion popup that appears while typing code.",
    fields: [
      { key: "editorWidget", label: "Popup Background", hint: "popup surface" },
      { key: "editorWidgetSel", label: "Selected Row", hint: "highlighted suggestion" },
      { key: "editorWidgetBorder", label: "Popup Border", hint: "popup outline" },
      { key: "suggestFg", label: "Text", hint: "suggestion names" },
      { key: "suggestHighlight", label: "Highlight", hint: "typed matching letters" },
      { key: "suggestSelectedFg", label: "Selected Row Text", hint: "text on highlighted row" },
    ],
  },
  {
    title: "Panel / Terminal",
    desc: "The bottom Output/Terminal panel and the xterm terminal.",
    fields: [
      { key: "panelBg", label: "Panel Bg", hint: "bottom panel surface" },
      { key: "panelBorder", label: "Panel Border", hint: "top separator line" },
      { key: "panelTitleActiveFg", label: "Active Tab", hint: "OUTPUT / TERMINAL active" },
      { key: "panelTitleInactiveFg", label: "Inactive Tab", hint: "unselected tab" },
      { key: "terminalBg", label: "Terminal Bg", hint: "xterm surface" },
      { key: "terminalFg", label: "Terminal Text", hint: "xterm text" },
      { key: "terminalCursor", label: "Terminal Cursor", hint: "xterm caret" },
      { key: "terminalSelection", label: "Terminal Selection", hint: "xterm selection" },
    ],
  },
  {
    title: "Inputs / Buttons / Badges",
    desc: "Text inputs, selects, buttons and the small labels in the DOCS panel.",
    fields: [
      { key: "inputBg", label: "Input Bg", hint: "text field surface" },
      { key: "inputFg", label: "Input Text", hint: "typed text" },
      { key: "inputBorder", label: "Input Border", hint: "field outline" },
      { key: "inputPlaceholder", label: "Placeholder", hint: "hint text in fields" },
      { key: "selectBg", label: "Select Bg", hint: "dropdown field surface" },
      { key: "selectFg", label: "Select Text", hint: "dropdown field text" },
      { key: "selectBorder", label: "Select Border", hint: "dropdown field outline" },
      { key: "buttonBg", label: "Button Bg", hint: "button fill" },
      { key: "buttonFg", label: "Button Text", hint: "button label" },
      { key: "buttonHoverBg", label: "Button Hover", hint: "button under mouse" },
      { key: "buttonSecondaryBg", label: "Secondary Btn", hint: "Run / Create buttons" },
      { key: "badgeBg", label: "Badge Bg", hint: "docs badge fill" },
      { key: "badgeFg", label: "Badge Text", hint: "docs badge text" },
    ],
  },
];

/* Right-side settings panel: resizable, categorized, live preview of the
 * autocomplete popup. Every change applies instantly (onSave -> App store). */
export default function SettingsPanel({ initial, onClose, onSave }: Props) {
  const [theme, setTheme] = useState<ThemeName>(initial.theme);
  const [fontFamily, setFontFamily] = useState(initial.fontFamily);
  const [fontSize, setFontSize] = useState(initial.fontSize);
  const [textSize, setTextSize] = useState(initial.textSize);
  const [scale, setScale] = useState(initial.scale);
  const [language, setLanguage] = useState<UiLanguage>(initial.language);
  const [custom, setCustom] = useState<Partial<Palette>>(initial.customColors ?? {});
  const [width, setWidth] = useState(DEFAULT_W);

  /* Live palette including manual overrides — drives the preview AND gives
   * the color pickers their current values. */
  const palette = palettesOf({
    theme,
    fontFamily,
    fontSize,
    textSize,
    scale,
    language,
    customColors: custom,
  });

  const commit = (next: {
    theme?: ThemeName;
    fontFamily?: string;
    fontSize?: number;
    textSize?: number;
    scale?: number;
    language?: UiLanguage;
    customColors?: Partial<Palette>;
  }) => {
    const s: IdeSettings = {
      theme: next.theme ?? theme,
      fontFamily: next.fontFamily ?? fontFamily,
      fontSize: next.fontSize ?? fontSize,
      textSize: next.textSize ?? textSize,
      scale: next.scale ?? scale,
      language: next.language ?? language,
      customColors: next.customColors ?? custom,
    };
    onSave(s);
  };

  const setColor = (key: keyof Palette, value: string) => {
    const nextCustom = { ...custom, [key]: value };
    setCustom(nextCustom);
    commit({ customColors: nextCustom });
  };

  const resetColors = () => {
    setCustom({});
    commit({ customColors: {} });
  };

  /* Drag the left edge of the panel to resize it. */
  const startResize = (e: React.MouseEvent) => {
    e.preventDefault();
    const startX = e.clientX;
    const startW = width;
    const move = (ev: MouseEvent) => {
      /* dragging the LEFT edge: moving left grows the panel */
      const w = Math.min(MAX_W, Math.max(MIN_W, startW - (ev.clientX - startX)));
      setWidth(w);
    };
    const up = () => {
      window.removeEventListener("mousemove", move);
      window.removeEventListener("mouseup", up);
      document.body.style.cursor = "";
      document.body.style.userSelect = "";
    };
    window.addEventListener("mousemove", move);
    window.addEventListener("mouseup", up);
    document.body.style.cursor = "col-resize";
    document.body.style.userSelect = "none";
  };

  return (
    <aside className="settings-panel" style={{ width, flexBasis: width }}>
      <div
        className="settings-resizer"
        onMouseDown={startResize}
        title="Drag to resize"
      />
      <div className="settings-panel-head">
        <span className="settings-panel-title">IDE Settings</span>
        <button className="icon-btn" onClick={onClose} title="Close settings">
          <X size={14} />
        </button>
      </div>

      <div className="settings-panel-body">
        {/* EDITOR */}
        <div className="settings-category">Editor</div>
        <div className="settings-group">
          <div className="settings-title">Font</div>
          <input
            className="modal-input"
            value={fontFamily}
            onChange={(e) => {
              setFontFamily(e.target.value);
              commit({ fontFamily: e.target.value });
            }}
            placeholder="'Cascadia Code', Consolas, monospace"
          />
        </div>
        <div className="settings-group">
          <div className="settings-title">Font Size</div>
          <select
            className="modal-select"
            value={fontSize}
            onChange={(e) => {
              const v = Number(e.target.value);
              setFontSize(v);
              commit({ fontSize: v });
            }}
          >
            {[10, 11, 12, 13, 14, 16, 18, 20, 24].map((n) => (
              <option key={n} value={n}>
                {n}px
              </option>
            ))}
          </select>
        </div>

        {/* INTERFACE */}
        <div className="settings-category">Interface</div>
        <div className="settings-group">
          <div className="settings-title">Language / Dil</div>
          <select
            className="modal-select"
            value={language}
            onChange={(e) => {
              const v = e.target.value as UiLanguage;
              setLanguage(v);
              commit({ language: v });
            }}
          >
            <option value="en">English</option>
            <option value="tr">Türkçe</option>
          </select>
        </div>
        <div className="settings-group">
          <div className="settings-title">UI Text Size</div>
          <select
            className="modal-select"
            value={textSize}
            onChange={(e) => {
              const v = Number(e.target.value);
              setTextSize(v);
              commit({ textSize: v });
            }}
          >
            {[11, 12, 13, 14, 15, 16].map((n) => (
              <option key={n} value={n}>
                {n}px
              </option>
            ))}
          </select>
        </div>
        <div className="settings-group">
          <div className="settings-title">UI Scale</div>
          <select
            className="modal-select"
            value={scale}
            onChange={(e) => {
              const v = Number(e.target.value);
              setScale(v);
              commit({ scale: v });
            }}
          >
            {[0.8, 0.9, 1, 1.1, 1.25, 1.5].map((s) => (
              <option key={s} value={s}>
                {Math.round(s * 100)}%
              </option>
            ))}
          </select>
        </div>

        {/* THEME */}
        <div className="settings-category">Theme</div>
        <div className="settings-group">
          <div className="settings-title">Preset (shades of one color)</div>
          <div className="theme-swatches">
            {THEME_NAMES.map((t) => {
              const p = palettesOf({ ...initial, theme: t.name, customColors: custom });
              return (
                <button
                  key={t.name}
                  className={`theme-swatch ${theme === t.name ? "active" : ""}`}
                  style={{ background: p.acc }}
                  title={t.label}
                  onClick={() => {
                    setTheme(t.name);
                    commit({ theme: t.name });
                  }}
                >
                  {theme === t.name ? "✓" : ""}
                </button>
              );
            })}
          </div>
        </div>

        {/* COLORS */}
        <div className="settings-category">
          Colors
          <button
            className="icon-btn reset-colors"
            onClick={resetColors}
            title="Restore theme defaults"
          >
            <RotateCcw size={12} /> reset
          </button>
        </div>
        <div className="color-grid">
          {COLOR_GROUPS.map((group) => (
            <div key={group.title} className="color-group">
              <div className="settings-subtitle">{group.title}</div>
              <p className="color-group-desc">{group.desc}</p>
              {group.fields.map((f) => (
                <label key={f.key} className="color-row">
                  <input
                    type="color"
                    className="color-input"
                    value={palette[f.key]}
                    onChange={(e) => setColor(f.key, e.target.value)}
                  />
                  <span className="color-label">{f.label}</span>
                  <span className="color-hint">{f.hint}</span>
                </label>
              ))}
            </div>
          ))}
        </div>

        {/* PREVIEW: live suggest widget, same as the autocomplete popup */}
        <div className="settings-category">Popup Preview</div>
        <p className="suggest-preview-label">
          Exactly how the autocomplete popup looks with these colors:
        </p>
        <div
          className="suggest-preview"
          style={{
            background: palette.editorWidget,
            border: `1px solid ${palette.border}`,
            color: palette.fg0,
          }}
        >
          <div
            className="suggest-preview-row preview-focused"
            style={{
              background: palette.editorWidgetSel,
              color: palette.fg0,
            }}
          >
            <span className="spk">ƒ</span>
            <span>
              <span style={{ color: palette.acc }}>Init</span>DisplayMode
            </span>
            <span className="spd" style={{ color: palette.fg1 }}>
              pyRaLib.InitDisplayMode(...)
            </span>
          </div>
          <div className="suggest-preview-row" style={{ color: palette.fg0 }}>
            <span className="spk">ƒ</span>
            <span>
              <span style={{ color: palette.acc }}>pr</span>int
            </span>
            <span className="spd" style={{ color: palette.fg1 }}>
              print(...)
            </span>
          </div>
          <div className="suggest-preview-row" style={{ color: palette.fg0 }}>
            <span className="spk">≡</span>
            <span>
              <span style={{ color: palette.acc }}>Au</span>dioDeviceReady
            </span>
            <span className="spd" style={{ color: palette.fg1 }}>
              pyRaLib.AudioDeviceReady(...)
            </span>
          </div>
        </div>
        <p className="modal-hint">
          All changes apply instantly. Use <b>Main Text</b> and{" "}
          <b>Panel Background</b> to fix unreadable popup text.
        </p>
      </div>
    </aside>
  );
}
