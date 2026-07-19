/**
 * GnuchanOS Python Tutorials — Dynamic Lesson Loader
 * Falls back to normal navigation on file:// protocol (CORS blocks fetch)
 */
(function() {
  'use strict';

  var isFileProtocol = window.location.protocol === 'file:';
  var mainContent = document.getElementById('main-content');
  var sidebarLinks = document.querySelectorAll('.sidebar-link');
  var gridHTML = mainContent ? mainContent.innerHTML : '';

  function showGrid() {
    if (!mainContent) return;
    mainContent.innerHTML = gridHTML;
    setupContentLinks();
    window.location.hash = '';
    sidebarLinks.forEach(function(l) { l.classList.remove('active'); });
  }

  function loadLesson(url) {
    if (!mainContent) return;
    if (url === 'index.html' || !url) { showGrid(); return; }

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
        mainContent.innerHTML = '<div class="warn"><strong>⚠ Error loading lesson:</strong> ' + err.message + '</div>';
      });
  }

  function navigateTo(url) {
    if (url === 'index.html') {
      showGrid();
    } else {
      loadLesson(url);
    }
  }

  function setupContentLinks() {
    if (!mainContent) return;
    var anchors = mainContent.querySelectorAll('a[href]');
    anchors.forEach(function(a) {
      var href = a.getAttribute('href');
      if (href && href.endsWith('.html') && !href.startsWith('http') && !href.startsWith('//')) {
        a.addEventListener('click', function(e) {
          if (isFileProtocol) return; // let default navigation work on file://
          e.preventDefault();
          navigateTo(this.getAttribute('href'));
        });
      }
    });
  }

  sidebarLinks.forEach(function(link) {
    link.addEventListener('click', function(e) {
      if (isFileProtocol) return; // let default navigation work on file://
      e.preventDefault();
      navigateTo(this.getAttribute('href'));
    });
  });

  var hash = window.location.hash.replace('#', '');
  if (!isFileProtocol && hash && hash.endsWith('.html') && hash !== 'index.html') {
    navigateTo(hash);
  }
  setupContentLinks();
})();
