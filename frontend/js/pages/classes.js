/** Page « Classes » : liste, CRUD et consultation des effectifs. */
(function (global) {
  'use strict';
  global.Pages = global.Pages || {};

  global.Pages.classes = {
    render: function (container) {
      if (Api.can('ADMIN')) {
        App.action('+ Nouvelle classe', function () { openForm(null); });
      }
      return Promise.all([
        Api.get('/api/classes'),
        App.teachers(),
        App.schoolYears()
      ]).then(function (r) {
        var classes = r[0].items || [];
        container.innerHTML = view(classes);
        bind(container, classes);
      });
    }
  };

  function view(classes) {
    var columns = [
      { key: 'name', label: 'Classe', render: function (c) { return '<span class="name">' + UI.esc(c.name) + '</span>'; } },
      { key: 'level', label: 'Niveau' },
      { key: 'school_year_label', label: 'Année scolaire', render: function (c) { return UI.text(c.school_year_label); } },
      { key: 'main_teacher_name', label: 'Enseignant principal', render: function (c) { return UI.text(c.main_teacher_name); } },
      { key: 'room', label: 'Salle', render: function (c) { return UI.text(c.room); } },
      {
        key: 'student_count', label: 'Effectif', cls: 't-center',
        render: function (c) {
          var text = c.student_count + (c.capacity ? ' / ' + c.capacity : '');
          var full = c.capacity && c.student_count >= c.capacity;
          return UI.tag(text, full ? 'warn' : 'mark');
        }
      },
      { key: 'subject_count', label: 'Matières', cls: 't-center' },
      {
        key: 'actions', label: '', cls: 't-right',
        render: function (c) {
          var out = '<div class="row-actions">' +
            '<button class="btn btn--sm" data-view="' + c.id + '">Élèves</button>' +
            '<a class="btn btn--sm" href="#/results?class=' + c.id + '">Classement</a>';
          if (Api.can('ADMIN')) {
            out += '<button class="btn btn--sm" data-edit="' + c.id + '">' + UI.icon('edit') + '</button>' +
              '<button class="btn btn--danger btn--sm" data-del="' + c.id + '">' + UI.icon('trash') + '</button>';
          }
          return out + '</div>';
        }
      }
    ];

    var enrolled = classes.reduce(function (s, c) { return s + (c.student_count || 0); }, 0);
    var seats = classes.reduce(function (s, c) { return s + (c.capacity || 0); }, 0);

    return UI.readings([
      { value: classes.length, label: 'Classes ouvertes' },
      { value: enrolled, label: 'Élèves affectés',
        note: seats ? 'sur ' + seats + ' places déclarées' : null }
    ]) +
      '<div class="panel">' + UI.table(columns, classes, { emptyTitle: 'Aucune classe enregistrée' }) + '</div>';
  }

  function bind(container, classes) {
    function find(id) { return classes.filter(function (c) { return String(c.id) === String(id); })[0]; }

    container.querySelectorAll('[data-view]').forEach(function (btn) {
      btn.onclick = function () { showStudents(find(btn.dataset.view)); };
    });
    container.querySelectorAll('[data-edit]').forEach(function (btn) {
      btn.onclick = function () { openForm(find(btn.dataset.edit)); };
    });
    container.querySelectorAll('[data-del]').forEach(function (btn) {
      btn.onclick = function () {
        var c = find(btn.dataset.del);
        UI.confirm('Supprimer la classe',
          'Supprimer la classe « ' + c.name + ' » ? Les élèves ne seront pas supprimés mais perdront leur affectation.',
          function () {
            Api.del('/api/classes/' + c.id).then(function () {
              UI.success('Classe supprimée.');
              App.invalidate('classes');
              App.reload();
            }).catch(UI.showError);
          });
      };
    });
  }

  function showStudents(classRoom) {
    UI.modal({ title: 'Élèves de ' + classRoom.name, body: UI.loading(), buttons: [{ label: 'Fermer' }] });
    Api.get('/api/classes/' + classRoom.id + '/students').then(function (data) {
      document.getElementById('modal-body').innerHTML = UI.table([
        { key: 'matricule', label: 'Matricule', cls: 'mono' },
        {
          key: 'full_name', label: 'Nom',
          render: function (s) { return '<a href="#/students/' + s.id + '">' + UI.esc(s.full_name) + '</a>'; }
        },
        {
          key: 'status', label: 'Statut',
          render: function (s) { return UI.tag(s.status_label || s.status, UI.STATUS_TAG[s.status]); }
        }
      ], data.items, { emptyTitle: 'Cette classe ne contient aucun élève' });
    }).catch(function (err) {
      document.getElementById('modal-body').innerHTML =
        '<div class="notice notice--error">' + UI.esc(err.message) + '</div>';
    });
  }

  function openForm(classRoom) {
    Promise.all([App.teachers(), App.schoolYears()]).then(function (r) {
      var teachers = r[0], years = r[1], c = classRoom || {};
      var currentYear = years.filter(function (y) { return y.is_current; })[0];

      var fields = [
        { name: 'name', label: 'Nom de la classe', value: c.name, required: true, half: true, placeholder: '6ème A' },
        { name: 'level', label: 'Niveau', value: c.level, required: true, half: true, placeholder: '6ème' },
        {
          name: 'school_year_id', label: 'Année scolaire', type: 'select', half: true,
          value: c.school_year_id || (currentYear ? currentYear.id : ''),
          options: years.map(function (y) { return { value: y.id, label: y.label }; })
        },
        {
          name: 'main_teacher_id', label: 'Enseignant principal', type: 'select', half: true,
          value: c.main_teacher_id,
          options: [{ value: '', label: '— Aucun —' }].concat(teachers.map(function (t) {
            return { value: t.id, label: t.full_name };
          }))
        },
        { name: 'room', label: 'Salle', value: c.room, half: true },
        { name: 'capacity', label: 'Capacité', type: 'number', min: 0, value: c.capacity, half: true }
      ];

      UI.modal({
        title: classRoom ? 'Modifier la classe' : 'Nouvelle classe',
        body: UI.form(fields, 'class-form'),
        buttons: [
          { label: 'Annuler' },
          {
            label: 'Enregistrer', variant: 'primary',
            onClick: function (button) {
              var form = document.getElementById('class-form');
              var data = UI.readForm(form);
              if (data.school_year_id) data.school_year_id = Number(data.school_year_id);
              if (data.main_teacher_id) data.main_teacher_id = Number(data.main_teacher_id);
              if (data.capacity) data.capacity = Number(data.capacity);
              button.disabled = true;

              var request = classRoom
                ? Api.put('/api/classes/' + classRoom.id, data)
                : Api.post('/api/classes', data);

              request.then(function () {
                UI.closeModal();
                UI.success(classRoom ? 'Classe mise à jour.' : 'Classe créée.');
                App.invalidate('classes');
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
})(window);
