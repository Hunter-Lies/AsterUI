(function (root) {
  "use strict";

  function onReady(callback) {
    if (root.asterui) {
      callback();
      return;
    }
    root.addEventListener("asterui:ready", callback, { once: true });
  }

  function emit(name, payload) {
    if (root.asterui) {
      root.asterui.emit(name, payload);
    }
  }

  function on(name, callback) {
    if (root.asterui) {
      return root.asterui.on(name, callback);
    }
    return function () {};
  }

  root.AsterDemo = Object.freeze({ onReady: onReady, emit: emit, on: on });
})(window);
