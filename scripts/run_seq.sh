#!/usr/bin/env bash
set -euo pipefail
./build/bin/screensaver_seq -w "${1:-800}" -h "${2:-600}" -n "${3:-5}" --palette "${4:-nebula}" --zspeed "${5:-0.15}"