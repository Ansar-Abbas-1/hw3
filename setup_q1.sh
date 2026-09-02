#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PX4="$ROOT/src/PX4-Autopilot"
CUSTOM="$ROOT/q1_custom"

echo "Initializing repository submodules..."
git -C "$ROOT" submodule update --init --recursive

echo "Applying Q1 custom PX4 files..."

cp -a "$CUSTOM/x100" \
"$PX4/Tools/simulation/gz/models/"

cp -a "$CUSTOM/x100_base" \
"$PX4/Tools/simulation/gz/models/"

cp "$CUSTOM/22000_gz_x100" \
"$PX4/ROMFS/px4fmu_common/init.d-posix/airframes/"

cp "$CUSTOM/airframes_CMakeLists.txt" \
"$PX4/ROMFS/px4fmu_common/init.d-posix/airframes/CMakeLists.txt"

cp "$CUSTOM/dds_topics.yaml" \
"$PX4/src/modules/uxrce_dds_client/dds_topics.yaml"

echo "Q1 PX4 overlay applied successfully."
