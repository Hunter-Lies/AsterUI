(function (root) {
  "use strict";

  if (!root || root.asterui) {
    return;
  }

  var listeners = new Map();
  var sent = [];

  function valid(name) {
    return typeof name === "string" &&
      /^[A-Za-z0-9_.:\-]{1,64}$/.test(name);
  }

  var api = {
    on: function (name, callback) {
      if (!valid(name)) {
        throw new TypeError("asterui.on: invalid event name");
      }
      if (typeof callback !== "function") {
        throw new TypeError("asterui.on: callback must be a function");
      }
      var set = listeners.get(name);
      if (!set) {
        set = new Set();
        listeners.set(name, set);
      }
      set.add(callback);
      return function () {
        api.off(name, callback);
      };
    },
    off: function (name, callback) {
      var set = listeners.get(name);
      if (!set) {
        return false;
      }
      if (callback === undefined) {
        listeners.delete(name);
        return true;
      }
      var removed = set.delete(callback);
      if (set.size === 0) {
        listeners.delete(name);
      }
      return removed;
    },
    emit: function (name, payload) {
      if (!valid(name)) {
        throw new TypeError("asterui.emit: invalid event name");
      }
      sent.push({ name: name, payload: payload });
      console.info("[AsterUI mock emit]", name, payload);
    }
  };

  Object.defineProperty(api, "__dispatch", {
    value: function (name, payload) {
      var set = listeners.get(name);
      if (!set) {
        return;
      }
      Array.from(set).forEach(function (callback) {
        callback(payload);
      });
    }
  });
  Object.defineProperty(api, "__mock", {
    value: Object.freeze({
      dispatch: api.__dispatch,
      sent: function () {
        return sent.slice();
      }
    })
  });

  Object.defineProperty(root, "asterui", {
    value: Object.freeze(api),
    enumerable: true
  });
  Object.defineProperty(root, "gtanui", {
    value: api,
    enumerable: true
  });

  function ready() {
    root.dispatchEvent(new Event("asterui:ready"));
    root.dispatchEvent(new Event("gtanui:ready"));
  }
  if (document.readyState === "loading") {
    document.addEventListener("DOMContentLoaded", ready, { once: true });
  } else {
    ready();
  }
})(window);
