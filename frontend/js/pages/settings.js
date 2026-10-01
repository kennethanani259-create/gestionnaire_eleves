/** Page « Paramètres » : profil, mot de passe, années scolaires, état du serveur. */
(function (global) {
  'use strict';
  global.Pages = global.Pages || {};

  global.Pages.settings = {
    render: function (container) {
      return Promise.all([
        Api.me(),
        Api.get('/api/school-years').catch(function () { return { items: [] }; }),
        Api.get('/api/health').catch(function () { return null; })
      ]).then(function (r) {
        container.innerHTML = view(r[0], r[1].items || [], r[2]);
        bind(container);
      });
    }
  };

  function view(user, years, health) {
    var html = '<div class="grid-2">';

    html += '<div class="card"><div class="card-header"><h2>👤 Mon profil</h2></div>' +
      '<div class="card-body"><div class="detail-grid">' +
      item('Identifiant', user.username) +
      item('Nom complet', user.full_name) +
      item('E-mail', user.email) +
      item('Rôle', user.role_label || user.role) +
      item('Dernière connexion', UI.dateTime(user.last_login_at)) +
      '</div></div></div>';

    html += '<div class="card"><div class="card-header"><h2>🔑 Changer mon mot de passe</h2></div>' +
      '<div class="card-body">' +
      UI.form([
        { name: 'current_password', label: 'Mot de passe actuel', type: 'password', required: true },
        { name: 'new_password', label: 'Nouveau mot de passe', type: 'password', required: true, help: '8 caractères minimum' },
        { name: 'confirm_password', label: 'Confirmer le nouveau mot de passe', type: 'password', required: true }
      ], 'pwd-form') +
      '<button class="btn btn-primary" id="pwd-submit">Mettre à jour</button></div></div>';

    html += '<div class="card"><div class="card-header"><h2>📆 Années scolaires</h2>' +
      (Api.can('ADMIN') ? '<button class="btn btn-secondary btn-sm" id="year-add">+ Ajouter</button>' : '') +
      '</div>' +
      UI.table([
        { key: 'label', label: 'Libellé' },
        { key: 'start_date', label: 'Début', render: function (y) { return UI.date(y.start_date); } },
        { key: 'end_date', label: 'Fin', render: function (y) { return UI.date(y.end_date); } },
        {
          key: 'is_current', label: 'En cours',
          render: function (y) { return y.is_current ? UI.badge('Oui', 'badge-success') : '<span class="muted">—</span>'; }
        }
      ], years, { emptyMessage: 'Aucune année scolaire' }) + '</div>';

    html += '<div class="card"><div class="card-header"><h2>🖥 État du serveur</h2></div>' +
      '<div class="card-body"><div class="detail-grid">' +
      item('API', health ? (health.status || 'ok') : 'injoignable') +
      item('Version', health && health.version ? health.version : '—') +
      item('Base de données', health && health.database ? health.database : 'SQLite') +
      item('Adresse du frontend', location.origin) +
      '</div>' +
      '<p class="muted" style="margin-top:14px">Les calculs de moyennes, de classement et de taux ' +
      'de présence sont réalisés côté serveur (C++17) ; cette interface se contente de les afficher.</p>' +
      '</div></div>';

    html += '</div>';
    return html;
  }

  function item(label, value) {
    return '<div class="detail-item"><div class="label">' + UI.esc(label) + '</div>' +
      '<div class="value">' + UI.text(value) + '</div></div>';
  }

  function bind(container) {
    container.querySelector('#pwd-submit').onclick = function () {
      var form = document.getElementById('pwd-form');
      var data = UI.readForm(form);
      if (!data.current_password || !data.new_password) {
        UI.error('Renseignez les deux mots de passe.');
        return;
      }
      if (data.new_password !== data.confirm_password) {
        UI.markFieldErrors(form, { confirm_password: 'Les deux mots de passe ne correspondent pas' });
        return;
      }
      var button = this;
      button.disabled = true;
      Api.changePassword(data.current_password, data.new_password).then(function () {
        UI.success('Mot de passe modifié.');
        form.reset();
        UI.markFieldErrors(form, {});
      }).catch(function (err) {
        UI.markFieldErrors(form, err.fields);
        UI.showError(err);
      }).then(function () { button.disabled = false; });
    };

    var addYear = container.querySelector('#year-add');
    if (addYear) addYear.onclick = openYearForm;
  }

  function openYearForm() {
    var year = new Date().getFullYear();
    UI.modal({
      title: 'Nouvelle année scolaire',
      body: UI.form([
        { name: 'label', label: 'Libellé', value: year + '-' + (year + 1), required: true },
        { name: 'start_date', label: 'Date de début', type: 'date', value: year + '-09-01', col: 'half' },
        { name: 'end_date', label: 'Date de fin', type: 'date', value: (year + 1) + '-06-30', col: 'half' },
        {
          name: 'is_current', label: 'Année en cours', type: 'select', value: 'true',
          options: [{ value: 'true', label: 'Oui' }, { value: 'false', label: 'Non' }]
        }
      ], 'year-form'),
      buttons: [
        { label: 'Annuler', className: 'btn-secondary' },
        {
          label: 'Enregistrer', className: 'btn-primary',
          onClick: function (button) {
            var form = document.getElementById('year-form');
            var data = UI.readForm(form);
            data.is_current = data.is_current === 'true';
            button.disabled = true;
            Api.post('/api/school-years', data).then(function () {
              UI.closeModal();
              UI.success('Année scolaire créée.');
              App.invalidate('years');
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
})(window);
