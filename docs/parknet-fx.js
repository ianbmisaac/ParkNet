/* =====================================================================
   ParkNet FX · efectos visuales compartidos (index.html + admin.html)
   Solo observa el DOM y agrega animaciones. No modifica la lógica de las
   páginas (MQTT, Firebase, vista 3D). Si algo falla, la página sigue igual.
   ===================================================================== */
(function () {
  "use strict";
  var reduce = false;
  try { reduce = window.matchMedia("(prefers-reduced-motion: reduce)").matches; } catch (e) {}
  var body = document.body;
  var isAdmin = body.classList.contains("pn-admin");
  var isIndex = body.classList.contains("pn-index");

  function q(s, r) { return (r || document).querySelector(s); }
  function el(tag, cls, html) {
    var e = document.createElement(tag);
    if (cls) e.className = cls;
    if (html != null) e.innerHTML = html;
    return e;
  }
  function pad(n) { return String(n).padStart(2, "0"); }
  function safe(fn) { try { fn(); } catch (e) { if (window.console) console.warn("[ParkNet FX]", e); } }
  // observa el texto de un nodo y avisa solo cuando CAMBIA de verdad
  function watchText(node, cb) {
    if (!node || !window.MutationObserver) return;
    var last = node.textContent;
    new MutationObserver(function () {
      var t = node.textContent;
      if (t !== last) { var p = last; last = t; try { cb(t, p); } catch (e) {} }
    }).observe(node, { childList: true, characterData: true, subtree: true });
  }
  function bump(node) {
    if (!node || reduce) return;
    node.classList.remove("pn-pop");
    void node.offsetWidth;
    node.classList.add("pn-pop");
  }

  /* ---------- fondo aurora ---------- */
  safe(function () {
    var a = el("div", "pn-aurora", "<i></i><i></i><i></i>");
    a.setAttribute("aria-hidden", "true");
    body.insertBefore(a, body.firstChild);
  });

  /* ---------- pantalla de inicio (una vez por pestaña) ---------- */
  safe(function () {
    try { if (sessionStorage.getItem("pnSplash")) return; sessionStorage.setItem("pnSplash", "1"); } catch (e) {}
    if (reduce) return;
    var s = el("div", "pn-splash",
      '<div class="pn-splash-logo"><svg viewBox="0 0 100 100"><circle class="a" cx="50" cy="50" r="44"/>' +
      '<circle class="b" cx="50" cy="50" r="44"/></svg><b>P</b></div>' +
      '<div class="pn-splash-word">Park<span>Net</span></div>' +
      '<div class="pn-splash-sub">' + (isAdmin ? "Panel de administración" : "Parking S2 · en vivo") + "</div>" +
      '<div class="pn-splash-bar"><i></i></div>');
    s.setAttribute("aria-hidden", "true");
    body.appendChild(s);
    setTimeout(function () { if (s.parentNode) s.parentNode.removeChild(s); }, 2200);
  });

  /* ---------- avisos emergentes ---------- */
  var toasts = null;
  function toast(title, msg, kind, ms) {
    safe(function () {
      if (!toasts) { toasts = el("div", "pn-toasts"); toasts.setAttribute("aria-live", "polite"); body.appendChild(toasts); }
      ms = ms || 4200;
      var icon = kind === "coral" ? "!" : kind === "amber" ? "i" : "✓";
      var t = el("div", "pn-toast " + (kind || ""));
      t.style.setProperty("--ms", ms + "ms");
      var ic = el("div", "pn-toast-ic"); ic.textContent = icon;
      var tx = el("div");
      var s1 = el("strong"); s1.textContent = title;
      tx.appendChild(s1);
      if (msg) { var s2 = el("span"); s2.textContent = msg; tx.appendChild(s2); }
      t.appendChild(ic); t.appendChild(tx);
      toasts.appendChild(t);
      while (toasts.children.length > 4) toasts.removeChild(toasts.firstChild);
      setTimeout(function () { t.classList.add("out"); setTimeout(function () { if (t.parentNode) t.parentNode.removeChild(t); }, 380); }, ms);
    });
  }

  /* ---------- ripple en botones ---------- */
  document.addEventListener("pointerdown", function (e) {
    safe(function () {
      var b = e.target.closest && e.target.closest("button, .btn, .page-tab, .space-choice");
      if (!b || b.disabled || b.classList.contains("user-avatar-edit") || reduce) return;
      var r = b.getBoundingClientRect();
      var d = Math.max(r.width, r.height) * 2;
      var s = el("span", "pn-rip");
      s.style.cssText = "width:" + d + "px;height:" + d + "px;left:" + (e.clientX - r.left - d / 2) + "px;top:" + (e.clientY - r.top - d / 2) + "px";
      if (getComputedStyle(b).position === "static") b.style.position = "relative";
      b.classList.add("pn-rh");
      b.appendChild(s);
      setTimeout(function () { if (s.parentNode) s.parentNode.removeChild(s); }, 700);
    });
  }, { passive: true });

  /* ---------- foco de luz que sigue al mouse ---------- */
  (function () {
    var SPOT = ".summary,.model-panel,.booking-card,.service-row,.live-stall,body.pn-admin .card";
    var raf = 0, ev = null;
    document.addEventListener("pointermove", function (e) {
      if (e.pointerType === "touch") return;
      ev = e;
      if (raf) return;
      raf = requestAnimationFrame(function () {
        raf = 0;
        safe(function () {
          var c = ev.target.closest && ev.target.closest(SPOT);
          if (!c) return;
          var r = c.getBoundingClientRect();
          c.style.setProperty("--mx", (ev.clientX - r.left) + "px");
          c.style.setProperty("--my", (ev.clientY - r.top) + "px");
        });
      });
    }, { passive: true });
  }());

  /* ---------- confeti ---------- */
  function confetti() {
    if (reduce) return;
    safe(function () {
      var c = el("canvas", "pn-confetti");
      c.width = innerWidth; c.height = innerHeight;
      body.appendChild(c);
      var ctx = c.getContext("2d");
      var cols = ["#7af0c4", "#35d99a", "#a5f8d7", "#2d72a5", "#f0b866", "#ff8a7c", "#ffffff"];
      var P = [], i;
      for (i = 0; i < 150; i++) {
        P.push({
          x: innerWidth / 2 + (Math.random() - .5) * 180, y: innerHeight * .4,
          vx: (Math.random() - .5) * 17, vy: -Math.random() * 17 - 4, g: .36 + Math.random() * .2,
          s: 6 + Math.random() * 7, r: Math.random() * 6.28, vr: (Math.random() - .5) * .4,
          c: cols[(Math.random() * cols.length) | 0]
        });
      }
      var t0 = performance.now();
      (function step(now) {
        ctx.clearRect(0, 0, c.width, c.height);
        var life = Math.max(0, 1 - (now - t0) / 2700);
        P.forEach(function (p) {
          p.vy += p.g; p.vx *= .992; p.x += p.vx; p.y += p.vy; p.r += p.vr;
          ctx.save(); ctx.translate(p.x, p.y); ctx.rotate(p.r);
          ctx.globalAlpha = life; ctx.fillStyle = p.c;
          ctx.fillRect(-p.s / 2, -p.s / 4, p.s, p.s / 2);
          ctx.restore();
        });
        if (now - t0 < 2800) requestAnimationFrame(step);
        else if (c.parentNode) c.parentNode.removeChild(c);
      }(t0));
    });
  }

  /* =================================================================
     INDEX
     ================================================================= */
  function initIndex() {
    /* pestañas con pastilla deslizante */
    var tabs = q(".page-tabs");
    if (tabs) {
      var pill = el("span", "pn-pill");
      tabs.insertBefore(pill, tabs.firstChild);
      var place = function () {
        safe(function () {
          var sel = q('.page-tab[aria-selected="true"]', tabs);
          if (!sel || !sel.offsetWidth) return;
          pill.style.width = sel.offsetWidth + "px";
          pill.style.transform = "translateX(" + sel.offsetLeft + "px)";
          tabs.classList.add("pn-pilled");
        });
      };
      place();
      window.addEventListener("resize", place);
      window.addEventListener("load", place);
      if (document.fonts && document.fonts.ready) document.fonts.ready.then(place);
      Array.prototype.forEach.call(tabs.querySelectorAll(".page-tab"), function (t) {
        if (window.MutationObserver) new MutationObserver(place).observe(t, { attributes: true, attributeFilter: ["aria-selected"] });
      });
    }

    /* contador de lugares libres */
    var libres = q("#libres"), summary = q(".summary");
    watchText(libres, function (t) {
      bump(libres);
      if (summary) summary.classList.toggle("pn-full", t.trim() === "0");
    });

    /* plazas: aro de aviso + toast cuando cambian */
    ["liveSlot1", "liveSlot2", "liveSlot3"].forEach(function (id, i) {
      var s = document.getElementById(id);
      if (!s) return;
      var ring = el("i", "pn-ring");
      s.appendChild(ring);
      watchText(s.querySelector("strong"), function (t, prev) {
        var known = function (x) { return x === "Libre" || x === "Ocupada"; };
        if (!known(t) || !known(prev)) return;
        var occ = t === "Ocupada";
        if (!reduce) {
          ring.removeAttribute("data-f");
          void ring.offsetWidth;
          ring.setAttribute("data-f", occ ? "occ" : "free");
        }
        toast("Plaza A" + (i + 1) + (occ ? " ocupada" : " libre"),
          occ ? "Un auto acaba de estacionar." : "El espacio quedó disponible.", occ ? "coral" : "");
      });
    });

    /* conexión en vivo */
    var pillLabel = q("#livePillLabel");
    watchText(pillLabel, function (t, prev) {
      if (t === "Sensores en vivo") toast("Conectado a los sensores", "Datos en vivo desde la placa Arduino.", "");
      else if (prev === "Sensores en vivo") toast("Señal perdida", "Esperando que la placa vuelva a reportar.", "amber");
    });

    /* escena 3D: HUD + escaneo + tilt */
    var scene = q(".scene-wrap");
    if (scene) {
      var hud = el("div", "pn-hud",
        '<span class="pn-badge"><i></i><b>EN VIVO · 3D</b></span>' +
        '<span class="pn-corner tl"></span><span class="pn-corner tr"></span>' +
        '<span class="pn-corner bl"></span><span class="pn-corner br"></span><span class="pn-scan"></span>');
      scene.appendChild(hud);
      var badge = q(".pn-badge", hud), badgeTxt = q("b", badge), lp = q("#livePill");
      var sync = function () {
        if (!lp) return;
        var off = lp.classList.contains("is-offline");
        badge.classList.toggle("off", off);
        badgeTxt.textContent = off ? "SIN SEÑAL · 3D" : "EN VIVO · 3D";
      };
      sync();
      if (lp && window.MutationObserver) new MutationObserver(sync).observe(lp, { attributes: true, attributeFilter: ["class"] });
      var cv = q(".live-park-view", scene);
      if (cv && !reduce && window.matchMedia("(hover: hover)").matches) {
        scene.addEventListener("pointermove", function (e) {
          var r = scene.getBoundingClientRect();
          var nx = (e.clientX - r.left) / r.width - .5, ny = (e.clientY - r.top) / r.height - .5;
          cv.style.setProperty("--ry", (nx * 6).toFixed(2) + "deg");
          cv.style.setProperty("--rx", (-ny * 5).toFixed(2) + "deg");
        }, { passive: true });
        scene.addEventListener("pointerleave", function () {
          cv.style.setProperty("--ry", "0deg"); cv.style.setProperty("--rx", "0deg");
        });
      }
    }

    /* reservas: atajos de hora (sin restricciones) */
    var fields = q("#bookingFields");
    if (fields) {
      var quick = el("div", "pn-quick");
      quick.setAttribute("role", "group");
      quick.setAttribute("aria-label", "Atajos de hora");
      var make = function (label, fn) {
        var b = el("button"); b.type = "button"; b.textContent = label;
        b.addEventListener("click", function () { safe(fn); });
        quick.appendChild(b);
      };
      var setSlot = function (d) {
        var date = q("#bookingDate"), time = q("#bookingTime");
        date.value = d.getFullYear() + "-" + pad(d.getMonth() + 1) + "-" + pad(d.getDate());
        time.value = pad(d.getHours()) + ":00";
        date.dispatchEvent(new Event("change", { bubbles: true }));
        time.dispatchEvent(new Event("change", { bubbles: true }));
        toast("Horario elegido", d.toLocaleDateString("es-CL", { weekday: "long", day: "numeric", month: "short" }) +
          " · " + pad(d.getHours()) + ":00–" + pad((d.getHours() + 1) % 24) + ":00", "");
      };
      make("Ahora", function () { setSlot(new Date()); });
      make("En 1 hora", function () { setSlot(new Date(Date.now() + 3600e3)); });
      make("En 2 horas", function () { setSlot(new Date(Date.now() + 7200e3)); });
      make("Mañana 09:00", function () { var d = new Date(); d.setDate(d.getDate() + 1); d.setHours(9, 0, 0, 0); setSlot(d); });
      fields.appendChild(quick);
    }

    /* confirmación de reserva */
    var status = q("#bookingStatus"), shownAt = 0;
    var showOk = function (space) {
      var date = (q("#bookingDate") || {}).value, time = (q("#bookingTime") || {}).value || "00:00";
      var h = Number(time.slice(0, 2));
      var when = "";
      try {
        when = new Date(date + "T00:00:00").toLocaleDateString("es-CL", { weekday: "short", day: "numeric", month: "short" }) +
          " · " + pad(h) + ":00–" + pad((h + 1) % 24) + ":00";
      } catch (e) {}
      var o = el("div", "pn-ok");
      o.setAttribute("role", "dialog"); o.setAttribute("aria-modal", "true"); o.setAttribute("aria-label", "Reserva confirmada");
      var card = el("div", "pn-ok-card",
        '<svg class="pn-ok-check" viewBox="0 0 52 52" aria-hidden="true"><circle class="c" cx="26" cy="26" r="24"/><path class="t" d="M14.5 27.5l8 8 15-17"/></svg>');
      var h3 = el("h3"); h3.textContent = "¡Reserva confirmada!";
      var m = el("p", "pn-ok-main"); m.textContent = space ? "Espacio " + space : "Tu espacio";
      var s = el("p", "pn-ok-sub"); s.textContent = when;
      var hint = el("p", "pn-ok-hint"); hint.textContent = "Muestra el QR de tu reserva (en “Mis reservas”) al llegar para abrir la barrera.";
      var act = el("div", "pn-ok-actions");
      var b1 = el("button", "main"); b1.type = "button"; b1.textContent = "Ver mis reservas";
      var b2 = el("button"); b2.type = "button"; b2.textContent = "Cerrar";
      act.appendChild(b1); act.appendChild(b2);
      [h3, m, s, hint, act].forEach(function (n) { card.appendChild(n); });
      o.appendChild(card);
      body.appendChild(o);
      var close = function () {
        if (!o.parentNode) return;
        o.classList.add("out");
        setTimeout(function () { if (o.parentNode) o.parentNode.removeChild(o); }, 320);
      };
      b2.addEventListener("click", close);
      o.addEventListener("click", function (ev) { if (ev.target === o) close(); });
      b1.addEventListener("click", function () {
        close();
        var t = document.getElementById("myReservationsTitle");
        if (t && t.scrollIntoView) t.scrollIntoView({ behavior: reduce ? "auto" : "smooth", block: "center" });
      });
      document.addEventListener("keydown", function onKey(ev) {
        if (ev.key === "Escape") { close(); document.removeEventListener("keydown", onKey); }
      });
      b1.focus();
      confetti();
      setTimeout(close, 9000);
    };
    if (status && window.MutationObserver) {
      new MutationObserver(function () {
        var t = status.textContent || "";
        if (!status.classList.contains("is-success") || !t) return;
        var now = Date.now();
        if (now - shownAt < 1500) return;
        if (/^Reserva confirmada/i.test(t)) {
          shownAt = now;
          var mm = t.match(/para\s+([A-Z]\d)/);
          safe(function () { showOk(mm ? mm[1] : ""); });
        } else if (/cancelada/i.test(t)) {
          shownAt = now;
          toast("Reserva cancelada", "El espacio volvió a quedar disponible.", "amber");
        }
      }).observe(status, { childList: true, characterData: true, attributes: true, subtree: true });
    }
  }

  /* =================================================================
     ADMIN
     ================================================================= */
  function initAdmin() {
    var pillBox = q("#pnHeadPill"), pillTxt = pillBox ? q("span", pillBox) : null;
    var mq = q("#stMqtt");
    var paintPill = function () {
      if (!pillBox || !mq) return;
      var c = mq.className.indexOf("ok") > -1 ? "ok" : mq.className.indexOf("bad") > -1 ? "bad" : "warn";
      pillBox.className = "pn-headpill " + c;
      pillTxt.textContent = c === "ok" ? "Hardware conectado" : c === "bad" ? "Placa sin señal" : "Conectando hardware…";
    };
    if (mq && window.MutationObserver) {
      new MutationObserver(paintPill).observe(mq, { attributes: true, attributeFilter: ["class"], childList: true, characterData: true, subtree: true });
      watchText(mq, function (t, prev) {
        if (/conectado ✅/.test(t) && !/conectado ✅/.test(prev)) toast("Placa conectada", "Recibiendo datos del Arduino por MQTT.", "");
        else if (/sin señal/i.test(t) && !/sin señal/i.test(prev)) toast("Placa sin señal", "Revisa la alimentación USB y el WiFi.", "coral", 6000);
      });
      paintPill();
    }

    /* chips de plazas: estado + aviso al cambiar */
    ["p1", "p2", "p3"].forEach(function (id, i) {
      var c = document.getElementById(id);
      if (!c) return;
      var read = function () {
        var t = c.textContent;
        return /OCUPADA/.test(t) ? "occ" : /LIBRE/.test(t) ? "libre" : "none";
      };
      var prev = read();
      c.setAttribute("data-pn", prev);
      watchText(c, function () {
        var now = read();
        c.setAttribute("data-pn", now);
        bump(c);
        if (prev !== "none" && now !== "none" && now !== prev) {
          toast("Plaza A" + (i + 1) + (now === "occ" ? " ocupada" : " libre"),
            now === "occ" ? "Se detectó un auto en la plaza." : "El espacio quedó disponible.", now === "occ" ? "coral" : "");
        }
        prev = now;
      });
    });
    var barr = q("#barrChip");
    watchText(barr, function (t, p) {
      bump(barr);
      var a = /ABIERTA|ABRIENDO/i.test(t), b = /ABIERTA|ABRIENDO/i.test(p);
      if (p.indexOf("–") === -1 && a !== b) toast(a ? "Barrera abierta" : "Barrera cerrada", "Estado reportado por la placa.", a ? "amber" : "");
    });
    watchText(q("#libres"), function () { bump(q("#libres")); });

    /* avisos al validar QR */
    var qr = q("#qrResultado");
    watchText(qr, function (t) {
      if (/VÁLIDA/i.test(t)) toast("Reserva válida", "Puedes abrir la barrera.", "");
      else if (t.trim()) toast("QR no válido", t.slice(0, 90), "coral", 5200);
    });
  }

  function start() {
    if (isIndex) safe(initIndex);
    if (isAdmin) safe(initAdmin);
    window.PN = { toast: toast, confetti: confetti };
  }
  if (document.readyState === "loading") document.addEventListener("DOMContentLoaded", start);
  else start();
}());
