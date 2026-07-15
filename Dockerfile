# syntax=docker/dockerfile:1

# ---------- Stage 1: build the elpris executable with Bazel ----------
FROM debian:bookworm-slim AS builder

RUN apt-get update && apt-get install -y --no-install-recommends \
        curl ca-certificates git build-essential libcurl4-openssl-dev \
        python3 \
    && rm -rf /var/lib/apt/lists/*

# Install bazelisk (reads .bazelversion and fetches the matching Bazel release).
RUN curl -fsSL -o /usr/local/bin/bazel \
        https://github.com/bazelbuild/bazelisk/releases/latest/download/bazelisk-linux-amd64 \
    && chmod +x /usr/local/bin/bazel

WORKDIR /src
COPY MODULE.bazel MODULE.bazel.lock BUILD.bazel .bazelversion ./
COPY include ./include
COPY src ./src

RUN bazel build --curses=no //:elpris \
    && mkdir -p /out \
    && cp bazel-bin/elpris /out/elpris

# ---------- Stage 2: slim runtime image with Flask frontend ----------
FROM python:3.12-slim

RUN apt-get update && apt-get install -y --no-install-recommends \
        libcurl4 ca-certificates \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY --from=builder /out/elpris /app/bin/elpris
RUN chmod +x /app/bin/elpris

COPY webapp/requirements.txt ./requirements.txt
RUN pip install --no-cache-dir -r requirements.txt

COPY webapp/ ./

ENV ELPRIS_BIN=/app/bin/elpris
ENV PORT=8080
EXPOSE 8080

CMD ["gunicorn", "--bind", "0.0.0.0:8080", "--workers", "2", "--timeout", "60", "app:app"]
