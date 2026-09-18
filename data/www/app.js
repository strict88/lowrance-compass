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
      var reason = status.heading.reason_if_invalid;
      // The degree value is shown whenever the sensor is connected at all
      // (status.heading.valid) -- low accuracy no longer blanks it out, it
      // only adds the warning below, so the user always has a heading to
      // steer by even before/while calibration reaches full accuracy.
      if (status.heading.valid) {
        headingValue.textContent = fmtDeg(status.heading.heading_deg, 0);
      } else {
        headingValue.textContent = "--";
      }
      if (!status.heading.valid) {
        invalidReason.textContent = reason || "Heading unavailable";
        invalidReason.hidden = false;
      } else if (reason === "SENSOR_ACCURACY_LOW" || reason === "SENSOR_NOT_CALIBRATED") {
        invalidReason.textContent = "Low sensor accuracy -- heading shown may not be fully reliable.";
        invalidReason.hidden = false;
      } else {
        invalidReason.hidden = true;
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
    var reason = status.heading && status.heading.reason_if_invalid;

    if (!stageADone) {
      readinessBanner.textContent =
        reason === "SENSOR_ACCURACY_LOW"
          ? "Calibration in progress -- heading shown is a low-confidence estimate until sensor accuracy " +
            "reaches High on all three sensors."
          : "Welcome! This compass hasn't been calibrated yet. Open the Calibration tab to complete Stage A " +
            "(bench sensor calibration) before it can transmit a trustworthy heading.";
      return;
    }

    if (reason) {
      readinessBanner.textContent =
        reason === "SENSOR_DISCONNECTED"
          ? "Heading is temporarily unavailable (sensor disconnected)."
          : "Sensor accuracy is currently low (" + reason + ") even though Stage A was previously completed -- " +
            "the heading shown may not be fully reliable.";
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

  // A full HTTP status fetch, applied the same way a WebSocket "status"
  // message would be (both the Dashboard's own render and calibration.js's/
  // settings.js's CompassBus listeners) -- used both for the initial load
  // and as a periodic safety net below.
  function pollStatusOnce() {
    return fetch("/api/status")
      .then(function (r) {
        return r.json();
      })
      .then(function (status) {
        status.type = "status";
        renderStatus(status);
        window.CompassBus._dispatch(status);
      })
      .catch(function () {});
  }

  pollStatusOnce();

  // Safety net: a WebSocket "status" push should always keep the UI current,
  // but if one is ever missed for any reason, this guarantees the page
  // self-corrects within a couple of seconds instead of needing a manual
  // reload. Runs continuously (cheap, idempotent), not just during a
  // calibration session, so it also catches a WS message missed at any
  // other time.
  setInterval(pollStatusOnce, 2000);

  connect();
})();
