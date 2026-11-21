#!/bin/bash
# Build dependencies for the Linux / ChromeOS (Crostini, Debian 12) build.
set -euo pipefail
apt-get update
DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
  build-essential cmake git ca-certificates pkg-config dpkg-dev file \
  libasound2-dev libfreetype-dev libfontconfig1-dev libx11-dev libxrandr-dev \
  libxinerama-dev libxcursor-dev libxext-dev libxcomposite-dev \
  xvfb xauth
