/**
 * Page « Aujourd'hui ».
 *
 * Choix produit : un directeur ou un enseignant n'ouvre pas cet écran pour
 * contempler des compteurs, mais pour savoir *qui* demande son attention.
 * L'écran répond donc dans cet ordre :
 *   1. une phrase qui résume la situation (chapeau éditorial) ;
 *   2. quatre repères chiffrés, sans vignettes décoratives ;
 *   3. la liste nominative des élèves à suivre — c'est la zone la plus utile,
 *      elle occupe la colonne principale et mène directement à la fiche ;
 *   4. les distributions, en appui, pour comprendre d'où viennent ces chiffres.
 *
 * Les élèves en difficulté ne sont pas déduits côté navigateur : on interroge
 * le classement de chaque classe, calculé par le serveur.
 */
(function (global) {
  'use strict';
  global.Pages = global.Pages || {};

  var state = { classId: '' };

  global.Pages.dashboard = {
    render: function (container) {
      App.action('Actualiser', function () { App.reload(); }, { icon: 'refresh' });

      return Promise.all([
        App.classes(),
        Api.get('/api/dashboard' + Api.qs({ class_id: state.classId }))
      ]).then(function (r) {
        var classes = r[0], data = r[1];
        container.innerHTML = view(classes, data);
        bind(container);
        // Le suivi nominatif demande un appel par classe : on l'affiche dès
        // qu'il arrive, sans retarder le reste de la page.
        loadWatchlist(container, classes);
      });
    }
  };

  /* ------------------------------------------------------------- Rendu --- */
  function view(classes, d) {
    var scope = state.classId
      ? (classes.filter(function (c) { return String(c.id) === String(state.classId); })[0] || {}).name
      : null;

    var html = '';

    html += '<div class="filters">' +
      '<label for="dash-scope">Portée</label>' +
      '<select id="dash-scope">' +
      '<option value="">Tout l\'établissement</option>' +
      UI.opts(classes, 'id', function (c) { return c.name + ' — ' + c.level; }, state.classId, false) +
      '</select>' +
      '<span class="muted">Période : année scolaire en cours</span>' +
      '</div>';

    html += lede(d, scope);

    html += UI.readings([
      {
        value: d.student_count,
        label: scope ? 'Élèves dans ' + scope : 'Élèves inscrits',
        note: d.active_count === d.student_count
          ? 'tous actifs'
          : (d.student_count - d.active_count) + 'non actif(s)'
      },
      {
        value: UI.num(d.general_average), unit: '/20',
        label: 'Moyenne générale',
        tone: d.general_average >= 10 ? 'good' : 'alert',
        note: d.graded_student_count + ' élève(s) noté(s)'
      },
      {
        value: UI.num(d.attendance_rate, 1), unit: '%',
        label: 'Taux de présence',
        tone: d.attendance_rate >= 90 ? 'good' : 'alert',
        note: d.absence_count + 'absence(s), ' + d.late_count + 'retard(s)'
      },
      {
        value: d.struggling_count,
        label: 'Sous la moyenne',
        tone: d.struggling_count ? 'alert' : 'good',
        note: d.struggling_count ? 'à suivre ci-dessous' : 'aucun élève en difficulté'
      }
    ]);

    /* --- Colonne principale : les personnes. Colonne d'appui : les courbes. */
    html += '<div class="columns columns--lead">';

    html += '<div>' +
      UI.panel('Élèves à suivre',
        '<div id="watchlist">' + UI.loading('Lecture des classements…') + '</div>',
        {
          raw: true,
          hint: 'Moyenne inférieure à 10/20'
        }) +
      distinction(d) +
      '</div>';

    html += '<div>' +
      UI.panel('Répartition des moyennes',
        '<div class="chart">' + Charts.bar((d.average_distribution || []).map(function (b) {
          return { label: b.range, value: b.count };
        }), { integer: true, color: 'ink' }) + '</div>',
        { hint: 'élèves par tranche' }) +
      UI.panel('Absences par mois',
        '<div class="chart">' + Charts.line((d.monthly_absences || []).map(function (m) {
          return { label: shortMonth(m.month), value: m.count };
        }), { integer: true, color: 'red' }) + '</div>') +
      UI.panel('Répartition par sexe',
        '<div class="chart">' + Charts.donut([
          { label: 'Garçons', value: d.male_count || 0, tone: 'board' },
          { label: 'Filles', value: d.female_count || 0, tone: 'ochre' }
        ], { centerLabel: 'élèves' }) + '</div>') +
      '</div>';

    html += '</div>';

    /* --- Matières : tableau plutôt que graphique seul, on compare des nombres. */
    var stats = d.subject_statistics || [];
    html += UI.panel('Niveau par matière',
      UI.table([
        { key: 'subject_name', label: 'Matière', render: function (s) {
            return '<span class="name">' + UI.text(s.subject_name || s.name) + '</span>'; } },
        { key: 'coefficient', label: 'Coef.', cls: 't-center',
          render: function (s) { return UI.num(s.coefficient, 1); } },
        { key: 'grade_count', label: 'Notes', cls: 't-right',
          render: function (s) { return UI.text(s.grade_count); } },
        { key: 'average', label: 'Moyenne', cls: 't-right',
          render: function (s) { return UI.score(s.average); } },
        { key: 'bar', label: 'Niveau', render: function (s) { return UI.gauge(s.average); } }
      ], stats, {
        emptyTitle: 'Aucune note enregistrée',
        emptyHint: 'Les moyennes par matière apparaîtront dès la première évaluation saisie.'
      }),
      { raw: true, hint: 'moyennes calculées par le serveur' });

    return html;
  }

  /** Chapeau : la situation en une phrase, pas en douze vignettes. */
  function lede(d, scope) {
    if (!d.student_count) {
      return '<p class="lede">Le registre est vide. Commencez par créer une classe, ' +
        'puis inscrivez vos premiers élèves.</p>';
    }

    var parts = [];
    parts.push('<b>' + d.student_count + '</b> élève' + (d.student_count > 1 ? 's' : '') +
      (scope ? 'dans <b>' + UI.esc(scope) + '</b>' : 'répartis dans <b>' + d.class_count + '</b> classe' +
        (d.class_count > 1 ? 's' : '')));

    if (d.graded_student_count) {
      parts.push('une moyenne générale de <b class="u">' + UI.num(d.general_average) + '/20</b>');
    }
    if (d.absence_count || d.late_count) {
      parts.push('<b>' + d.absence_count + '</b> absence' + (d.absence_count > 1 ? 's' : '') +
        'et <b>' + d.late_count + '</b> retard' + (d.late_count > 1 ? 's' : '') + 'relevés');
    }

    var tail = d.struggling_count
      ? ' <b>' + d.struggling_count + '</b> élève' + (d.struggling_count > 1 ? 's sont' : 'est') +
        'sous la barre des 10/20.'
      : 'Aucun élève n\'est actuellement sous la barre des 10/20.';

    return '<p class="lede">' + parts.join(', ') + '.' + tail + '</p>';
  }

  /** Mise en avant du meilleur résultat : une ligne, pas une carte-trophée. */
  function distinction(d) {
    var best = d.best_student;
    if (!best || !best.student_id) return '';
    return '<div class="panel"><div class="panel__body" ' +
      'style="display:flex;align-items:baseline;gap:var(--s4);flex-wrap:wrap">' +
      '<span class="tag tag--mark">Meilleure moyenne</span>' +
      '<a href="#/students/' + best.student_id + '" class="name">' + UI.esc(best.full_name) + '</a>' +
      '<span class="mono muted">' + UI.esc(best.matricule || '') + '</span>' +
      '<span style="margin-left:auto" class="score score--good">' + UI.num(best.average) + '</span>' +
      '</div></section>';
  }

  /* ------------------------------------------- Liste nominative à suivre --- */
  function loadWatchlist(container, classes) {
    var host = container.querySelector('#watchlist');
    if (!host) return;

    var targets = state.classId
      ? classes.filter(function (c) { return String(c.id) === String(state.classId); })
      : classes;

    if (!targets.length) {
      host.innerHTML = UI.blank('Aucune classe', 'Créez une classe pour suivre des élèves.', 'class');
      return;
    }

    // Garde-fou : au-delà de 12 classes, on évite une rafale de requêtes.
    var capped = targets.slice(0, 12);

    Promise.all(capped.map(function (c) {
      return Api.get('/api/classes/' + c.id + '/ranking')
        .then(function (data) {
          return (data.items || []).map(function (entry) {
            entry.class_name = c.name;
            return entry;
          });
        })
        .catch(function () { return []; });  // une classe illisible n'invalide pas la liste
    })).then(function (lists) {
      var flat = [];
      lists.forEach(function (l) { flat = flat.concat(l); });

      var watch = flat
        .filter(function (e) { return e.grade_count > 0 && Number(e.average) < 10; })
        .sort(function (a, b) { return a.average - b.average; });

      if (!watch.length) {
        host.innerHTML = UI.blank(
          'Aucun élève sous la moyenne',
          flat.length ? 'Tous les élèves notés atteignent 10/20.'
                      : 'Aucune note n\'a encore été saisie.',
          'check');
        return;
      }

      var shown = watch.slice(0, 10);
      host.innerHTML = '<div class="todo">' + shown.map(function (e) {
        return '<a class="todo__item" href="#/students/' + e.student_id + '">' +
          '<span class="todo__mark' + (e.average >= 8 ? 'todo__mark--warn' : '') + '"></span>' +
          '<span class="todo__body">' +
          '<span class="name">' + UI.esc(e.full_name) + '</span>' +
          '<span class="todo__why">' + UI.esc(e.class_name) + ' · ' + e.grade_count +
          'note' + (e.grade_count > 1 ? 's' : '') + ' · rang ' + e.rank + '</span></span>' +
          '<span class="score score--low">' + UI.num(e.average) + '</span>' +
          '</a>';
      }).join('') + '</div>' +
        (watch.length > shown.length
          ? '<p class="pager"><span>' + (watch.length - shown.length) +
            'autre(s) élève(s) concerné(s)</span>' +
            '<a class="btn btn--sm" href="#/results">Voir les classements</a></p>'
          : '') +
        (targets.length > capped.length
          ? '<p class="pager"><span class="muted">Limité aux ' + capped.length +
            'premières classes.</span></p>'
          : '');
    });
  }

  /* ----------------------------------------------------------- Divers --- */
  function shortMonth(value) {
    var p = String(value || '').split('-');
    if (p.length !== 2) return value;
    var names = ['jan', 'fév', 'mar', 'avr', 'mai', 'jun', 'jul', 'aoû', 'sep', 'oct', 'nov', 'déc'];
    return names[Number(p[1]) - 1] || p[1];
  }

  function bind(container) {
    container.querySelector('#dash-scope').onchange = function () {
      state.classId = this.value;
      App.reload();
    };
  }
})(window);
