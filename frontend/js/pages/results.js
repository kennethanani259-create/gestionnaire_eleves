/** Page « Résultats » : classement d'une classe et statistiques par matière. */
(function (global) {
  'use strict';
  global.Pages = global.Pages || {};

  var state = { classId: '', term: '' };

  global.Pages.results = {
    render: function (container) {
      return App.classes().then(function (classes) {
        if (!classes.length) {
          container.innerHTML = UI.blank('Créez une classe pour consulter les résultats', '');
          return;
        }
        // Classe pré-sélectionnée via #/results?class=3
        var match = /class=(\d+)/.exec(location.hash);
        if (match && !state.classId) state.classId = match[1];
        if (!state.classId) state.classId = String(classes[0].id);

        App.action('Rapport de classe (PDF)', function () {
          Api.download('/api/reports/classes/' + state.classId + '/pdf' + Api.qs({ term: state.term }),
            'rapport-classe-' + state.classId + '.pdf')
            .then(function () { UI.success('Rapport téléchargé.'); }).catch(UI.showError);
        });

        return Api.get('/api/classes/' + state.classId + '/ranking' + Api.qs({ term: state.term }))
          .then(function (data) {
            container.innerHTML = view(classes, data);
            bind(container);
          });
      });
    }
  };

  function view(classes, data) {
    var ranking = data.items || [];
    var stats = data.subject_statistics || [];

    var html = '<div class="filters">' +
      '<select id="r-class">' + classes.map(function (c) {
        return '<option value="' + c.id + '"' + (String(c.id) === String(state.classId) ? 'selected' : '') +
          '>' + UI.esc(c.name) + ' — ' + UI.esc(c.level) + '</option>';
      }).join('') + '</select>' +
      '<select id="r-term"><option value="">Année complète</option>' +
      [1, 2, 3].map(function (t) {
        return '<option value="' + t + '"' + (String(t) === String(state.term) ? 'selected' : '') +
          '>Trimestre ' + t + '</option>';
      }).join('') + '</select>' +
      '</div>';

    var graded = ranking.filter(function (r) { return r.grade_count > 0; });
    var average = graded.length
      ? graded.reduce(function (s, r) { return s + Number(r.average || 0); }, 0) / graded.length : null;
    var struggling = graded.filter(function (r) { return Number(r.average) < 10; }).length;

    html += UI.readings([
      { value: ranking.length, label: 'Élèves classés',
        note: graded.length + ' avec au moins une note' },
      { value: UI.num(average), unit: '/20', label: 'Moyenne de la classe',
        tone: average === null ? null : (average >= 10 ? 'good' : 'alert') },
      { value: graded.length ? UI.num(graded[0].average) : '—', unit: graded.length ? '/20' : '',
        label: 'Meilleure moyenne',
        note: graded.length ? UI.esc(graded[0].full_name) : null },
      { value: struggling, label: 'Sous la moyenne',
        tone: struggling ? 'alert' : 'good' }
    ]);

    html += '<div class="panel"><div class="panel__head"><h2>Classement</h2>' +
      '<span class="muted">Moyennes pondérées par les coefficients, calculées par le serveur</span></div>' +
      UI.table([
        {
          key: 'rank', label: 'Rang', cls: 't-center',
          render: function (r) {
            return '<span class="rank' + (r.rank === 1 ? ' rank--1' : '') + '">' +
              r.rank + (r.rank === 1 ? '<sup>er</sup>' : '<sup>e</sup>') + '</span>';
          }
        },
        { key: 'matricule', label: 'Matricule', cls: 'mono' },
        {
          key: 'full_name', label: 'Élève',
          render: function (r) {
            return '<a href="#/students/' + r.student_id + '" class="name">' + UI.esc(r.full_name) + '</a>';
          }
        },
        { key: 'grade_count', label: 'Notes', cls: 't-center' },
        { key: 'average', label: 'Moyenne / 20', cls: 't-right', render: function (r) { return UI.score(r.average); } },
        { key: 'bar', label: 'Niveau', render: function (r) { return UI.gauge(r.average); } },
        {
          key: 'pdf', label: '', cls: 't-right',
          render: function (r) {
            return '<button class="btn btn--sm" data-pdf="' + r.student_id +
              '" data-name="' + UI.esc(r.matricule || r.student_id) + '"> Bulletin</button>';
          }
        }
      ], ranking, { emptyTitle: 'Aucun élève dans cette classe' }) + '</div>';

    html += '<div class="columns">' +
      '<div class="panel"><div class="panel__head"><h2>Moyennes par matière</h2></div><div class="panel__body">' +
      Charts.hbar(stats.map(function (s) {
        return { label: s.subject_code || s.subject_name || s.name, value: Number(s.average || 0) };
      }), { max: 20 }) + '</div></div>' +
      '<div class="panel"><div class="panel__head"><h2>Détail par matière</h2></div>' +
      UI.table([
        { key: 'subject_name', label: 'Matière', render: function (s) { return UI.text(s.subject_name || s.name); } },
        { key: 'coefficient', label: 'Coef.', cls: 't-center', render: function (s) { return UI.num(s.coefficient, 1); } },
        { key: 'grade_count', label: 'Notes', cls: 't-center', render: function (s) { return UI.text(s.grade_count); } },
        { key: 'average', label: 'Moyenne', cls: 't-right', render: function (s) { return UI.score(s.average); } },
        { key: 'best', label: 'Max', cls: 't-right', render: function (s) { return UI.num(s.best); } },
        { key: 'worst', label: 'Min', cls: 't-right', render: function (s) { return UI.num(s.worst); } }
      ], stats, { emptyTitle: 'Aucune statistique disponible' }) + '</div></div>';

    return html;
  }

  function bind(container) {
    container.querySelector('#r-class').onchange = function () { state.classId = this.value; App.reload(); };
    container.querySelector('#r-term').onchange = function () { state.term = this.value; App.reload(); };
    container.querySelectorAll('[data-pdf]').forEach(function (btn) {
      btn.onclick = function () {
        btn.disabled = true;
        Api.download('/api/reports/students/' + btn.dataset.pdf + '/pdf' + Api.qs({ term: state.term }),
          'bulletin-' + btn.dataset.name + '.pdf')
          .then(function () { UI.success('Bulletin téléchargé.'); })
          .catch(UI.showError)
          .then(function () { btn.disabled = false; });
      };
    });
  }
})(window);
