/**
 * api.js — client HTTP de l'API REST.
 *
 * Toutes les requêtes utilisent des URL relatives : le frontend est servi par
 * le backend lui-même, il n'y a donc aucun problème de CORS.
 * Le jeton JWT est conservé dans localStorage et envoyé en en-tête
 * `Authorization: Bearer <jeton>`.
 */
(function (global) {
  'use strict';

  var TOKEN_KEY = 'ge.token';
  var USER_KEY = 'ge.user';

  /**
   * Stockage de session résistant aux environnements où localStorage est
   * indisponible (navigation privée stricte, iframe au stockage cloisonné,
   * cookies tiers bloqués). On bascule alors silencieusement sur une mémoire
   * volatile : la session reste valable pour l'onglet courant.
   */
  var Store = (function () {
    var memory = {};
    var available = (function () {
      try {
        var probe = '__ge_probe__';
        window.localStorage.setItem(probe, '1');
        window.localStorage.removeItem(probe);
        return true;
      } catch (e) {
        return false;
      }
    })();

    return {
      persistent: available,
      get: function (key) {
        if (available) {
          try { return window.localStorage.getItem(key); } catch (e) { /* bascule mémoire */ }
        }
        return Object.prototype.hasOwnProperty.call(memory, key) ? memory[key] : null;
      },
      set: function (key, value) {
        memory[key] = value;
        if (available) {
          try { window.localStorage.setItem(key, value); } catch (e) { /* mémoire seule */ }
        }
      },
      remove: function (key) {
        delete memory[key];
        if (available) {
          try { window.localStorage.removeItem(key); } catch (e) { /* mémoire seule */ }
        }
      }
    };
  })();

  /** Erreur applicative portant le statut HTTP et les détails de validation. */
  function ApiError(status, code, message, fields) {
    var err = new Error(message || 'Erreur inattendue');
    err.name = 'ApiError';
    err.status = status;
    err.code = code || 'ERROR';
    err.fields = fields || {};
    return err;
  }

  var Api = {
    /* ----------------------------- Session ----------------------------- */
    storageIsPersistent: Store.persistent,
    token: function () { return Store.get(TOKEN_KEY); },
    user: function () {
      try { return JSON.parse(Store.get(USER_KEY) || 'null'); }
      catch (e) { return null; }
    },
    setSession: function (token, user) {
      Store.set(TOKEN_KEY, token);
      Store.set(USER_KEY, JSON.stringify(user || null));
      // Canal de secours : certains intermediaires reseau suppriment l'en-tete
      // Authorization. Le cookie voyage alors avec la requete.
      try {
        document.cookie = 'ge_token=' + token + '; path=/; max-age=43200; SameSite=Lax';
      } catch (e) { /* cookies indisponibles : les en-tetes suffiront */ }
    },
    clearSession: function () {
      Store.remove(TOKEN_KEY);
      Store.remove(USER_KEY);
      try {
        document.cookie = 'ge_token=; path=/; max-age=0; SameSite=Lax';
      } catch (e) { /* rien a nettoyer */ }
    },
    isLoggedIn: function () { return !!this.token(); },
    role: function () { var u = this.user(); return u ? u.role : null; },
    /** Hiérarchie ADMIN > TEACHER > VIEWER. */
    can: function (minRole) {
      var levels = { VIEWER: 1, TEACHER: 2, ADMIN: 3 };
      return (levels[this.role()] || 0) >= (levels[minRole] || 99);
    },

    /* ----------------------------- Requêtes ----------------------------- */
    request: function (method, path, body, options) {
      options = options || {};
      var headers = { 'Accept': 'application/json' };
      var token = this.token();
      if (token) {
        headers['Authorization'] = 'Bearer ' + token;
        // Doublon volontaire : si un proxy consomme l'en-tete Authorization,
        // le serveur accepte aussi X-Auth-Token.
        headers['X-Auth-Token'] = token;
      }

      var payload;
      if (body !== undefined && body !== null) {
        if (typeof body === 'string') {
          headers['Content-Type'] = options.contentType || 'text/plain; charset=utf-8';
          payload = body;
        } else {
          headers['Content-Type'] = 'application/json';
          payload = JSON.stringify(body);
        }
      }

      return fetch(path, { method: method, headers: headers, body: payload })
        .then(function (res) {
          if (res.status === 204) return null;
          var type = res.headers.get('Content-Type') || '';

          if (options.raw) {
            if (!res.ok) return res.text().then(function (t) { throw parseError(res.status, t); });
            return res.blob();
          }

          return res.text().then(function (text) {
            var data = null;
            if (text && type.indexOf('application/json') >= 0) {
              try { data = JSON.parse(text); } catch (e) { data = null; }
            }
            if (!res.ok) {
              if (res.status === 401 && path.indexOf('/api/auth/login') < 0) {
                Api.clearSession();
                if (global.App && global.App.showLogin) {
                  global.App.showLogin('Votre session a expiré, merci de vous reconnecter.');
                }
              }
              var e = data && data.error ? data.error : {};
              throw ApiError(res.status, e.code,
                e.message || ('Erreur HTTP ' + res.status),
                (e.details && e.details.fields) || {});
            }
            return data;
          });
        });
    },

    get: function (p, o) { return this.request('GET', p, null, o); },
    post: function (p, b, o) { return this.request('POST', p, b, o); },
    put: function (p, b, o) { return this.request('PUT', p, b, o); },
    del: function (p) { return this.request('DELETE', p, null); },

    /** Construit une query-string en ignorant les valeurs vides. */
    qs: function (params) {
      var parts = [];
      Object.keys(params || {}).forEach(function (key) {
        var value = params[key];
        if (value === undefined || value === null || value === '') return;
        parts.push(encodeURIComponent(key) + '=' + encodeURIComponent(value));
      });
      return parts.length ? '?' + parts.join('&') : '';
    },

    /** Télécharge un fichier protégé par le jeton (PDF, CSV, JSON). */
    download: function (path, filename) {
      return this.get(path, { raw: true }).then(function (blob) {
        var url = URL.createObjectURL(blob);
        var a = document.createElement('a');
        a.href = url;
        a.download = filename;
        document.body.appendChild(a);
        a.click();
        document.body.removeChild(a);
        setTimeout(function () { URL.revokeObjectURL(url); }, 2000);
      });
    },

    /* ----------------------------- Auth ----------------------------- */
    login: function (username, password) {
      var self = this;
      return this.post('/api/auth/login', { username: username, password: password })
        .then(function (data) {
          self.setSession(data.token, data.user);
          return data.user;
        });
    },
    me: function () { return this.get('/api/auth/me'); },
    changePassword: function (current, next) {
      return this.post('/api/auth/password',
        { current_password: current, new_password: next });
    }
  };

  function parseError(status, text) {
    try {
      var data = JSON.parse(text);
      if (data && data.error) {
        return ApiError(status, data.error.code, data.error.message,
          (data.error.details || {}).fields);
      }
    } catch (e) { /* corps non JSON */ }
    return ApiError(status, 'ERROR', 'Erreur HTTP ' + status, {});
  }

  global.Api = Api;
})(window);
