/**
 * GnuchanOS Lua Tutorials — Dynamic Lesson Loader
 * Left sidebar navigation with SPA-like content switching.
 */
(function() {
  'use strict';

  const mainContent = document.getElementById('main-content');
  const sidebarLinks = document.querySelectorAll('.sidebar-link');
  const gridHTML = mainContent ? mainContent.innerHTML : '';

  function showGrid() {
    if (!mainContent) return;
    mainContent.innerHTML = gridHTML;
    setupContentLinks();
    window.location.hash = '';
    sidebarLinks.forEach(function(l) { l.classList.remove('active'); });
  }

  function loadLesson(url) {
    if (!mainContent) return;
    if (url === 'index.html' || !url) {
      showGrid();
      return;
    }

    sidebarLinks.forEach(function(link) {
      var href = link.getAttribute('href');
      link.classList.toggle('active', href === url || href === url.split('/').pop());
    });

    fetch(url)
      .then(function(res) {
        if (!res.ok) throw new Error('HTTP ' + res.status);
        return res.text();
      })
      .then(function(html) {
        var parser = new DOMParser();
        var doc = parser.parseFromString(html, 'text/html');
        var container = doc.querySelector('.container');
        if (container) {
          mainContent.innerHTML = container.innerHTML;
        } else {
          mainContent.innerHTML = '<p class="warn">⚠ Could not extract lesson content.</p>';
        }
        window.location.hash = url;
        mainContent.scrollTop = 0;
        setupContentLinks();
      })
      .catch(function(err) {
        mainContent.innerHTML = '<div class="warn"><strong>⚠ Error loading lesson:</strong> ' +
          err.message + '</div>';
      });
  }

  function setupContentLinks() {
    if (!mainContent) return;
    var anchors = mainContent.querySelectorAll('a[href]');
    anchors.forEach(function(a) {
      var href = a.getAttribute('href');
      if (href && href.endsWith('.html') && !href.startsWith('http') && !href.startsWith('//')) {
        a.addEventListener('click', function(e) {
          var targetHref = this.getAttribute('href');
          if (targetHref === 'index.html') {
            e.preventDefault();
            showGrid();
          } else {
            e.preventDefault();
            loadLesson(targetHref);
          }
        });
      }
    });
  }

  sidebarLinks.forEach(function(link) {
    link.addEventListener('click', function(e) {
      e.preventDefault();
      var href = this.getAttribute('href');
      loadLesson(href);
    });
  });

  var hash = window.location.hash.replace('#', '');
  if (hash && hash.endsWith('.html') && hash !== 'index.html') {
    loadLesson(hash);
  }

  setupContentLinks();
})();
