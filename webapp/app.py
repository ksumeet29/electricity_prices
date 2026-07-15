#!/usr/bin/env python3
"""Flask web frontend for the elpris electricity analyzer executable."""

import json
import os
import subprocess
from datetime import datetime, timedelta

from flask import Flask, render_template, request

app = Flask(__name__)

# Path to the compiled elpris binary inside the container.
EXECUTABLE = os.environ.get("ELPRIS_BIN", "/app/bin/elpris")

AREAS = [
    {"value": "SE1", "label": "SE1 - Luleå/Norra Sverige"},
    {"value": "SE2", "label": "SE2 - Sundsvall/Norra Mellansverige"},
    {"value": "SE3", "label": "SE3 - Stockholm/Södra Mellansverige"},
    {"value": "SE4", "label": "SE4 - Malmö/Södra Sverige"},
]
AREA_VALUES = [area["value"] for area in AREAS]
CYCLES = [
    {"value": 1, "label": "Cotton/Heavy (3.5h)"},
    {"value": 2, "label": "Synthetic (2.0h)"},
    {"value": 3, "label": "Quick wash (1.0h)"},
]


def format_slot_window(start_time: str, hours: float) -> str:
    try:
        start_dt = datetime.fromisoformat(start_time.replace("Z", "+00:00"))
        end_dt = start_dt + timedelta(hours=hours)
        return f"{start_dt.strftime('%Y-%m-%d %H:%M')} - {end_dt.strftime('%H:%M')}"
    except (TypeError, ValueError):
        return start_time or ""


def build_chart_series(intervals: list[dict], start_time: str, hours: float) -> dict:
    labels = []
    values = []
    selected = []

    for item in intervals:
        time_value = item.get("time", "")
        if not time_value:
            continue
        labels.append(time_value)
        values.append(float(item.get("price", 0) or 0))

    try:
        start_dt = datetime.fromisoformat(start_time.replace("Z", "+00:00"))
        end_dt = start_dt + timedelta(hours=hours)
        selected_range = [
            (idx, item.get("time", ""))
            for idx, item in enumerate(intervals)
            if item.get("time")
            and start_dt <= datetime.fromisoformat(item["time"].replace("Z", "+00:00")) < end_dt
        ]
    except (TypeError, ValueError):
        selected_range = []

    for idx, _ in selected_range:
        selected.append(idx)

    return {"labels": labels, "values": values, "selected": selected}


@app.route("/", methods=["GET"])
def index():
    return render_template("index.html", areas=AREAS, cycles=CYCLES)


@app.route("/analyze", methods=["POST"])
def analyze():
    area = request.form.get("area", "SE3")
    cycle = request.form.get("cycle", "1")

    if area not in AREA_VALUES:
        return render_template(
            "index.html", areas=AREAS, cycles=CYCLES,
            error=f"Invalid area '{area}'. Choose one of: SE1, SE2, SE3, SE4.",
        ), 400

    if cycle not in {"1", "2", "3"}:
        return render_template(
            "index.html", areas=AREAS, cycles=CYCLES,
            error="Invalid washing programme selected.",
        ), 400

    if not os.path.exists(EXECUTABLE) or not os.access(EXECUTABLE, os.X_OK):
        return render_template(
            "index.html", areas=AREAS, cycles=CYCLES,
            error=f"Analyzer binary is missing or not executable: {EXECUTABLE}",
        ), 500

    try:
        proc = subprocess.run(
            [EXECUTABLE, "--area", area, "--cycle", cycle, "--json"],
            capture_output=True,
            text=True,
            timeout=30,
            check=True,
        )
        data = json.loads(proc.stdout)
        data["slot_window"] = format_slot_window(data.get("start", ""), data.get("hours", 0))
        data["chart"] = build_chart_series(data.get("intervals", []), data.get("start", ""), data.get("hours", 0))
        return render_template(
            "result.html", areas=AREAS, cycles=CYCLES, data=data,
        )
    except subprocess.CalledProcessError as exc:
        return render_template(
            "index.html", areas=AREAS, cycles=CYCLES,
            error=f"Analyzer failed: {exc.stderr.strip() or exc}",
        ), 500
    except subprocess.TimeoutExpired:
        return render_template(
            "index.html", areas=AREAS, cycles=CYCLES,
            error="Analyzer timed out. Please try again.",
        ), 504
    except (FileNotFoundError, PermissionError, OSError) as exc:
        return render_template(
            "index.html", areas=AREAS, cycles=CYCLES,
            error=f"Analyzer startup failed: {exc}",
        ), 500
    except (json.JSONDecodeError, ValueError):
        return render_template(
            "index.html", areas=AREAS, cycles=CYCLES,
            error="Analyzer returned unexpected output.",
        ), 500


@app.route("/healthz")
def healthz():
    return {"status": "ok"}


if __name__ == "__main__":
    app.run(host="0.0.0.0", port=int(os.environ.get("PORT", 8080)))
