(function () {
  "use strict";

  var whatWhyEl = document.getElementById("stage-a-what-why");
  var beforeListEl = document.getElementById("stage-a-before-list");
  var stepsEl = document.getElementById("stage-a-steps");
  var durationEl = document.getElementById("stage-a-duration");
  var guideEl = document.getElementById("stage-a-guide");
  var progressEl = document.getElementById("stage-a-progress");
  var resultEl = document.getElementById("stage-a-result");
  var statusPillEl = document.getElementById("stage-a-status-pill");
  var startBtn = document.getElementById("stage-a-start-btn");
  var cancelBtn = document.getElementById("stage-a-cancel-btn");
  var recalBtn = document.getElementById("stage-a-recalibrate-btn");
  var qualityMagEl = document.getElementById("quality-mag");
  var qualityAccelEl = document.getElementById("quality-accel");
  var qualityGyroEl = document.getElementById("quality-gyro");
  var positionsRowEl = document.getElementById("positions-row");
  var coverageFillEl = document.getElementById("coverage-bar-fill");

  var kPositionLabels = {
    POS_X: "+X",
    NEG_X: "-X",
    POS_Y: "+Y",
    NEG_Y: "-Y",
    POS_Z: "+Z",
    NEG_Z: "-Z",
  };

  function renderQualityDots(container, level) {
    container.innerHTML = "";
    for (var i = 0; i < 3; i++) {
      var dot = document.createElement("span");
      dot.className = "quality-dot" + (i < level ? " quality-dot--filled" : "");
      container.appendChild(dot);
    }
  }

  function renderPositions(doneList) {
    positionsRowEl.innerHTML = "";
    var doneSet = {};
    (doneList || []).forEach(function (p) {
      doneSet[p] = true;
    });
    Object.keys(kPositionLabels).forEach(function (key) {
      var chip = document.createElement("div");
      chip.className = "position-chip" + (doneSet[key] ? " position-chip--done" : "");
      chip.textContent = kPositionLabels[key];
      positionsRowEl.appendChild(chip);
    });
  }

  function renderGuide(guide) {
    var a = guide.stages && guide.stages.a;
    if (!a) return;

    whatWhyEl.textContent = a.what_and_why || "";

    beforeListEl.innerHTML = "";
    (a.before_you_start || []).forEach(function (item) {
      var li = document.createElement("li");
      li.textContent = item;
      beforeListEl.appendChild(li);
    });

    stepsEl.innerHTML = "";
    (a.steps || []).forEach(function (step, index) {
      var li = document.createElement("li");
      li.dataset.stepIndex = String(index);

      if (step.illustration_svg_id) {
        var svgNs = "http://www.w3.org/2000/svg";
        var svg = document.createElementNS(svgNs, "svg");
        svg.setAttribute("class", "cal-step__illustration");
        svg.setAttribute("viewBox", "0 0 120 120");
        var use = document.createElementNS(svgNs, "use");
        use.setAttributeNS("http://www.w3.org/1999/xlink", "href",
          "illustrations/stage_a.svg#" + step.illustration_svg_id);
        use.setAttribute("href", "illustrations/stage_a.svg#" + step.illustration_svg_id);
        svg.appendChild(use);
        li.appendChild(svg);
      }

      var title = document.createElement("strong");
      title.textContent = step.title || "";
      li.appendChild(title);

      var detail = document.createElement("div");
      detail.textContent = step.detail || "";
      li.appendChild(detail);

      stepsEl.appendChild(li);
    });

    durationEl.textContent = a.duration_estimate ? "Estimated time: " + a.duration_estimate : "";
  }

  function setStatusPill(text, variant) {
    statusPillEl.textContent = text;
    statusPillEl.className = "status-pill status-pill--" + variant;
  }

  function render(status) {
    var stageA = status.stages && status.stages.a;
    var session = status.session || {};
    var active = session.active_stage === "A";

    guideEl.hidden = active;
    progressEl.hidden = !active;
    startBtn.hidden = active || (stageA && stageA.state === "DONE");
    recalBtn.hidden = active || !(stageA && stageA.state === "DONE");
    cancelBtn.hidden = !active;

    if (active) {
      setStatusPill("In progress", "in-progress");
      var progress = session.progress || {};
      renderQualityDots(qualityMagEl, progress.mag_acc || 0);
      renderQualityDots(qualityAccelEl, progress.accel_acc || 0);
      renderQualityDots(qualityGyroEl, progress.gyro_acc || 0);
      renderPositions(progress.positions_done);
      coverageFillEl.style.width = (progress.rotation_coverage_pct || 0) + "%";
    } else if (stageA && stageA.state === "DONE") {
      setStatusPill("Done", "done");
    } else {
      setStatusPill("Not done", "not-done");
    }
  }

  function showResult(msg) {
    if (msg.stage !== "A") return;
    var text;
    switch (msg.outcome) {
      case "SAVED":
        text = "Saved! Stage A calibration completed successfully.";
        break;
      case "TIMED_OUT":
        text = "Timed out before reaching full accuracy. " + (msg.reason || "") + " The previous calibration (if any) is unchanged.";
        break;
      case "REJECTED":
        text = "Calibration was not saved. " + (msg.reason || "");
        break;
      case "CANCELLED":
        text = "Cancelled. The previous calibration (if any) is unchanged.";
        break;
      default:
        text = "";
    }
    resultEl.textContent = text;
    resultEl.className = "cal-result cal-result--" + msg.outcome.toLowerCase();
    resultEl.hidden = false;
    setTimeout(function () {
      resultEl.hidden = true;
    }, 8000);
  }

  function postJson(path, body) {
    return fetch(path, {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify(body || {}),
    }).then(function (r) {
      return r.json().then(function (data) {
        return { ok: r.ok, data: data };
      });
    });
  }

  startBtn.addEventListener("click", function () {
    postJson("/api/calibration/a/start").catch(function () {});
  });
  cancelBtn.addEventListener("click", function () {
    postJson("/api/calibration/a/cancel").catch(function () {});
  });
  recalBtn.addEventListener("click", function () {
    postJson("/api/calibration/a/start").catch(function () {});
  });

  window.CompassBus.on("status", render);
  window.CompassBus.on("calibration_result", showResult);

  fetch("/api/guide")
    .then(function (r) {
      return r.json();
    })
    .then(renderGuide)
    .catch(function () {});

  fetch("/api/status")
    .then(function (r) {
      return r.json();
    })
    .then(render)
    .catch(function () {});
})();
