/**
 * ui.js — briques d'interface réutilisables : toasts, modales, tableaux,
 * formulaires, formatage. Aucune dépendance externe.
 */
(function (global) {
  'use strict';

  var UI = {
    /* ----------------------------- Échappement ----------------------------- */
    esc: function (value) {
      if (value === null || value === undefined) return '';
      return String(value)
        .replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;')
        .replace(/"/g, '&quot;').replace(/'/g, '&#39;');
    },

    /* ----------------------------- Formatage ----------------------------- */
    date: function (iso) {
      if (!iso) return '—';
      var parts = String(iso).substring(0, 10).split('-');
      if (parts.length !== 3) return iso;
      return parts[2] + '/' + parts[1] + '/' + parts[0];
    },
    dateTime: function (iso) {
      if (!iso) return '—';
      return UI.date(iso) + ' ' + String(iso).substring(11, 16);
    },
    num: function (value, digits) {
      if (value === null || value === undefined || isNaN(value)) return '—';
      return Number(value).toFixed(digits === undefined ? 2 : digits).replace('.', ',');
    },
    avg: function (value) {
      if (value === null || value === undefined || isNaN(value)) return '<span class="muted">—</span>';
      var cls = value >= 12 ? 'avg-good' : (value >= 10 ? 'avg-mid' : 'avg-bad');
      return '<span class="avg-pill ' + cls + '">' + UI.num(value) + '</span>';
    },
    text: function (value) { return (value === null || value === undefined || value === '') ? '—' : UI.esc(value); },

    STATUS_BADGE: {
      ACTIVE: 'badge-success', INACTIVE: 'badge-neutral',
      TRANSFERRED: 'badge-warning', EXPELLED: 'badge-danger'
    },
    ATTENDANCE_BADGE: {
      PRESENT: 'badge-success', ABSENT: 'badge-danger',
      EXCUSED: 'badge-warning', LATE: 'badge-primary'
    },
    badge: function (label, cls) {
      return '<span class="badge ' + (cls || 'badge-neutral') + '">' + UI.esc(label) + '</span>';
    },

    /* ----------------------------- Toasts ----------------------------- */
    toast: function (message, kind) {
      var host = document.getElementById('toasts');
      var el = document.createElement('div');
      el.className = 'toast ' + (kind || '');
      el.textContent = message;
      host.appendChild(el);
      setTimeout(function () { el.remove(); }, 4200);
    },
    success: function (m) { UI.toast(m, 'success'); },
    error: function (m) { UI.toast(m, 'error'); },

    /** Affiche proprement une ApiError (message + champs invalides). */
    showError: function (err) {
      var message = err && err.message ? err.message : 'Erreur inattendue';
      if (err && err.fields) {
        var keys = Object.keys(err.fields);
        if (keys.length) {
          message += ' — ' + keys.map(function (k) { return k + ' : ' + err.fields[k]; }).join(' ; ');
        }
      }
      UI.error(message);
    },

    /* ----------------------------- Modale ----------------------------- */
    modal: function (options) {
      var backdrop = document.getElementById('modal-backdrop');
      document.getElementById('modal-title').textContent = options.title || '';
      var body = document.getElementById('modal-body');
      body.innerHTML = options.body || '';
      var footer = document.getElementById('modal-footer');
      footer.innerHTML = '';

      (options.buttons || []).forEach(function (btn) {
        var el = document.createElement('button');
        el.className = 'btn ' + (btn.className || 'btn-secondary');
        el.textContent = btn.label;
        el.onclick = function () { btn.onClick ? btn.onClick(el) : UI.closeModal(); };
        footer.appendChild(el);
      });

      backdrop.classList.remove('hidden');
      if (options.onOpen) options.onOpen(body);
      var first = body.querySelector('input, select, textarea');
      if (first) first.focus();
    },
    closeModal: function () {
      document.getElementById('modal-backdrop').classList.add('hidden');
      document.getElementById('modal-body').innerHTML = '';
    },

    /** Confirmation explicite avant une action destructrice. */
    confirm: function (title, message, onConfirm) {
      UI.modal({
        title: title,
        body: '<p>' + UI.esc(message) + '</p>',
        buttons: [
          { label: 'Annuler', className: 'btn-secondary' },
          {
            label: 'Confirmer', className: 'btn-danger',
            onClick: function () { UI.closeModal(); onConfirm(); }
          }
        ]
      });
    },

    /* ----------------------------- Formulaires ----------------------------- */
    /**
     * Construit un formulaire HTML à partir d'une description de champs.
     * fields: [{name, label, type, value, options, required, placeholder, help, col}]
     */
    form: function (fields, id) {
      var html = '<form id="' + (id || 'generic-form') + '" autocomplete="off">';
      var pending = [];
      fields.forEach(function (f) {
        if (f.col === 'half') { pending.push(f); if (pending.length === 2) { html += row(pending); pending = []; } }
        else { if (pending.length) { html += row(pending); pending = []; } html += field(f); }
      });
      if (pending.length) html += row(pending);
      return html + '</form>';

      function row(items) {
        return '<div class="field-row">' + items.map(field).join('') + '</div>';
      }
      function field(f) {
        var value = f.value === null || f.value === undefined ? '' : f.value;
        var req = f.required ? ' required' : '';
        var control;
        if (f.type === 'select') {
          control = '<select name="' + f.name + '"' + req + '>' +
            (f.options || []).map(function (o) {
              var selected = String(o.value) === String(value) ? ' selected' : '';
              return '<option value="' + UI.esc(o.value) + '"' + selected + '>' + UI.esc(o.label) + '</option>';
            }).join('') + '</select>';
        } else if (f.type === 'textarea') {
          control = '<textarea name="' + f.name + '" rows="' + (f.rows || 3) + '" placeholder="' +
            UI.esc(f.placeholder || '') + '"' + req + '>' + UI.esc(value) + '</textarea>';
        } else {
          control = '<input type="' + (f.type || 'text') + '" name="' + f.name + '" value="' +
            UI.esc(value) + '" placeholder="' + UI.esc(f.placeholder || '') + '"' + req +
            (f.step ? ' step="' + f.step + '"' : '') +
            (f.min !== undefined ? ' min="' + f.min + '"' : '') +
            (f.max !== undefined ? ' max="' + f.max + '"' : '') + '>';
        }
        return '<div class="field" data-field="' + f.name + '">' +
          '<label for="' + f.name + '">' + UI.esc(f.label) + (f.required ? ' *' : '') + '</label>' +
          control +
          (f.help ? '<small class="muted">' + UI.esc(f.help) + '</small>' : '') +
          '</div>';
      }
    },

    /** Lit un formulaire en objet (chaînes vides → null). */
    readForm: function (formEl) {
      var data = {};
      Array.prototype.forEach.call(formEl.elements, function (el) {
        if (!el.name) return;
        var value = el.value;
        data[el.name] = (typeof value === 'string' && value.trim() === '') ? null : value;
      });
      return data;
    },

    /** Marque les champs refusés par le backend. */
    markFieldErrors: function (formEl, fields) {
      formEl.querySelectorAll('.error-text').forEach(function (n) { n.remove(); });
      formEl.querySelectorAll('.invalid').forEach(function (n) { n.classList.remove('invalid'); });
      Object.keys(fields || {}).forEach(function (name) {
        var wrap = formEl.querySelector('[data-field="' + name + '"]');
        if (!wrap) return;
        var input = wrap.querySelector('input, select, textarea');
        if (input) input.classList.add('invalid');
        var small = document.createElement('span');
        small.className = 'error-text';
        small.textContent = fields[name];
        wrap.appendChild(small);
      });
    },

    /* ----------------------------- Tableaux ----------------------------- */
    /** columns: [{key, label, render, className, sortable}] */
    table: function (columns, rows, options) {
      options = options || {};
      if (!rows || !rows.length) {
        return '<div class="empty-state"><span class="icon">📭</span>' +
          UI.esc(options.emptyMessage || 'Aucune donnée à afficher') + '</div>';
      }
      var head = columns.map(function (c) {
        var cls = (c.sortable ? 'sortable ' : '') + (c.className || '');
        var arrow = '';
        if (c.sortable && options.sortBy === c.key) arrow = options.sortDir === 'desc' ? ' ▼' : ' ▲';
        return '<th class="' + cls + '"' + (c.sortable ? ' data-sort="' + c.key + '"' : '') + '>' +
          UI.esc(c.label) + arrow + '</th>';
      }).join('');

      var body = rows.map(function (row, index) {
        var tds = columns.map(function (c) {
          var content = c.render ? c.render(row, index) : UI.text(row[c.key]);
          return '<td class="' + (c.className || '') + '">' + content + '</td>';
        }).join('');
        var attr = options.rowId ? ' data-id="' + UI.esc(options.rowId(row)) + '"' : '';
        return '<tr' + attr + '>' + tds + '</tr>';
      }).join('');

      return '<div class="table-wrap"><table><thead><tr>' + head +
        '</tr></thead><tbody>' + body + '</tbody></table></div>';
    },

    pagination: function (total, limit, offset) {
      if (!total) return '';
      var from = total === 0 ? 0 : offset + 1;
      var to = Math.min(offset + limit, total);
      var prevDisabled = offset <= 0 ? ' disabled' : '';
      var nextDisabled = offset + limit >= total ? ' disabled' : '';
      return '<div class="pagination"><span>' + from + '–' + to + ' sur ' + total + '</span>' +
        '<div class="pagination-buttons">' +
        '<button class="btn btn-secondary btn-sm" data-page="prev"' + prevDisabled + '>← Précédent</button>' +
        '<button class="btn btn-secondary btn-sm" data-page="next"' + nextDisabled + '>Suivant →</button>' +
        '</div></div>';
    },

    spinner: function () { return '<div class="spinner"></div>'; },

    empty: function (message, icon) {
      return '<div class="empty-state"><span class="icon">' + (icon || '📭') + '</span>' +
        UI.esc(message) + '</div>';
    },

    /** Déclenche une fonction après un délai d'inactivité (recherche). */
    debounce: function (fn, delay) {
      var timer = null;
      return function () {
        var args = arguments, self = this;
        clearTimeout(timer);
        timer = setTimeout(function () { fn.apply(self, args); }, delay || 280);
      };
    },

    /** Liste d'options à partir d'un tableau d'entités. */
    options: function (items, valueKey, labelKey, placeholder) {
      var html = placeholder === false ? '' : '<option value="">' + UI.esc(placeholder || 'Tous') + '</option>';
      return html + (items || []).map(function (item) {
        return '<option value="' + UI.esc(item[valueKey]) + '">' + UI.esc(item[labelKey]) + '</option>';
      }).join('');
    }
  };

  global.UI = UI;
})(window);
