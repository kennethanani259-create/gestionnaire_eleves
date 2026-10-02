/** Page « Mes enfants » — espace des comptes parents. */
(function (global) {
  'use strict';
  global.Pages = global.Pages || {};

  global.Pages.children = {
    render: function (container) {
      return Api.get('/api/parent/children').then(function (data) {
        var children = data.items || [];
        if (!children.length) {
          container.innerHTML = UI.blank(
            'Aucun enfant rattaché à votre compte',
            "L'établissement doit rattacher votre ou vos enfants à ce compte. " +
            'Adressez-vous au secrétariat en indiquant votre identifiant.');
          return null;
        }
        // Un bulletin par enfant : les parents en ont rarement plus de trois,
        // on affiche donc tout plutôt que d'imposer une sélection.
        return Promise.all(children.map(function (child) {
          return Promise.all([
            Api.get('/api/parent/children/' + child.id + '/results').catch(function () { return null; }),
            Api.get('/api/parent/children/' + child.id + '/attendance').catch(function () { return null; })
          ]).then(function (parts) {
            return { child: child, results: parts[0], attendance: parts[1] };
          });
        })).then(function (rows) {
          container.innerHTML = rows.map(view).join('');
        });
      });
    }
  };

  function view(row) {
    var child = row.child;
    var results = row.results;
    var attendance = row.attendance;

    var readings = [];
    if (results) {
      readings.push({
        value: UI.num(results.general_average, 2), unit: '/20', label: 'Moyenne générale',
        tone: results.general_average >= 10 ? 'good' : 'bad',
        note: results.appreciation || ''
      });
      if (results.rank) {
        readings.push({
          value: results.rank, unit: 'e', label: 'Rang',
          note: 'sur ' + results.class_size + ' élèves'
        });
      }
    }
    if (attendance && attendance.summary) {
      readings.push({
        value: UI.num(attendance.summary.attendance_rate, 1), unit: '%', label: 'Présence',
        tone: attendance.summary.attendance_rate >= 90 ? 'good' : 'warn',
        note: attendance.summary.absent + ' absence(s), ' + attendance.summary.late + ' retard(s)'
      });
    }

    var body = '<p class="lede">' + UI.esc(child.class_name || 'Classe non affectée') +
      ' · matricule ' + UI.esc(child.matricule) + '</p>' +
      (readings.length ? UI.readings(readings) : '');

    if (results && results.subjects && results.subjects.length) {
      body += UI.table([
        { key: 'subject_name', label: 'Matière' },
        { key: 'coefficient', label: 'Coef.', cls: 't-center',
          render: function (s) { return UI.num(s.coefficient, 1); } },
        { key: 'average', label: 'Moyenne', cls: 't-right',
          render: function (s) { return UI.score(s.average); } },
        { key: 'appreciation', label: 'Appréciation',
          render: function (s) { return UI.text(s.appreciation); } }
      ], results.subjects, { emptyTitle: 'Aucune note enregistrée' });
    } else {
      body += '<p class="muted">Aucune note enregistrée pour le moment.</p>';
    }

    return UI.panel(child.full_name || (child.first_name + ' ' + child.last_name), body);
  }
})(window);
