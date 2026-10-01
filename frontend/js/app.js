/**
 * app.js — amorçage, authentification et routeur par fragment (#/...).
 * Chaque page est un module exposé dans `window.Pages`.
 */
(function (global) {
  'use strict';

  var ROUTES = {
    dashboard:  { title: "Aujourd'hui", sub: "L'etat du registre en un coup d'oeil", page: 'dashboard' },
    students:   { title: 'Eleves', sub: 'Inscriptions, coordonnees et affectations', page: 'students' },
    student:    { title: 'Fiche eleve', page: 'students', view: 'detail', nav: 'students' },
    classes:    { title: 'Classes', sub: 'Niveaux, effectifs et enseignants principaux', page: 'classes' },
    subjects:   { title: 'Matieres', sub: 'Codes et coefficients utilises dans les moyennes', page: 'subjects' },
    grades:     { title: 'Notes', sub: 'Saisie et relecture des evaluations', page: 'grades' },
    attendance: { title: 'Appel', sub: 'Presences, absences, retards et justifications', page: 'attendance' },
    results:    { title: 'Resultats', sub: 'Moyennes ponderees et classement par classe', page: 'results' },
    reports:    { title: 'Bulletins et rapports', sub: 'Documents PDF, exports et imports', page: 'reports' },
    users:      { title: 'Comptes', sub: 'Acces et roles', page: 'users', role: 'ADMIN' },
    settings:   { title: 'Reglages', sub: 'Profil, annee scolaire et etat du service', page: 'settings' }
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
      document.getElementById('app').hidden = true;
      document.getElementById('login-screen').hidden = false;
      var box = document.getElementById('login-error');
      if (message) { box.textContent = message; box.hidden = false; }
      else { box.hidden = true; }
      document.getElementById('login-password').value = '';
    },

    showApp: function () {
      document.getElementById('login-screen').hidden = true;
      document.getElementById('app').hidden = false;

      var user = Api.user() || {};
      document.getElementById('user-name').textContent = user.full_name || user.username || '—';
      document.getElementById('user-role').textContent = user.role_label || user.role || '';
      document.getElementById('user-initials').textContent = initials(user);

      // Les entrées réservées à l'administrateur sont masquées pour les autres.
      document.querySelectorAll('.is-admin').forEach(function (el) {
        el.hidden = !Api.can('ADMIN');
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
      document.getElementById('page-sub').textContent = route.sub || '';
      document.getElementById('page-actions').innerHTML = '';
      closeRail();

      var navKey = route.nav || target.name;
      document.querySelectorAll('.rail__item').forEach(function (el) {
        var active = el.dataset.nav === navKey;
        el.classList.toggle('is-active', active);
        if (active) el.setAttribute('aria-current', 'page');
        else el.removeAttribute('aria-current');
      });

      var content = document.getElementById('page-content');
      content.innerHTML = UI.loading();

      var module = global.Pages[route.page];
      if (!module) { content.innerHTML = UI.blank('Page introuvable', "Cette adresse ne correspond a aucune section."); return; }

      Promise.resolve()
        .then(function () {
          return route.view === 'detail' && module.renderDetail
            ? module.renderDetail(content, target.params)
            : module.render(content, target.params);
        })
        .catch(function (err) {
          content.innerHTML = '<p class="notice notice--error">' +
            UI.esc(err && err.message ? err.message : 'Erreur de chargement') +
            '</p><button class="btn" onclick="App.reload()">Reessayer</button>';
        });
    },

    /** Recharge la page courante (après une création/suppression). */
    reload: function () { App.route(); },

    /** Ajoute un bouton dans la barre supérieure. */
    action: function (label, onClick, options) {
      options = options || {};
      var btn = document.createElement('button');
      btn.className = 'btn' + (options.variant ? ' btn--' + options.variant : '');
      btn.innerHTML = (options.icon ? UI.icon(options.icon) : '') + '<span>' + UI.esc(label) + '</span>';
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
      box.hidden = true;
      button.disabled = true;
      button.textContent = 'Ouverture…';

      Api.login(document.getElementById('login-username').value.trim(),
                document.getElementById('login-password').value)
        .then(function (user) {
          UI.success('Bienvenue ' + (user.full_name || user.username) + ' !');
          location.hash = '#/dashboard';
          App.showApp();
        })
        .catch(function (err) {
          box.textContent = err.message || 'Connexion impossible';
          box.hidden = false;
          document.getElementById('login-password').focus();
        })
        .then(function () {
          button.disabled = false;
          button.textContent = 'Entrer';
        });
    });
  }

  function bindShell() {
    document.getElementById('logout-btn').onclick = function () {
      UI.confirm('Fermer la session',
        'Vous devrez saisir a nouveau votre mot de passe pour rouvrir le registre.',
        App.logout, 'Fermer la session');
    };

    document.getElementById('menu-toggle').onclick = function () {
      var rail = document.getElementById('sidebar');
      var open = rail.classList.toggle('is-open');
      this.setAttribute('aria-expanded', open ? 'true' : 'false');
    };
    document.getElementById('modal-close').onclick = UI.closeModal;
    document.getElementById('modal-backdrop').addEventListener('click', function (event) {
      if (event.target === this) UI.closeModal();
    });
    document.addEventListener('keydown', function (event) {
      if (event.key !== 'Escape') return;
      if (!document.getElementById('modal-backdrop').hidden) UI.closeModal();
      else closeRail();
    });

    // Tri des tableaux au clavier : l'en-tete se comporte comme un bouton.
    document.getElementById('page-content').addEventListener('keydown', function (event) {
      if ((event.key === 'Enter' || event.key === ' ') && event.target.matches('th[data-sort]')) {
        event.preventDefault();
        event.target.click();
      }
    });
  }

  function closeRail() {
    var rail = document.getElementById('sidebar');
    if (!rail) return;
    rail.classList.remove('is-open');
    var toggle = document.getElementById('menu-toggle');
    if (toggle) toggle.setAttribute('aria-expanded', 'false');
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
