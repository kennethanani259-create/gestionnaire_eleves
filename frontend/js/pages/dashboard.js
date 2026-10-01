/** Page « Tableau de bord » : indicateurs clés et graphiques. */
(function (global) {
  'use strict';
  global.Pages = global.Pages || {};

  var state = { classId: '' };

  global.Pages.dashboard = {
    render: function (container) {
      return Promise.all([
        App.classes(),
        Api.get('/api/dashboard' + Api.qs({ class_id: state.classId }))
      ]).then(function (results) {
        var classes = results[0], data = results[1];
        container.innerHTML = view(classes, data);
        bind(container);
      });
    }
  };

  function view(classes, d) {
    var html = '';

    html += '<div class="toolbar">' +
      '<select id="dash-class" style="min-width:220px">' +
      '<option value="">Tout l\'établissement</option>' +
      classes.map(function (c) {
        return '<option value="' + c.id + '"' + (String(c.id) === String(state.classId) ? ' selected' : '') +
          '>' + UI.esc(c.name) + ' — ' + UI.esc(c.level) + '</option>';
      }).join('') +
      '</select>' +
      '<button class="btn btn-secondary" id="dash-refresh">↻ Actualiser</button>' +
      '</div>';

    html += '<div class="stats-grid">' +
      stat('👥', d.student_count, 'Élèves inscrits') +
      stat('✅', d.active_count, 'Élèves actifs') +
      stat('🏫', d.class_count, 'Classes') +
      stat('📈', UI.num(d.general_average), 'Moyenne générale / 20') +
      stat('🚫', d.absence_count, 'Absences') +
      stat('⏰', d.late_count, 'Retards') +
      stat('📅', UI.num(d.attendance_rate, 1) + ' %', 'Taux de présence') +
      stat('⚠️', d.struggling_count, 'Élèves en difficulté') +
      '</div>';

    // Meilleur élève
    if (d.best_student && d.best_student.student_id) {
      html += '<div class="card"><div class="card-body" style="display:flex;align-items:center;gap:16px;flex-wrap:wrap">' +
        '<div class="stat-icon" style="background:var(--warning-light)">🏆</div>' +
        '<div style="flex:1"><div class="stat-label">Meilleur élève</div>' +
        '<div class="strong" style="font-size:17px">' + UI.esc(d.best_student.full_name) + '</div>' +
        '<div class="muted mono">' + UI.esc(d.best_student.matricule || '') + '</div></div>' +
        '<div class="text-right"><div class="stat-value">' + UI.num(d.best_student.average) + '</div>' +
        '<div class="stat-label">moyenne / 20</div></div>' +
        '<a class="btn btn-secondary" href="#/students/' + d.best_student.student_id + '">Voir la fiche</a>' +
        '</div></div>';
    }

    html += '<div class="grid-2">';

    html += card('Répartition par sexe', Charts.donut([
      { label: 'Garçons', value: d.male_count || 0, color: '#2563eb' },
      { label: 'Filles', value: d.female_count || 0, color: '#db2777' }
    ], { centerLabel: 'élèves' }));

    html += card('Répartition des moyennes',
      Charts.bar((d.average_distribution || []).map(function (b) {
        return { label: b.range, value: b.count };
      }), { color: '#2563eb', integer: true }));

    html += card('Absences par mois',
      Charts.line((d.monthly_absences || []).map(function (m) {
        return { label: shortMonth(m.month), value: m.count };
      }), { color: '#dc2626', integer: true }));

    html += card('Moyennes par matière',
      Charts.hbar((d.subject_statistics || []).map(function (s) {
        return { label: s.subject_name || s.name, value: Number(s.average || 0) };
      }), { max: 20 }));

    html += '</div>';
    return html;
  }

  function card(title, body) {
    return '<div class="card"><div class="card-header"><h2>' + UI.esc(title) + '</h2></div>' +
      '<div class="card-body"><div class="chart-container">' + body + '</div></div></div>';
  }

  function stat(icon, value, label) {
    return '<div class="stat-card"><div class="stat-icon">' + icon + '</div><div>' +
      '<div class="stat-value">' + UI.esc(value === null || value === undefined ? '—' : value) + '</div>' +
      '<div class="stat-label">' + UI.esc(label) + '</div></div></div>';
  }

  function shortMonth(value) {
    // "2026-03" → "03/26"
    var parts = String(value || '').split('-');
    return parts.length === 2 ? parts[1] + '/' + parts[0].substring(2) : value;
  }

  function bind(container) {
    container.querySelector('#dash-class').onchange = function () {
      state.classId = this.value;
      App.reload();
    };
    container.querySelector('#dash-refresh').onclick = function () { App.reload(); };
  }
})(window);
