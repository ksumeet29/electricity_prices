#!/usr/bin/env python3
"""Flask web frontend for the elpris electricity analyzer executable."""

import json
import os
import subprocess

from flask import Flask, render_template, request

app = Flask(__name__)

# Path to the compiled elpris binary inside the container.
EXECUTABLE = os.environ.get("ELPRIS_BIN", "/app/bin/elpris")

AREAS = ["SE1", "SE2", "SE3", "SE4"]
CYCLES = [
    {"value": 1, "label": "Cotton/Heavy (3.5h)"},
    {"value": 2, "label": "Synthetic (2.0h)"},
    {"value": 3, "label": "Quick wash (1.0h)"},
]


@app.route("/", methods=["GET"])
def index():
    return render_template("index.html", areas=AREAS, cycles=CYCLES)


@app.route("/analyze", methods=["POST"])
def analyze():
    area = request.form.get("area", "SE3")
    cycle = request.form.get("cycle", "1")

    if area not in AREAS:
        return render_template(
            "index.html", areas=AREAS, cycles=CYCLES,
            error=f"Invalid area '{area}'. Choose one of: SE1, SE2, SE3, SE4.",
        ), 400

    if cycle not in {"1", "2", "3"}:
        return render_template(
            "index.html", areas=AREAS, cycles=CYCLES,
            error="Invalid washing programme selected.",
        ), 400

    try:
        proc = subprocess.run(
            [EXECUTABLE, "--area", area, "--cycle", cycle, "--json"],
            capture_output=True,
            text=True,
            timeout=30,
            check=True,
        )
        data = json.loads(proc.stdout)
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
