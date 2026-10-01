/** Page « Résultats » : classement d'une classe et statistiques par matière. */
(function (global) {
  'use strict';
  global.Pages = global.Pages || {};

  var state = { classId: '', term: '' };

  global.Pages.results = {
    render: function (container) {
      return App.classes().then(function (classes) {
        if (!classes.length) {
          container.innerHTML = UI.empty('Créez une classe pour consulter les résultats', '🏫');
          return;
        }
        // Classe pré-sélectionnée via #/results?class=3
        var match = /class=(\d+)/.exec(location.hash);
        if (match && !state.classId) state.classId = match[1];
        if (!state.classId) state.classId = String(classes[0].id);

        App.action('📄 Rapport de classe (PDF)', function () {
          Api.download('/api/reports/classes/' + state.classId + '/pdf' + Api.qs({ term: state.term }),
            'rapport-classe-' + state.classId + '.pdf')
            .then(function () { UI.success('Rapport téléchargé.'); }).catch(UI.showError);
        }, 'btn-secondary');

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

    var html = '<div class="toolbar">' +
      '<select id="r-class">' + classes.map(function (c) {
        return '<option value="' + c.id + '"' + (String(c.id) === String(state.classId) ? ' selected' : '') +
          '>' + UI.esc(c.name) + ' — ' + UI.esc(c.level) + '</option>';
      }).join('') + '</select>' +
      '<select id="r-term"><option value="">Année complète</option>' +
      [1, 2, 3].map(function (t) {
        return '<option value="' + t + '"' + (String(t) === String(state.term) ? ' selected' : '') +
          '>Trimestre ' + t + '</option>';
      }).join('') + '</select>' +
      '</div>';

    var graded = ranking.filter(function (r) { return r.grade_count > 0; });
    var average = graded.length
      ? graded.reduce(function (s, r) { return s + Number(r.average || 0); }, 0) / graded.length : null;
    var struggling = graded.filter(function (r) { return Number(r.average) < 10; }).length;

    html += '<div class="stats-grid">' +
      stat('👥', ranking.length, 'Élèves classés') +
      stat('📈', UI.num(average), 'Moyenne de la classe') +
      stat('🏆', graded.length ? UI.num(graded[0].average) : '—', 'Meilleure moyenne') +
      stat('⚠️', struggling, 'Élèves en difficulté') +
      '</div>';

    html += '<div class="card"><div class="card-header"><h2>Classement</h2>' +
      '<span class="muted">Moyennes pondérées par les coefficients, calculées par le serveur</span></div>' +
      UI.table([
        {
          key: 'rank', label: 'Rang', className: 'text-center',
          render: function (r) {
            var medals = { 1: '🥇', 2: '🥈', 3: '🥉' };
            return '<span class="strong">' + (medals[r.rank] || r.rank) + '</span>';
          }
        },
        { key: 'matricule', label: 'Matricule', className: 'mono' },
        {
          key: 'full_name', label: 'Élève',
          render: function (r) {
            return '<a href="#/students/' + r.student_id + '" class="strong">' + UI.esc(r.full_name) + '</a>';
          }
        },
        { key: 'grade_count', label: 'Notes', className: 'text-center' },
        { key: 'average', label: 'Moyenne / 20', className: 'text-right', render: function (r) { return UI.avg(r.average); } },
        {
          key: 'bar', label: 'Niveau',
          render: function (r) {
            var pct = Math.max(0, Math.min(100, (Number(r.average) / 20) * 100));
            var color = r.average >= 12 ? 'var(--success)' : (r.average >= 10 ? 'var(--warning)' : 'var(--danger)');
            return '<div style="background:#e2e8f0;border-radius:6px;height:8px;min-width:90px">' +
              '<div style="width:' + pct.toFixed(1) + '%;background:' + color +
              ';height:8px;border-radius:6px"></div></div>';
          }
        },
        {
          key: 'pdf', label: '', className: 'text-right',
          render: function (r) {
            return '<button class="btn btn-secondary btn-sm" data-pdf="' + r.student_id +
              '" data-name="' + UI.esc(r.matricule || r.student_id) + '">📄 Bulletin</button>';
          }
        }
      ], ranking, { emptyMessage: 'Aucun élève dans cette classe' }) + '</div>';

    html += '<div class="grid-2">' +
      '<div class="card"><div class="card-header"><h2>Moyennes par matière</h2></div><div class="card-body">' +
      Charts.hbar(stats.map(function (s) {
        return { label: s.subject_code || s.subject_name || s.name, value: Number(s.average || 0) };
      }), { max: 20 }) + '</div></div>' +
      '<div class="card"><div class="card-header"><h2>Détail par matière</h2></div>' +
      UI.table([
        { key: 'subject_name', label: 'Matière', render: function (s) { return UI.text(s.subject_name || s.name); } },
        { key: 'coefficient', label: 'Coef.', className: 'text-center', render: function (s) { return UI.num(s.coefficient, 1); } },
        { key: 'grade_count', label: 'Notes', className: 'text-center', render: function (s) { return UI.text(s.grade_count); } },
        { key: 'average', label: 'Moyenne', className: 'text-right', render: function (s) { return UI.avg(s.average); } },
        { key: 'best', label: 'Max', className: 'text-right', render: function (s) { return UI.num(s.best); } },
        { key: 'worst', label: 'Min', className: 'text-right', render: function (s) { return UI.num(s.worst); } }
      ], stats, { emptyMessage: 'Aucune statistique disponible' }) + '</div></div>';

    return html;
  }

  function stat(icon, value, label) {
    return '<div class="stat-card"><div class="stat-icon">' + icon + '</div><div>' +
      '<div class="stat-value">' + UI.esc(value) + '</div>' +
      '<div class="stat-label">' + UI.esc(label) + '</div></div></div>';
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
