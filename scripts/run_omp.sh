#!/usr/bin/env bash
set -euo pipefail
export OMP_NUM_THREADS="${OMP_NUM_THREADS:-4}"
./build/bin/screensaver_omp -w "${1:-800}" -h "${2:-600}" -n "${3:-6}" --palette "${4:-nebula}" --zspeed "${5:-0.2}"