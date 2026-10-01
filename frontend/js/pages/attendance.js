/** Page « Présences » : appel groupé d'une classe et historique filtrable. */
(function (global) {
  'use strict';
  global.Pages = global.Pages || {};

  var STATUSES = [
    { value: 'PRESENT', label: 'Présent' },
    { value: 'ABSENT', label: 'Absent' },
    { value: 'EXCUSED', label: 'Absent justifié' },
    { value: 'LATE', label: 'Retard' }
  ];

  var state = { classId: '', status: '', from: '', to: '' };

  global.Pages.attendance = {
    render: function (container) {
      if (Api.can('TEACHER')) App.action('✅ Faire l\'appel', openRollCall);

      return Promise.all([
        App.classes(),
        Api.get('/api/attendance' + Api.qs({
          class_id: state.classId, status: state.status,
          from: state.from, to: state.to, limit: 200
        }))
      ]).then(function (r) {
        var entries = r[1].items || [];
        container.innerHTML = view(r[0], entries);
        bind(container, entries);
      });
    },
    STATUSES: STATUSES
  };

  function view(classes, entries) {
    var counts = { PRESENT: 0, ABSENT: 0, EXCUSED: 0, LATE: 0 };
    entries.forEach(function (e) { if (counts[e.status] !== undefined) counts[e.status]++; });
    var total = entries.length;
    var rate = total ? ((counts.PRESENT + counts.LATE) / total) * 100 : null;

    var html = '<div class="toolbar">' +
      '<select id="a-class"><option value="">Toutes les classes</option>' +
      classes.map(function (c) {
        return '<option value="' + c.id + '"' + (String(c.id) === String(state.classId) ? ' selected' : '') +
          '>' + UI.esc(c.name) + '</option>';
      }).join('') + '</select>' +
      '<select id="a-status"><option value="">Tous les statuts</option>' +
      STATUSES.map(function (s) {
        return '<option value="' + s.value + '"' + (s.value === state.status ? ' selected' : '') +
          '>' + UI.esc(s.label) + '</option>';
      }).join('') + '</select>' +
      '<label class="muted">Du</label><input type="date" id="a-from" value="' + UI.esc(state.from) + '">' +
      '<label class="muted">au</label><input type="date" id="a-to" value="' + UI.esc(state.to) + '">' +
      '<button class="btn btn-ghost" id="a-reset">Réinitialiser</button>' +
      '</div>';

    html += '<div class="stats-grid">' +
      stat('✅', counts.PRESENT, 'Présents') +
      stat('🚫', counts.ABSENT, 'Absents') +
      stat('📝', counts.EXCUSED, 'Absences justifiées') +
      stat('⏰', counts.LATE, 'Retards') +
      stat('📊', UI.num(rate, 1) + ' %', 'Taux de présence') +
      '</div>';

    html += '<div class="grid-2"><div class="card">' +
      '<div class="card-header"><h2>Répartition des relevés</h2></div><div class="card-body">' +
      Charts.donut([
        { label: 'Présents', value: counts.PRESENT, color: '#16a34a' },
        { label: 'Absents', value: counts.ABSENT, color: '#dc2626' },
        { label: 'Justifiés', value: counts.EXCUSED, color: '#d97706' },
        { label: 'Retards', value: counts.LATE, color: '#2563eb' }
      ], { centerLabel: 'relevés' }) + '</div></div>';

    html += '<div class="card"><div class="card-header"><h2>Relevés par jour</h2></div><div class="card-body">' +
      Charts.bar(absencesByDay(entries), { color: '#dc2626', integer: true }) + '</div></div></div>';

    var columns = [
      { key: 'date', label: 'Date', render: function (e) { return UI.date(e.date); } },
      { key: 'time', label: 'Heure', render: function (e) { return UI.text(e.time); } },
      {
        key: 'student_name', label: 'Élève',
        render: function (e) { return '<a href="#/students/' + e.student_id + '">' + UI.text(e.student_name) + '</a>'; }
      },
      { key: 'subject_name', label: 'Matière', render: function (e) { return UI.text(e.subject_name); } },
      {
        key: 'status', label: 'Statut',
        render: function (e) { return UI.badge(e.status_label || e.status, UI.ATTENDANCE_BADGE[e.status]); }
      },
      { key: 'justification', label: 'Justification', render: function (e) { return UI.text(e.justification); } }
    ];
    if (Api.can('TEACHER')) {
      columns.push({
        key: 'actions', label: '', className: 'text-right',
        render: function (e) {
          return '<div class="row-actions">' +
            '<button class="btn btn-secondary btn-sm" data-edit="' + e.id + '">✏️</button>' +
            '<button class="btn btn-danger btn-sm" data-del="' + e.id + '">🗑</button></div>';
        }
      });
    }

    return html + '<div class="card"><div class="card-header"><h2>Historique</h2></div>' +
      UI.table(columns, entries, { emptyMessage: 'Aucun relevé de présence' }) + '</div>';
  }

  function absencesByDay(entries) {
    var map = {};
    entries.forEach(function (e) {
      if (e.status === 'PRESENT') return;
      var day = String(e.date).substring(0, 10);
      map[day] = (map[day] || 0) + 1;
    });
    return Object.keys(map).sort().slice(-12).map(function (day) {
      return { label: day.substring(8) + '/' + day.substring(5, 7), value: map[day] };
    });
  }

  function stat(icon, value, label) {
    return '<div class="stat-card"><div class="stat-icon">' + icon + '</div><div>' +
      '<div class="stat-value">' + UI.esc(value) + '</div>' +
      '<div class="stat-label">' + UI.esc(label) + '</div></div></div>';
  }

  function bind(container, entries) {
    container.querySelector('#a-class').onchange = function () { state.classId = this.value; App.reload(); };
    container.querySelector('#a-status').onchange = function () { state.status = this.value; App.reload(); };
    container.querySelector('#a-from').onchange = function () { state.from = this.value; App.reload(); };
    container.querySelector('#a-to').onchange = function () { state.to = this.value; App.reload(); };
    container.querySelector('#a-reset').onclick = function () {
      state.classId = ''; state.status = ''; state.from = ''; state.to = '';
      App.reload();
    };

    function find(id) { return entries.filter(function (e) { return String(e.id) === String(id); })[0]; }
    container.querySelectorAll('[data-edit]').forEach(function (btn) {
      btn.onclick = function () { openEdit(find(btn.dataset.edit)); };
    });
    container.querySelectorAll('[data-del]').forEach(function (btn) {
      btn.onclick = function () {
        var e = find(btn.dataset.del);
        UI.confirm('Supprimer le relevé',
          'Supprimer le relevé du ' + UI.date(e.date) + ' pour ' + (e.student_name || '') + ' ?',
          function () {
            Api.del('/api/attendance/' + e.id).then(function () {
              UI.success('Relevé supprimé.');
              App.reload();
            }).catch(UI.showError);
          });
      };
    });
  }

  function openEdit(entry) {
    UI.modal({
      title: 'Modifier le relevé',
      body: UI.form([
        { name: 'date', label: 'Date', type: 'date', value: String(entry.date).substring(0, 10), required: true, col: 'half' },
        { name: 'time', label: 'Heure', type: 'time', value: entry.time, col: 'half' },
        { name: 'status', label: 'Statut', type: 'select', value: entry.status, options: STATUSES },
        { name: 'justification', label: 'Justification', type: 'textarea', value: entry.justification, rows: 2 },
        { name: 'comment', label: 'Commentaire', type: 'textarea', value: entry.comment, rows: 2 }
      ], 'att-form'),
      buttons: [
        { label: 'Annuler', className: 'btn-secondary' },
        {
          label: 'Enregistrer', className: 'btn-primary',
          onClick: function (button) {
            var form = document.getElementById('att-form');
            var data = UI.readForm(form);
            data.student_id = entry.student_id;
            data.subject_id = entry.subject_id;
            button.disabled = true;
            Api.put('/api/attendance/' + entry.id, data).then(function () {
              UI.closeModal();
              UI.success('Relevé mis à jour.');
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

  /* ----------------------------- Appel groupé ----------------------------- */
  function openRollCall() {
    App.classes().then(function (classes) {
      if (!classes.length) {
        UI.error('Créez d\'abord une classe.');
        return;
      }
      UI.modal({
        title: 'Faire l\'appel',
        body: '<div class="field-row">' +
          '<div class="field"><label>Classe *</label><select id="rc-class">' +
          classes.map(function (c) {
            return '<option value="' + c.id + '">' + UI.esc(c.name) + ' — ' + UI.esc(c.level) + '</option>';
          }).join('') + '</select></div>' +
          '<div class="field"><label>Date *</label><input type="date" id="rc-date" value="' +
          new Date().toISOString().substring(0, 10) + '"></div></div>' +
          '<div class="field"><label>Heure</label><input type="time" id="rc-time"></div>' +
          '<div id="rc-list">' + UI.spinner() + '</div>',
        buttons: [
          { label: 'Annuler', className: 'btn-secondary' },
          { label: 'Enregistrer l\'appel', className: 'btn-primary', onClick: submitRollCall }
        ],
        onOpen: function () {
          var select = document.getElementById('rc-class');
          select.onchange = function () { loadRoll(this.value); };
          loadRoll(select.value);
        }
      });
    });
  }

  function loadRoll(classId) {
    var host = document.getElementById('rc-list');
    host.innerHTML = UI.spinner();
    Api.get('/api/classes/' + classId + '/students?active_only=true').then(function (data) {
      var students = data.items || [];
      if (!students.length) {
        host.innerHTML = UI.empty('Cette classe ne contient aucun élève actif');
        return;
      }
      host.innerHTML = '<div class="table-wrap"><table><thead><tr><th>Élève</th>' +
        STATUSES.map(function (s) { return '<th class="text-center">' + UI.esc(s.label) + '</th>'; }).join('') +
        '</tr></thead><tbody>' +
        students.map(function (s) {
          return '<tr><td>' + UI.esc(s.full_name) + '</td>' +
            STATUSES.map(function (st) {
              return '<td class="text-center"><input type="radio" style="width:auto" name="rc-' + s.id +
                '" value="' + st.value + '"' + (st.value === 'PRESENT' ? ' checked' : '') + '></td>';
            }).join('') + '</tr>';
        }).join('') + '</tbody></table></div>';
    }).catch(function (err) {
      host.innerHTML = '<div class="alert alert-error">' + UI.esc(err.message) + '</div>';
    });
  }

  function submitRollCall(button) {
    var classId = Number(document.getElementById('rc-class').value);
    var date = document.getElementById('rc-date').value;
    var time = document.getElementById('rc-time').value;
    var entries = [];
    document.querySelectorAll('#rc-list input[type=radio]:checked').forEach(function (input) {
      entries.push({ student_id: Number(input.name.replace('rc-', '')), status: input.value });
    });
    if (!entries.length) { UI.error('Aucun élève à enregistrer.'); return; }

    button.disabled = true;
    Api.post('/api/attendance/bulk', {
      class_id: classId, date: date, time: time || null, entries: entries
    }).then(function (result) {
      UI.closeModal();
      UI.success(result.created + ' relevé(s) enregistré(s).');
      App.reload();
    }).catch(function (err) { button.disabled = false; UI.showError(err); });
  }
})(window);
