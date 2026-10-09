#!/usr/bin/env bash
# Builds Cimmerian's X11 backend in a Debian container and runs
# x11_capture_stability under Xvfb, once on its own and once with xcompmgr
# compositing. For checking the X11 backend from a machine without X11, such
# as macOS. Xvfb renders in software with no vsync, so this measures the
# polling itself, not a real compositor or GPU race.
#
# Usage: tools/x11-capture-stability/run-in-docker.sh [--trials N] [--size PIXELS]
set -euo pipefail

repoRoot=$(cd "$(dirname "$0")/../.." && pwd)
image=cimmerian-x11-check

docker build --quiet --tag "$image" "$repoRoot/tools/x11-capture-stability" >/dev/null

# The checkout is mounted read-only and copied, so nothing is written back.
docker run --rm --volume "$repoRoot":/src:ro "$image" bash -c '
  set -euo pipefail
  mkdir /work
  tar -C /src --exclude=./build --exclude=./compile_commands.json -cf - . | tar -xf - -C /work
  # Quiet unless it fails: the vendored stb headers warn on every build.
  { cmake -S /work -B /build -DCMAKE_BUILD_TYPE=Release -DCIMMERIAN_ENABLE_VISUAL_TESTING=ON \
      && cmake --build /build --target x11_capture_stability -j"$(nproc)"; } >/build.log 2>&1 \
    || { cat /build.log; exit 1; }

  Xvfb :99 -screen 0 1280x1024x24 >/dev/null 2>&1 &
  export DISPLAY=:99
  sleep 1
  echo "=== Xvfb"
  /build/x11_capture_stability "$@"

  xcompmgr -a >/dev/null 2>&1 &
  sleep 1
  echo
  echo "=== Xvfb + xcompmgr"
  /build/x11_capture_stability "$@"
' run-in-docker "$@"
