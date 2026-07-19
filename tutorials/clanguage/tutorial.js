/**
 * GnuchanOS C Tutorials — Dynamic Lesson Loader
 * Left sidebar navigation with SPA-like content switching.
 * Fetches lesson HTML, extracts .container content, injects into main area.
 */
(function() {
  'use strict';

  const mainContent = document.getElementById('main-content');
  const sidebarLinks = document.querySelectorAll('.sidebar-link');
  const gridHTML = mainContent ? mainContent.innerHTML : '';

  // Shared lesson styles already in index.html <head>, so content-only extraction works.

  /** Show the grid (table of contents) view */
  function showGrid() {
    if (!mainContent) return;
    mainContent.innerHTML = gridHTML;
    setupContentLinks();
    window.location.hash = '';
    sidebarLinks.forEach(function(l) { l.classList.remove('active'); });
  }

  /** Load a lesson by URL into the main content area */
  function loadLesson(url) {
    if (!mainContent) return;
    if (url === 'index.html' || !url) {
      showGrid();
      return;
    }

    // Highlight active sidebar link
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
        // Scroll to top of content
        mainContent.scrollTop = 0;
        setupContentLinks();
      })
      .catch(function(err) {
        mainContent.innerHTML = '<div class="warn"><strong>⚠ Error loading lesson:</strong> ' +
          err.message + '</div>';
      });
  }

  /** Intercept all anchor clicks inside main content for SPA navigation */
  function setupContentLinks() {
    if (!mainContent) return;
    var anchors = mainContent.querySelectorAll('a[href]');
    anchors.forEach(function(a) {
      var href = a.getAttribute('href');
      // Only intercept .html links that aren't external
      if (href && href.endsWith('.html') && !href.startsWith('http') && !href.startsWith('//')) {
        // Remove old listener by cloning? Use a new listener.
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

  // --- Sidebar link click handlers ---
  sidebarLinks.forEach(function(link) {
    link.addEventListener('click', function(e) {
      e.preventDefault();
      var href = this.getAttribute('href');
      loadLesson(href);
    });
  });

  // --- Load from URL hash on initial load ---
  var hash = window.location.hash.replace('#', '');
  if (hash && hash.endsWith('.html') && hash !== 'index.html') {
    loadLesson(hash);
  }
  // If no hash or index.html, grid view is already shown (it's in the HTML)

  // Make sure content links in the initial grid view work
  setupContentLinks();
})();
