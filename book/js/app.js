/* GnuchanOS — GCL Book
   English-only. Content is loaded directly from *-en.json data files. */

(function () {
  "use strict";

  const urlParams = new URLSearchParams(window.location.search);
  const state = {
    activeTab: urlParams.get("tab") || localStorage.getItem("gclbook-tab") || "gcl",
    activeChapter: urlParams.get("chapter") || localStorage.getItem("gclbook-chapter") || null,
    ui: null,
    content: null
  };

  /* Dil kavramı tamamen kaldırıldı — eski `gclbook-lang` localStorage kalıntısı
     "tr" olsa bile artık okunmuyor. Temizlik için sil. */
  localStorage.removeItem("gclbook-lang");

  function updateUrl() {
    const p = new URLSearchParams();
    p.set("tab", state.activeTab);
    if (state.activeChapter) p.set("chapter", state.activeChapter);
    const qs = p.toString();
    history.replaceState(null, "", window.location.pathname + "?" + qs);
  }

  const contentCache = {};

  /* ---------- Utilities ---------- */

  /* Entity string'lerini runtime'da inşa ediyoruz; dosyada "<" gibi
     literal yok, böylece editör/tool pipeline'i bozamaz. */
  function ent(name) { return String.fromCharCode(38) + name; }

  function escapeHtml(s) {
    return String(s)
      .replace(/&/g, ent("amp;"))
      .replace(/</g, ent("lt;"))
      .replace(/>/g, ent("gt;"))
      .replace(/"/g, ent("quot;"))
      .replace(/'/g, ent("#39;"));
  }

  /* Basit GCL sözdizimi vurgulama — HTML'e escape edilmiş metin üzerinde regex */
  function highlight(src) {
    let html = escapeHtml(src);
    html = html.replace(/(\/\/[^\n]*|#[^\n]*)/g, '<span class="cm">$1</span>');
    var qs = ent("quot;");
    var as = ent("#39;");
    var strRe = new RegExp("(" + qs + "[^&]*?" + qs + "|" + as + "[^&]*?" + as + ")", "g");
    html = html.replace(strRe, '<span class="str">$1</span>');
    html = html.replace(/\b(\d[\d_.]*)([fFlL]?)\b/g, '<span class="num">$1$2</span>');
    html = html.replace(/(#[a-zA-Z_]+)/g, '<span class="pre">$1</span>');
    const kws = ["if","else","while","for","switch","case","default","break","continue","return","struct","enum","typedef","const","public","private","global","inline","sizeof","strlen","true","false","null","void","defined"];
    const kwRe = new RegExp("\\b(" + kws.join("|") + ")\\b", "g");
    html = html.replace(kwRe, '<span class="kw">$1</span>');
    const types = ["int8","int16","int32","int64","int128","float16","float32","float64","float128","uint8","uint16","uint32","uint64","uint128","char","short","int","long","float","double","unsigned","bool","gcChar"];
    const tyRe = new RegExp("\\b(" + types.join("|") + ")\\b", "g");
    html = html.replace(tyRe, '<span class="ty">$1</span>');
    html = html.replace(/\b([a-zA-Z_][a-zA-Z0-9_]*)\s*\(/g, '<span class="fn">$1</span>(');
    return html;
  }

  function createElement(tag, cls, html) {
    const el = document.createElement(tag);
    if (cls) el.className = cls;
    if (html !== undefined) el.innerHTML = html;
    return el;
  }

  /* ---------- Data loading ---------- */

  async function fetchJson(url) {
    const resp = await fetch(url);
    if (!resp.ok) throw new Error("HTTP " + resp.status + " " + url);
    return resp.json();
  }

  async function loadContent(tab) {
    const map = {
      ui: "data/ui-en.json",
      gcl: "data/gcl-en.json",
      lua: "data/lua-en.json",
      python: "data/python-en.json"
    };
    if (["gcl","lua","python"].includes(tab)) {
      const key = tab + "-en";
      if (!contentCache[key]) {
        contentCache[key] = await fetchJson(map[tab]);
      }
      return contentCache[key];
    }
    return null;
  }

  /* ---------- Render ---------- */

  function renderTopbar() {
    const ui = state.ui;
    const topbar = document.getElementById("topbar");
    topbar.innerHTML = "";

    const menuBtn = createElement("button", "menu-btn");
    menuBtn.id = "menuBtn";
    menuBtn.textContent = "\u2630";
    menuBtn.setAttribute("aria-label", ui.menu);

    const brand = createElement("div", "brand");
    brand.innerHTML = '<span class="brand-logo">G</span><span class="brand-text">' +
      escapeHtml(ui.brand) + ' <small>' + escapeHtml(ui.brandSub) + "</small></span>";

    const spacer = createElement("div", "topbar-spacer");

    const tabs = createElement("div", "tabs");
    ["gcl", "lua", "python"].forEach((key) => {
      const btn = createElement("button", "tab");
      btn.dataset.tab = key;
      btn.textContent = ui.tabs[key];
      if (key === "lua" || key === "python") btn.classList.add("coming");
      if (key === state.activeTab) btn.classList.add("active");
      btn.addEventListener("click", () => { setTab(key); });
      tabs.appendChild(btn);
    });

    topbar.appendChild(menuBtn);
    topbar.appendChild(brand);
    topbar.appendChild(spacer);
    topbar.appendChild(tabs);
  }

  function renderSidebar(data) {
    const ui = state.ui;
    const sidebar = document.getElementById("sidebar");
    sidebar.innerHTML = "";

    const heading = createElement("h2", null, escapeHtml(ui.sidebar.chapters));
    sidebar.appendChild(heading);

    if (!data || !data.chapters) {
      const coming = createElement("div", "sidebar-coming");
      coming.innerHTML = "<strong>" + escapeHtml(ui.sidebar.comingSoon) + "</strong><br>" +
        escapeHtml(ui.sidebar.comingSoonDesc);
      sidebar.appendChild(coming);
      return;
    }

    data.chapters.forEach((ch, idx) => {
      const btn = createElement("button", "chapter");
      btn.dataset.chapter = ch.id;
      const activeId = state.activeChapter || data.chapters[0].id;
      if (ch.id === activeId) btn.classList.add("active");
      btn.innerHTML = '<span class="idx">' + (idx + 1) + "</span>" + escapeHtml(ch.title);
      btn.addEventListener("click", () => setChapter(ch.id, true));
      sidebar.appendChild(btn);
    });

    const coming = createElement("div", "sidebar-coming");
    coming.innerHTML = "<strong>" + escapeHtml(ui.sidebar.comingSoon) + "</strong><br>" +
      escapeHtml(ui.sidebar.comingSoonDesc);
    sidebar.appendChild(coming);
  }

  function renderContent(data) {
    const ui = state.ui;
    const content = document.getElementById("content");
    content.innerHTML = "";

    if (!data || !data.chapters) {
      const card = createElement("div", "coming-card");
      card.innerHTML = '<div class="big">\u{1F6A7}</div>' +
        "<h1>" + escapeHtml(ui.comingCard.title) + "</h1>" +
        "<p>" + escapeHtml(ui.comingCard.desc) + "</p>" +
        '<span class="badge">' + escapeHtml(ui.comingCard.badge) + "</span>";
      content.appendChild(card);
      return;
    }

    const activeId = state.activeChapter || data.chapters[0].id;
    const chapter = data.chapters.find((ch) => ch.id === activeId) || data.chapters[0];
    state.activeChapter = chapter.id;
    localStorage.setItem("gclbook-chapter", chapter.id);

    const crumb = createElement("div", "crumb");
    crumb.textContent = data.meta.title + " / " + chapter.title;
    content.appendChild(crumb);

    const header = createElement("div", "book-header");
    header.innerHTML = '<span class="hero-badge">\u2728 ' + escapeHtml(data.meta.badge) + "</span>" +
      "<h1>" + escapeHtml(data.meta.title) + "</h1>" +
      "<p>" + escapeHtml(data.meta.subtitle) + "</p>";
    content.appendChild(header);

    const body = createElement("div", "chapter-body");
    chapter.sections.forEach((sec) => {
      const secEl = createElement("div", "section");
      secEl.appendChild(createElement("h2", null, escapeHtml(sec.title)));

      (sec.paragraphs || []).forEach((p) => {
        secEl.appendChild(createElement("p", null, p));
      });

      if (sec.code && sec.code.src) {
        const block = createElement("div", "code-block");
        const head = createElement("div", "code-head");
        head.innerHTML = '<span class="dot r"></span><span class="dot y"></span><span class="dot g"></span>' +
          '<span class="fn">' + escapeHtml(sec.code.file || "code") + "</span>";
        const pre = createElement("pre");
        pre.innerHTML = highlight(sec.code.src);
        block.appendChild(head);
        block.appendChild(pre);
        secEl.appendChild(block);
      }

      if (sec.output) {
        const out = createElement("div", "note tip");
        out.innerHTML = '<span class="icon">\u25B6</span><div class="body">' +
          "<strong>" + escapeHtml(ui.labels.output) + "</strong><pre>" + escapeHtml(sec.output) + "</pre></div>";
        secEl.appendChild(out);
      }

      if (sec.table && sec.table.headers && sec.table.rows) {
        const table = document.createElement("table");
        const thead = document.createElement("thead");
        const trh = document.createElement("tr");
        sec.table.headers.forEach((h) => {
          const th = document.createElement("th");
          th.innerHTML = h;
          trh.appendChild(th);
        });
        thead.appendChild(trh);
        table.appendChild(thead);
        const tbody = document.createElement("tbody");
        sec.table.rows.forEach((row) => {
          const tr = document.createElement("tr");
          row.forEach((cell) => {
            const td = document.createElement("td");
            td.innerHTML = cell;
            tr.appendChild(td);
          });
          tbody.appendChild(tr);
        });
        table.appendChild(tbody);
        secEl.appendChild(table);
      }

      (sec.notes || []).forEach((n) => {
        const note = createElement("div", "note " + (n.type || "info"));
        const icon = n.type === "warn" ? "\u26A0\uFE0F" : (n.type === "tip" ? "\u{1F4A1}" : "\u2139\uFE0F");
        note.innerHTML = '<span class="icon">' + icon + '</span><div class="body">' + n.text + "</div>";
        secEl.appendChild(note);
      });

      body.appendChild(secEl);
    });
    content.appendChild(body);
  }

  function renderFooter() {
    const ui = state.ui;
    const footer = document.getElementById("footer");
    footer.innerHTML = '<span class="heart">\u{1F49C}</span> ' + escapeHtml(ui.footer.text) +
      " \u00B7 " + escapeHtml(ui.footer.made) + " GnuchanOS";
  }

  /* ---------- Actions ---------- */

  async function renderAll() {
    renderTopbar();
    renderFooter();
    const data = await loadContent(state.activeTab);
    renderSidebar(data);
    renderContent(data);
    updateActiveTabUI();
    document.getElementById("sidebar").classList.remove("open");
  }

  function updateActiveTabUI() {
    document.querySelectorAll(".tab").forEach((t) => {
      t.classList.toggle("active", t.dataset.tab === state.activeTab);
    });
  }

  async function setTab(tab) {
    if (tab === state.activeTab) return;
    state.activeTab = tab;
    localStorage.setItem("gclbook-tab", tab);
    state.activeChapter = null;
    updateUrl();
    await renderAll();
  }

  async function setChapter(id, scrollTop) {
    state.activeChapter = id;
    localStorage.setItem("gclbook-chapter", id);
    updateUrl();
    const data = await loadContent(state.activeTab);
    renderContent(data);
    renderSidebar(data);
    if (scrollTop) window.scrollTo({ top: 0, behavior: "smooth" });
  }

  /* ---------- Init ---------- */

  async function init() {
    try {
      state.ui = await fetchJson("data/ui-en.json");
      await renderAll();
    } catch (e) {
      console.error(e);
      const content = document.getElementById("content");
      content.innerHTML = "<div class='coming-card'><h1>Data load error</h1><p>" +
        escapeHtml(String(e.message)) + "</p></div>";
    }

    const menuBtn = document.getElementById("menuBtn");
    if (menuBtn) menuBtn.addEventListener("click", () => {
      document.getElementById("sidebar").classList.toggle("open");
    });
  }

  document.addEventListener("DOMContentLoaded", init);
})();
