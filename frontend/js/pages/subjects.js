/** Page « Matières » : CRUD des matières et de leurs coefficients. */
(function (global) {
  'use strict';
  global.Pages = global.Pages || {};

  var state = { classId: '' };

  global.Pages.subjects = {
    render: function (container) {
      if (Api.can('ADMIN')) {
        App.action('+ Nouvelle matière', function () { openForm(null); });
        App.action('Gérer les enseignants', openTeachers);
      }
      return Promise.all([
        App.classes(),
        Api.get('/api/subjects' + Api.qs({ class_id: state.classId }))
      ]).then(function (r) {
        var classes = r[0], subjects = r[1].items || [];
        container.innerHTML = view(classes, subjects);
        bind(container, subjects);
      });
    }
  };

  function view(classes, subjects) {
    var html = '<div class="filters"><select id="s-class" style="min-width:220px">' +
      '<option value="">Toutes les classes</option>' +
      classes.map(function (c) {
        return '<option value="' + c.id + '"' + (String(c.id) === String(state.classId) ? 'selected' : '') +
          '>' + UI.esc(c.name) + '</option>';
      }).join('') + '</select></div>';

    var columns = [
      { key: 'code', label: 'Code', cls: 'mono' },
      { key: 'name', label: 'Matière', render: function (s) { return '<span class="name">' + UI.esc(s.name) + '</span>'; } },
      { key: 'class_name', label: 'Classe', render: function (s) { return UI.text(s.class_name); } },
      {
        key: 'coefficient', label: 'Coefficient', cls: 't-center',
        render: function (s) { return UI.tag('× ' + UI.num(s.coefficient, 1), 'mark'); }
      },
      { key: 'teacher_name', label: 'Enseignant', render: function (s) { return UI.text(s.teacher_name); } }
    ];
    if (Api.can('ADMIN')) {
      columns.push({
        key: 'actions', label: '', cls: 't-right',
        render: function (s) {
          return '<div class="row-actions">' +
            '<button class="btn btn--sm" data-edit="' + s.id + '">' + UI.icon('edit') + '</button>' +
            '<button class="btn btn--danger btn--sm" data-del="' + s.id + '">' + UI.icon('trash') + '</button></div>';
        }
      });
    }

    return html + '<div class="panel">' +
      UI.table(columns, subjects, { emptyTitle: 'Aucune matière enregistrée' }) + '</div>';
  }

  function bind(container, subjects) {
    container.querySelector('#s-class').onchange = function () {
      state.classId = this.value;
      App.reload();
    };
    function find(id) { return subjects.filter(function (s) { return String(s.id) === String(id); })[0]; }

    container.querySelectorAll('[data-edit]').forEach(function (btn) {
      btn.onclick = function () { openForm(find(btn.dataset.edit)); };
    });
    container.querySelectorAll('[data-del]').forEach(function (btn) {
      btn.onclick = function () {
        var s = find(btn.dataset.del);
        UI.confirm('Supprimer la matière',
          'Supprimer « ' + s.name + ' » ? Les notes associées seront également supprimées.',
          function () {
            Api.del('/api/subjects/' + s.id).then(function () {
              UI.success('Matière supprimée.');
              App.invalidate('subjects');
              App.reload();
            }).catch(UI.showError);
          });
      };
    });
  }

  function openForm(subject) {
    Promise.all([App.classes(), App.teachers()]).then(function (r) {
      var classes = r[0], teachers = r[1], s = subject || {};
      var fields = [
        { name: 'name', label: 'Nom', value: s.name, required: true, half: true, placeholder: 'Mathématiques' },
        { name: 'code', label: 'Code', value: s.code, required: true, half: true, placeholder: 'MATH' },
        {
          name: 'class_id', label: 'Classe', type: 'select', value: s.class_id || state.classId, half: true,
          options: classes.map(function (c) { return { value: c.id, label: c.name + ' — ' + c.level }; })
        },
        {
          name: 'coefficient', label: 'Coefficient', type: 'number', step: '0.5', min: 0.5,
          value: s.coefficient === undefined ? 1 : s.coefficient, half: true
        },
        {
          name: 'teacher_id', label: 'Enseignant', type: 'select', value: s.teacher_id,
          options: [{ value: '', label: '— Aucun —' }].concat(teachers.map(function (t) {
            return { value: t.id, label: t.full_name };
          }))
        }
      ];

      UI.modal({
        title: subject ? 'Modifier la matière' : 'Nouvelle matière',
        body: UI.form(fields, 'subject-form'),
        buttons: [
          { label: 'Annuler' },
          {
            label: 'Enregistrer', variant: 'primary',
            onClick: function (button) {
              var form = document.getElementById('subject-form');
              var data = UI.readForm(form);
              data.class_id = data.class_id ? Number(data.class_id) : null;
              data.coefficient = Number(data.coefficient || 1);
              if (data.teacher_id) data.teacher_id = Number(data.teacher_id);
              button.disabled = true;

              var request = subject
                ? Api.put('/api/subjects/' + subject.id, data)
                : Api.post('/api/subjects', data);

              request.then(function () {
                UI.closeModal();
                UI.success(subject ? 'Matière mise à jour.' : 'Matière créée.');
                App.invalidate('subjects');
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

  /* ----------------------------- Enseignants ----------------------------- */
  function openTeachers() {
    Api.get('/api/teachers').then(function (data) {
      var teachers = data.items || [];
      UI.modal({
        title: 'Enseignants',
        body: UI.table([
          { key: 'full_name', label: 'Nom' },
          { key: 'speciality', label: 'Spécialité', render: function (t) { return UI.text(t.speciality); } },
          { key: 'email', label: 'E-mail', render: function (t) { return UI.text(t.email); } },
          { key: 'phone', label: 'Téléphone', render: function (t) { return UI.text(t.phone); } }
        ], teachers, { emptyTitle: 'Aucun enseignant enregistré' }),
        buttons: [
          { label: 'Fermer' },
          { label: '+ Ajouter', variant: 'primary', onClick: openTeacherForm }
        ]
      });
    }).catch(UI.showError);
  }

  function openTeacherForm() {
    UI.modal({
      title: 'Nouvel enseignant',
      body: UI.form([
        { name: 'last_name', label: 'Nom', required: true, half: true },
        { name: 'first_name', label: 'Prénom', required: true, half: true },
        { name: 'speciality', label: 'Spécialité', half: true },
        { name: 'phone', label: 'Téléphone', half: true },
        { name: 'email', label: 'E-mail', type: 'email' }
      ], 'teacher-form'),
      buttons: [
        { label: 'Annuler' },
        {
          label: 'Enregistrer', variant: 'primary',
          onClick: function (button) {
            var form = document.getElementById('teacher-form');
            button.disabled = true;
            Api.post('/api/teachers', UI.readForm(form)).then(function () {
              UI.closeModal();
              UI.success('Enseignant ajouté.');
              App.invalidate('teachers');
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
