/* IDE ayarlari: tema (tek rengin tonlari), font, text size, UI scale.
 * localStorage'da gcl-ide-settings anahtarinda saklanir.
 *
 * Renk sistemi VS Code "colorCustomizations" gibi DURUMLU/bolgesel:
 * her alan hangi parcanin (title bar, activity bar, sidebar, status bar,
 * editor, autocomplete...) rengi oldugunu adiyla anlatir. Tum alanlar
 * App.tsx'ten CSS degiskenlerine baglanir; degisiklik ANINDA uygulanir. */

export type ThemeName =
  | "purple"
  | "blue"
  | "red"
  | "green"
  | "orange"
  | "pink"
  | "cyan"
  | "yellow"
  | "dark"
  | "black"
  | "white";

export interface Palette {
  /* ---- global ---- */
  bg0: string; // darkest background
  bg1: string; // panel background
  bg2: string; // hover / raised surface
  bg3: string; // active surface / tab
  border: string; // separators
  fg0: string; // main text
  fg1: string; // secondary text
  fg2: string; // dim text
  acc: string; // accent
  accDim: string; // muted accent
  alt: string; // contrast accent
  iconFg: string; // icons
  focusBorder: string; // focused element border
  selectionBg: string; // generic selection
  scrollbarBg: string; // scrollbar thumb
  scrollbarHoverBg: string; // scrollbar thumb hover

  /* ---- title bar / menu bar ---- */
  titleBarBg: string;
  titleBarFg: string;
  menuBarBg: string;
  menuBarFg: string;

  /* ---- activity bar (sol ikon cubugu) ---- */
  activityBarBg: string;
  activityBarFg: string;
  activityBarInactiveFg: string;
  activityBarBorder: string;
  activityBarActiveBg: string;
  activityBarBadgeBg: string;
  activityBarBadgeFg: string;

  /* ---- sidebar / explorer ---- */
  sideBarBg: string;
  sideBarFg: string;
  sideBarBorder: string;
  sideBarSectionHeaderBg: string;
  sideBarSectionHeaderFg: string;
  sideBarTitleFg: string;

  /* ---- list (explorer satirlari) ---- */
  listHoverBg: string;
  listHoverFg: string;
  listActiveBg: string;
  listActiveFg: string;
  listHighlight: string;

  /* ---- status bar ---- */
  statusBarBg: string;
  statusBarFg: string;
  statusBarBorder: string;
  statusBarItemHoverBg: string;

  /* ---- menuler (dropdown + context) ---- */
  menuBg: string;
  menuFg: string;
  menuSelectionBg: string;
  menuSelectionFg: string;
  menuBorder: string;

  /* ---- tab strip ---- */
  tabActiveBg: string;
  tabActiveFg: string;
  tabActiveBorderTop: string;
  tabInactiveBg: string;
  tabInactiveFg: string;
  tabHoverBg: string;
  tabBorder: string;

  /* ---- editor ---- */
  editorBg: string; // editor background
  editorFg: string; // editor text
  editorLine: string; // line highlight
  editorLineNum: string; // line number color
  editorCursor: string; // cursor color
  editorSelection: string; // selection background
  editorSelectionFg: string; // selection text

  /* ---- autocomplete (suggest widget) ---- */
  editorWidget: string; // suggest popup background
  editorWidgetSel: string; // selected row background
  editorWidgetBorder: string; // popup border
  suggestFg: string; // popup text
  suggestHighlight: string; // matching letters
  suggestSelectedFg: string; // selected row text

  /* ---- bottom panel (output/terminal) ---- */
  panelBg: string;
  panelBorder: string;
  panelTitleActiveFg: string;
  panelTitleInactiveFg: string;

  /* ---- terminal ---- */
  terminalBg: string;
  terminalFg: string;
  terminalCursor: string;
  terminalSelection: string;

  /* ---- inputs / selects ---- */
  inputBg: string;
  inputFg: string;
  inputBorder: string;
  inputPlaceholder: string;
  selectBg: string;
  selectFg: string;
  selectBorder: string;

  /* ---- buttons ---- */
  buttonBg: string;
  buttonFg: string;
  buttonHoverBg: string;
  buttonSecondaryBg: string;

  /* ---- badges ---- */
  badgeBg: string;
  badgeFg: string;
}

export type UiLanguage = "en" | "tr";

export interface IdeSettings {
  theme: ThemeName;
  fontFamily: string;
  fontSize: number;
  textSize: number; // UI base font size (px)
  scale: number; // UI scale multiplier 0.8 .. 1.5
  /* Arayuz dili: "en" (varsayilan) veya "tr". Tüm UI string'leri
   * ideSettings.ts icindeki t() sozlugunden gelir; bu deger degisince
   * arayuz aninda guncellenir. */
  language: UiLanguage;
  /* Manuel renk override'lari — tema paletinin ustune biner.
   * Bos obje = tema varsayilani. Opsiyoneldir; eski localStorage
   * degerleri (customColors'suz) tip hatasi vermez. */
  customColors?: Partial<Palette>;
}
export const DEFAULT_SETTINGS: IdeSettings = {
  theme: "purple",
  fontFamily: "'Cascadia Code', Consolas, monospace",
  fontSize: 14,
  textSize: 13,
  scale: 1,
  language: "en",
  customColors: {},
};

const KEY = "gcl-ide-settings";

export const THEME_NAMES: { name: ThemeName; label: string; color: string }[] = [
  { name: "purple", label: "Purple", color: "#7b2ff7" },
  { name: "blue", label: "Blue", color: "#2f6df7" },
  { name: "red", label: "Red", color: "#e63946" },
  { name: "green", label: "Green", color: "#3fb950" },
  { name: "orange", label: "Orange", color: "#ff7b1c" },
  { name: "pink", label: "Pink", color: "#ff4d8d" },
  { name: "cyan", label: "Cyan", color: "#2dd4bf" },
  { name: "yellow", label: "Yellow", color: "#e3b341" },
  { name: "dark", label: "Dark", color: "#3b4252" },
  { name: "black", label: "Black", color: "#1a1a1a" },
  { name: "white", label: "White", color: "#f5f5f5" },
];


/* Renk turetme: Monaco `defineTheme` token renkleri YALNIZCA hex kabul eder.
 * Ayrica Monaco bazi alfalari (#RRGGBBAA) reddeder -> yazi kaybolur. Bu
 * yuzden a == 1 ise SAF 6-haneli #RRGGBB; alfa yalnizca gereken alanlarda. */
function hslToHex(h: number, s: number, l: number, a = 1): string {
  const sn = Math.min(Math.max(s, 0), 100) / 100;
  const ln = Math.min(Math.max(l, 0), 100) / 100;
  const an = Math.min(Math.max(a, 0), 1);
  const hp = ((h % 360) + 360) % 360 / 60;
  const c = (1 - Math.abs(2 * ln - 1)) * sn;
  const x = c * (1 - Math.abs((hp % 2) - 1));
  let r = 0;
  let g = 0;
  let b = 0;
  if (hp < 1) { r = c; g = x; }
  else if (hp < 2) { r = x; g = c; }
  else if (hp < 3) { g = c; b = x; }
  else if (hp < 4) { g = x; b = c; }
  else if (hp < 5) { r = x; b = c; }
  else { r = c; b = x; }
  const m = ln - c / 2;
  const bytes = [r + m, g + m, b + m].map((n) =>
    Math.round(n * 255).toString(16).padStart(2, "0"),
  );
  if (an >= 1) return `#${bytes.join("")}`;
  return `#${bytes.join("")}${Math.round(an * 255).toString(16).padStart(2, "0")}`;
}

/* Tema doygunluk carpani: renklerin "ton" orani. Renkli temalarda 1
 * (tam renklilik), dark'ta dusuk (koyu mavi-gri), white/black'ta 0
 * (SAF MONOKROM — sadece siyah-beyaz arasi tonlar). */
let SAT = 1;
const SHADE = (h: number, s: number, l: number, a = 1) =>
  hslToHex(h, Math.min(100, s * SAT), l, a);

function palette(hue: number, light: boolean): Palette {
  if (light) {
    const b = 250;
    const f = hue;
    return {
      bg0: SHADE(b, 8, 96),
      bg1: SHADE(b, 10, 92),
      bg2: SHADE(b, 12, 86),
      bg3: SHADE(b, 14, 80),
      border: SHADE(b, 10, 74),
      fg0: SHADE(f, 20, 16),
      fg1: SHADE(f, 12, 34),
      fg2: SHADE(f, 8, 52),
      acc: SHADE(f, 80, 38),
      accDim: SHADE(f, 50, 82),
      alt: SHADE((f + 30) % 360, 70, 40),
      iconFg: SHADE(f, 40, 30),
      focusBorder: SHADE(f, 90, 40),
      selectionBg: SHADE(f, 50, 82),
      scrollbarBg: SHADE(b, 12, 74),
      scrollbarHoverBg: SHADE(b, 10, 58),
      titleBarBg: SHADE(b, 14, 88),
      titleBarFg: SHADE(f, 30, 18),
      menuBarBg: SHADE(b, 12, 92),
      menuBarFg: SHADE(f, 20, 16),
      activityBarBg: SHADE(b, 14, 90),
      activityBarFg: SHADE(f, 50, 25),
      activityBarInactiveFg: SHADE(b, 10, 55),
      activityBarBorder: SHADE(b, 10, 74),
      activityBarActiveBg: SHADE(f, 40, 82),
      activityBarBadgeBg: SHADE(f, 80, 38),
      activityBarBadgeFg: "#ffffff",
      sideBarBg: SHADE(b, 12, 92),
      sideBarFg: SHADE(f, 20, 20),
      sideBarBorder: SHADE(b, 10, 74),
      sideBarSectionHeaderBg: SHADE(b, 14, 86),
      sideBarSectionHeaderFg: SHADE(f, 30, 25),
      sideBarTitleFg: SHADE(f, 80, 38),
      listHoverBg: SHADE(b, 16, 82),
      listHoverFg: SHADE(f, 30, 15),
      listActiveBg: SHADE(f, 50, 80),
      listActiveFg: SHADE(f, 60, 15),
      listHighlight: SHADE(f, 90, 40),
      statusBarBg: SHADE(f, 50, 30),
      statusBarFg: "#ffffff",
      statusBarBorder: SHADE(b, 10, 74),
      statusBarItemHoverBg: SHADE(f, 50, 45),
      menuBg: SHADE(b, 14, 94),
      menuFg: SHADE(f, 20, 16),
      menuSelectionBg: SHADE(f, 50, 80),
      menuSelectionFg: SHADE(f, 60, 12),
      menuBorder: SHADE(b, 10, 70),
      tabActiveBg: SHADE(f, 45, 88),
      tabActiveFg: SHADE(f, 40, 12),
      tabActiveBorderTop: SHADE(f, 90, 40),
      tabInactiveBg: SHADE(b, 14, 90),
      tabInactiveFg: SHADE(f, 15, 45),
      tabHoverBg: SHADE(b, 18, 84),
      tabBorder: SHADE(b, 10, 74),
      editorBg: SHADE(b, 8, 97),
      editorFg: SHADE(f, 25, 15),
      editorLine: SHADE(b, 14, 90),
      editorLineNum: SHADE(b, 8, 62),
      editorCursor: SHADE(f, 80, 38),
      editorSelection: SHADE(f, 50, 82),
      editorSelectionFg: SHADE(f, 60, 10),
      editorWidget: SHADE(b, 12, 95),
      editorWidgetSel: SHADE(f, 30, 85),
      editorWidgetBorder: SHADE(f, 60, 60),
      suggestFg: SHADE(f, 25, 15),
      suggestHighlight: SHADE(f, 90, 40),
      suggestSelectedFg: SHADE(f, 60, 10),
      panelBg: SHADE(b, 12, 95),
      panelBorder: SHADE(b, 10, 74),
      panelTitleActiveFg: SHADE(f, 80, 38),
      panelTitleInactiveFg: SHADE(f, 15, 45),
      terminalBg: SHADE(b, 8, 95),
      terminalFg: SHADE(f, 20, 18),
      terminalCursor: SHADE(f, 80, 35),
      terminalSelection: SHADE(f, 40, 78),
      inputBg: SHADE(b, 12, 96),
      inputFg: SHADE(f, 25, 15),
      inputBorder: SHADE(b, 10, 68),
      inputPlaceholder: SHADE(f, 10, 55),
      selectBg: SHADE(b, 12, 96),
      selectFg: SHADE(f, 25, 15),
      selectBorder: SHADE(b, 10, 68),
      buttonBg: SHADE(f, 80, 38),
      buttonFg: "#ffffff",
      buttonHoverBg: SHADE(f, 80, 30),
      buttonSecondaryBg: SHADE(f, 45, 75),
      badgeBg: SHADE(f, 80, 38),
      badgeFg: "#ffffff",
    };
  }
  // koyu tema
  return {
    bg0: SHADE(hue, 33, 7),
    bg1: SHADE(hue, 30, 11),
    bg2: SHADE(hue, 28, 16),
    bg3: SHADE(hue, 26, 21),
    border: SHADE(hue, 22, 26),
    fg0: SHADE(hue, 20, 88),
    fg1: SHADE(hue, 16, 72),
    fg2: SHADE(hue, 12, 52),
    acc: SHADE(hue, 90, 64),
    accDim: SHADE(hue, 45, 26),
    alt: SHADE((hue + 40) % 360, 85, 64),
    iconFg: SHADE(hue, 30, 82),
    focusBorder: SHADE(hue, 90, 66),
    selectionBg: SHADE(hue, 45, 32),
    scrollbarBg: SHADE(hue, 26, 21),
    scrollbarHoverBg: SHADE(hue, 20, 40),
    titleBarBg: SHADE(hue, 33, 7),
    titleBarFg: SHADE(hue, 40, 90),
    menuBarBg: SHADE(hue, 30, 11),
    menuBarFg: SHADE(hue, 20, 88),
    activityBarBg: SHADE(hue, 33, 7),
    activityBarFg: SHADE(hue, 30, 80),
    activityBarInactiveFg: SHADE(hue, 15, 50),
    activityBarBorder: SHADE(hue, 22, 26),
    activityBarActiveBg: SHADE(hue, 26, 21),
    activityBarBadgeBg: SHADE(hue, 90, 64),
    activityBarBadgeFg: SHADE(hue, 40, 8),
    sideBarBg: SHADE(hue, 30, 11),
    sideBarFg: SHADE(hue, 20, 88),
    sideBarBorder: SHADE(hue, 22, 26),
    sideBarSectionHeaderBg: SHADE(hue, 26, 16),
    sideBarSectionHeaderFg: SHADE(hue, 16, 72),
    sideBarTitleFg: SHADE((hue + 40) % 360, 85, 64),
    listHoverBg: SHADE(hue, 28, 16),
    listHoverFg: SHADE(hue, 20, 92),
    listActiveBg: SHADE(hue, 26, 21),
    listActiveFg: SHADE(hue, 20, 96),
    listHighlight: SHADE(hue, 90, 70),
    statusBarBg: SHADE(hue, 26, 21),
    statusBarFg: SHADE(hue, 16, 72),
    statusBarBorder: SHADE(hue, 22, 26),
    statusBarItemHoverBg: SHADE(hue, 28, 30),
    menuBg: SHADE(hue, 30, 11),
    menuFg: SHADE(hue, 20, 88),
    menuSelectionBg: SHADE(hue, 26, 21),
    menuSelectionFg: SHADE(hue, 20, 96),
    menuBorder: SHADE(hue, 22, 26),
    tabActiveBg: SHADE(hue, 26, 21),
    tabActiveFg: SHADE(hue, 20, 90),
    tabActiveBorderTop: SHADE(hue, 90, 64),
    tabInactiveBg: SHADE(hue, 33, 7),
    tabInactiveFg: SHADE(hue, 12, 52),
    tabHoverBg: SHADE(hue, 28, 16),
    tabBorder: SHADE(hue, 22, 26),
    editorBg: SHADE(hue, 33, 8),
    editorFg: SHADE(hue, 20, 86),
    editorLine: SHADE(hue, 30, 13),
    editorLineNum: SHADE(hue, 16, 38),
    editorCursor: SHADE(hue, 100, 68),
    editorSelection: SHADE(hue, 45, 30, 0.6),
    editorSelectionFg: SHADE(hue, 20, 96),
    editorWidget: SHADE(hue, 30, 12),
    editorWidgetSel: SHADE(hue, 35, 21),
    editorWidgetBorder: SHADE(hue, 22, 26),
    suggestFg: SHADE(hue, 20, 88),
    suggestHighlight: SHADE(hue, 90, 64),
    suggestSelectedFg: SHADE(hue, 20, 96),
    panelBg: SHADE(hue, 33, 7),
    panelBorder: SHADE(hue, 22, 26),
    panelTitleActiveFg: SHADE(hue, 90, 64),
    panelTitleInactiveFg: SHADE(hue, 12, 52),
    terminalBg: SHADE(hue, 33, 7),
    terminalFg: SHADE(hue, 16, 72),
    terminalCursor: SHADE(hue, 100, 68),
    terminalSelection: SHADE(hue, 45, 30, 0.6),
    inputBg: SHADE(hue, 33, 8),
    inputFg: SHADE(hue, 20, 88),
    inputBorder: SHADE(hue, 22, 26),
    inputPlaceholder: SHADE(hue, 12, 52),
    selectBg: SHADE(hue, 33, 8),
    selectFg: SHADE(hue, 20, 88),
    selectBorder: SHADE(hue, 22, 26),
    buttonBg: SHADE(hue, 45, 26),
    buttonFg: SHADE(hue, 20, 94),
    buttonHoverBg: SHADE(hue, 50, 34),
    buttonSecondaryBg: SHADE(hue, 26, 21),
    badgeBg: SHADE(hue, 90, 64),
    badgeFg: SHADE(hue, 40, 8),
  };
}

const HUES: Record<Exclude<ThemeName, "black" | "white">, number> = {
  purple: 270,
  blue: 215,
  red: 352,
  green: 140,
  orange: 24,
  pink: 330,
  cyan: 176,
  yellow: 45,
  dark: 222,
};
/* Black / White: SAT=0 -> SAF MONOKROM (gercek siyah-beyaz tonlari).
 * Dark: SAT=0.12 -> koyu mavi-gri (renksiz hissi, hafif serin ton).
 * Renkli temalar: SAT=1 -> tam renklilik. */
const GRAY_HUE = 220;

export function palettesOf(settings: IdeSettings): Palette {
  const t = settings.theme;
  const isGray = t === "black" || t === "white";
  SAT = isGray ? 0 : t === "dark" ? 0.12 : 1;
  const hue = isGray ? GRAY_HUE : (HUES[t] ?? HUES.purple);
  const base = palette(hue, t === "white");
  /* manuel renk override'lari tema paletinin ustune biner (undefined ise no-op) */
  return { ...base, ...settings.customColors };
}

/* UI dil sozlugu: t("key", lang). Varsayilan "en"; "tr" desteklenir.
 * IDE'de kullaniciya gorunen metinlerin tek dogruluk kaynagi burasidir. */
const STRINGS: Record<string, { en: string; tr: string }> = {
  "menu.file": { en: "File", tr: "Dosya" },
  "menu.project": { en: "Project", tr: "Proje" },
  "menu.build": { en: "Build", tr: "Derle" },
  "menu.openFile": { en: "Open File", tr: "Dosya Aç" },
  "menu.openFolder": { en: "Open Folder", tr: "Klasör Aç" },
  "menu.save": { en: "Save", tr: "Kaydet" },
  "menu.saveProject": { en: "Save Project", tr: "Projeyi Kaydet" },
  "menu.exportProject": { en: "Export Project", tr: "Projeyi Dışa Aktar" },
  "menu.exit": { en: "Exit", tr: "Çıkış" },
  "menu.newProject": { en: "New Project", tr: "Yeni Proje" },
  "menu.openProject": { en: "Open Project", tr: "Proje Aç" },
  "menu.buildRun": { en: "Build & Run", tr: "Derle ve Çalıştır" },
  "menu.run": { en: "Run", tr: "Çalıştır" },
  "menu.stop": { en: "Stop", tr: "Durdur" },
  "welcome.title": {
    en: "Create or open a GCL project — the file tree appears here.",
    tr: "Bir GCL projesi oluşturun veya açın — dosya ağacı burada görünür.",
  },
  "welcome.editor": {
    en: "Pick a file from the Explorer to start editing.",
    tr: "Düzenlemek için Explorer'dan bir dosya seçin.",
  },
  "status.gcl": { en: "GCL ✓", tr: "GCL ✓" },
  "status.noGcl": { en: "gcl not found", tr: "gcl bulunamadı" },
  "status.noFile": { en: "no file", tr: "dosya yok" },
  "docs.guides": { en: "Guides", tr: "Rehberler" },
  "docs.api": { en: "API Reference", tr: "İmzalı API" },
  "docs.helpers": { en: "Helpers", tr: "Yardımcılar" },
  "docs.title": { en: "Docs", tr: "Dokümanlar" },
  "docs.empty": {
    en: "No docs found in Library/.",
    tr: "Library/ içinde doküman bulunamadı.",
  },
  "docs.open": {
    en: "Open a project to see docs.",
    tr: "Dokümanları görmek için bir proje açın.",
  },
  "settings.title": { en: "Settings", tr: "Ayarlar" },
  "settings.language": { en: "Language", tr: "Dil" },
  "tab.empty": {
    en: "click a file in Explorer",
    tr: "Explorer'da bir dosyaya tıklayın",
  },
  "output.empty": { en: "Waiting for output...", tr: "Çıktı bekleniyor..." },
  "explorer.empty": { en: "Folder is empty.", tr: "Klasör boş." },
};

export function t(key: string, lang: UiLanguage): string {
  const s = STRINGS[key];
  if (!s) return key;
  return s[lang] ?? s.en;
}

export function loadSettings(): IdeSettings {
  try {
    const raw = localStorage.getItem(KEY);
    if (!raw) return DEFAULT_SETTINGS;
    const parsed = JSON.parse(raw) as Partial<IdeSettings>;
    return {
      theme: parsed.theme ?? DEFAULT_SETTINGS.theme,
      fontFamily: parsed.fontFamily ?? DEFAULT_SETTINGS.fontFamily,
      fontSize: parsed.fontSize ?? DEFAULT_SETTINGS.fontSize,
      textSize: parsed.textSize ?? DEFAULT_SETTINGS.textSize,
      scale: parsed.scale ?? DEFAULT_SETTINGS.scale,
      language: (parsed.language as UiLanguage) ?? DEFAULT_SETTINGS.language,
      customColors: parsed.customColors ?? {},
    };
  } catch {
    return DEFAULT_SETTINGS;
  }
}

export function saveSettings(s: IdeSettings) {
  try {
    localStorage.setItem(KEY, JSON.stringify(s));
  } catch {
    /* localStorage kapali olabilir */
  }
}
