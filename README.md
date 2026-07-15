# Elpris Electricity Analyzer

Find the cheapest contiguous electricity price window (SE1 / SE2 / SE3 / SE4 price areas)
for a given washing programme, using real Swedish spot prices from
[elprisetjustnu.se](https://www.elprisetjustnu.se/). Available both as a
C++ command-line tool and as a web application.

## How it works

1. A C++ executable (`elpris`) fetches today's and tomorrow's hourly spot
   prices for a given price area via libcurl, parses the JSON response, and
   finds the cheapest contiguous window long enough to cover the chosen
   washing programme's duration.
2. A Flask web app wraps the executable: the user picks an area (SE1/SE2/SE3/SE4)
   and a washing programme in a form, the backend calls `elpris --json` and
   renders the result as a human-readable page.
3. Everything is packaged into a single Docker image (Bazel build stage +
   slim Python runtime stage) for deployment to Render.com.

## Project layout

```
include/           Header files (fetch.hpp, parse.hpp)
src/               C++ source (main.cpp, fetch.cpp, parse.cpp)
BUILD.bazel        Bazel build rules for the elpris binary
MODULE.bazel       Bazel module dependencies (rules_cc)
run.py             Local dev helper: cmake build + run + cleanup
webapp/            Flask frontend (app.py, templates/, static/)
Dockerfile         Multi-stage build: Bazel build -> Flask runtime image
render.yaml         Render.com Blueprint for one-click deployment
```

## Washing programmes

| Cycle | Programme        | Duration |
|-------|------------------|----------|
| 1     | Cotton/Heavy     | 3.5 h    |
| 2     | Synthetic        | 2.0 h    |
| 3     | Quick wash       | 1.0 h    |

## Running the CLI locally

Using Bazel directly:

```bash
bazel build //:elpris
./bazel-bin/elpris --area SE3 --cycle 1          # human-readable output
./bazel-bin/elpris --area SE4 --cycle 2 --json   # machine-readable JSON
```

CLI flags:
- `--area <SE1|SE2|SE3|SE4>` — price area (default `SE3`)
- `--cycle <1|2|3>` — washing programme (prompts interactively if omitted)
- `--json` — emit machine-readable JSON instead of human-readable text

## Running the web app locally

### With Docker (recommended — matches production)

```bash
docker build -t elpris-web .
docker rm -f elpris-web 2>$null
docker run -d -p 8080:8080 --name elpris-web elpris-web
```

Open http://localhost:8080, pick an area and washing programme, and submit.
Health check: `curl http://localhost:8080/healthz`.

Stop and remove the container when done:

```bash
docker rm -f elpris-web
```

### Without Docker (requires a local `elpris` build)

```bash
bazel build //:elpris
export ELPRIS_BIN=$(pwd)/bazel-bin/elpris
cd webapp
pip install -r requirements.txt
python3 app.py   # serves on http://localhost:8080
```

## Deploying to Render.com (free tier)

1. Push this repository to GitHub.
2. In Render, choose **New → Blueprint** and select the repo — Render reads
   `render.yaml` and provisions a free Docker web service automatically.
   (Alternatively: **New → Web Service → Docker**, pointing at the repo root
   with `Dockerfile` as the build file.)
3. Render builds the image (Bazel build stage compiles `elpris`, runtime
   stage runs the Flask app under gunicorn) and exposes it at a free
   `https://<service-name>.onrender.com` subdomain.
4. The `/healthz` endpoint is used by Render for health checks.

## Continuous Integration

`.github/workflows/ci.yml` runs on every push and pull request:

- **bazel-build** — installs libcurl dev headers, sets up Bazelisk (uses the
  version pinned in `.bazelversion`), builds `//:elpris`, and smoke-tests
  that an invalid washing cycle exits with a clean error code.
- **docker-build** — builds the full multi-stage Docker image, runs the
  container, polls `/healthz` until it responds, and verifies the index
  page renders correctly.

## Architecture notes

- The `elpris` executable supports both interactive (`std::cin`) and
  non-interactive (`--area`, `--cycle`, `--json`) modes, which is what makes
  it safe to invoke from the Flask backend as a subprocess.
- The Docker build never bakes in stale binaries: `build/`, `bazel-*/` are
  excluded via `.dockerignore`, and Bazel is re-run fresh in the builder
  stage on every image build.
