"use strict";
// Full screen for Choowivision and the stage (the owner, 2026-10-13: "full screen doesn't work properly"): the
// standard call or WebKit's, Android's navigation bar hidden, the phone turned sideways where it allows it (the
// stage only), the layout redrawn once the new size is in, and the button showing which way it goes.
(function () {
  const d = document, root = d.documentElement;
  const active = () => d.fullscreenElement || d.webkitFullscreenElement;
  async function enter(landscape) {
    try {
      if (root.requestFullscreen) await root.requestFullscreen({navigationUI: "hide"});
      else root.webkitRequestFullscreen();
    } catch { return; }
    if (landscape) try { await screen.orientation.lock("landscape"); } catch { /* a computer, or not allowed: fine */ }
  }
  function leave() {
    try { screen.orientation.unlock(); } catch { /* fine */ }
    const exit = d.exitFullscreen || d.webkitExitFullscreen;
    if (exit) Promise.resolve(exit.call(d)).catch(() => {});
  }
  window.ForgeFull = (button, {landscape = false} = {}) => {
    if (!root.requestFullscreen && !root.webkitRequestFullscreen) { button.hidden = true; return; }   // iPhone: pages can't
    const show = () => {
      const on = !!active();
      button.textContent = on ? "✕" : "⤢";
      button.setAttribute("aria-label", on ? "leave full screen" : "full screen"); button.title = on ? "leave full screen" : "full screen";
      requestAnimationFrame(() => requestAnimationFrame(() => dispatchEvent(new Event("resize"))));    // the new size is in by then
    };
    button.addEventListener("click", () => active() ? leave() : enter(landscape));
    d.addEventListener("fullscreenchange", show); d.addEventListener("webkitfullscreenchange", show);
    show();
  };
})();
