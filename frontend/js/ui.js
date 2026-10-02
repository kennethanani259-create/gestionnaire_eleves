/**
 * ui.js — vocabulaire d'interface partagé.
 *
 * Un seul endroit décide de l'apparence d'un tableau, d'un formulaire, d'une
 * alerte ou d'une moyenne. Les pages décrivent *ce qu'elles montrent*, jamais
 * *comment c'est dessiné* : c'est ce qui garantit la cohérence du système.
 */
(function (global) {
  'use strict';

  var UI = {
    /* ------------------------------------------------------- Échappement */
    esc: function (value) {
      if (value === null || value === undefined) return '';
      return String(value)
        .replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;')
        .replace(/"/g, '&quot;').replace(/'/g, '&#39;');
    },

    /** Icône du jeu maison (voir le sprite SVG dans index.html). */
    icon: function (name, extraClass) {
      return '<svg class="icon ' + (extraClass || '') + '" aria-hidden="true">' +
        '<use href="#i-' + name + '"/></svg>';
    },

    /* --------------------------------------------------------- Formatage */
    MONTHS: ['janvier', 'février', 'mars', 'avril', 'mai', 'juin', 'juillet',
             'août', 'septembre', 'octobre', 'novembre', 'décembre'],

    date: function (iso) {
      if (!iso) return '<span class="sub">—</span>';
      var p = String(iso).substring(0, 10).split('-');
      return p.length === 3 ? p[2] + '.' + p[1] + '.' + p[0] : UI.esc(iso);
    },
    dateLong: function (iso) {
      if (!iso) return '—';
      var p = String(iso).substring(0, 10).split('-');
      if (p.length !== 3) return UI.esc(iso);
      return Number(p[2]) + ' ' + UI.MONTHS[Number(p[1]) - 1] + ' ' + p[0];
    },
    dateTime: function (iso) {
      if (!iso) return '<span class="sub">—</span>';
      return UI.date(iso) + ' <span class="sub">' + String(iso).substring(11, 16) + '</span>';
    },
    /** Nombre en notation française, chiffres tabulaires. */
    num: function (value, digits) {
      if (value === null || value === undefined || value === '' || isNaN(value)) return '—';
      return Number(value).toFixed(digits === undefined ? 2 : digits).replace('.', ',');
    },
    text: function (value) {
      return (value === null || value === undefined || value === '')
        ? '<span class="sub">—</span>' : UI.esc(value);
    },

    /**
     * Moyenne sur 20. La couleur n'est jamais le seul signal : un chevron
     * typographique double l'information (daltonisme, impression N&B).
     */
    score: function (value) {
      if (value === null || value === undefined || isNaN(value)) return '<span class="sub">—</span>';
      var v = Number(value);
      var level = v >= 12 ? 'good' : (v >= 10 ? 'mid' : 'low');
      return '<span class="score score--' + level + '">' + UI.num(v) + '</span>';
    },
    /** Jauge horizontale 0–20, en complément d'un chiffre déjà affiché. */
    gauge: function (value, max) {
      var v = Number(value) || 0, m = max || 20;
      var pct = Math.max(0, Math.min(100, (v / m) * 100));
      var level = v >= 12 ? '' : (v >= 10 ? ' gauge--mid' : ' gauge--low');
      return '<div class="gauge' + level + '" role="img" aria-label="' + UI.num(v) +
        ' sur ' + m + '"><span style="width:' + pct.toFixed(1) + '%"></span></div>';
    },

    STATUS_TAG: { ACTIVE: 'good', INACTIVE: 'neutral', TRANSFERRED: 'warn', EXPELLED: 'bad' },
    ATTENDANCE_TAG: { PRESENT: 'good', ABSENT: 'bad', EXCUSED: 'warn', LATE: 'mark' },
    ROLE_TAG: { ADMIN: 'bad', TEACHER: 'mark', VIEWER: 'neutral' },

    tag: function (label, kind) {
      return '<span class="tag tag--' + (kind || 'neutral') + '">' + UI.esc(label) + '</span>';
    },

    /* ------------------------------------------------------------ Toasts */
    toast: function (message, kind) {
      var host = document.getElementById('toasts');
      var el = document.createElement('div');
      el.className = 'toast' + (kind ? ' toast--' + kind : '');
      el.textContent = message;
      host.appendChild(el);
      setTimeout(function () { el.remove(); }, 4500);
    },
    success: function (m) { UI.toast(m, 'ok'); },
    error: function (m) { UI.toast(m, 'err'); },

    /** Rend lisible une erreur d'API, détails de validation compris. */
    showError: function (err) {
      var message = (err && err.message) ? err.message : 'Erreur inattendue';
      if (err && err.fields) {
        var keys = Object.keys(err.fields);
        if (keys.length) {
          message += ' — ' + keys.map(function (k) { return err.fields[k]; }).join(' ; ');
        }
      }
      UI.error(message);
    },

    /* ---------------------------------------------------------- Dialogue */
    _lastFocus: null,

    modal: function (options) {
      var backdrop = document.getElementById('modal-backdrop');
      UI._lastFocus = document.activeElement;

      document.getElementById('modal-title').textContent = options.title || '';
      var body = document.getElementById('modal-body');
      body.innerHTML = options.body || '';
      var foot = document.getElementById('modal-footer');
      foot.innerHTML = '';

      (options.buttons || []).forEach(function (spec) {
        var btn = document.createElement('button');
        btn.className = 'btn' + (spec.variant ? ' btn--' + spec.variant : '');
        btn.textContent = spec.label;
        btn.onclick = function () { spec.onClick ? spec.onClick(btn) : UI.closeModal(); };
        foot.appendChild(btn);
      });

      backdrop.hidden = false;
      if (options.onOpen) options.onOpen(body);
      var first = body.querySelector('input, select, textarea') ||
                  foot.querySelector('.btn--primary, .btn--danger');
      if (first) first.focus();
    },

    closeModal: function () {
      document.getElementById('modal-backdrop').hidden = true;
      document.getElementById('modal-body').innerHTML = '';
      if (UI._lastFocus && UI._lastFocus.focus) UI._lastFocus.focus();
      UI._lastFocus = null;
    },

    /**
     * Confirmation d'une action destructrice. Le texte doit dire ce qui sera
     * perdu, pas seulement poser une question.
     */
    confirm: function (title, message, onConfirm, confirmLabel) {
      UI.modal({
        title: title,
        body: '<p>' + UI.esc(message) + '</p>',
        buttons: [
          { label: 'Annuler' },
          {
            label: confirmLabel || 'Supprimer', variant: 'danger',
            onClick: function () { UI.closeModal(); onConfirm(); }
          }
        ]
      });
    },

    /* ------------------------------------------------------ Formulaires */
    /** fields: [{name,label,type,value,options,required,placeholder,help,half,step,min,max,rows}] */
    form: function (fields, id) {
      var html = '<form id="' + (id || 'form') + '" novalidate>';
      var pair = [];
      fields.forEach(function (f) {
        if (f.half) {
          pair.push(f);
          if (pair.length === 2) { html += '<div class="field-pair">' + pair.map(field).join('') + '</div>'; pair = []; }
        } else {
          if (pair.length) { html += '<div class="field-pair">' + pair.map(field).join('') + '</div>'; pair = []; }
          html += field(f);
        }
      });
      if (pair.length) html += '<div class="field-pair">' + pair.map(field).join('') + '</div>';
      return html + '</form>';

      function field(f) {
        var id = 'f-' + f.name;
        var value = (f.value === null || f.value === undefined) ? '' : f.value;
        var req = f.required ? ' required' : '';
        var control;

        if (f.type === 'select') {
          control = '<select id="' + id + '" name="' + f.name + '"' + req + '>' +
            (f.options || []).map(function (o) {
              return '<option value="' + UI.esc(o.value) + '"' +
                (String(o.value) === String(value) ? ' selected' : '') + '>' +
                UI.esc(o.label) + '</option>';
            }).join('') + '</select>';
        } else if (f.type === 'textarea') {
          control = '<textarea id="' + id + '" name="' + f.name + '" rows="' + (f.rows || 3) +
            '" placeholder="' + UI.esc(f.placeholder || '') + '"' + req + '>' +
            UI.esc(value) + '</textarea>';
        } else {
          control = '<input id="' + id + '" type="' + (f.type || 'text') + '" name="' + f.name +
            '" value="' + UI.esc(value) + '" placeholder="' + UI.esc(f.placeholder || '') + '"' + req +
            (f.step !== undefined ? ' step="' + f.step + '"' : '') +
            (f.min !== undefined ? ' min="' + f.min + '"' : '') +
            (f.max !== undefined ? ' max="' + f.max + '"' : '') + '>';
        }

        return '<div class="field" data-field="' + f.name + '">' +
          '<label for="' + id + '">' + UI.esc(f.label) + (f.required ? ' <abbr title="obligatoire">*</abbr>' : '') + '</label>' +
          control +
          (f.help ? '<small class="field__help">' + UI.esc(f.help) + '</small>' : '') +
          '</div>';
      }
    },

    readForm: function (formEl) {
      var data = {};
      Array.prototype.forEach.call(formEl.elements, function (el) {
        if (!el.name) return;
        var v = el.value;
        data[el.name] = (typeof v === 'string' && v.trim() === '') ? null : v;
      });
      return data;
    },

    /** Reporte les erreurs renvoyées par le serveur sur les champs concernés. */
    markFieldErrors: function (formEl, fields) {
      formEl.querySelectorAll('.field__error').forEach(function (n) { n.remove(); });
      formEl.querySelectorAll('[aria-invalid]').forEach(function (n) { n.removeAttribute('aria-invalid'); });

      var first = null;
      Object.keys(fields || {}).forEach(function (name) {
        var wrap = formEl.querySelector('[data-field="' + name + '"]');
        if (!wrap) return;
        var input = wrap.querySelector('input, select, textarea');
        if (input) { input.setAttribute('aria-invalid', 'true'); if (!first) first = input; }
        var msg = document.createElement('span');
        msg.className = 'field__error';
        msg.textContent = fields[name];
        wrap.appendChild(msg);
      });
      if (first) first.focus();
    },

    /* --------------------------------------------------------- Tableaux */
    /** columns: [{key,label,render,cls,sortable}] */
    table: function (columns, rows, options) {
      options = options || {};
      if (!rows || !rows.length) {
        return UI.blank(options.emptyTitle || 'Rien à afficher', options.emptyHint);
      }

      var head = columns.map(function (c) {
        var cls = [c.sortable ? 'is-sortable' : '', c.cls || ''].join(' ').trim();
        var mark = '';
        if (c.sortable && options.sortBy === c.key) {
          mark = ' <span class="sort" aria-hidden="true">' + (options.sortDir === 'desc' ? '↓' : '↑') + '</span>';
        }
        var aria = c.sortable
          ? ' aria-sort="' + (options.sortBy === c.key ? (options.sortDir === 'desc' ? 'descending' : 'ascending') : 'none') + '"'
          : '';
        return '<th class="' + cls + '"' + aria + (c.sortable ? ' data-sort="' + c.key + '" tabindex="0" role="button"' : '') +
          '>' + UI.esc(c.label) + mark + '</th>';
      }).join('');

      var body = rows.map(function (row, i) {
        return '<tr>' + columns.map(function (c) {
          return '<td class="' + (c.cls || '') + '">' +
            (c.render ? c.render(row, i) : UI.text(row[c.key])) + '</td>';
        }).join('') + '</tr>';
      }).join('');

      return '<div class="table-scroll"><table>' +
        (options.caption ? '<caption>' + UI.esc(options.caption) + '</caption>' : '') +
        '<thead><tr>' + head + '</tr></thead><tbody>' + body + '</tbody></table></div>';
    },

    pager: function (total, limit, offset) {
      if (!total) return '';
      var from = offset + 1, to = Math.min(offset + limit, total);
      return '<div class="pager"><span>' + from + '–' + to + ' sur ' + total + '</span>' +
        '<div class="pager__btns">' +
        '<button class="btn btn--sm" data-page="prev"' + (offset <= 0 ? ' disabled' : '') + '>' +
        UI.icon('prev') + 'Précédent</button>' +
        '<button class="btn btn--sm" data-page="next"' + (offset + limit >= total ? ' disabled' : '') + '>' +
        'Suivant' + UI.icon('next') + '</button></div></div>';
    },

    /* ------------------------------------------------------------- États */
    loading: function (label) {
      return '<p class="loading">' + UI.esc(label || 'Chargement…') + '</p>';
    },
    /** État vide : dire pourquoi c'est vide et quoi faire ensuite. */
    blank: function (title, hint, iconName) {
      return '<div class="blank">' + UI.icon(iconName || 'empty') +
        '<p class="blank__title">' + UI.esc(title) + '</p>' +
        (hint ? '<p class="blank__hint">' + UI.esc(hint) + '</p>' : '') + '</div>';
    },

    /* --------------------------------------------------------- Divers */
    debounce: function (fn, delay) {
      var timer = null;
      return function () {
        var args = arguments, self = this;
        clearTimeout(timer);
        timer = setTimeout(function () { fn.apply(self, args); }, delay || 280);
      };
    },

    /** <option> d'une liste d'entités, avec sélection courante. */
    opts: function (items, valueKey, labelFn, selected, placeholder) {
      var html = placeholder === false ? ''
        : '<option value="">' + UI.esc(placeholder || 'Toutes') + '</option>';
      return html + (items || []).map(function (it) {
        return '<option value="' + UI.esc(it[valueKey]) + '"' +
          (String(it[valueKey]) === String(selected) ? ' selected' : '') + '>' +
          UI.esc(labelFn(it)) + '</option>';
      }).join('');
    },

    /** Champ de recherche avec icône. */
    searchField: function (id, placeholder, value) {
      return '<div class="search-field">' + UI.icon('search') +
        '<input id="' + id + '" type="search" placeholder="' + UI.esc(placeholder) +
        '" value="' + UI.esc(value || '') + '" aria-label="' + UI.esc(placeholder) + '"></div>';
    },

    /** Bandeau de repères chiffrés (remplace les cartes KPI). */
    readings: function (items) {
      return '<div class="readings">' + items.map(function (it) {
        return '<div class="reading' + (it.tone ? ' reading--' + it.tone : '') + '">' +
          '<p class="reading__v">' + it.value +
          (it.unit ? '<span class="unit">' + UI.esc(it.unit) + '</span>' : '') + '</p>' +
          '<p class="reading__k">' + UI.esc(it.label) + '</p>' +
          (it.note ? '<p class="reading__note">' + it.note + '</p>' : '') +
          '</div>';
      }).join('') + '</div>';
    },

    panel: function (title, bodyHtml, options) {
      options = options || {};
      return '<section class="panel' + (options.cls ? ' ' + options.cls : '') + '">' +
        (title ? '<header class="panel__head"><h2>' + UI.esc(title) + '</h2>' +
          (options.hint ? '<p class="hint">' + UI.esc(options.hint) + '</p>' : '') +
          (options.aside || '') + '</header>' : '') +
        (options.raw ? bodyHtml : '<div class="panel__body">' + bodyHtml + '</div>') +
        '</section>';
    }
  };

  global.UI = UI;
})(window);
