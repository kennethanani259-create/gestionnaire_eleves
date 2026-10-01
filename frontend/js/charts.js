/**
 * charts.js — graphiques SVG tracés à la main.
 *
 * Pourquoi pas de bibliothèque : les quatre formes nécessaires tiennent en
 * 200 lignes, et une dépendance externe imposerait sa propre identité
 * graphique (couleurs, polices, info-bulles) à rebours du système défini ici.
 *
 * Les couleurs reprennent la palette du registre : encre et ardoise pour les
 * volumes neutres, ocre pour la distinction, rouge correcteur pour ce qui
 * manque. Chaque élément porte un <title> lisible par les lecteurs d'écran.
 */
(function (global) {
  'use strict';

  var TONES = {
    board: '#2b4034',
    ink:   '#3f4d44',
    ochre: '#b9782a',
    green: '#2f6b4f',
    red:   '#a8322a',
    amber: '#8a6410'
  };
  var SERIES = [TONES.board, TONES.ochre, TONES.green, TONES.red, TONES.amber, TONES.ink];

  var RULE = '#ddd5c3';
  var LABEL = '#646a62';
  var TEXT = '#1b1f1d';
  var SERIF = "'Iowan Old Style',Palatino,Georgia,serif";

  function esc(s) {
    return String(s === null || s === undefined ? '' : s)
      .replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;');
  }
  function tone(name, fallback) { return TONES[name] || name || fallback; }
  function f(n) { return Math.round(n * 100) / 100; }
  function fmt(v) {
    return Math.abs(v - Math.round(v)) < 0.005
      ? String(Math.round(v)) : v.toFixed(1).replace('.', ',');
  }
  function open(w, h) {
    return '<svg viewBox="0 0 ' + w + ' ' + h + '" width="100%" role="img" ' +
      'preserveAspectRatio="xMidYMid meet" xmlns="http://www.w3.org/2000/svg">';
  }
  function text(x, y, value, size, color, anchor, weight, family) {
    return '<text x="' + f(x) + '" y="' + f(y) + '" font-size="' + size + '" fill="' + color +
      '" text-anchor="' + (anchor || 'start') + '"' +
      (weight ? ' font-weight="' + weight + '"' : '') +
      ' font-family="' + (family || 'system-ui, sans-serif') + '">' + esc(value) + '</text>';
  }
  function blank(message) {
    return '<p class="blank"><span class="blank__title">' + esc(message) + '</span></p>';
  }
  /** Lignes de repère horizontales, dans l'esprit d'une page réglée. */
  function rules(x, y, w, h, max, integer) {
    var out = '';
    for (var i = 0; i <= 4; i++) {
      var gy = y + (h / 4) * i;
      var v = max - (max / 4) * i;
      out += '<line x1="' + x + '" y1="' + f(gy) + '" x2="' + f(x + w) + '" y2="' + f(gy) +
        '" stroke="' + RULE + '" stroke-width="0.25"/>';
      out += text(x - 1.5, gy + 1, integer ? Math.round(v) : fmt(v), 2.5, LABEL, 'end');
    }
    return out;
  }

  var Charts = {
    TONES: TONES,

    /** Histogramme vertical. data: [{label, value}] */
    bar: function (data, options) {
      options = options || {};
      data = data || [];
      if (!data.length) return blank('Aucune donnée');

      var W = 100, H = 46, top = 6, bottom = 12, left = 8, right = 2;
      var max = Math.max.apply(null, data.map(function (d) { return d.value; })) || 1;
      var pw = W - left - right, ph = H - top - bottom;
      var slot = pw / data.length, bw = Math.min(slot * 0.62, 9);

      var svg = open(W, H) + rules(left, top, pw, ph, max, options.integer);
      data.forEach(function (d, i) {
        var h = (d.value / max) * ph;
        var x = left + slot * i + (slot - bw) / 2;
        var y = top + ph - h;
        svg += '<rect x="' + f(x) + '" y="' + f(y) + '" width="' + f(bw) + '" height="' + f(h) +
          '" fill="' + tone(options.color, TONES.board) + '">' +
          '<title>' + esc(d.label) + ' : ' + d.value + '</title></rect>';
        if (d.value > 0) svg += text(x + bw / 2, y - 1.3, fmt(d.value), 2.8, TEXT, 'middle', 600, SERIF);
        svg += text(x + bw / 2, H - 4, d.label, 2.6, LABEL, 'middle');
      });
      return svg + '</svg>';
    },

    /** Courbe d'évolution. data: [{label, value}] */
    line: function (data, options) {
      options = options || {};
      data = data || [];
      if (data.length < 2) return blank('Au moins deux périodes sont nécessaires');

      var W = 100, H = 46, top = 6, bottom = 12, left = 8, right = 3;
      var max = Math.max.apply(null, data.map(function (d) { return d.value; })) || 1;
      var pw = W - left - right, ph = H - top - bottom;
      var step = pw / (data.length - 1);
      var color = tone(options.color, TONES.board);

      var pts = data.map(function (d, i) {
        return { x: left + step * i, y: top + ph - (d.value / max) * ph, d: d };
      });
      var path = pts.map(function (p, i) { return (i ? 'L' : 'M') + f(p.x) + ' ' + f(p.y); }).join(' ');

      var svg = open(W, H) + rules(left, top, pw, ph, max, options.integer);
      svg += '<path d="' + path + ' L' + f(pts[pts.length - 1].x) + ' ' + f(top + ph) +
        ' L' + f(pts[0].x) + ' ' + f(top + ph) + ' Z" fill="' + color + '" opacity=".09"/>';
      svg += '<path d="' + path + '" fill="none" stroke="' + color +
        '" stroke-width=".7" stroke-linejoin="round" stroke-linecap="round"/>';

      var every = Math.ceil(data.length / 10);
      pts.forEach(function (p, i) {
        svg += '<circle cx="' + f(p.x) + '" cy="' + f(p.y) + '" r=".9" fill="#fbf9f3" stroke="' +
          color + '" stroke-width=".55"><title>' + esc(p.d.label) + ' : ' + p.d.value +
          '</title></circle>';
        if (i % every === 0 || i === data.length - 1) {
          svg += text(p.x, H - 4, p.d.label, 2.6, LABEL, 'middle');
        }
      });
      return svg + '</svg>';
    },

    /** Anneau de répartition. data: [{label, value, tone}] */
    donut: function (data, options) {
      options = options || {};
      data = (data || []).filter(function (d) { return d.value > 0; });
      if (!data.length) return blank('Aucune donnée');

      var total = data.reduce(function (s, d) { return s + d.value; }, 0);
      var W = 100, H = 56, cx = 27, cy = 28, r2 = 22, r1 = 14;
      var svg = open(W, H);
      var a = -Math.PI / 2;

      data.forEach(function (d, i) {
        var sweep = (d.value / total) * Math.PI * 2;
        var color = tone(d.tone, SERIES[i % SERIES.length]);
        svg += '<path d="' + ring(cx, cy, r1, r2, a, a + sweep) + '" fill="' + color + '">' +
          '<title>' + esc(d.label) + ' : ' + d.value + ' (' +
          Math.round(d.value / total * 100) + ' %)</title></path>';
        a += sweep;
      });

      svg += text(cx, cy + 1, total, 8, TEXT, 'middle', 600, SERIF);
      svg += text(cx, cy + 6, options.centerLabel || 'total', 3, LABEL, 'middle');

      data.forEach(function (d, i) {
        var y = 16 + i * 11;
        svg += '<rect x="58" y="' + (y - 3.2) + '" width="3.2" height="3.2" fill="' +
          tone(d.tone, SERIES[i % SERIES.length]) + '"/>';
        svg += text(63, y, d.label, 3.4, TEXT, 'start');
        svg += text(63, y + 4.4, d.value + ' · ' + Math.round(d.value / total * 100) + ' %',
          3, LABEL, 'start');
      });
      return svg + '</svg>';
    },

    /** Barres horizontales — comparaison de moyennes sur 20. */
    hbar: function (data, options) {
      options = options || {};
      data = data || [];
      if (!data.length) return blank('Aucune donnée');

      var max = options.max || Math.max.apply(null, data.map(function (d) { return d.value; })) || 1;
      var rowH = 10, W = 100, H = Math.max(18, data.length * rowH + 3);
      var labelW = 26, barW = W - labelW - 9;

      var svg = open(W, H);
      data.forEach(function (d, i) {
        var y = 2 + i * rowH;
        var w = Math.max(0.3, (d.value / max) * barW);
        var color = d.value >= 12 ? TONES.green : (d.value >= 10 ? TONES.amber : TONES.red);
        svg += text(0, y + 5, d.label, 3.3, TEXT, 'start');
        svg += '<rect x="' + labelW + '" y="' + (y + 1.8) + '" width="' + barW +
          '" height="4.4" fill="#ece7da"/>';
        svg += '<rect x="' + labelW + '" y="' + (y + 1.8) + '" width="' + f(w) +
          '" height="4.4" fill="' + color + '"><title>' + esc(d.label) + ' : ' +
          fmt(d.value) + '</title></rect>';
        svg += text(W, y + 5.4, fmt(d.value), 3.2, TEXT, 'end', 600, SERIF);
      });
      return svg + '</svg>';
    }
  };

  function ring(cx, cy, r1, r2, a1, a2) {
    if (a2 - a1 >= Math.PI * 2 - 1e-6) a2 = a1 + Math.PI * 2 - 1e-4;
    var large = (a2 - a1) > Math.PI ? 1 : 0;
    return 'M' + pt(cx, cy, r2, a1) + ' A' + r2 + ' ' + r2 + ' 0 ' + large + ' 1 ' + pt(cx, cy, r2, a2) +
      ' L' + pt(cx, cy, r1, a2) + ' A' + r1 + ' ' + r1 + ' 0 ' + large + ' 0 ' + pt(cx, cy, r1, a1) + ' Z';
  }
  function pt(cx, cy, r, a) { return f(cx + r * Math.cos(a)) + ' ' + f(cy + r * Math.sin(a)); }

  global.Charts = Charts;
})(window);
