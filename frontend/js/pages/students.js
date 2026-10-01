/** Pages « Élèves » (liste + recherche + CRUD) et « Fiche élève ». */
(function (global) {
  'use strict';
  global.Pages = global.Pages || {};

  var STATUS = [
    { value: 'ACTIVE', label: 'Actif' },
    { value: 'INACTIVE', label: 'Inactif' },
    { value: 'TRANSFERRED', label: 'Transféré' },
    { value: 'EXPELLED', label: 'Exclu' }
  ];
  var GENDERS = [{ value: 'M', label: 'Masculin' }, { value: 'F', label: 'Féminin' }];

  var state = {
    q: '', classId: '', status: '', gender: '',
    sortBy: 'last_name', sortDir: 'asc', limit: 25, offset: 0
  };

  global.Pages.students = {
    render: function (container) {
      if (Api.can('TEACHER')) {
        App.action('+ Nouvel élève', function () { openForm(null); });
      }
      if (Api.can('ADMIN')) {
        App.action('Importer CSV', openImport, 'btn-secondary');
      }
      App.action('Exporter CSV', exportCsv, 'btn-secondary');

      return Promise.all([App.classes(), fetchList()]).then(function (results) {
        container.innerHTML = view(results[0], results[1]);
        bind(container, results[1]);
      });
    },

    renderDetail: function (container, params) {
      var id = params.id;
      return Promise.all([
        Api.get('/api/students/' + id),
        Api.get('/api/students/' + id + '/results'),
        Api.get('/api/students/' + id + '/attendance')
      ]).then(function (r) {
        var student = r[0], results = r[1], attendance = r[2];
        document.getElementById('page-title').textContent = student.full_name;

        App.action('📄 Bulletin PDF', function () {
          Api.download('/api/reports/students/' + id + '/pdf',
            'bulletin-' + (student.matricule || id) + '.pdf')
            .then(function () { UI.success('Bulletin téléchargé.'); })
            .catch(UI.showError);
        }, 'btn-secondary');
        if (Api.can('TEACHER')) {
          App.action('Modifier', function () { openForm(student); }, 'btn-secondary');
        }
        App.action('← Retour', function () { location.hash = '#/students'; }, 'btn-ghost');

        container.innerHTML = detailView(student, results, attendance);
      });
    },

    openForm: openForm
  };

  /* ----------------------------- Liste ----------------------------- */
  function fetchList() {
    return Api.get('/api/students' + Api.qs({
      q: state.q, class_id: state.classId, status: state.status, gender: state.gender,
      sort_by: state.sortBy, sort_dir: state.sortDir, limit: state.limit, offset: state.offset
    }));
  }

  function view(classes, page) {
    var html = '<div class="toolbar">' +
      '<input class="search" id="f-q" type="search" placeholder="Rechercher un nom, prénom, matricule…" value="' +
      UI.esc(state.q) + '">' +
      select('f-class', 'Toutes les classes', classes.map(function (c) {
        return { value: c.id, label: c.name + ' — ' + c.level };
      }), state.classId) +
      select('f-status', 'Tous les statuts', STATUS, state.status) +
      select('f-gender', 'Tous les sexes', GENDERS, state.gender) +
      '<button class="btn btn-ghost" id="f-reset">Réinitialiser</button>' +
      '</div>';

    var columns = [
      { key: 'matricule', label: 'Matricule', sortable: true, className: 'mono' },
      {
        key: 'last_name', label: 'Nom', sortable: true,
        render: function (s) {
          return '<a href="#/students/' + s.id + '" class="strong">' +
            UI.esc(s.last_name + ' ' + s.first_name) + '</a>';
        }
      },
      { key: 'birth_date', label: 'Naissance', sortable: true, render: function (s) { return UI.date(s.birth_date); } },
      { key: 'gender', label: 'Sexe', render: function (s) { return s.gender === 'F' ? 'F' : 'M'; } },
      { key: 'class_name', label: 'Classe', render: function (s) { return UI.text(s.class_name); } },
      { key: 'guardian_name', label: 'Parent / tuteur', render: function (s) { return UI.text(s.guardian_name); } },
      { key: 'phone', label: 'Téléphone', render: function (s) { return UI.text(s.phone); } },
      {
        key: 'status', label: 'Statut',
        render: function (s) { return UI.badge(s.status_label || s.status, UI.STATUS_BADGE[s.status]); }
      },
      {
        key: 'actions', label: '', className: 'text-right',
        render: function (s) {
          var out = '<div class="row-actions">' +
            '<a class="btn btn-secondary btn-sm" href="#/students/' + s.id + '">Fiche</a>';
          if (Api.can('TEACHER')) out += '<button class="btn btn-secondary btn-sm" data-edit="' + s.id + '">✏️</button>';
          if (Api.can('ADMIN')) out += '<button class="btn btn-danger btn-sm" data-del="' + s.id + '">🗑</button>';
          return out + '</div>';
        }
      }
    ];

    html += '<div class="card">' +
      UI.table(columns, page.items, {
        sortBy: state.sortBy, sortDir: state.sortDir,
        emptyMessage: 'Aucun élève ne correspond à votre recherche'
      }) +
      UI.pagination(page.total, page.limit, page.offset) +
      '</div>';
    return html;
  }

  function select(id, placeholder, options, value) {
    return '<select id="' + id + '"><option value="">' + UI.esc(placeholder) + '</option>' +
      options.map(function (o) {
        return '<option value="' + UI.esc(o.value) + '"' +
          (String(o.value) === String(value) ? ' selected' : '') + '>' + UI.esc(o.label) + '</option>';
      }).join('') + '</select>';
  }

  function bind(container, page) {
    var search = container.querySelector('#f-q');
    search.oninput = UI.debounce(function () {
      state.q = search.value; state.offset = 0; App.reload();
    });
    ['class', 'status', 'gender'].forEach(function (key) {
      container.querySelector('#f-' + key).onchange = function () {
        state[key === 'class' ? 'classId' : key] = this.value;
        state.offset = 0;
        App.reload();
      };
    });
    container.querySelector('#f-reset').onclick = function () {
      state.q = ''; state.classId = ''; state.status = ''; state.gender = ''; state.offset = 0;
      App.reload();
    };

    container.querySelectorAll('th[data-sort]').forEach(function (th) {
      th.onclick = function () {
        var key = th.dataset.sort;
        if (state.sortBy === key) state.sortDir = state.sortDir === 'asc' ? 'desc' : 'asc';
        else { state.sortBy = key; state.sortDir = 'asc'; }
        App.reload();
      };
    });

    container.querySelectorAll('[data-page]').forEach(function (btn) {
      btn.onclick = function () {
        state.offset = Math.max(0, state.offset + (btn.dataset.page === 'next' ? state.limit : -state.limit));
        App.reload();
      };
    });

    container.querySelectorAll('[data-edit]').forEach(function (btn) {
      btn.onclick = function () {
        var student = page.items.filter(function (s) { return String(s.id) === btn.dataset.edit; })[0];
        openForm(student);
      };
    });

    container.querySelectorAll('[data-del]').forEach(function (btn) {
      btn.onclick = function () {
        var student = page.items.filter(function (s) { return String(s.id) === btn.dataset.del; })[0];
        UI.confirm('Supprimer l\'élève',
          'Supprimer définitivement « ' + student.full_name + ' » ? ' +
          'Ses notes et ses présences seront également supprimées. Cette action est irréversible.',
          function () {
            Api.del('/api/students/' + student.id).then(function () {
              UI.success('Élève supprimé.');
              App.reload();
            }).catch(UI.showError);
          });
      };
    });
  }

  /* ----------------------------- Formulaire ----------------------------- */
  function openForm(student) {
    App.classes().then(function (classes) {
      var s = student || {};
      var fields = [
        { name: 'last_name', label: 'Nom', value: s.last_name, required: true, col: 'half' },
        { name: 'first_name', label: 'Prénom', value: s.first_name, required: true, col: 'half' },
        { name: 'birth_date', label: 'Date de naissance', type: 'date', value: s.birth_date, col: 'half' },
        {
          name: 'gender', label: 'Sexe', type: 'select', value: s.gender || 'M',
          options: GENDERS, col: 'half'
        },
        { name: 'matricule', label: 'Matricule', value: s.matricule, col: 'half',
          help: 'Laisser vide pour une génération automatique' },
        {
          name: 'class_id', label: 'Classe', type: 'select', value: s.class_id, col: 'half',
          options: [{ value: '', label: '— Non affecté —' }].concat(classes.map(function (c) {
            return { value: c.id, label: c.name + ' — ' + c.level };
          }))
        },
        { name: 'address', label: 'Adresse', value: s.address },
        { name: 'phone', label: 'Téléphone', value: s.phone, col: 'half' },
        { name: 'email', label: 'E-mail', type: 'email', value: s.email, col: 'half' },
        { name: 'guardian_name', label: 'Parent / tuteur', value: s.guardian_name, col: 'half' },
        { name: 'guardian_phone', label: 'Téléphone du parent', value: s.guardian_phone, col: 'half' },
        {
          name: 'enrollment_date', label: 'Date d\'inscription', type: 'date',
          value: s.enrollment_date || today(), col: 'half'
        },
        {
          name: 'status', label: 'Statut', type: 'select',
          value: s.status || 'ACTIVE', options: STATUS, col: 'half'
        },
        { name: 'photo_path', label: 'Photo (URL ou chemin)', value: s.photo_path }
      ];

      UI.modal({
        title: student ? 'Modifier l\'élève' : 'Nouvel élève',
        body: UI.form(fields, 'student-form'),
        buttons: [
          { label: 'Annuler', className: 'btn-secondary' },
          {
            label: 'Enregistrer', className: 'btn-primary',
            onClick: function (button) {
              var form = document.getElementById('student-form');
              var data = UI.readForm(form);
              if (data.class_id) data.class_id = Number(data.class_id);
              button.disabled = true;

              var request = student
                ? Api.put('/api/students/' + student.id, data)
                : Api.post('/api/students', data);

              request.then(function () {
                UI.closeModal();
                UI.success(student ? 'Élève mis à jour.' : 'Élève créé.');
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
    });
  }

  /* ----------------------------- Import / export ----------------------------- */
  function exportCsv() {
    Api.download('/api/export/students.csv' + Api.qs({
      q: state.q, class_id: state.classId, status: state.status, gender: state.gender
    }), 'eleves.csv').then(function () { UI.success('Export CSV téléchargé.'); }).catch(UI.showError);
  }

  function openImport() {
    App.classes().then(function (classes) {
      UI.modal({
        title: 'Importer des élèves (CSV)',
        body: '<p class="muted">Le fichier doit contenir une ligne d\'en-tête. Colonnes reconnues : ' +
          '<code>matricule, nom, prenom, date_naissance, sexe, adresse, telephone, email, parent, ' +
          'telephone_parent, date_inscription, statut, classe</code>. ' +
          '<strong>nom</strong> et <strong>prenom</strong> sont obligatoires.</p>' +
          '<div class="field"><label>Classe par défaut</label><select id="imp-class">' +
          '<option value="">— Aucune —</option>' +
          classes.map(function (c) {
            return '<option value="' + c.id + '">' + UI.esc(c.name) + '</option>';
          }).join('') + '</select></div>' +
          '<div class="field"><label>Fichier CSV</label><input type="file" id="imp-file" accept=".csv,text/csv"></div>' +
          '<div id="imp-result"></div>',
        buttons: [
          { label: 'Fermer', className: 'btn-secondary' },
          {
            label: 'Importer', className: 'btn-primary',
            onClick: function (button) {
              var input = document.getElementById('imp-file');
              if (!input.files.length) { UI.error('Choisissez un fichier CSV.'); return; }
              button.disabled = true;
              var reader = new FileReader();
              reader.onload = function () {
                var classId = document.getElementById('imp-class').value;
                Api.post('/api/import/students/csv' + Api.qs({ class_id: classId }),
                         String(reader.result), { contentType: 'text/csv; charset=utf-8' })
                  .then(function (report) {
                    document.getElementById('imp-result').innerHTML = importReport(report);
                    UI.success(report.inserted + ' élève(s) importé(s).');
                    button.disabled = false;
                  })
                  .catch(function (err) { button.disabled = false; UI.showError(err); });
              };
              reader.readAsText(input.files[0]);
            }
          }
        ]
      });
    });
  }

  function importReport(report) {
    var html = '<div class="alert ' + (report.error_count ? 'alert-warning' : 'alert-success') + '">' +
      '<strong>' + report.inserted + '</strong> importé(s), <strong>' + report.skipped +
      '</strong> ignoré(s), <strong>' + report.error_count + '</strong> erreur(s).</div>';
    if (report.errors && report.errors.length) {
      html += '<div class="table-wrap"><table><thead><tr><th>Ligne</th><th>Erreur</th></tr></thead><tbody>' +
        report.errors.map(function (e) {
          return '<tr><td class="mono">' + e.line + '</td><td>' + UI.esc(e.message) + '</td></tr>';
        }).join('') + '</tbody></table></div>';
    }
    return html;
  }

  /* ----------------------------- Fiche élève ----------------------------- */
  function detailView(student, results, attendance) {
    var html = '<div class="card"><div class="card-header"><h2>Informations personnelles</h2>' +
      UI.badge(student.status_label || student.status, UI.STATUS_BADGE[student.status]) +
      '</div><div class="card-body"><div class="detail-grid">' +
      item('Matricule', student.matricule) +
      item('Nom complet', student.full_name) +
      item('Date de naissance', UI.date(student.birth_date)) +
      item('Sexe', student.gender === 'F' ? 'Féminin' : 'Masculin') +
      item('Classe', student.class_name ? (student.class_name + ' (' + (student.class_level || '') + ')') : null) +
      item('Date d\'inscription', UI.date(student.enrollment_date)) +
      item('Adresse', student.address) +
      item('Téléphone', student.phone) +
      item('E-mail', student.email) +
      item('Parent / tuteur', student.guardian_name) +
      item('Téléphone du parent', student.guardian_phone) +
      '</div></div></div>';

    var a = attendance.summary || {};
    html += '<div class="stats-grid">' +
      mini('📈', UI.num(results.general_average), 'Moyenne générale') +
      mini('🏅', results.rank ? (results.rank + ' / ' + results.class_size) : '—', 'Rang dans la classe') +
      mini('✍️', results.grade_count, 'Notes saisies') +
      mini('📅', UI.num(a.attendance_rate, 1) + ' %', 'Taux de présence') +
      mini('🚫', a.absent || 0, 'Absences') +
      mini('⏰', a.late || 0, 'Retards') +
      '</div>';

    if (results.appreciation) {
      html += '<div class="alert alert-info"><strong>Appréciation :</strong> ' +
        UI.esc(results.appreciation) + '</div>';
    }

    html += '<div class="grid-2">';
    html += '<div class="card"><div class="card-header"><h2>Résultats par matière</h2></div>' +
      UI.table([
        { key: 'subject_name', label: 'Matière' },
        { key: 'coefficient', label: 'Coef.', className: 'text-center' },
        { key: 'grade_count', label: 'Notes', className: 'text-center' },
        { key: 'average', label: 'Moyenne', className: 'text-right', render: function (s) { return UI.avg(s.average); } },
        { key: 'best', label: 'Meilleure', className: 'text-right', render: function (s) { return UI.num(s.best); } },
        { key: 'worst', label: 'Pire', className: 'text-right', render: function (s) { return UI.num(s.worst); } },
        { key: 'appreciation', label: 'Appréciation' }
      ], results.subjects, { emptyMessage: 'Aucune note enregistrée' }) + '</div>';

    html += '<div class="card"><div class="card-header"><h2>Moyennes par matière</h2></div>' +
      '<div class="card-body">' + Charts.hbar((results.subjects || []).map(function (s) {
        return { label: s.subject_code || s.subject_name, value: Number(s.average || 0) };
      }), { max: 20 }) + '</div></div>';
    html += '</div>';

    html += '<div class="card"><div class="card-header"><h2>Historique des présences</h2>' +
      '<span class="muted">' + (a.total || 0) + ' enregistrement(s)</span></div>' +
      UI.table([
        { key: 'date', label: 'Date', render: function (e) { return UI.date(e.date); } },
        { key: 'subject_name', label: 'Matière', render: function (e) { return UI.text(e.subject_name); } },
        {
          key: 'status', label: 'Statut',
          render: function (e) { return UI.badge(e.status_label || e.status, UI.ATTENDANCE_BADGE[e.status]); }
        },
        { key: 'justification', label: 'Justification', render: function (e) { return UI.text(e.justification); } }
      ], (attendance.items || []).slice(0, 30), { emptyMessage: 'Aucune présence enregistrée' }) +
      '</div>';

    return html;
  }

  function item(label, value) {
    return '<div class="detail-item"><div class="label">' + UI.esc(label) + '</div>' +
      '<div class="value">' + UI.text(value) + '</div></div>';
  }
  function mini(icon, value, label) {
    return '<div class="stat-card"><div class="stat-icon">' + icon + '</div><div>' +
      '<div class="stat-value">' + UI.esc(value === null || value === undefined ? '—' : value) + '</div>' +
      '<div class="stat-label">' + UI.esc(label) + '</div></div></div>';
  }
  function today() { return new Date().toISOString().substring(0, 10); }
})(window);
