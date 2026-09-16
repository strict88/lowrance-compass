// Shared WebSocket message bus: calibration.js/settings.js subscribe here
// instead of opening their own /ws connection.
window.CompassBus = (function () {
  "use strict";
  var listeners = {};
  return {
    on: function (type, fn) {
      (listeners[type] = listeners[type] || []).push(fn);
    },
    _dispatch: function (msg) {
      (listeners[msg.type] || []).forEach(function (fn) {
        fn(msg);
      });
    },
  };
})();

(function () {
  "use strict";

  var tabs = document.querySelectorAll(".tab");
  var views = document.querySelectorAll(".view");

  tabs.forEach(function (tab) {
    tab.addEventListener("click", function () {
      var target = tab.getAttribute("data-view");
      tabs.forEach(function (t) {
        t.classList.toggle("tab--active", t === tab);
      });
      views.forEach(function (v) {
        v.hidden = v.getAttribute("data-view") !== target;
      });
    });
  });

  var connIndicator = document.getElementById("conn-indicator");
  var headingValue = document.getElementById("heading-value");
  var invalidReason = document.getElementById("heading-invalid-reason");
  var pitchValue = document.getElementById("pitch-value");
  var rollValue = document.getElementById("roll-value");
  var rotValue = document.getElementById("rot-value");
  var n2kStateValue = document.getElementById("n2k-state-value");
  var n2kAddressValue = document.getElementById("n2k-address-value");
  var readinessBanner = document.getElementById("readiness-banner");

  function fmtDeg(value, decimals) {
    if (typeof value !== "number" || isNaN(value)) {
      return "--";
    }
    return value.toFixed(decimals === undefined ? 1 : decimals);
  }

  function renderStatus(status) {
    if (status.heading) {
      if (status.heading.valid) {
        headingValue.textContent = fmtDeg(status.heading.heading_deg, 0);
        invalidReason.hidden = true;
      } else {
        headingValue.textContent = "--";
        invalidReason.textContent = status.heading.reason_if_invalid || "Heading unavailable";
        invalidReason.hidden = false;
      }
      pitchValue.textContent = fmtDeg(status.heading.pitch_deg) + "°";
      rollValue.textContent = fmtDeg(status.heading.roll_deg) + "°";
      rotValue.textContent = fmtDeg(status.heading.rate_of_turn_deg_s) + "°/s";
    }

    if (status.n2k) {
      n2kStateValue.textContent = status.n2k.bus_state || "--";
      n2kAddressValue.textContent =
        typeof status.n2k.address === "number" ? String(status.n2k.address) : "--";
    }

    renderReadinessBanner(status);
  }

  // FR-003 (first-boot welcome) / FR-004 (explain what's lost by skipping a
  // stage): the banner text is specific to *why* readiness isn't READY, not
  // just a generic warning.
  function renderReadinessBanner(status) {
    if (!status.readiness) return;

    if (status.readiness === "READY") {
      readinessBanner.hidden = true;
      return;
    }

    readinessBanner.hidden = false;
    var stages = status.stages || {};
    var stageADone = stages.a && stages.a.state === "DONE";

    if (!stageADone) {
      readinessBanner.textContent =
        status.heading && !status.heading.valid && status.heading.reason_if_invalid === "SENSOR_ACCURACY_LOW"
          ? "Calibration in progress -- heading is withheld until sensor accuracy reaches High on all three sensors."
          : "Welcome! This compass hasn't been calibrated yet. Open the Calibration tab to complete Stage A " +
            "(bench sensor calibration) before it can transmit a trustworthy heading.";
      return;
    }

    if (status.heading && !status.heading.valid) {
      readinessBanner.textContent =
        "Heading is temporarily withheld (" + (status.heading.reason_if_invalid || "unknown reason") +
        ") even though Stage A was previously completed.";
      return;
    }

    var missing = [];
    if (!(stages.b && stages.b.state === "DONE")) missing.push("installation alignment (Stage B)");
    if (!(stages.c && stages.c.state === "DONE")) missing.push("compass swing (Stage C)");
    readinessBanner.textContent =
      "Usable, but incomplete: heading is transmitting, but without " + missing.join(" and ") +
      ", it will not account for mounting offset or the boat's own magnetic deviation.";
  }

  var socket = null;
  var reconnectDelayMs = 1000;
  var maxReconnectDelayMs = 10000;

  function setConnected(connected) {
    connIndicator.classList.toggle("conn-indicator--up", connected);
    connIndicator.classList.toggle("conn-indicator--down", !connected);
    connIndicator.title = connected ? "Connected" : "Connecting...";
  }

  function connect() {
    var proto = window.location.protocol === "https:" ? "wss:" : "ws:";
    socket = new WebSocket(proto + "//" + window.location.host + "/ws");

    socket.onopen = function () {
      setConnected(true);
      reconnectDelayMs = 1000;
    };

    socket.onclose = function () {
      setConnected(false);
      setTimeout(connect, reconnectDelayMs);
      reconnectDelayMs = Math.min(reconnectDelayMs * 2, maxReconnectDelayMs);
    };

    socket.onerror = function () {
      socket.close();
    };

    socket.onmessage = function (event) {
      var msg;
      try {
        msg = JSON.parse(event.data);
      } catch (e) {
        return;
      }
      if (msg.type === "status") {
        renderStatus(msg);
      }
      window.CompassBus._dispatch(msg);
    };
  }

  // Poll GET /api/status once immediately so the Dashboard isn't blank while
  // the WebSocket connects.
  fetch("/api/status")
    .then(function (r) {
      return r.json();
    })
    .then(renderStatus)
    .catch(function () {});

  connect();
})();
