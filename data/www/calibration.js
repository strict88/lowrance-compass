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
  var stageBPillEl = document.getElementById("stage-b-status-pill");
  var stageCPillEl = document.getElementById("stage-c-status-pill");

  var stageBWhatWhyEl = document.getElementById("stage-b-what-why");
  var stageBBeforeListEl = document.getElementById("stage-b-before-list");
  var stageBDurationEl = document.getElementById("stage-b-duration");
  var stageBGuideEl = document.getElementById("stage-b-guide");
  var stageBLevelStepEl = document.getElementById("stage-b-level-step");
  var stageBLevelBtn = document.getElementById("stage-b-level-btn");
  var stageBMethodStepEl = document.getElementById("stage-b-method-step");
  var stageBKnownBearingBtn = document.getElementById("stage-b-known-bearing-btn");
  var stageBGpsCourseBtn = document.getElementById("stage-b-gps-course-btn");
  var stageBBearingEntryEl = document.getElementById("stage-b-bearing-entry");
  var stageBBearingInput = document.getElementById("stage-b-bearing-input");
  var stageBBearingSubmitBtn = document.getElementById("stage-b-bearing-submit-btn");
  var stageBGpsWaitingEl = document.getElementById("stage-b-gps-waiting");
  var stageBWaitingTextEl = document.getElementById("stage-b-waiting-text");
  var stageBPreviewEl = document.getElementById("stage-b-preview");
  var stageBPreviewValueEl = document.getElementById("stage-b-preview-value");
  var stageBAcceptBtn = document.getElementById("stage-b-accept-btn");
  var stageBDiscardBtn = document.getElementById("stage-b-discard-btn");
  var stageBResultEl = document.getElementById("stage-b-result");
  var stageBStartBtn = document.getElementById("stage-b-start-btn");
  var stageBCancelBtn = document.getElementById("stage-b-cancel-btn");
  var stageBRecalBtn = document.getElementById("stage-b-recalibrate-btn");

  var stageCWhatWhyEl = document.getElementById("stage-c-what-why");
  var stageCBeforeListEl = document.getElementById("stage-c-before-list");
  var stageCDurationEl = document.getElementById("stage-c-duration");
  var stageCGuideEl = document.getElementById("stage-c-guide");
  var stageCStartGpsBtn = document.getElementById("stage-c-start-gps-btn");
  var stageCStartManualBtn = document.getElementById("stage-c-start-manual-btn");
  var stageCSwingProgressEl = document.getElementById("stage-c-swing-progress");
  var stageCCoverageFillEl = document.getElementById("stage-c-coverage-fill");
  var stageCTurnsValueEl = document.getElementById("stage-c-turns-value");
  var stageCTooFastEl = document.getElementById("stage-c-too-fast");
  var stageCManualEntryEl = document.getElementById("stage-c-manual-entry");
  var stageCManualPointIndexEl = document.getElementById("stage-c-manual-point-index");
  var stageCManualBearingInput = document.getElementById("stage-c-manual-bearing-input");
  var stageCManualSubmitBtn = document.getElementById("stage-c-manual-submit-btn");
  var stageCResultReadyEl = document.getElementById("stage-c-result-ready");
  var stageCMaxDeviationValueEl = document.getElementById("stage-c-max-deviation-value");
  var stageCResidualValueEl = document.getElementById("stage-c-residual-value");
  var stageCCoefficientsRowEl = document.getElementById("stage-c-coefficients-row");
  var stageCAcceptBtn = document.getElementById("stage-c-accept-btn");
  var stageCDiscardBtn = document.getElementById("stage-c-discard-btn");
  var stageCResultEl = document.getElementById("stage-c-result");
  var stageCCancelBtn = document.getElementById("stage-c-cancel-btn");
  var stageCRecalBtn = document.getElementById("stage-c-recalibrate-btn");

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

    renderStageBGuide(guide.stages && guide.stages.b);
    renderStageCGuide(guide.stages && guide.stages.c);
  }

  function renderStageBGuide(b) {
    if (!b) return;
    stageBWhatWhyEl.textContent = b.what_and_why || "";
    stageBBeforeListEl.innerHTML = "";
    (b.before_you_start || []).forEach(function (item) {
      var li = document.createElement("li");
      li.textContent = item;
      stageBBeforeListEl.appendChild(li);
    });
    stageBDurationEl.textContent = b.duration_estimate ? "Estimated time: " + b.duration_estimate : "";
  }

  function renderStageCGuide(c) {
    if (!c) return;
    stageCWhatWhyEl.textContent = c.what_and_why || "";
    stageCBeforeListEl.innerHTML = "";
    (c.before_you_start || []).forEach(function (item) {
      var li = document.createElement("li");
      li.textContent = item;
      stageCBeforeListEl.appendChild(li);
    });
    stageCDurationEl.textContent = c.duration_estimate ? "Estimated time: " + c.duration_estimate : "";
  }

  function setStatusPill(text, variant) {
    statusPillEl.textContent = text;
    statusPillEl.className = "status-pill status-pill--" + variant;
  }

  function render(status) {
    var stages = status.stages || {};
    var stageA = stages.a;
    var session = status.session || {};
    var activeA = session.active_stage === "A";

    guideEl.hidden = activeA;
    progressEl.hidden = !activeA;
    startBtn.hidden = activeA || (stageA && stageA.state === "DONE");
    recalBtn.hidden = activeA || !(stageA && stageA.state === "DONE");
    cancelBtn.hidden = !activeA;

    if (activeA) {
      setStatusPill("In progress", "in-progress");
      var progress = session.progress || {};
      renderQualityDots(qualityMagEl, progress.mag_acc || 0);
      renderQualityDots(qualityAccelEl, progress.accel_acc || 0);
      renderQualityDots(qualityGyroEl, progress.gyro_acc || 0);
      renderPositions(progress.positions_done);
      coverageFillEl.style.width = (progress.rotation_coverage_pct || 0) + "%";
    } else if (stageA && stageA.state === "DONE") {
      var savedAt = stageA.saved_at ? new Date(stageA.saved_at).toLocaleDateString() : "date unavailable";
      setStatusPill("Done · " + savedAt, "done");
    } else {
      setStatusPill("Not done", "not-done");
    }

    renderStageB(status);
    renderStageC(status);
  }

  function renderStageB(status) {
    var stages = status.stages || {};
    var stageB = stages.b;
    var session = status.session || {};
    var activeB = session.active_stage === "B";
    var progress = session.progress || {};

    stageBGuideEl.hidden = activeB;
    stageBStartBtn.hidden = activeB || (stageB && stageB.state === "DONE");
    stageBRecalBtn.hidden = activeB || !(stageB && stageB.state === "DONE");
    stageBCancelBtn.hidden = !activeB;

    stageBLevelStepEl.hidden = true;
    stageBMethodStepEl.hidden = true;
    stageBBearingEntryEl.hidden = true;
    stageBGpsWaitingEl.hidden = true;
    stageBPreviewEl.hidden = true;

    if (activeB) {
      setPill(stageBPillEl, "In progress", "in-progress");

      if (progress.preview_offset_deg !== null && progress.preview_offset_deg !== undefined) {
        stageBPreviewEl.hidden = false;
        stageBPreviewValueEl.textContent = Number(progress.preview_offset_deg).toFixed(1);
      } else if (progress.awaiting_gps_alignment) {
        stageBGpsWaitingEl.hidden = false;
        if (progress.waiting_reason === "SPEED_TOO_LOW") {
          stageBWaitingTextEl.textContent = "Waiting for boat speed to come up...";
        } else if (progress.waiting_reason === "COURSE_NOT_STEADY") {
          stageBWaitingTextEl.textContent = "Waiting for a steadier course...";
        } else {
          stageBWaitingTextEl.textContent = "Gathering course samples...";
        }
      } else if (progress.awaiting_bearing_entry) {
        stageBBearingEntryEl.hidden = false;
      } else if (progress.awaiting_method_choice) {
        stageBMethodStepEl.hidden = false;
      } else if (!progress.level_set) {
        stageBLevelStepEl.hidden = false;
      }
    } else if (stageB && stageB.state === "DONE") {
      var savedAt = stageB.saved_at ? new Date(stageB.saved_at).toLocaleDateString() : "date unavailable";
      setPill(stageBPillEl, "Done · " + savedAt, "done");
    } else {
      setPill(stageBPillEl, "Not done", "not-done");
    }
  }

  function renderStageC(status) {
    var stages = status.stages || {};
    var stageC = stages.c;
    var session = status.session || {};
    var activeC = session.active_stage === "C";
    var progress = session.progress || {};

    stageCGuideEl.hidden = activeC || (stageC && stageC.state === "DONE");
    stageCRecalBtn.hidden = activeC || !(stageC && stageC.state === "DONE");
    stageCCancelBtn.hidden = !activeC;

    stageCSwingProgressEl.hidden = true;
    stageCManualEntryEl.hidden = true;
    stageCResultReadyEl.hidden = true;

    if (activeC) {
      setPill(stageCPillEl, "In progress", "in-progress");

      var awaitingPoint = progress.manual_point_index !== null && progress.manual_point_index !== undefined;
      var hasCandidate = progress.candidate_max_deviation_deg !== null && progress.candidate_max_deviation_deg !== undefined;

      if (hasCandidate) {
        stageCResultReadyEl.hidden = false;
        stageCMaxDeviationValueEl.textContent = Number(progress.candidate_max_deviation_deg).toFixed(1);
        stageCResidualValueEl.textContent = Number(progress.candidate_residual_rms_deg).toFixed(1);
        var coeffs = progress.candidate_coefficients || [0, 0, 0, 0, 0];
        stageCCoefficientsRowEl.innerHTML = "";
        coeffs.forEach(function (v) {
          var td = document.createElement("td");
          td.textContent = Number(v).toFixed(3);
          stageCCoefficientsRowEl.appendChild(td);
        });
      } else if (awaitingPoint) {
        stageCManualEntryEl.hidden = false;
        stageCManualPointIndexEl.textContent = String(progress.manual_point_index);
      } else {
        stageCSwingProgressEl.hidden = false;
        stageCCoverageFillEl.style.width = (progress.sector_coverage_pct || 0) + "%";
        stageCTurnsValueEl.textContent = Number(progress.turns_completed || 0).toFixed(1);
        stageCTooFastEl.hidden = !progress.turning_too_fast;
      }
    } else if (stageC && stageC.state === "DONE") {
      var savedAt = stageC.saved_at ? new Date(stageC.saved_at).toLocaleDateString() : "date unavailable";
      setPill(stageCPillEl, "Done · " + savedAt, "done");
    } else {
      setPill(stageCPillEl, "Not done", "not-done");
    }
  }

  function setPill(pillEl, text, variant) {
    pillEl.textContent = text;
    pillEl.className = "status-pill status-pill--" + variant;
  }

  function showResult(msg) {
    if (msg.stage !== "A" && msg.stage !== "B" && msg.stage !== "C") return;
    var label = msg.stage === "A" ? "Stage A calibration" : msg.stage === "B" ? "Installation alignment" : "Compass swing";
    var text;
    switch (msg.outcome) {
      case "SAVED":
        text = "Saved! " + label + " completed successfully.";
        break;
      case "TIMED_OUT":
        text = "Timed out before reaching full accuracy. " + (msg.reason || "") + " The previous result (if any) is unchanged.";
        break;
      case "REJECTED":
        text = label + " was not saved. " + (msg.reason || "");
        break;
      case "CANCELLED":
        text = (msg.reason || "Cancelled") + ". The previous result (if any) is unchanged.";
        break;
      default:
        text = "";
    }

    var el = msg.stage === "A" ? resultEl : msg.stage === "B" ? stageBResultEl : stageCResultEl;
    el.textContent = text;
    el.className = "cal-result cal-result--" + msg.outcome.toLowerCase();
    el.hidden = false;
    setTimeout(function () {
      el.hidden = true;
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

  stageBStartBtn.addEventListener("click", function () {
    postJson("/api/calibration/b/start").catch(function () {});
  });
  stageBRecalBtn.addEventListener("click", function () {
    postJson("/api/calibration/b/start").catch(function () {});
  });
  stageBCancelBtn.addEventListener("click", function () {
    postJson("/api/calibration/b/cancel").catch(function () {});
  });
  stageBLevelBtn.addEventListener("click", function () {
    postJson("/api/calibration/b/level").catch(function () {});
  });
  stageBKnownBearingBtn.addEventListener("click", function () {
    stageBMethodStepEl.hidden = true;
    stageBBearingEntryEl.hidden = false;
  });
  stageBGpsCourseBtn.addEventListener("click", function () {
    postJson("/api/calibration/b/gps-course").catch(function () {});
  });
  stageBBearingSubmitBtn.addEventListener("click", function () {
    var bearingDeg = parseFloat(stageBBearingInput.value);
    if (isNaN(bearingDeg)) return;
    postJson("/api/calibration/b/bearing", { bearing_deg: bearingDeg }).catch(function () {});
  });
  stageBAcceptBtn.addEventListener("click", function () {
    postJson("/api/calibration/b/apply").catch(function () {});
  });
  stageBDiscardBtn.addEventListener("click", function () {
    postJson("/api/calibration/b/discard").catch(function () {});
  });

  stageCStartGpsBtn.addEventListener("click", function () {
    postJson("/api/calibration/c/start", { mode: "gps" }).catch(function () {});
  });
  stageCStartManualBtn.addEventListener("click", function () {
    postJson("/api/calibration/c/start", { mode: "manual" }).catch(function () {});
  });
  stageCRecalBtn.addEventListener("click", function () {
    postJson("/api/calibration/c/start", { mode: "gps" }).catch(function () {});
  });
  stageCCancelBtn.addEventListener("click", function () {
    postJson("/api/calibration/c/cancel").catch(function () {});
  });
  stageCManualSubmitBtn.addEventListener("click", function () {
    var bearingDeg = parseFloat(stageCManualBearingInput.value);
    if (isNaN(bearingDeg)) return;
    postJson("/api/calibration/c/manual-point", { reference_heading_deg: bearingDeg }).catch(function () {});
    stageCManualBearingInput.value = "";
  });
  stageCAcceptBtn.addEventListener("click", function () {
    postJson("/api/calibration/c/apply").catch(function () {});
  });
  stageCDiscardBtn.addEventListener("click", function () {
    postJson("/api/calibration/c/discard").catch(function () {});
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
