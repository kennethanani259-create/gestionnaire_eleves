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
        App.action('Importer CSV', openImport);
      }
      App.action('Exporter CSV', exportCsv);

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

        App.action('Bulletin PDF', function () {
          Api.download('/api/reports/students/' + id + '/pdf',
            'bulletin-' + (student.matricule || id) + '.pdf')
            .then(function () { UI.success('Bulletin téléchargé.'); })
            .catch(UI.showError);
        });
        if (Api.can('TEACHER')) {
          App.action('Modifier', function () { openForm(student); });
        }
        App.action('Retour', function () { location.hash = '#/students'; }, { variant: 'ghost' });

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
    var html = '<div class="filters">' +
      '<input class="search" id="f-q" type="search" placeholder="Rechercher un nom, prénom, matricule…" value="' +
      UI.esc(state.q) + '">' +
      select('f-class', 'Toutes les classes', classes.map(function (c) {
        return { value: c.id, label: c.name + ' — ' + c.level };
      }), state.classId) +
      select('f-status', 'Tous les statuts', STATUS, state.status) +
      select('f-gender', 'Tous les sexes', GENDERS, state.gender) +
      '<button class="btn btn--ghost" id="f-reset">Réinitialiser</button>' +
      '</div>';

    var columns = [
      { key: 'matricule', label: 'Matricule', sortable: true, cls: 'mono' },
      {
        key: 'last_name', label: 'Nom', sortable: true,
        render: function (s) {
          return '<a href="#/students/' + s.id + '" class="name">' +
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
        render: function (s) { return UI.tag(s.status_label || s.status, UI.STATUS_TAG[s.status]); }
      },
      {
        key: 'actions', label: '', cls: 't-right',
        render: function (s) {
          var out = '<div class="row-actions">' +
            '<a class="btn btn--sm" href="#/students/' + s.id + '">Fiche</a>';
          if (Api.can('TEACHER')) out += '<button class="btn btn--sm" data-edit="' + s.id + '">' + UI.icon('edit') + '</button>';
          if (Api.can('ADMIN')) out += '<button class="btn btn--danger btn--sm" data-del="' + s.id + '">' + UI.icon('trash') + '</button>';
          return out + '</div>';
        }
      }
    ];

    html += '<div class="panel">' +
      UI.table(columns, page.items, {
        sortBy: state.sortBy, sortDir: state.sortDir,
        emptyTitle: 'Aucun élève ne correspond à votre recherche'
      }) +
      UI.pager(page.total, page.limit, page.offset) +
      '</div>';
    return html;
  }

  function select(id, placeholder, options, value) {
    return '<select id="' + id + '"><option value="">' + UI.esc(placeholder) + '</option>' +
      options.map(function (o) {
        return '<option value="' + UI.esc(o.value) + '"' +
          (String(o.value) === String(value) ? 'selected' : '') + '>' + UI.esc(o.label) + '</option>';
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
        { name: 'last_name', label: 'Nom', value: s.last_name, required: true, half: true },
        { name: 'first_name', label: 'Prénom', value: s.first_name, required: true, half: true },
        { name: 'birth_date', label: 'Date de naissance', type: 'date', value: s.birth_date, half: true },
        {
          name: 'gender', label: 'Sexe', type: 'select', value: s.gender || 'M',
          options: GENDERS, half: true
        },
        { name: 'matricule', label: 'Matricule', value: s.matricule, half: true,
          help: 'Laisser vide pour une génération automatique' },
        {
          name: 'class_id', label: 'Classe', type: 'select', value: s.class_id, half: true,
          options: [{ value: '', label: '— Non affecté —' }].concat(classes.map(function (c) {
            return { value: c.id, label: c.name + ' — ' + c.level };
          }))
        },
        { name: 'address', label: 'Adresse', value: s.address },
        { name: 'phone', label: 'Téléphone', value: s.phone, half: true },
        { name: 'email', label: 'E-mail', type: 'email', value: s.email, half: true },
        { name: 'guardian_name', label: 'Parent / tuteur', value: s.guardian_name, half: true },
        { name: 'guardian_phone', label: 'Téléphone du parent', value: s.guardian_phone, half: true },
        {
          name: 'enrollment_date', label: 'Date d\'inscription', type: 'date',
          value: s.enrollment_date || today(), half: true
        },
        {
          name: 'status', label: 'Statut', type: 'select',
          value: s.status || 'ACTIVE', options: STATUS, half: true
        },
        { name: 'photo_path', label: 'Photo (URL ou chemin)', value: s.photo_path }
      ];

      UI.modal({
        title: student ? 'Modifier l\'élève' : 'Nouvel élève',
        body: UI.form(fields, 'student-form'),
        buttons: [
          { label: 'Annuler' },
          {
            label: 'Enregistrer', variant: 'primary',
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
          { label: 'Fermer' },
          {
            label: 'Importer', variant: 'primary',
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
      html += '<div class="table-scroll"><table><thead><tr><th>Ligne</th><th>Erreur</th></tr></thead><tbody>' +
        report.errors.map(function (e) {
          return '<tr><td class="mono">' + e.line + '</td><td>' + UI.esc(e.message) + '</td></tr>';
        }).join('') + '</tbody></table></div>';
    }
    return html;
  }

  /* ----------------------------- Fiche élève ----------------------------- */
  function detailView(student, results, attendance) {
    var html = '<div class="panel"><div class="panel__head"><h2>Informations personnelles</h2>' +
      UI.tag(student.status_label || student.status, UI.STATUS_TAG[student.status]) +
      '</div><div class="panel__body"><div class="record">' +
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
    html += UI.readings([
      { value: UI.num(results.general_average), unit: '/20', label: 'Moyenne générale',
        tone: results.general_average === null ? null : (results.general_average >= 10 ? 'good' : 'alert'),
        note: results.grade_count + ' note(s) sur ' + UI.num(results.total_coefficients, 0) + ' points de coefficient' },
      { value: results.rank || '—',
        unit: results.rank ? ' / ' + results.class_size : '',
        label: 'Rang dans la classe' },
      { value: UI.num(a.attendance_rate, 1), unit: '%', label: 'Taux de présence',
        tone: a.attendance_rate >= 90 ? 'good' : 'alert',
        note: (a.total || 0) + ' relevé(s)' },
      { value: a.absent || 0, label: 'Absences',
        tone: (a.absent || 0) ? 'alert' : 'good',
        note: (a.excused || 0) + ' justifiée(s), ' + (a.late || 0) + ' retard(s)' }
    ]);

    if (results.appreciation) {
      html += '<div class="notice notice--info"><strong>Appréciation :</strong> ' +
        UI.esc(results.appreciation) + '</div>';
    }

    html += '<div class="columns">';
    html += '<div class="panel"><div class="panel__head"><h2>Résultats par matière</h2></div>' +
      UI.table([
        { key: 'subject_name', label: 'Matière' },
        { key: 'coefficient', label: 'Coef.', cls: 't-center' },
        { key: 'grade_count', label: 'Notes', cls: 't-center' },
        { key: 'average', label: 'Moyenne', cls: 't-right', render: function (s) { return UI.score(s.average); } },
        { key: 'best', label: 'Meilleure', cls: 't-right', render: function (s) { return UI.num(s.best); } },
        { key: 'worst', label: 'Pire', cls: 't-right', render: function (s) { return UI.num(s.worst); } },
        { key: 'appreciation', label: 'Appréciation' }
      ], results.subjects, { emptyTitle: 'Aucune note enregistrée' }) + '</div>';

    html += '<div class="panel"><div class="panel__head"><h2>Moyennes par matière</h2></div>' +
      '<div class="panel__body">' + Charts.hbar((results.subjects || []).map(function (s) {
        return { label: s.subject_code || s.subject_name, value: Number(s.average || 0) };
      }), { max: 20 }) + '</div></div>';
    html += '</div>';

    html += '<div class="panel"><div class="panel__head"><h2>Historique des présences</h2>' +
      '<span class="muted">' + (a.total || 0) + 'enregistrement(s)</span></div>' +
      UI.table([
        { key: 'date', label: 'Date', render: function (e) { return UI.date(e.date); } },
        { key: 'subject_name', label: 'Matière', render: function (e) { return UI.text(e.subject_name); } },
        {
          key: 'status', label: 'Statut',
          render: function (e) { return UI.tag(e.status_label || e.status, UI.ATTENDANCE_TAG[e.status]); }
        },
        { key: 'justification', label: 'Justification', render: function (e) { return UI.text(e.justification); } }
      ], (attendance.items || []).slice(0, 30), { emptyTitle: 'Aucune présence enregistrée' }) +
      '</div>';

    return html;
  }

  function item(label, value) {
    return '<div><p class="record__k">' + UI.esc(label) + '</p>' +
      '<p class="record__v">' + UI.text(value) + '</p></div>';
  }
  function today() { return new Date().toISOString().substring(0, 10); }
})(window);
