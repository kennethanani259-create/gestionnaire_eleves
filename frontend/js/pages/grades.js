/** Page « Notes » : saisie, filtrage et modification des évaluations. */
(function (global) {
  'use strict';
  global.Pages = global.Pages || {};

  var EVAL_TYPES = [
    { value: 'HOMEWORK', label: 'Devoir' },
    { value: 'QUIZ', label: 'Interrogation' },
    { value: 'EXAM', label: 'Examen' },
    { value: 'LAB', label: 'Travaux pratiques' },
    { value: 'PROJECT', label: 'Projet' },
    { value: 'CONTINUOUS', label: 'Contrôle continu' }
  ];
  var TERMS = [{ value: 1, label: '1er trimestre' }, { value: 2, label: '2e trimestre' }, { value: 3, label: '3e trimestre' }];

  var state = { classId: '', subjectId: '', term: '', evalType: '', limit: 100, offset: 0 };

  global.Pages.grades = {
    render: function (container) {
      if (Api.can('TEACHER')) App.action('+ Saisir une note', function () { openForm(null); });
      App.action('Exporter CSV', function () {
        Api.download('/api/export/grades.csv' + Api.qs({ class_id: state.classId, term: state.term }), 'notes.csv')
          .then(function () { UI.success('Export téléchargé.'); }).catch(UI.showError);
      }, 'btn-secondary');

      return Promise.all([
        App.classes(), App.subjects(),
        Api.get('/api/grades' + Api.qs({
          class_id: state.classId, subject_id: state.subjectId, term: state.term,
          eval_type: state.evalType, limit: state.limit, offset: state.offset
        }))
      ]).then(function (r) {
        var grades = r[2].items || [];
        container.innerHTML = view(r[0], r[1], grades);
        bind(container, grades);
      });
    },
    EVAL_TYPES: EVAL_TYPES,
    TERMS: TERMS
  };

  function view(classes, subjects, grades) {
    var visibleSubjects = state.classId
      ? subjects.filter(function (s) { return String(s.class_id) === String(state.classId); })
      : subjects;

    var html = '<div class="toolbar">' +
      sel('g-class', 'Toutes les classes', classes.map(function (c) {
        return { value: c.id, label: c.name };
      }), state.classId) +
      sel('g-subject', 'Toutes les matières', visibleSubjects.map(function (s) {
        return { value: s.id, label: s.name + (s.class_name ? ' (' + s.class_name + ')' : '') };
      }), state.subjectId) +
      sel('g-term', 'Tous les trimestres', TERMS, state.term) +
      sel('g-type', 'Tous les types', EVAL_TYPES, state.evalType) +
      '<button class="btn btn-ghost" id="g-reset">Réinitialiser</button>' +
      '</div>';

    var columns = [
      { key: 'eval_date', label: 'Date', render: function (g) { return UI.date(g.eval_date); } },
      {
        key: 'student_name', label: 'Élève',
        render: function (g) {
          return '<a href="#/students/' + g.student_id + '">' + UI.text(g.student_name) + '</a>';
        }
      },
      { key: 'subject_name', label: 'Matière', render: function (g) { return UI.text(g.subject_name); } },
      {
        key: 'eval_type', label: 'Type',
        render: function (g) { return UI.badge(g.eval_type_label || g.eval_type, 'badge-neutral'); }
      },
      { key: 'term', label: 'Trim.', className: 'text-center' },
      {
        key: 'score', label: 'Note', className: 'text-right',
        render: function (g) {
          return '<span class="strong">' + UI.num(g.score) + '</span> <span class="muted">/ ' +
            UI.num(g.max_score, 0) + '</span>';
        }
      },
      { key: 'score_20', label: 'Sur 20', className: 'text-right', render: function (g) { return UI.avg(g.score_20); } },
      { key: 'comment', label: 'Commentaire', render: function (g) { return UI.text(g.comment); } }
    ];
    if (Api.can('TEACHER')) {
      columns.push({
        key: 'actions', label: '', className: 'text-right',
        render: function (g) {
          return '<div class="row-actions">' +
            '<button class="btn btn-secondary btn-sm" data-edit="' + g.id + '">✏️</button>' +
            '<button class="btn btn-danger btn-sm" data-del="' + g.id + '">🗑</button></div>';
        }
      });
    }

    var average = grades.length
      ? grades.reduce(function (sum, g) { return sum + Number(g.score_20 || 0); }, 0) / grades.length
      : null;

    html += '<div class="stats-grid">' +
      '<div class="stat-card"><div class="stat-icon">✍️</div><div><div class="stat-value">' +
      grades.length + '</div><div class="stat-label">Notes affichées</div></div></div>' +
      '<div class="stat-card"><div class="stat-icon">📊</div><div><div class="stat-value">' +
      UI.num(average) + '</div><div class="stat-label">Moyenne des notes affichées</div></div></div>' +
      '</div>';

    return html + '<div class="card">' +
      UI.table(columns, grades, { emptyMessage: 'Aucune note ne correspond aux filtres' }) + '</div>';
  }

  function sel(id, placeholder, options, value) {
    return '<select id="' + id + '"><option value="">' + UI.esc(placeholder) + '</option>' +
      options.map(function (o) {
        return '<option value="' + UI.esc(o.value) + '"' +
          (String(o.value) === String(value) ? ' selected' : '') + '>' + UI.esc(o.label) + '</option>';
      }).join('') + '</select>';
  }

  function bind(container, grades) {
    container.querySelector('#g-class').onchange = function () {
      state.classId = this.value; state.subjectId = ''; App.reload();
    };
    container.querySelector('#g-subject').onchange = function () { state.subjectId = this.value; App.reload(); };
    container.querySelector('#g-term').onchange = function () { state.term = this.value; App.reload(); };
    container.querySelector('#g-type').onchange = function () { state.evalType = this.value; App.reload(); };
    container.querySelector('#g-reset').onclick = function () {
      state.classId = ''; state.subjectId = ''; state.term = ''; state.evalType = '';
      App.reload();
    };

    function find(id) { return grades.filter(function (g) { return String(g.id) === String(id); })[0]; }
    container.querySelectorAll('[data-edit]').forEach(function (btn) {
      btn.onclick = function () { openForm(find(btn.dataset.edit)); };
    });
    container.querySelectorAll('[data-del]').forEach(function (btn) {
      btn.onclick = function () {
        var g = find(btn.dataset.del);
        UI.confirm('Supprimer la note',
          'Supprimer la note de ' + (g.student_name || '') + ' en ' + (g.subject_name || '') + ' ?',
          function () {
            Api.del('/api/grades/' + g.id).then(function () {
              UI.success('Note supprimée.');
              App.reload();
            }).catch(UI.showError);
          });
      };
    });
  }

  function openForm(grade) {
    var g = grade || {};
    Promise.all([
      App.subjects(),
      Api.get('/api/students' + Api.qs({ class_id: state.classId, limit: 500, sort_by: 'last_name' }))
    ]).then(function (r) {
      var subjects = r[0], students = r[1].items || [];

      var fields = [
        {
          name: 'student_id', label: 'Élève', type: 'select', required: true, value: g.student_id,
          options: students.map(function (s) {
            return { value: s.id, label: s.last_name + ' ' + s.first_name + (s.class_name ? ' — ' + s.class_name : '') };
          })
        },
        {
          name: 'subject_id', label: 'Matière', type: 'select', required: true, value: g.subject_id,
          options: subjects.map(function (s) {
            return { value: s.id, label: s.name + (s.class_name ? ' (' + s.class_name + ')' : '') };
          })
        },
        { name: 'eval_type', label: 'Type d\'évaluation', type: 'select', value: g.eval_type || 'HOMEWORK', options: EVAL_TYPES, col: 'half' },
        { name: 'term', label: 'Trimestre', type: 'select', value: g.term || 1, options: TERMS, col: 'half' },
        { name: 'score', label: 'Note obtenue', type: 'number', step: '0.25', min: 0, value: g.score, required: true, col: 'half' },
        { name: 'max_score', label: 'Barème', type: 'number', step: '0.5', min: 1, value: g.max_score === undefined ? 20 : g.max_score, required: true, col: 'half' },
        { name: 'eval_date', label: 'Date de l\'évaluation', type: 'date', value: g.eval_date || today() },
        { name: 'comment', label: 'Commentaire', type: 'textarea', value: g.comment, rows: 2 }
      ];

      if (!students.length) {
        UI.modal({
          title: 'Saisir une note',
          body: '<div class="alert alert-warning">Aucun élève disponible. Créez d\'abord des élèves.</div>',
          buttons: [{ label: 'Fermer' }]
        });
        return;
      }

      UI.modal({
        title: grade ? 'Modifier la note' : 'Saisir une note',
        body: UI.form(fields, 'grade-form'),
        buttons: [
          { label: 'Annuler', className: 'btn-secondary' },
          {
            label: 'Enregistrer', className: 'btn-primary',
            onClick: function (button) {
              var form = document.getElementById('grade-form');
              var data = UI.readForm(form);
              data.student_id = Number(data.student_id);
              data.subject_id = Number(data.subject_id);
              data.term = Number(data.term);
              data.score = Number(data.score);
              data.max_score = Number(data.max_score);
              button.disabled = true;

              var request = grade
                ? Api.put('/api/grades/' + grade.id, data)
                : Api.post('/api/grades', data);

              request.then(function () {
                UI.closeModal();
                UI.success(grade ? 'Note mise à jour.' : 'Note enregistrée.');
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
    }).catch(UI.showError);
  }

  function today() { return new Date().toISOString().substring(0, 10); }
})(window);
