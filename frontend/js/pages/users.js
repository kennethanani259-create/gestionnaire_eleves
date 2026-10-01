/** Page « Utilisateurs » (administrateurs uniquement). */
(function (global) {
  'use strict';
  global.Pages = global.Pages || {};

  var ROLES = [
    { value: 'ADMIN', label: 'Administrateur' },
    { value: 'TEACHER', label: 'Enseignant' },
    { value: 'VIEWER', label: 'Lecteur' }
  ];
  var ROLE_TAG = { ADMIN: 'bad', TEACHER: 'mark', VIEWER: 'neutral' };

  global.Pages.users = {
    render: function (container) {
      App.action('+ Nouvel utilisateur', function () { openForm(null); });
      return Api.get('/api/users').then(function (data) {
        var users = data.items || data || [];
        container.innerHTML = view(users);
        bind(container, users);
      });
    }
  };

  function view(users) {
    var me = Api.user() || {};
    var columns = [
      {
        key: 'username', label: 'Identifiant',
        render: function (u) {
          return '<span class="name">' + UI.esc(u.username) + '</span>' +
            (String(u.id) === String(me.id) ? ' <span class="muted">(vous)</span>' : '');
        }
      },
      { key: 'full_name', label: 'Nom complet', render: function (u) { return UI.text(u.full_name); } },
      { key: 'email', label: 'E-mail', render: function (u) { return UI.text(u.email); } },
      {
        key: 'role', label: 'Rôle',
        render: function (u) { return UI.tag(u.role_label || u.role, ROLE_TAG[u.role]); }
      },
      {
        key: 'is_active', label: 'État',
        render: function (u) {
          return u.is_active ? UI.tag('Actif', 'good') : UI.tag('Désactivé', 'neutral');
        }
      },
      { key: 'last_login_at', label: 'Dernière connexion', render: function (u) { return UI.dateTime(u.last_login_at); } },
      {
        key: 'actions', label: '', cls: 't-right',
        render: function (u) {
          return '<div class="row-actions">' +
            '<button class="btn btn--sm" data-edit="' + u.id + '">' + UI.icon('edit') + '</button>' +
            '<button class="btn btn--sm" data-pwd="' + u.id + '">' + UI.icon('key') + '</button>' +
            (String(u.id) === String(me.id) ? '' :
              '<button class="btn btn--danger btn--sm" data-del="' + u.id + '">' + UI.icon('trash') + '</button>') +
            '</div>';
        }
      }
    ];

    return pendingView(users) +
      '<div class="notice notice--info">Les mots de passe sont stockés sous forme d\'empreinte ' +
      'PBKDF2-SHA256 salée : ils ne peuvent jamais être relus, seulement réinitialisés.</div>' +
      '<div class="panel">' + UI.table(columns, users, { emptyTitle: 'Aucun utilisateur' }) + '</div>';
  }

  /** Comptes inactifs : demandes d'inscription en attente d'une décision. */
  function pending(users) {
    return (users || []).filter(function (u) { return !u.is_active; });
  }

  function pendingView(users) {
    var waiting = pending(users);
    if (!waiting.length) return '';

    var rows = waiting.map(function (u) {
      return '<li class="todo__item">' +
        '<span><span class="strong">' + UI.esc(u.full_name || u.username) + '</span> ' +
        '<span class="muted">' + UI.esc(u.username) + ' · ' + UI.esc(u.email || '—') + '</span>' +
        '<span class="muted"> · demandé le ' + UI.date(u.created_at) + '</span></span>' +
        '<span class="row-actions">' +
        '<button class="btn btn--primary btn--sm" data-approve="' + u.id + '">Valider</button>' +
        '<button class="btn btn--danger btn--sm" data-reject="' + u.id + '">Refuser</button>' +
        '</span></li>';
    }).join('');

    return UI.panel('Demandes de compte en attente',
      '<p class="lede">' + waiting.length + ' compte' + (waiting.length > 1 ? 's' : '') +
      ' ne peu' + (waiting.length > 1 ? 'vent' : 't') + ' pas encore se connecter. ' +
      'Valider ouvre un accès en consultation.</p>' +
      '<ul class="todo">' + rows + '</ul>', { raw: true });
  }

  function bind(container, users) {
    function find(id) { return users.filter(function (u) { return String(u.id) === String(id); })[0]; }

    container.querySelectorAll('[data-edit]').forEach(function (btn) {
      btn.onclick = function () { openForm(find(btn.dataset.edit)); };
    });
    container.querySelectorAll('[data-pwd]').forEach(function (btn) {
      btn.onclick = function () { openReset(find(btn.dataset.pwd)); };
    });
    container.querySelectorAll('[data-approve]').forEach(function (btn) {
      btn.onclick = function () {
        var u = find(btn.dataset.approve);
        btn.disabled = true;
        Api.put('/api/users/' + u.id, {
          username: u.username, email: u.email, full_name: u.full_name,
          role: u.role, is_active: true
        }).then(function () {
          UI.success('Compte « ' + u.username + ' » validé.');
          App.reload();
        }).catch(function (err) { btn.disabled = false; UI.showError(err); });
      };
    });

    container.querySelectorAll('[data-reject]').forEach(function (btn) {
      btn.onclick = function () {
        var u = find(btn.dataset.reject);
        UI.confirm('Refuser la demande',
          'Supprimer la demande de « ' + (u.full_name || u.username) + ' » ? ' +
          'La personne devra en formuler une nouvelle.',
          function () {
            Api.del('/api/users/' + u.id).then(function () {
              UI.success('Demande refusée.');
              App.reload();
            }).catch(UI.showError);
          }, 'Refuser');
      };
    });

    container.querySelectorAll('[data-del]').forEach(function (btn) {
      btn.onclick = function () {
        var u = find(btn.dataset.del);
        UI.confirm('Supprimer l\'utilisateur',
          'Supprimer le compte « ' + u.username + ' » ? Cette action est irréversible.',
          function () {
            Api.del('/api/users/' + u.id).then(function () {
              UI.success('Utilisateur supprimé.');
              App.reload();
            }).catch(UI.showError);
          });
      };
    });
  }

  function openForm(user) {
    var u = user || {};
    var fields = [
      { name: 'username', label: 'Identifiant', value: u.username, required: true, half: true },
      { name: 'email', label: 'E-mail', type: 'email', value: u.email, required: true, half: true },
      { name: 'full_name', label: 'Nom complet', value: u.full_name, required: true },
      { name: 'role', label: 'Rôle', type: 'select', value: u.role || 'VIEWER', options: ROLES, half: true }
    ];
    if (user) {
      fields.push({
        name: 'is_active', label: 'État', type: 'select', half: true,
        value: u.is_active ? 'true' : 'false',
        options: [{ value: 'true', label: 'Actif' }, { value: 'false', label: 'Désactivé' }]
      });
    } else {
      fields.push({
        name: 'password', label: 'Mot de passe', type: 'password', required: true, half: true,
        help: '8 caractères minimum'
      });
    }

    UI.modal({
      title: user ? 'Modifier l\'utilisateur' : 'Nouvel utilisateur',
      body: UI.form(fields, 'user-form'),
      buttons: [
        { label: 'Annuler' },
        {
          label: 'Enregistrer', variant: 'primary',
          onClick: function (button) {
            var form = document.getElementById('user-form');
            var data = UI.readForm(form);
            if ('is_active'in data) data.is_active = data.is_active === 'true';
            button.disabled = true;

            var request = user ? Api.put('/api/users/' + user.id, data) : Api.post('/api/users', data);
            request.then(function () {
              UI.closeModal();
              UI.success(user ? 'Utilisateur mis à jour.' : 'Utilisateur créé.');
              App.reload();
            }).catch(function (err) {
              button.disabled = false;
              UI.markFieldErrors(form, err.fields);
              UI.showError(err);
            });
          }
        }
      ]
    });
  }

  function openReset(user) {
    UI.modal({
      title: 'Réinitialiser le mot de passe',
      body: '<p class="muted">Compte concerné : <strong>' + UI.esc(user.username) + '</strong></p>' +
        UI.form([{
          name: 'password', label: 'Nouveau mot de passe', type: 'password',
          required: true, help: '8 caractères minimum'
        }], 'reset-form'),
      buttons: [
        { label: 'Annuler' },
        {
          label: 'Réinitialiser', variant: 'primary',
          onClick: function (button) {
            var form = document.getElementById('reset-form');
            button.disabled = true;
            Api.post('/api/users/' + user.id + '/password', UI.readForm(form)).then(function () {
              UI.closeModal();
              UI.success('Mot de passe réinitialisé.');
            }).catch(function (err) {
              button.disabled = false;
              UI.markFieldErrors(form, err.fields);
              UI.showError(err);
            });
          }
        }
      ]
    });
  }
})(window);
