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

    if (status.readiness) {
      if (status.readiness === "READY") {
        readinessBanner.hidden = true;
      } else {
        readinessBanner.hidden = false;
        readinessBanner.textContent =
          status.readiness === "NOT_CALIBRATED"
            ? "Not calibrated yet -- open the Calibration tab to get started."
            : "Calibration incomplete -- some accuracy may be missing.";
      }
    }
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
