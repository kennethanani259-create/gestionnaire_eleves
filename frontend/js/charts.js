/**
 * charts.js — petits graphiques SVG générés à la main (aucune bibliothèque).
 * Chaque fonction renvoie une chaîne SVG prête à être insérée dans le DOM.
 */
(function (global) {
  'use strict';

  var PALETTE = ['#2563eb', '#16a34a', '#d97706', '#dc2626', '#7c3aed',
                 '#0891b2', '#db2777', '#65a30d', '#ea580c', '#4f46e5'];

  function esc(s) {
    return String(s === null || s === undefined ? '' : s)
      .replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;');
  }

  var Charts = {
    /**
     * Histogramme vertical.
     * data: [{label, value}]
     */
    bar: function (data, options) {
      options = options || {};
      data = data || [];
      if (!data.length) return emptyBox('Aucune donnée');

      var width = 100, height = 46, padTop = 6, padBottom = 12, padLeft = 7, padRight = 2;
      var max = Math.max.apply(null, data.map(function (d) { return d.value; }));
      if (max <= 0) max = 1;
      var plotW = width - padLeft - padRight;
      var plotH = height - padTop - padBottom;
      var slot = plotW / data.length;
      var barW = slot * 0.6;

      var svg = open(width, height);
      svg += grid(padLeft, padTop, plotW, plotH, max, options.integer);

      data.forEach(function (d, i) {
        var h = (d.value / max) * plotH;
        var x = padLeft + slot * i + (slot - barW) / 2;
        var y = padTop + plotH - h;
        var color = options.color || PALETTE[i % PALETTE.length];
        svg += '<rect x="' + f(x) + '" y="' + f(y) + '" width="' + f(barW) + '" height="' + f(h) +
          '" rx="0.6" fill="' + color + '"><title>' + esc(d.label) + ' : ' + d.value + '</title></rect>';
        if (d.value > 0) {
          svg += label(x + barW / 2, y - 1.2, formatValue(d.value), 2.6, '#0f172a', 'middle', 600);
        }
        svg += label(x + barW / 2, height - 4.5, d.label, 2.5, '#64748b', 'middle');
      });
      return svg + '</svg>';
    },

    /**
     * Courbe (évolution temporelle).
     * data: [{label, value}]
     */
    line: function (data, options) {
      options = options || {};
      data = data || [];
      if (data.length < 2) return emptyBox('Données insuffisantes pour tracer une courbe');

      var width = 100, height = 46, padTop = 6, padBottom = 12, padLeft = 7, padRight = 3;
      var max = Math.max.apply(null, data.map(function (d) { return d.value; }));
      if (max <= 0) max = 1;
      var plotW = width - padLeft - padRight;
      var plotH = height - padTop - padBottom;
      var step = plotW / (data.length - 1);
      var color = options.color || PALETTE[0];

      var points = data.map(function (d, i) {
        return { x: padLeft + step * i, y: padTop + plotH - (d.value / max) * plotH, d: d };
      });

      var svg = open(width, height);
      svg += grid(padLeft, padTop, plotW, plotH, max, options.integer);

      var path = points.map(function (p, i) { return (i ? 'L' : 'M') + f(p.x) + ' ' + f(p.y); }).join(' ');
      var area = path + ' L' + f(points[points.length - 1].x) + ' ' + f(padTop + plotH) +
        ' L' + f(points[0].x) + ' ' + f(padTop + plotH) + ' Z';

      svg += '<path d="' + area + '" fill="' + color + '" opacity="0.12"/>';
      svg += '<path d="' + path + '" fill="none" stroke="' + color +
        '" stroke-width="0.7" stroke-linejoin="round" stroke-linecap="round"/>';

      points.forEach(function (p, i) {
        svg += '<circle cx="' + f(p.x) + '" cy="' + f(p.y) + '" r="0.9" fill="#fff" stroke="' +
          color + '" stroke-width="0.6"><title>' + esc(p.d.label) + ' : ' + p.d.value + '</title></circle>';
        if (i % Math.ceil(data.length / 12) === 0 || i === data.length - 1) {
          svg += label(p.x, height - 4.5, p.d.label, 2.4, '#64748b', 'middle');
        }
      });
      return svg + '</svg>';
    },

    /**
     * Anneau (répartition).
     * data: [{label, value, color?}]
     */
    donut: function (data, options) {
      options = options || {};
      data = (data || []).filter(function (d) { return d.value > 0; });
      if (!data.length) return emptyBox('Aucune donnée');

      var total = data.reduce(function (sum, d) { return sum + d.value; }, 0);
      var size = 100, cx = 32, cy = 50, outer = 26, inner = 15;
      var svg = open(size, size);
      var angle = -Math.PI / 2;

      data.forEach(function (d, i) {
        var sweep = (d.value / total) * Math.PI * 2;
        var color = d.color || PALETTE[i % PALETTE.length];
        svg += '<path d="' + ringSlice(cx, cy, inner, outer, angle, angle + sweep) +
          '" fill="' + color + '"><title>' + esc(d.label) + ' : ' + d.value +
          ' (' + Math.round(d.value / total * 100) + ' %)</title></path>';
        angle += sweep;
      });

      svg += '<text x="' + cx + '" y="' + (cy - 1) + '" text-anchor="middle" font-size="7" ' +
        'font-weight="700" fill="#0f172a">' + total + '</text>';
      svg += '<text x="' + cx + '" y="' + (cy + 5) + '" text-anchor="middle" font-size="3.4" ' +
        'fill="#64748b">' + esc(options.centerLabel || 'total') + '</text>';

      // Légende à droite
      data.forEach(function (d, i) {
        var y = 26 + i * 9;
        var color = d.color || PALETTE[i % PALETTE.length];
        svg += '<rect x="66" y="' + (y - 3) + '" width="4" height="4" rx="1" fill="' + color + '"/>';
        svg += label(72, y, d.label, 3.4, '#0f172a', 'start');
        svg += label(72, y + 4, d.value + ' (' + Math.round(d.value / total * 100) + ' %)',
          3, '#64748b', 'start');
      });
      return svg + '</svg>';
    },

    /** Barres horizontales (utile pour les moyennes par matière). */
    hbar: function (data, options) {
      options = options || {};
      data = data || [];
      if (!data.length) return emptyBox('Aucune donnée');

      var max = options.max || Math.max.apply(null, data.map(function (d) { return d.value; })) || 1;
      var rowH = 11, width = 100;
      var height = Math.max(20, data.length * rowH + 4);
      var labelW = 30, barW = width - labelW - 10;

      var svg = open(width, height, height);
      data.forEach(function (d, i) {
        var y = 3 + i * rowH;
        var w = Math.max(0.4, (d.value / max) * barW);
        var color = d.color || (d.value >= 12 ? '#16a34a' : (d.value >= 10 ? '#d97706' : '#dc2626'));
        svg += label(0, y + 5, d.label, 3.4, '#0f172a', 'start');
        svg += '<rect x="' + labelW + '" y="' + (y + 1.5) + '" width="' + barW +
          '" height="5" rx="2.5" fill="#e2e8f0"/>';
        svg += '<rect x="' + labelW + '" y="' + (y + 1.5) + '" width="' + f(w) +
          '" height="5" rx="2.5" fill="' + color + '"><title>' + esc(d.label) + ' : ' +
          d.value + '</title></rect>';
        svg += label(width, y + 5.6, formatValue(d.value), 3.2, '#0f172a', 'end', 700);
      });
      return svg + '</svg>';
    }
  };

  /* ----------------------------- Utilitaires ----------------------------- */
  function open(w, h, viewH) {
    return '<svg viewBox="0 0 ' + w + ' ' + (viewH || h) + '" width="100%" ' +
      'preserveAspectRatio="xMidYMid meet" xmlns="http://www.w3.org/2000/svg" ' +
      'font-family="system-ui, sans-serif" role="img">';
  }

  function grid(x, y, w, h, max, integer) {
    var out = '';
    for (var i = 0; i <= 4; i++) {
      var gy = y + (h / 4) * i;
      var value = max - (max / 4) * i;
      out += '<line x1="' + x + '" y1="' + f(gy) + '" x2="' + f(x + w) + '" y2="' + f(gy) +
        '" stroke="#e2e8f0" stroke-width="0.2"/>';
      out += label(x - 1, gy + 1, integer ? Math.round(value) : formatValue(value), 2.4, '#94a3b8', 'end');
    }
    return out;
  }

  function label(x, y, text, size, color, anchor, weight) {
    return '<text x="' + f(x) + '" y="' + f(y) + '" font-size="' + size + '" fill="' + color +
      '" text-anchor="' + (anchor || 'start') + '"' +
      (weight ? ' font-weight="' + weight + '"' : '') + '>' + esc(text) + '</text>';
  }

  function ringSlice(cx, cy, r1, r2, a1, a2) {
    // Un anneau complet ne peut pas être dessiné d'un seul arc : on le coupe.
    if (a2 - a1 >= Math.PI * 2 - 1e-6) a2 = a1 + Math.PI * 2 - 1e-4;
    var large = (a2 - a1) > Math.PI ? 1 : 0;
    var p1 = pt(cx, cy, r2, a1), p2 = pt(cx, cy, r2, a2);
    var p3 = pt(cx, cy, r1, a2), p4 = pt(cx, cy, r1, a1);
    return 'M' + p1 + ' A' + r2 + ' ' + r2 + ' 0 ' + large + ' 1 ' + p2 +
      ' L' + p3 + ' A' + r1 + ' ' + r1 + ' 0 ' + large + ' 0 ' + p4 + ' Z';
  }

  function pt(cx, cy, r, a) { return f(cx + r * Math.cos(a)) + ' ' + f(cy + r * Math.sin(a)); }
  function f(n) { return Math.round(n * 100) / 100; }
  function formatValue(v) {
    return (Math.abs(v - Math.round(v)) < 0.005 ? String(Math.round(v)) : v.toFixed(1).replace('.', ','));
  }
  function emptyBox(message) {
    return '<div class="empty-state"><span class="icon">📈</span>' + esc(message) + '</div>';
  }

  Charts.PALETTE = PALETTE;
  global.Charts = Charts;
})(window);
