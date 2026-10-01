/** Page « Rapports & bulletins » : exports, imports et génération de PDF. */
(function (global) {
  'use strict';
  global.Pages = global.Pages || {};

  global.Pages.reports = {
    render: function (container) {
      return Promise.all([App.classes(), Api.get('/api/reports')]).then(function (r) {
        container.innerHTML = view(r[0], r[1]);
        bind(container);
      });
    }
  };

  function view(classes, catalog) {
    var classOptions = classes.map(function (c) {
      return '<option value="' + c.id + '">' + UI.esc(c.name) + ' — ' + UI.esc(c.level) + '</option>';
    }).join('');
    var termOptions = '<option value="">Année complète</option>' +
      [1, 2, 3].map(function (t) { return '<option value="' + t + '">Trimestre ' + t + '</option>'; }).join('');

    var html = '<div class="grid-2">';

    /* --- Bulletin individuel --- */
    html += block('📄 Bulletin d\'un élève',
      'Génère le bulletin PDF d\'un élève : moyennes par matière, moyenne générale pondérée, ' +
      'rang dans la classe, appréciation et bilan des absences.',
      '<div class="field"><label>Rechercher un élève</label>' +
      '<input type="search" id="rep-student-q" placeholder="Nom, prénom ou matricule…"></div>' +
      '<div class="field"><label>Élève</label><select id="rep-student"><option value="">—</option></select></div>' +
      '<div class="field"><label>Période</label><select id="rep-student-term">' + termOptions + '</select></div>' +
      '<button class="btn btn-primary btn-block" id="rep-student-go">Télécharger le bulletin</button>');

    /* --- Rapport de classe --- */
    html += block('🏫 Rapport de classe',
      'Rapport PDF complet d\'une classe : effectif, moyenne générale, classement des élèves ' +
      'et statistiques par matière.',
      '<div class="field"><label>Classe</label><select id="rep-class">' + classOptions + '</select></div>' +
      '<div class="field"><label>Période</label><select id="rep-class-term">' + termOptions + '</select></div>' +
      '<button class="btn btn-primary btn-block" id="rep-class-go">Télécharger le rapport</button>');

    /* --- Exports --- */
    html += block('📤 Exporter les données',
      'Exporte les données au format CSV (tableur) ou JSON (sauvegarde / migration).',
      '<div class="field"><label>Classe (facultatif)</label>' +
      '<select id="rep-export-class"><option value="">Toutes les classes</option>' + classOptions + '</select></div>' +
      '<div style="display:flex;gap:10px;flex-wrap:wrap">' +
      '<button class="btn btn-secondary" data-export="students-csv">Élèves (CSV)</button>' +
      '<button class="btn btn-secondary" data-export="students-json">Élèves (JSON)</button>' +
      '<button class="btn btn-secondary" data-export="grades-csv">Notes (CSV)</button>' +
      '</div>');

    /* --- Imports --- */
    if (Api.can('ADMIN')) {
      html += block('📥 Importer des élèves',
        'Importe un fichier CSV ou JSON. Chaque ligne invalide est signalée avec son numéro, ' +
        'les lignes valides sont enregistrées.',
        '<div class="field"><label>Classe de destination (facultatif)</label>' +
        '<select id="rep-import-class"><option value="">— Aucune —</option>' + classOptions + '</select></div>' +
        '<div class="field"><label>Fichier (.csv ou .json)</label>' +
        '<input type="file" id="rep-import-file" accept=".csv,.json"></div>' +
        '<button class="btn btn-primary btn-block" id="rep-import-go">Importer</button>' +
        '<div id="rep-import-result"></div>');
    }

    html += '</div>';

    /* --- Catalogue des rapports exposés par l'API --- */
    var available = (catalog && catalog.reports) || [];
    if (available.length) {
      html += '<div class="card"><div class="card-header"><h2>Rapports disponibles via l\'API</h2></div>' +
        UI.table([
          { key: 'name', label: 'Rapport', render: function (r) { return UI.text(r.name || r.id); } },
          { key: 'description', label: 'Description', render: function (r) { return UI.text(r.description); } },
          { key: 'url', label: 'Point d\'entrée', className: 'mono', render: function (r) { return UI.text(r.url); } }
        ], available, { emptyMessage: 'Aucun rapport' }) + '</div>';
    }

    return html;
  }

  function block(title, description, body) {
    return '<div class="card"><div class="card-header"><h2>' + UI.esc(title) + '</h2></div>' +
      '<div class="card-body"><p class="muted" style="margin-bottom:14px">' + UI.esc(description) + '</p>' +
      body + '</div></div>';
  }

  function bind(container) {
    /* Recherche d'élève */
    var search = container.querySelector('#rep-student-q');
    var select = container.querySelector('#rep-student');
    var refresh = function () {
      Api.get('/api/students' + Api.qs({ q: search.value, limit: 50, sort_by: 'last_name' }))
        .then(function (page) {
          select.innerHTML = '<option value="">—</option>' + (page.items || []).map(function (s) {
            return '<option value="' + s.id + '" data-mat="' + UI.esc(s.matricule || s.id) + '">' +
              UI.esc(s.last_name + ' ' + s.first_name) +
              (s.class_name ? ' — ' + UI.esc(s.class_name) : '') + '</option>';
          }).join('');
        }).catch(UI.showError);
    };
    search.oninput = UI.debounce(refresh, 300);
    refresh();

    container.querySelector('#rep-student-go').onclick = function () {
      var id = select.value;
      if (!id) { UI.error('Sélectionnez un élève.'); return; }
      var term = container.querySelector('#rep-student-term').value;
      var matricule = select.options[select.selectedIndex].dataset.mat || id;
      download(this, '/api/reports/students/' + id + '/pdf' + Api.qs({ term: term }),
        'bulletin-' + matricule + '.pdf');
    };

    container.querySelector('#rep-class-go').onclick = function () {
      var id = container.querySelector('#rep-class').value;
      if (!id) { UI.error('Sélectionnez une classe.'); return; }
      var term = container.querySelector('#rep-class-term').value;
      download(this, '/api/reports/classes/' + id + '/pdf' + Api.qs({ term: term }),
        'rapport-classe-' + id + '.pdf');
    };

    container.querySelectorAll('[data-export]').forEach(function (btn) {
      btn.onclick = function () {
        var classId = container.querySelector('#rep-export-class').value;
        var map = {
          'students-csv': ['/api/export/students.csv' + Api.qs({ class_id: classId }), 'eleves.csv'],
          'students-json': ['/api/export/students.json' + Api.qs({ class_id: classId }), 'eleves.json'],
          'grades-csv': ['/api/export/grades.csv' + Api.qs({ class_id: classId }), 'notes.csv']
        };
        var target = map[btn.dataset.export];
        download(btn, target[0], target[1]);
      };
    });

    var importBtn = container.querySelector('#rep-import-go');
    if (importBtn) importBtn.onclick = function () { runImport(container, importBtn); };
  }

  function download(button, url, filename) {
    button.disabled = true;
    Api.download(url, filename)
      .then(function () { UI.success('Fichier téléchargé : ' + filename); })
      .catch(UI.showError)
      .then(function () { button.disabled = false; });
  }

  function runImport(container, button) {
    var input = container.querySelector('#rep-import-file');
    if (!input.files.length) { UI.error('Choisissez un fichier.'); return; }
    var file = input.files[0];
    var isJson = /\.json$/i.test(file.name);
    var classId = container.querySelector('#rep-import-class').value;

    button.disabled = true;
    var reader = new FileReader();
    reader.onload = function () {
      var request;
      if (isJson) {
        var parsed;
        try { parsed = JSON.parse(String(reader.result)); }
        catch (e) {
          button.disabled = false;
          UI.error('Fichier JSON invalide : ' + e.message);
          return;
        }
        var payload = Array.isArray(parsed) ? { items: parsed } : parsed;
        if (classId) payload.class_id = Number(classId);
        request = Api.post('/api/import/students/json', payload);
      } else {
        request = Api.post('/api/import/students/csv' + Api.qs({ class_id: classId }),
          String(reader.result), { contentType: 'text/csv; charset=utf-8' });
      }

      request.then(function (report) {
        container.querySelector('#rep-import-result').innerHTML = reportTable(report);
        UI.success(report.inserted + ' élève(s) importé(s).');
        App.invalidate('classes');
      }).catch(UI.showError).then(function () { button.disabled = false; });
    };
    reader.readAsText(file);
  }

  function reportTable(report) {
    var html = '<div class="alert ' + (report.error_count ? 'alert-warning' : 'alert-success') + '">' +
      '<strong>' + report.inserted + '</strong> importé(s), <strong>' + report.skipped +
      '</strong> ignoré(s), <strong>' + report.error_count + '</strong> erreur(s).</div>';
    if (report.errors && report.errors.length) {
      html += UI.table([
        { key: 'line', label: 'Ligne', className: 'mono' },
        { key: 'message', label: 'Erreur' }
      ], report.errors, {});
    }
    return html;
  }
})(window);
