import { useCallback, useEffect, useMemo, useRef, useState } from "react";
import {
  BookOpen,
  ChevronDown,
  FolderTree,
  Play,
  Save,
  Settings,
  Square,
} from "lucide-react";
import DocViewer from "./components/DocViewer";
import DocsPanel from "./components/DocsPanel";
import EditorView from "./components/EditorView";
import ErrorBoundary from "./components/ErrorBoundary";
import ExplorerPanel from "./components/ExplorerPanel";
import OutputPanel from "./components/OutputPanel";
import ProjectModal from "./components/ProjectModal";
import SettingsPanel from "./components/SettingsPanel";
import StatusBar from "./components/StatusBar";
import TerminalPanel from "./components/TerminalPanel";
import {
  IdeSettings,
  loadSettings,
  palettesOf,
  saveSettings,
  t,
} from "./ideSettings";
import type { ProjectInfo } from "./types";

/* GnuChanOS kokundeki assets/ kullanimi (logo, welcome arka plani). */
import bgUrl from "../../../../assets/bg.png";
import logoUrl from "../../../../assets/logo.png";

type ProjectModalMode = "create" | "save" | "export";

interface TabFile {
  path: string;
  name: string;
  lang: string;
  content: string;
  savedContent: string;
  modified: boolean;
}

function langOf(path: string): string {
  const ext = path.split(".").pop()?.toLowerCase() ?? "";
  if (ext === "lua") return "lua";
  if (ext === "py") return "python";
  if (ext === "gcsf" || ext === "gclib" || ext === "gcl") return "gcl";
  if (ext === "doc") return "doc";
  if (ext === "gcreference") return "ref";
  return "plaintext";
}

/* guvenli okunabilir dokumanlar (editlenemez, HTML render) */
function isDocLang(lang: string): boolean {
  return lang === "doc" || lang === "ref";
}

let outputCounter = 0;

export default function App() {
  const [root, setRoot] = useState<string>("");
  const [tabs, setTabs] = useState<TabFile[]>([]);
  const [active, setActive] = useState(-1);
  const [output, setOutput] = useState<string[]>([]);
  const [cursor, setCursor] = useState({ line: 1, col: 1 });
  const [gclReady, setGclReady] = useState(false);
  const [refreshKey, setRefreshKey] = useState(0);
  const [bottomTab, setBottomTab] = useState<"output" | "terminal">("output");
  const [projectModal, setProjectModal] = useState<{
    mode: ProjectModalMode;
  } | null>(null);
  const [fileMenuOpen, setFileMenuOpen] = useState(false);
  const [projectMenuOpen, setProjectMenuOpen] = useState(false);
  const [buildMenuOpen, setBuildMenuOpen] = useState(false);
  const [settingsOpen, setSettingsOpen] = useState(false);
  const [sidebarTab, setSidebarTab] = useState<"explorer" | "docs">("explorer");
  const [sidebarOpen, setSidebarOpen] = useState(true);
  const [projectInfo, setProjectInfo] = useState<ProjectInfo | null>(null);
  const [settings, setSettings] = useState<IdeSettings>(() => loadSettings());
  const [sidebarWidth, setSidebarWidth] = useState(240);
  const [bottomHeight, setBottomHeight] = useState(180);

  /* VS Code davranisi: aktif ikona tekrar tiklayinca panel gizlenir,
   * baska ikona tiklayinca o panel acilir. */
  const selectTab = useCallback(
    (tab: "explorer" | "docs") => {
      if (tab === sidebarTab && sidebarOpen) {
        setSidebarOpen(false);
      } else {
        setSidebarTab(tab);
        setSidebarOpen(true);
      }
    },
    [sidebarTab, sidebarOpen],
  );

  const palette = useMemo(() => palettesOf(settings), [settings]);

  /* Tum tema renkleri CSS degiskenlerine baglanir. Herhangi bir ayar veya
   * manuel renk degistirildiginde palette nesnesi yenilenir ve bu effect
   * calisarak tum UI ANINDA guncellenir. */
  useEffect(() => {
    const el = document.documentElement;
    const set = (name: string, value: string) => el.style.setProperty(name, value);

    /* global */
    set("--c-bg0", palette.bg0);
    set("--c-bg1", palette.bg1);
    set("--c-bg2", palette.bg2);
    set("--c-bg3", palette.bg3);
    set("--c-border", palette.border);
    set("--c-fg0", palette.fg0);
    set("--c-fg1", palette.fg1);
    set("--c-fg2", palette.fg2);
    set("--c-acc", palette.acc);
    set("--c-acc-dim", palette.accDim);
    set("--c-alt", palette.alt);
    set("--c-icon", palette.iconFg);
    set("--c-focus", palette.focusBorder);
    set("--c-selection", palette.selectionBg);
    set("--c-scrollbar", palette.scrollbarBg);
    set("--c-scrollbar-hover", palette.scrollbarHoverBg);

    /* title + menu bar */
    set("--c-titlebar-bg", palette.titleBarBg);
    set("--c-titlebar-fg", palette.titleBarFg);
    set("--c-menubar-bg", palette.menuBarBg);
    set("--c-menubar-fg", palette.menuBarFg);

    /* activity bar */
    set("--c-act-bg", palette.activityBarBg);
    set("--c-act-fg", palette.activityBarFg);
    set("--c-act-inactive", palette.activityBarInactiveFg);
    set("--c-act-border", palette.activityBarBorder);
    set("--c-act-active-bg", palette.activityBarActiveBg);
    set("--c-act-badge-bg", palette.activityBarBadgeBg);
    set("--c-act-badge-fg", palette.activityBarBadgeFg);

    /* sidebar / explorer */
    set("--c-side-bg", palette.sideBarBg);
    set("--c-side-fg", palette.sideBarFg);
    set("--c-side-border", palette.sideBarBorder);
    set("--c-side-head-bg", palette.sideBarSectionHeaderBg);
    set("--c-side-head-fg", palette.sideBarSectionHeaderFg);
    set("--c-side-title", palette.sideBarTitleFg);

    /* list */
    set("--c-list-hover-bg", palette.listHoverBg);
    set("--c-list-hover-fg", palette.listHoverFg);
    set("--c-list-active-bg", palette.listActiveBg);
    set("--c-list-active-fg", palette.listActiveFg);
    set("--c-list-highlight", palette.listHighlight);

    /* status bar */
    set("--c-status-bg", palette.statusBarBg);
    set("--c-status-fg", palette.statusBarFg);
    set("--c-status-border", palette.statusBarBorder);
    set("--c-status-hover-bg", palette.statusBarItemHoverBg);

    /* menus */
    set("--c-menu-bg", palette.menuBg);
    set("--c-menu-fg", palette.menuFg);
    set("--c-menu-sel-bg", palette.menuSelectionBg);
    set("--c-menu-sel-fg", palette.menuSelectionFg);
    set("--c-menu-border", palette.menuBorder);

    /* tabs */
    set("--c-tab-active-bg", palette.tabActiveBg);
    set("--c-tab-active-fg", palette.tabActiveFg);
    set("--c-tab-active-border", palette.tabActiveBorderTop);
    set("--c-tab-inactive-bg", palette.tabInactiveBg);
    set("--c-tab-inactive-fg", palette.tabInactiveFg);
    set("--c-tab-hover-bg", palette.tabHoverBg);
    set("--c-tab-border", palette.tabBorder);

    /* editor */
    set("--c-editor-bg", palette.editorBg);
    set("--c-editor-fg", palette.editorFg);
    set("--c-editor-line", palette.editorLine);
    set("--c-editor-linenum", palette.editorLineNum);
    set("--c-editor-cursor", palette.editorCursor);
    set("--c-editor-sel", palette.editorSelection);
    set("--c-editor-sel-fg", palette.editorSelectionFg);

    /* suggest (autocomplete) */
    set("--c-sugg-bg", palette.editorWidget);
    set("--c-sugg-sel", palette.editorWidgetSel);
    set("--c-sugg-border", palette.editorWidgetBorder);
    set("--c-sugg-fg", palette.suggestFg);
    set("--c-sugg-highlight", palette.suggestHighlight);
    set("--c-sugg-sel-fg", palette.suggestSelectedFg);

    /* bottom panel */
    set("--c-panel-bg", palette.panelBg);
    set("--c-panel-border", palette.panelBorder);
    set("--c-panel-title-active", palette.panelTitleActiveFg);
    set("--c-panel-title-inactive", palette.panelTitleInactiveFg);

    /* terminal */
    set("--c-term-bg", palette.terminalBg);
    set("--c-term-fg", palette.terminalFg);
    set("--c-term-cursor", palette.terminalCursor);
    set("--c-term-sel", palette.terminalSelection);

    /* inputs / selects */
    set("--c-input-bg", palette.inputBg);
    set("--c-input-fg", palette.inputFg);
    set("--c-input-border", palette.inputBorder);
    set("--c-input-placeholder", palette.inputPlaceholder);
    set("--c-select-bg", palette.selectBg);
    set("--c-select-fg", palette.selectFg);
    set("--c-select-border", palette.selectBorder);

    /* buttons */
    set("--c-btn-bg", palette.buttonBg);
    set("--c-btn-fg", palette.buttonFg);
    set("--c-btn-hover", palette.buttonHoverBg);
    set("--c-btn-sec", palette.buttonSecondaryBg);

    /* badges */
    set("--c-badge-bg", palette.badgeBg);
    set("--c-badge-fg", palette.badgeFg);

    /* font / scale */
    set("--ui-font-size", `${settings.textSize}px`);
    set("--ui-font", settings.fontFamily);
    set("--scale", String(settings.scale));

    saveSettings(settings);
  }, [palette, settings]);

  /* ---- workspace root ---- */
  useEffect(() => {
    return window.ide.onWorkspaceRoot((r) => {
      setRoot(r);
      window.ide.findGcl().then((gcl) => setGclReady(!!gcl));
    });
  }, []);

  /* ---- chokidar events ---- */
  useEffect(() => {
    return window.ide.onWorkspaceEvent((ev) => {
      if (ev.event === "change" || ev.event === "add" || ev.event === "unlink") {
        setRefreshKey((k) => k + 1);
      }
    });
  }, []);

  /* ---- output ---- */
  useEffect(() => {
    return window.ide.onOutput((line) => {
      const id = outputCounter++;
      setOutput((o) => [...o.slice(-1999), `[${id}] ${line.trimEnd()}`]);
    });
  }, []);

  const pushOutput = useCallback((msg: string) => {
    setOutput((o) => [...o.slice(-1999), msg]);
  }, []);

  /* ---- tabs ---- */
  /* openFile'i tablardan bagimsiz tutar: ardisik openFile cagrilarinda
   * (proje acarken tum scriptler) stale tabs yakalanmaz. */
  const tabsRef = useRef<TabFile[]>([]);
  useEffect(() => {
    tabsRef.current = tabs;
  }, [tabs]);

  const openFile = useCallback(
    async (path: string) => {
      const existing = tabsRef.current.findIndex((t) => t.path === path);
      if (existing >= 0) {
        setActive(existing);
        return;
      }
      try {
        const content = await window.ide.readFile(path);
        const name = path.split(/[\\/]/).pop() ?? path;
        tabsRef.current = [
          ...tabsRef.current,
          {
            path,
            name,
            lang: langOf(path),
            content,
            savedContent: content,
            modified: false,
          },
        ];
        setTabs(tabsRef.current);
        setActive(tabsRef.current.length - 1);
      } catch {
        pushOutput(`[IDE] cannot open file: ${path}`);
      }
    },
    [pushOutput],
  );

  const closeTab = useCallback((idx: number) => {
    setTabs((prev) => prev.filter((_, i) => i !== idx));
    setActive((cur) => {
      if (cur >= idx) return cur - 1;
      return cur;
    });
  }, []);

  const saveActive = useCallback(async () => {
    const t = tabs[active];
    if (!t) return;
    await window.ide.writeFile(t.path, t.content);
    setTabs((prev) =>
      prev.map((tab, i) =>
        i === active ? { ...tab, savedContent: tab.content, modified: false } : tab,
      ),
    );
    /* FULL LSP ODAKLI: kaydedilen her dosya gcl-lsp'ye didChange olarak
     * bildirilir -> workspace yeniden indexlenir. Boylece yeni eklenen
     * import/sembol ("zamber ekledim görünmüyor") bir sonraki completion'da
     * aninda gorunur. Hata sessizce yutulur (gcl-lsp yoksa fallback). */
    try {
      await window.ide.lspDidChange(t.path);
    } catch {
      /* gcl-lsp not running */
    }
    pushOutput(`[IDE] saved: ${t.name}`);
    setFileMenuOpen(false);
  }, [tabs, active, pushOutput]);

  /* ---- Build ---- */
  const buildActive = useCallback(async () => {
    const t = tabs[active];
    if (!t) return;
    setBottomTab("output");
    await window.ide.buildFile(t.path, "build");
    setBuildMenuOpen(false);
  }, [tabs, active]);

  const buildRunActive = useCallback(async () => {
    const t = tabs[active];
    if (!t) return;
    setBottomTab("output");
    await saveActive();
    await window.ide.buildFile(t.path, "buildRun");
    setBuildMenuOpen(false);
  }, [tabs, active, saveActive]);

  const runActive = useCallback(async () => {
    const t = tabs[active];
    if (!t) return;
    setBottomTab("output");
    await window.ide.runFile(t.path);
    setBuildMenuOpen(false);
  }, [tabs, active]);

  /* Calisan gcl/embed surecini durdurur (Run'un yanindaki Stop). */
  const stopActive = useCallback(async () => {
    setBottomTab("output");
    await window.ide.stopRun();
    setBuildMenuOpen(false);
  }, []);

  /* ---- project system ---- */
  /* Proje acildiginda src/ icindeki TUM script dosyalarini acik tab olarak
   * geri yukler ("acik kalan scriptler gene acilir"). Siralama: once
   * main.gcsf, sonra digerleri (dosya adi sirasina gore). */
  const openProjectScripts = useCallback(
    async (dir: string) => {
      let files: string[] = [];
      try {
        files = await window.ide.projectSrcFiles(dir);
      } catch {
        files = [];
      }
      files.sort((a, b) => {
        const ka = a.toLowerCase().replace(/[\\/]/g, "/").endsWith("/main.gcsf")
          ? 0
          : 1;
        const kb = b.toLowerCase().replace(/[\\/]/g, "/").endsWith("/main.gcsf")
          ? 0
          : 1;
        if (ka !== kb) return ka - kb;
        return a.localeCompare(b);
      });
      for (const f of files) await openFile(f);
    },
    [openFile],
  );

  const handleCreateProject = useCallback(
    async (info: ProjectInfo, projectPath?: string) => {
      const dir = projectPath ?? "";
      if (!dir) {
        pushOutput("[Project] No path specified");
        return;
      }
      const res = await window.ide.createProject(dir, info);
      pushOutput(`[Project] ${res.message}`);
      if (res.ok) {
        setProjectInfo(info);
        await window.ide.setWorkspace(dir);
        setRoot(dir);
        setRefreshKey((k) => k + 1);
        await openProjectScripts(dir);
      }
    },
    [pushOutput, openProjectScripts],
  );

  const handleOpenProject = useCallback(async () => {
    const res = await window.ide.selectProjectData();
    if (!res) return;
    const { dir, info } = res;
    if (info) setProjectInfo(info);
    await window.ide.setWorkspace(dir);
    setRoot(dir);
    setRefreshKey((k) => k + 1);
    await openProjectScripts(dir);
    setProjectMenuOpen(false);
    setFileMenuOpen(false);
    pushOutput(
      info
        ? `[Project] ${info.name} opened (${dir})`
        : `[Project] Folder opened: ${dir} (no Project.gcDATA)`,
    );
  }, [pushOutput, openProjectScripts]);

  const handleOpenFile = useCallback(async () => {
    const file = await window.ide.openFileDialog();
    if (file) {
      await openFile(file);
      setFileMenuOpen(false);
    }
  }, [openFile]);

  /* "Open Project" (handleOpenProject) klasor ve Project.gcDATA dosyasini
   * birlikte seciyor — ayri "Open Folder" gereksiz, kaldirildi. */

  const handleSaveProject = useCallback(
    async (info: ProjectInfo) => {
      if (!root) {
        pushOutput("[Project] Open a project first");
        return;
      }
      const res = await window.ide.writeProject(root, info);
      pushOutput(`[Project] ${res.message}`);
      if (res.ok) setProjectInfo(info);
    },
    [root, pushOutput],
  );

  const handleQuit = useCallback(() => {
    window.ide.quit();
  }, []);

  const handleExportProject = useCallback(
    async (info: ProjectInfo) => {
      if (!root) {
        pushOutput("[Project] Open a project first");
        return;
      }
      const res = await window.ide.exportProject(root, info);
      pushOutput(`[Export] ${res.message}`);
      if (res.ok) setProjectInfo(info);
    },
    [root, pushOutput],
  );

  const activeTab = active >= 0 ? tabs[active] : null;

  const toggleSettings = useCallback(() => {
    setSettingsOpen((o) => !o);
  }, []);

  return (
    <div className="app">
      {/* menu bar */}
      <div className="menu-bar">
        <span className="brand">GnuChanIDE</span>
        {projectInfo && (
          <span className="project-name" title={root}>
            {projectInfo.name}
          </span>
        )}

        {/* File */}
        <div className="dropdown">
          <button className="menu-btn" onClick={() => setFileMenuOpen((o) => !o)}>
            {t("menu.file", settings.language)} <ChevronDown size={11} />
          </button>
          {fileMenuOpen && (
            <div className="dropdown-menu">
              <button className="menu-btn" onClick={handleOpenFile}>
                {t("menu.openFile", settings.language)}
              </button>
              <button className="menu-btn" onClick={saveActive}>
                {t("menu.save", settings.language)}
              </button>
              <button className="menu-btn" onClick={() => setProjectModal({ mode: "save" })}>
                {t("menu.saveProject", settings.language)}
              </button>
              <button className="menu-btn" onClick={() => setProjectModal({ mode: "export" })}>
                {t("menu.exportProject", settings.language)}
              </button>
              <button className="menu-btn" onClick={handleQuit}>
                {t("menu.exit", settings.language)}
              </button>
            </div>
          )}
        </div>

        {/* Project */}
        <div className="dropdown">
          <button className="menu-btn" onClick={() => setProjectMenuOpen((o) => !o)}>
            {t("menu.project", settings.language)} <ChevronDown size={11} />
          </button>
          {projectMenuOpen && (
            <div className="dropdown-menu">
              <button className="menu-btn" onClick={handleOpenProject}>
                {t("menu.openProject", settings.language)}
              </button>
              <button className="menu-btn" onClick={() => setProjectModal({ mode: "create" })}>
                {t("menu.newProject", settings.language)}
              </button>
            </div>
          )}
        </div>

        {/* Edit: settings artik activity bar'daki ikondan acilir */}

        {/* Build */}
        <div className="dropdown">
          <button className="menu-btn" onClick={() => setBuildMenuOpen((o) => !o)}>
            {t("menu.build", settings.language)} <ChevronDown size={11} />
          </button>
          {buildMenuOpen && (
            <div className="dropdown-menu">
              <button className="menu-btn" onClick={buildActive}>
                {t("menu.build", settings.language)}
              </button>
              <button className="menu-btn" onClick={buildRunActive}>
                {t("menu.buildRun", settings.language)}
              </button>
              <button className="menu-btn" onClick={runActive}>
                {t("menu.run", settings.language)}
              </button>
            </div>
          )}
        </div>

        <span className="menu-spacer" />

        {/* toolbar actions */}
        <button className="menu-btn run" onClick={runActive} title="Run (F5)">
          <Play size={13} /> {t("menu.run", settings.language)}
        </button>
        <button className="menu-btn danger" onClick={stopActive} title="Stop">
          <Square size={13} /> {t("menu.stop", settings.language)}
        </button>
        <button className="menu-btn" onClick={saveActive} title="Save (Ctrl+S)">
          <Save size={13} /> {t("menu.save", settings.language)}
        </button>
      </div>

      {/* body: welcome screen when no project is open */}
      {!root ? (
        <div
          className="welcome-screen"
          style={{ backgroundImage: `url(${bgUrl})` }}
        >
          <img className="welcome-logo" src={logoUrl} alt="GnuChanIDE" />
          <h1>GnuChanIDE</h1>
          <p>{t("welcome.title", settings.language)}</p>
          <div className="welcome-actions">
            <button className="menu-btn" onClick={() => setProjectModal({ mode: "create" })}>
              {t("menu.newProject", settings.language)}
            </button>
            <button className="menu-btn run" onClick={handleOpenProject}>
              {t("menu.openProject", settings.language)}
            </button>
          </div>
        </div>
      ) : (
        <div className="body">
          <aside
            className={`sidebar ${sidebarOpen ? "" : "collapsed"}`}
            style={{ width: sidebarOpen ? sidebarWidth : 44, minWidth: sidebarOpen ? undefined : 44 }}
          >
            {/* VS Code tarzi sol aktivite cubugu */}
            <div className="activity-bar">
              <button
                className={`at ${sidebarOpen && sidebarTab === "explorer" ? "active" : ""}`}
                title="Explorer"
                onClick={() => selectTab("explorer")}
              >
                <FolderTree size={20} />
                <span className="at-label">Explorer</span>
              </button>
              <button
                className={`at ${sidebarOpen && sidebarTab === "docs" ? "active" : ""}`}
                title="Docs"
                onClick={() => selectTab("docs")}
              >
                <BookOpen size={20} />
                <span className="at-label">Docs</span>
              </button>
              <div className="at-spacer" />
              <button
                className={`at ${settingsOpen ? "active" : ""}`}
                title="Settings"
                onClick={toggleSettings}
              >
                <Settings size={20} />
                <span className="at-label">Settings</span>
              </button>
            </div>
            {/* EXPLORER + DOCS ikisi de mount */}
            <div className={`sidebar-panels ${sidebarOpen ? "" : "hidden"}`}>
              <div className={`panel-tab-wrap ${sidebarTab === "explorer" ? "active" : ""}`}>
                <ErrorBoundary label="Explorer">
                  <ExplorerPanel root={root} onOpen={openFile} refreshKey={refreshKey} />
                </ErrorBoundary>
              </div>
              <div className={`panel-tab-wrap ${sidebarTab === "docs" ? "active" : ""}`}>
                <ErrorBoundary label="Docs">
                  <DocsPanel
                    root={root}
                    onOpen={openFile}
                    refreshKey={refreshKey}
                    language={settings.language}
                  />
                </ErrorBoundary>
              </div>
            </div>
            {sidebarOpen && (
              <div
                className="resizer-x"
                onMouseDown={(e) => {
                  e.preventDefault();
                  const startX = e.clientX;
                  const startW = sidebarWidth;
                  const move = (ev: MouseEvent) => {
                    const w = Math.min(520, Math.max(150, startW + (ev.clientX - startX)));
                    setSidebarWidth(w);
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
                }}
              />
            )}
          </aside>

          <main className="main">
            {/* tab strip */}
            <div className="tab-strip">
              {tabs.map((t, i) => (
                <div
                  key={t.path}
                  className={`tab ${i === active ? "active" : ""}`}
                  onClick={() => setActive(i)}
                >
                  <span>{t.name}</span>
                  {t.modified && <span className="tab-dot">●</span>}
                  <button
                    className="tab-close"
                    onClick={(e) => {
                      e.stopPropagation();
                      closeTab(i);
                    }}
                  >
                    ×
                  </button>
                </div>
              ))}
              {tabs.length === 0 && (
                <div className="tab empty">click a file in Explorer</div>
              )}
            </div>

            {/* editor */}
            <div className="editor-area">
              {activeTab && isDocLang(activeTab.lang) ? (
                <ErrorBoundary label="Doc viewer">
                  <DocViewer path={activeTab.path} />
                </ErrorBoundary>
              ) : activeTab ? (
                <ErrorBoundary label="Editor">
                  <EditorView
                    key={activeTab.path}
                    file={{
                      path: activeTab.path,
                      name: activeTab.name,
                      lang: activeTab.lang,
                    }}
                    root={root}
                    content={activeTab.content}
                    onContent={(v) => {
                      if (v === undefined) return;
                      setTabs((prev) =>
                        prev.map((tab, i) =>
                          i === active
                            ? { ...tab, content: v, modified: v !== tab.savedContent }
                            : tab,
                        ),
                      );
                    }}
                    onSave={saveActive}
                    onRun={runActive}
                    onCursor={(line, col) => setCursor({ line, col })}
                    palette={palette}
                    fontSize={settings.fontSize}
                    fontFamily={settings.fontFamily}
                  />
                </ErrorBoundary>
              ) : (
                <div className="welcome">
                  <h1>GnuChanIDE</h1>
                  <p>Pick a file from the Explorer to start editing.</p>
                </div>
              )}
            </div>

            {/* bottom resizer */}
            <div
              className="resizer-y"
              onMouseDown={(e) => {
                e.preventDefault();
                const startY = e.clientY;
                const startH = bottomHeight;
                const move = (ev: MouseEvent) => {
                  const h = Math.min(480, Math.max(90, startH - (ev.clientY - startY)));
                  setBottomHeight(h);
                };
                const up = () => {
                  window.removeEventListener("mousemove", move);
                  window.removeEventListener("mouseup", up);
                  document.body.style.cursor = "";
                  document.body.style.userSelect = "";
                };
                window.addEventListener("mousemove", move);
                window.addEventListener("mouseup", up);
                document.body.style.cursor = "row-resize";
                document.body.style.userSelect = "none";
              }}
            />

            {/* bottom panel */}
            <div className="bottom" style={{ height: bottomHeight }}>
              <div className="bottom-tabs">
                <button
                  className={`bt ${bottomTab === "output" ? "active" : ""}`}
                  onClick={() => setBottomTab("output")}
                >
                  OUTPUT
                </button>
                <button
                  className={`bt ${bottomTab === "terminal" ? "active" : ""}`}
                  onClick={() => setBottomTab("terminal")}
                >
                  TERMINAL
                </button>
              </div>
              <div className="bottom-body">
                {bottomTab === "output" ? (
                  <OutputPanel data={output} />
                ) : (
                  <ErrorBoundary label="Terminal">
                    <TerminalPanel
                      palette={palette}
                      fontSize={settings.fontSize}
                      fontFamily={settings.fontFamily}
                    />
                  </ErrorBoundary>
                )}
              </div>
            </div>
          </main>

          {/* sag panelde ayarlar */}
          {settingsOpen && (
            <SettingsPanel
              initial={settings}
              onClose={() => setSettingsOpen(false)}
              onSave={(s) => setSettings(s)}
            />
          )}
        </div>
      )}

      <StatusBar
        path={activeTab?.path ?? ""}
        lang={activeTab?.lang ?? ""}
        line={cursor.line}
        col={cursor.col}
        modified={activeTab?.modified ?? false}
        gclReady={gclReady}
      />

      {/* project modals */}
      {projectModal?.mode === "create" && (
        <ProjectModal
          mode="create"
          onClose={() => {
            setProjectModal(null);
            setProjectMenuOpen(false);
            setFileMenuOpen(false);
          }}
          onSubmit={handleCreateProject}
          onBrowse={() => window.ide.openFolderDialog()}
        />
      )}
      {projectModal?.mode === "save" && (
        <ProjectModal
          mode="save"
          initial={projectInfo ?? undefined}
          onClose={() => {
            setProjectModal(null);
            setFileMenuOpen(false);
          }}
          onSubmit={handleSaveProject}
        />
      )}
      {projectModal?.mode === "export" && (
        <ProjectModal
          mode="export"
          initial={projectInfo ?? undefined}
          onClose={() => {
            setProjectModal(null);
            setFileMenuOpen(false);
          }}
          onSubmit={handleExportProject}
        />
      )}
    </div>
  );
}
