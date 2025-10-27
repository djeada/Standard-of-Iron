#!/usr/bin/env bash
# Filter clang-tidy invocations to skip auto-generated Qt artifact files.
set -euo pipefail

if [[ $# -lt 1 ]]; then
  echo "clang_tidy_filter: missing clang-tidy executable path" >&2
  exit 2
fi

clang_tidy_bin="$1"
shift

pre_args=()
compile_args=()
source_file=""

while [[ $# -gt 0 ]]; do
  if [[ "$1" == "--" ]]; then
    shift
    break
  fi
  pre_args+=("$1")
  if [[ "$1" == *.c || "$1" == *.cc || "$1" == *.cpp || "$1" == *.cxx ]]; then
    source_file="$1"
  fi
  shift
done

if [[ $# -gt 0 ]]; then
  compile_args=("$@")
  if [[ -z "$source_file" ]]; then
    for candidate in "${compile_args[@]}"; do
      if [[ "$candidate" == *.c || "$candidate" == *.cc || "$candidate" == *.cpp || "$candidate" == *.cxx ]]; then
        source_file="$candidate"
        break
      fi
    done
  fi
fi

if [[ -z "$source_file" ]]; then
  echo "clang_tidy_filter: unable to determine source file" >&2
  exit 2
fi

if [[ "$source_file" == *"/build/.rcc/"* ]] \
  || [[ "$source_file" == *"qmltyperegistrations.cpp" ]] \
  || [[ "$source_file" == *"/third_party/"* ]]; then
  exit 0
fi

exec "$clang_tidy_bin" "${pre_args[@]}" -- "${compile_args[@]}"
