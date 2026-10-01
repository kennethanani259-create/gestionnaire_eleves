/**
 * app.js — amorçage, authentification et routeur par fragment (#/...).
 * Chaque page est un module exposé dans `window.Pages`.
 */
(function (global) {
  'use strict';

  var ROUTES = {
    dashboard:  { title: 'Tableau de bord', page: 'dashboard' },
    students:   { title: 'Élèves',          page: 'students' },
    student:    { title: 'Fiche élève',     page: 'students', view: 'detail', nav: 'students' },
    classes:    { title: 'Classes',         page: 'classes' },
    subjects:   { title: 'Matières',        page: 'subjects' },
    grades:     { title: 'Notes',           page: 'grades' },
    attendance: { title: 'Présences',       page: 'attendance' },
    results:    { title: 'Résultats',       page: 'results' },
    reports:    { title: 'Rapports & bulletins', page: 'reports' },
    users:      { title: 'Utilisateurs',    page: 'users', role: 'ADMIN' },
    settings:   { title: 'Paramètres',      page: 'settings' }
  };

  var App = {
    /** Cache partagé des référentiels (classes, matières, enseignants). */
    cache: {},

    start: function () {
      bindLogin();
      bindShell();
      global.addEventListener('hashchange', function () { App.route(); });

      if (Api.isLoggedIn()) {
        // On revalide le jeton auprès du serveur avant d'afficher l'application.
        Api.me().then(function (user) {
          Api.setSession(Api.token(), user);
          App.showApp();
        }).catch(function () { App.showLogin(); });
      } else {
        App.showLogin();
      }
    },

    /* ----------------------------- Écrans ----------------------------- */
    showLogin: function (message) {
      document.getElementById('app').classList.add('hidden');
      document.getElementById('login-screen').classList.remove('hidden');
      var box = document.getElementById('login-error');
      if (message) { box.textContent = message; box.classList.remove('hidden'); }
      else { box.classList.add('hidden'); }
      document.getElementById('login-password').value = '';
    },

    showApp: function () {
      document.getElementById('login-screen').classList.add('hidden');
      document.getElementById('app').classList.remove('hidden');

      var user = Api.user() || {};
      document.getElementById('user-name').textContent = user.full_name || user.username || '—';
      document.getElementById('user-role').textContent = user.role_label || user.role || '';
      document.getElementById('user-initials').textContent = initials(user);

      // Les entrées réservées à l'administrateur sont masquées pour les autres.
      document.querySelectorAll('.admin-only').forEach(function (el) {
        el.classList.toggle('hidden', !Api.can('ADMIN'));
      });

      App.cache = {};
      if (!location.hash || location.hash === '#') location.hash = '#/dashboard';
      else App.route();
    },

    logout: function () {
      Api.clearSession();
      App.cache = {};
      location.hash = '';
      App.showLogin('Vous êtes déconnecté.');
    },

    /* ----------------------------- Routage ----------------------------- */
    parseHash: function () {
      var raw = (location.hash || '#/dashboard').replace(/^#\/?/, '');
      var parts = raw.split('/').filter(function (p) { return p !== ''; });
      var name = parts[0] || 'dashboard';
      if (name === 'students' && parts[1]) return { name: 'student', params: { id: parts[1] } };
      return { name: ROUTES[name] ? name : 'dashboard', params: { id: parts[1] } };
    },

    route: function () {
      if (!Api.isLoggedIn()) { App.showLogin(); return; }
      var target = App.parseHash();
      var route = ROUTES[target.name];

      if (route.role && !Api.can(route.role)) {
        UI.error("Vous n'avez pas les droits nécessaires pour cette page.");
        location.hash = '#/dashboard';
        return;
      }

      document.getElementById('page-title').textContent = route.title;
      document.getElementById('page-actions').innerHTML = '';
      document.getElementById('sidebar').classList.remove('open');

      var navKey = route.nav || target.name;
      document.querySelectorAll('.nav-item').forEach(function (el) {
        el.classList.toggle('active', el.dataset.nav === navKey);
      });

      var content = document.getElementById('page-content');
      content.innerHTML = UI.spinner();

      var module = global.Pages[route.page];
      if (!module) { content.innerHTML = UI.empty('Page introuvable'); return; }

      Promise.resolve()
        .then(function () {
          return route.view === 'detail' && module.renderDetail
            ? module.renderDetail(content, target.params)
            : module.render(content, target.params);
        })
        .catch(function (err) {
          content.innerHTML = '<div class="alert alert-error">' +
            UI.esc(err && err.message ? err.message : 'Erreur de chargement') + '</div>';
        });
    },

    /** Recharge la page courante (après une création/suppression). */
    reload: function () { App.route(); },

    /** Ajoute un bouton dans la barre supérieure. */
    action: function (label, onClick, className) {
      var btn = document.createElement('button');
      btn.className = 'btn ' + (className || 'btn-primary');
      btn.textContent = label;
      btn.onclick = onClick;
      document.getElementById('page-actions').appendChild(btn);
      return btn;
    },

    /* ----------------------------- Référentiels ----------------------------- */
    classes: function () {
      if (!App.cache.classes) {
        App.cache.classes = Api.get('/api/classes').then(function (d) { return d.items || []; });
      }
      return App.cache.classes;
    },
    subjects: function () {
      if (!App.cache.subjects) {
        App.cache.subjects = Api.get('/api/subjects').then(function (d) { return d.items || []; });
      }
      return App.cache.subjects;
    },
    teachers: function () {
      if (!App.cache.teachers) {
        App.cache.teachers = Api.get('/api/teachers').then(function (d) { return d.items || []; });
      }
      return App.cache.teachers;
    },
    schoolYears: function () {
      if (!App.cache.years) {
        App.cache.years = Api.get('/api/school-years').then(function (d) { return d.items || []; });
      }
      return App.cache.years;
    },
    invalidate: function (key) { delete App.cache[key]; }
  };

  /* ----------------------------- Liaisons DOM ----------------------------- */
  function bindLogin() {
    var form = document.getElementById('login-form');
    form.addEventListener('submit', function (event) {
      event.preventDefault();
      var button = document.getElementById('login-submit');
      var box = document.getElementById('login-error');
      box.classList.add('hidden');
      button.disabled = true;
      button.textContent = 'Connexion…';

      Api.login(document.getElementById('login-username').value.trim(),
                document.getElementById('login-password').value)
        .then(function (user) {
          UI.success('Bienvenue ' + (user.full_name || user.username) + ' !');
          location.hash = '#/dashboard';
          App.showApp();
        })
        .catch(function (err) {
          box.textContent = err.message || 'Connexion impossible';
          box.classList.remove('hidden');
        })
        .then(function () {
          button.disabled = false;
          button.textContent = 'Se connecter';
        });
    });
  }

  function bindShell() {
    document.getElementById('logout-btn').onclick = function () {
      UI.confirm('Déconnexion', 'Voulez-vous vraiment vous déconnecter ?', App.logout);
    };
    document.getElementById('menu-toggle').onclick = function () {
      document.getElementById('sidebar').classList.toggle('open');
    };
    document.getElementById('modal-close').onclick = UI.closeModal;
    document.getElementById('modal-backdrop').addEventListener('click', function (event) {
      if (event.target === this) UI.closeModal();
    });
    document.addEventListener('keydown', function (event) {
      if (event.key === 'Escape') UI.closeModal();
    });
  }

  function initials(user) {
    var source = user.full_name || user.username || '?';
    return source.split(/[\s.]+/).filter(Boolean).slice(0, 2)
      .map(function (w) { return w[0].toUpperCase(); }).join('');
  }

  global.Pages = global.Pages || {};
  global.App = App;
  document.addEventListener('DOMContentLoaded', function () { App.start(); });
})(window);
