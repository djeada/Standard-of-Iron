#!/usr/bin/env bash
set -euo pipefail

root_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
map_output_dir="$root_dir/assets/campaign_map"

venv_path="$root_dir/tools/map_pipeline/venv"
requirements_path="$root_dir/tools/map_pipeline/requirements.txt"

python_cmd="${PYTHON:-python3}"
rebuild_maps="${map_pipeline_rebuild:-0}"

for arg in "$@"; do
  if [ "$arg" = "--rebuild" ] || [ "$arg" = "-f" ]; then
    rebuild_maps=1
  fi
done

required_outputs=(
  "$map_output_dir/campaign_base_color.png"
  "$map_output_dir/campaign_water.png"
  "$map_output_dir/coastlines_uv.json"
  "$map_output_dir/rivers_uv.json"
  "$map_output_dir/land_mesh.bin"
  "$map_output_dir/provinces.json"
  "$map_output_dir/hannibal_path.json"
)

missing_outputs=()
if [ -d "$map_output_dir" ]; then
  for output in "${required_outputs[@]}"; do
    if [ ! -s "$output" ]; then
      missing_outputs+=("$output")
    fi
  done
else
  missing_outputs=("${required_outputs[@]}")
fi

if [ "$rebuild_maps" != "1" ] && [ "${#missing_outputs[@]}" -eq 0 ]; then
  echo "Map pipeline output already present; skipping. Use --rebuild to force."
  exit 0
fi

if [ "$rebuild_maps" != "1" ] && [ "${#missing_outputs[@]}" -gt 0 ]; then
  echo "Map pipeline outputs missing; regenerating:"
  printf '  - %s\n' "${missing_outputs[@]}"
fi

# Ensure Python exists
if ! command -v "$python_cmd" >/dev/null 2>&1; then
  echo "Error: python3 not found" >&2
  exit 1
fi

# Create venv if missing
if [ ! -d "$venv_path" ]; then
  echo "Creating virtual environment at $venv_path"
  "$python_cmd" -m venv "$venv_path"
fi

python_bin="$venv_path/bin/python"

if [ ! -x "$python_bin" ]; then
  echo "Virtualenv python not found at $python_bin" >&2
  exit 1
fi

# ------------------------------------------------------------
# Bootstrap pip (works even if ensurepip is missing)
# ------------------------------------------------------------
if ! "$python_bin" -m pip --version >/dev/null 2>&1; then
  echo "pip missing in venv — bootstrapping via get-pip.py"

  get_pip="$venv_path/get-pip.py"

  if command -v curl >/dev/null 2>&1; then
    curl -fsSL https://bootstrap.pypa.io/get-pip.py -o "$get_pip"
  elif command -v wget >/dev/null 2>&1; then
    wget -q https://bootstrap.pypa.io/get-pip.py -O "$get_pip"
  else
    echo "Error: neither curl nor wget available" >&2
    exit 1
  fi

  "$python_bin" "$get_pip"
  rm -f "$get_pip"
fi

pip_bin="$venv_path/bin/pip"

# Upgrade tooling
"$pip_bin" install --upgrade pip setuptools wheel

# Install dependencies
"$pip_bin" install -r "$requirements_path"

# Run pipeline
"$python_bin" "$root_dir/tools/map_pipeline/pipeline.py"
"$python_bin" "$root_dir/tools/map_pipeline/provinces.py"
"$python_bin" "$root_dir/tools/map_pipeline/hannibal_path.py"
