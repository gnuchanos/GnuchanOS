/**
 * GnuchanOS Unified Tutorials — Fully Dynamic
 * No hardcoded lesson data. Scans language directories via their index.html.
 */
(function() {
  'use strict';

  const isFileProtocol = window.location.protocol === 'file:';
  const mainContent = document.getElementById('main-content');
  const contentArea = document.getElementById('content-area');
  const sidebarNav = document.getElementById('sidebar-nav');
  const sidebarHeader = document.getElementById('sidebar-header');
  const langTabsContainer = document.getElementById('lang-tabs');

  const LANG_FOLDERS = ['clanguage', 'lua', 'python'];

  const languages = [
    { key: 'clanguage', name: 'C Tutorial', icon: '🔥', folder: 'clanguage' },
    { key: 'lua',       name: 'Lua Tutorial', icon: '💜', folder: 'lua' },
    { key: 'python',    name: 'Python Tutorial', icon: '🐍', folder: 'python' }
  ];

  let currentLang = null;
  let lessonCache = {};

  async function fetchLessons(lang) {
    if (lessonCache[lang.key]) return lessonCache[lang.key];
    try {
      const resp = await fetch(lang.folder + '/index.html');
      if (!resp.ok) throw new Error('HTTP ' + resp.status);
      const html = await resp.text();
      const parser = new DOMParser();
      const doc = parser.parseFromString(html, 'text/html');
      const cards = doc.querySelectorAll('.lesson-card');
      const lessons = [];
      cards.forEach(function(card) {
        const href = card.getAttribute('href');
        const numEl = card.querySelector('.num');
        const titleEl = card.querySelector('.title');
        const descEl = card.querySelector('.desc');
        if (href) {
          var rawNum = numEl ? numEl.textContent.trim() : '';
          var numMatch = rawNum.match(/(\d+)/);
          var num = numMatch ? numMatch[1].padStart(2, '0') : '00';
          lessons.push({
            num: num,
            file: lang.folder + '/' + href,
            title: titleEl ? titleEl.textContent.trim() : 'Untitled',
            desc: descEl ? descEl.textContent.trim() : ''
          });
        }
      });
      lessonCache[lang.key] = lessons;
      return lessons;
    } catch (e) {
      console.warn('Failed to fetch lessons for', lang.key, e);
      return [];
    }
  }

  function buildSidebar(lang, lessons) {
    if (!sidebarNav) return;
    sidebarNav.innerHTML = '';
    sidebarHeader.innerHTML = '<h2>' + lang.icon + ' ' + lang.name + '</h2><div class="sub">' + lessons.length + ' Lessons</div>';
    var homeLink = document.createElement('a');
    homeLink.className = 'sidebar-link';
    homeLink.href = '#';
    homeLink.innerHTML = '<span class="num">🏠</span> <span class="lbl">Home</span>';
    homeLink.addEventListener('click', function(e) { e.preventDefault(); showGrid(lang); });
    sidebarNav.appendChild(homeLink);
    lessons.forEach(function(lesson) {
      var link = document.createElement('a');
      link.className = 'sidebar-link';
      link.href = lesson.file;
      link.innerHTML = '<span class="num">' + lesson.num + '</span> <span class="lbl">' + lesson.title + '</span>';
      link.addEventListener('click', function(e) {
        e.preventDefault();
        loadLesson(lang, lesson.file);
      });
      sidebarNav.appendChild(link);
    });
  }

  function showGrid(lang) {
    if (!contentArea) return;
    currentLang = lang;
    fetchLessons(lang).then(function(lessons) {
      buildSidebar(lang, lessons);
      var sidebarLinks = document.querySelectorAll('.sidebar-link');
      sidebarLinks.forEach(function(l) { l.classList.remove('active'); });
      var cardsHtml = '';
      lessons.forEach(function(lesson) {
        var numInt = parseInt(lesson.num, 10);
        var displayNum = numInt > 0 ? 'Lesson ' + numInt : lesson.num;
        cardsHtml += '<a href="' + lesson.file + '" class="lesson-card" data-lesson="' + lesson.file + '">' +
          '<span class="num">' + displayNum + '</span>' +
          '<span class="title">' + lesson.title + '</span>' +
          '<span class="desc">' + lesson.desc + '</span></a>';
      });
      contentArea.innerHTML = '<h1>' + lang.icon + ' ' + lang.name + '</h1>' +
        '<p class="subtitle">' + lessons.length + ' Lessons</p>' +
        '<div class="lesson-grid">' + cardsHtml + '</div>';
      var cards = contentArea.querySelectorAll('.lesson-card');
      cards.forEach(function(card) {
        card.addEventListener('click', function(e) {
          e.preventDefault();
          var file = this.getAttribute('data-lesson');
          if (file) loadLesson(lang, file);
        });
      });
    });
  }

  /** Handle internal lesson links: index.html = show grid, otherwise fetch lesson */
  function handleLessonLink(lang, href, e) {
    e.preventDefault();
    if (href === 'index.html') {
      showGrid(lang);
    } else {
      loadLesson(lang, href);
    }
  }

  /** Fix relative lesson hrefs by prepending language folder */
  function resolveHref(lang, href) {
    if (!href) return href;
    // Already has a language folder prefix
    for (var i = 0; i < LANG_FOLDERS.length; i++) {
      if (href.indexOf(LANG_FOLDERS[i] + '/') === 0) return href;
    }
    // Relative path — prepend language folder (including index.html)
    return lang.folder + '/' + href.replace(/^\.\//, '');
  }

  function loadLesson(lang, file) {
    if (!contentArea) return;
    currentLang = lang;
    var sidebarLinks = document.querySelectorAll('.sidebar-link');
    sidebarLinks.forEach(function(l) {
      l.classList.remove('active');
      if (l.getAttribute('href') === file) l.classList.add('active');
    });
    if (isFileProtocol) {
      window.location.href = file;
      return;
    }
    fetch(file)
      .then(function(res) {
        if (!res.ok) throw new Error('HTTP ' + res.status);
        return res.text();
      })
      .then(function(html) {
        var parser = new DOMParser();
        var doc = parser.parseFromString(html, 'text/html');
        var container = doc.querySelector('.container');
        if (container) {
          contentArea.innerHTML = '<p style="margin-bottom:12px"><a href="#" class="back-link" style="color:#a855f7;text-decoration:none;font-size:0.9em;">← Back to ' + lang.name + '</a></p>' + container.innerHTML;
          contentArea.querySelector('.back-link').addEventListener('click', function(e) {
            e.preventDefault();
            showGrid(lang);
          });
          var anchors = contentArea.querySelectorAll('a[href]');
          anchors.forEach(function(a) {
            var href = a.getAttribute('href');
            if (href && href.endsWith('.html') && !href.startsWith('http') && !a.classList.contains('back-link')) {
              var full = resolveHref(lang, href);
              a.setAttribute('href', full);
              a.addEventListener('click', function(e) {
                handleLessonLink(lang, a.getAttribute('href'), e);
              });
            }
          });
          if (mainContent) mainContent.scrollTop = 0;
        } else {
          contentArea.innerHTML = '<p class="warn">⚠ Could not load lesson content.</p>';
        }
      })
      .catch(function(err) {
        contentArea.innerHTML = '<div class="warn"><strong>⚠ Error loading lesson:</strong> ' + err.message + '</div>';
      });
  }

  function switchLanguage(langKey) {
    var lang = null;
    for (var i = 0; i < languages.length; i++) {
      if (languages[i].key === langKey) { lang = languages[i]; break; }
    }
    if (!lang) return;
    var tabs = document.querySelectorAll('.lang-tab');
    tabs.forEach(function(tab) {
      tab.classList.toggle('active', tab.getAttribute('data-lang') === langKey);
    });
    showGrid(lang);
  }

  // Build language tabs dynamically
  languages.forEach(function(lang) {
    var tab = document.createElement('div');
    tab.className = 'lang-tab';
    tab.setAttribute('data-lang', lang.key);
    tab.innerHTML = '<span class="icon">' + lang.icon + '</span>' + lang.name.replace(' Tutorial', '');
    tab.addEventListener('click', function() {
      switchLanguage(this.getAttribute('data-lang'));
    });
    langTabsContainer.appendChild(tab);
  });

  // On file:// protocol, redirect to first language's index (CORS blocks fetch)
  if (isFileProtocol) {
    window.location.href = languages[0].folder + '/index.html';
  } else if (languages.length > 0) {
    switchLanguage(languages[0].key);
  }
})();
