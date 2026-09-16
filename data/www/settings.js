(function () {
  "use strict";

  var currentSsidEl = document.getElementById("settings-current-ssid");
  var inputEl = document.getElementById("settings-ssid-input");
  var errorEl = document.getElementById("settings-ssid-error");
  var warningEl = document.getElementById("settings-warning");
  var saveBtn = document.getElementById("settings-save-btn");
  var resultEl = document.getElementById("settings-result");
  var networkResetBtn = document.getElementById("settings-network-reset-btn");

  function validateSsidClientSide(ssid) {
    if (ssid.length < 1 || ssid.length > 32) {
      return "SSID must be 1-32 characters.";
    }
    if (ssid !== ssid.trim()) {
      return "SSID must not have leading or trailing spaces.";
    }
    return null;
  }

  function refreshCurrentSsid() {
    fetch("/api/settings")
      .then(function (r) {
        return r.json();
      })
      .then(function (data) {
        currentSsidEl.textContent = data.ssid || "--";
      })
      .catch(function () {});
  }

  saveBtn.addEventListener("click", function () {
    var newSsid = inputEl.value;
    var clientError = validateSsidClientSide(newSsid);
    errorEl.hidden = !clientError;
    if (clientError) {
      errorEl.textContent = clientError;
      return;
    }

    if (!window.confirm(
      "Changing the network name will disconnect this device from \"" + newSsid + "\" briefly. Continue?"
    )) {
      return;
    }

    fetch("/api/settings", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ ssid: newSsid }),
    })
      .then(function (r) {
        return r.json().then(function (data) {
          return { ok: r.ok, data: data };
        });
      })
      .then(function (result) {
        if (!result.ok) {
          resultEl.className = "cal-result cal-result--rejected";
          resultEl.textContent = (result.data.error && result.data.error.message) || "Save failed.";
          resultEl.hidden = false;
          return;
        }
        warningEl.hidden = false;
        warningEl.textContent =
          "Network name changing to \"" + result.data.ssid + "\" in " + result.data.applies_in_s +
          "s -- this device will drop your connection briefly. Reconnect to the new name afterward.";
        resultEl.className = "cal-result cal-result--saved";
        resultEl.textContent = "Saved.";
        resultEl.hidden = false;
        refreshCurrentSsid();
      })
      .catch(function () {
        resultEl.className = "cal-result cal-result--rejected";
        resultEl.textContent = "Network error -- try again.";
        resultEl.hidden = false;
      });
  });

  networkResetBtn.addEventListener("click", function () {
    if (!window.confirm("Reset the network name to the factory default? This restarts the Wi-Fi access point immediately.")) {
      return;
    }
    fetch("/api/network/reset", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ confirm: true }),
    })
      .then(function (r) {
        return r.json();
      })
      .then(function (data) {
        resultEl.className = "cal-result cal-result--saved";
        resultEl.textContent = "Network name reset to \"" + (data.ssid || "default") + "\".";
        resultEl.hidden = false;
        refreshCurrentSsid();
      })
      .catch(function () {});
  });

  // Settings changes made from another client show up here too (spec Edge
  // Cases: two clients open at once).
  window.CompassBus.on("settings_changed", function (msg) {
    currentSsidEl.textContent = msg.ssid;
  });
  window.CompassBus.on("status", function (status) {
    if (status.settings && status.settings.ssid) {
      currentSsidEl.textContent = status.settings.ssid;
    }
  });

  refreshCurrentSsid();
})();
