#!/usr/bin/env bash
# Runs independent shell commands concurrently and reports them as one result.
#
# CI used to run the content, audio, validator and promo checks one after the
# other, each waiting for the previous one although none reads what another
# writes. This runs them side by side, bounded so a four-core runner is not
# oversubscribed, and keeps their output apart: each task's log is printed in
# one piece, under a collapsible group on GitHub Actions, when it finishes.
#
# Every task runs to the end even when another fails, so one run reports every
# broken check instead of the first. The exit status is non-zero if any failed.
# A duration table goes to the job summary when GITHUB_STEP_SUMMARY is set.
#
# usage: scripts/run-parallel.sh [--jobs N] [--title TEXT] "label::command" ...
#
# The command is handed to `bash -c`, so it may be a pipeline or set variables.

set -uo pipefail

jobs=${SOI_PARALLEL_JOBS:-4}
title="Parallel checks"
tasks=()

while [ $# -gt 0 ]; do
  case "$1" in
    --jobs)
      jobs=${2:?--jobs needs a number}
      shift 2
      ;;
    --title)
      title=${2:?--title needs text}
      shift 2
      ;;
    --)
      shift
      tasks+=("$@")
      break
      ;;
    *)
      tasks+=("$1")
      shift
      ;;
  esac
done

case "${jobs}" in
  '' | *[!0-9]* | 0)
    echo "error: --jobs must be a positive integer, got '${jobs}'" >&2
    exit 2
    ;;
esac
if [ "${#tasks[@]}" -eq 0 ]; then
  echo "error: no tasks given. usage: $0 [--jobs N] \"label::command\" ..." >&2
  exit 2
fi

labels=()
commands=()
for task in "${tasks[@]}"; do
  case "${task}" in
    *::*) ;;
    *)
      echo "error: task '${task}' is not of the form label::command" >&2
      exit 2
      ;;
  esac
  labels+=("${task%%::*}")
  commands+=("${task#*::}")
done

work=$(mktemp -d)
trap 'rm -rf "${work}"' EXIT

in_actions=0
if [ "${GITHUB_ACTIONS:-}" = "true" ]; then
  in_actions=1
fi

# Runs task $1 and leaves its log, status and wall time beside it.
run_task() {
  local index=$1
  local started=${SECONDS}
  bash -c "${commands[${index}]}" >"${work}/${index}.log" 2>&1
  echo $? >"${work}/${index}.status"
  echo $((SECONDS - started)) >"${work}/${index}.seconds"
}

# Prints a finished task's log in one piece.
report_task() {
  local index=$1
  local status seconds
  status=$(cat "${work}/${index}.status")
  seconds=$(cat "${work}/${index}.seconds")
  if [ "${in_actions}" -eq 1 ]; then
    echo "::group::${labels[${index}]} ($([ "${status}" -eq 0 ] && echo ok || echo FAILED), ${seconds}s)"
  else
    echo "--- ${labels[${index}]} ($([ "${status}" -eq 0 ] && echo ok || echo FAILED), ${seconds}s) ---"
  fi
  cat "${work}/${index}.log"
  if [ "${in_actions}" -eq 1 ]; then
    echo "::endgroup::"
    if [ "${status}" -ne 0 ]; then
      echo "::error title=${labels[${index}]}::${labels[${index}]} failed after ${seconds}s"
    fi
  fi
}

reported=()
running=0
wall_start=${SECONDS}

# Prints every task that has finished and has not been printed yet.
flush_finished() {
  local done_index
  for done_index in "${!tasks[@]}"; do
    if [ -f "${work}/${done_index}.seconds" ] && [ -z "${reported[${done_index}]:-}" ]; then
      report_task "${done_index}"
      reported[done_index]=1
    fi
  done
}

for index in "${!tasks[@]}"; do
  if [ "${running}" -ge "${jobs}" ]; then
    wait -n
    running=$((running - 1))
    flush_finished
  fi
  run_task "${index}" &
  running=$((running + 1))
done

while [ "${running}" -gt 0 ]; do
  wait -n
  running=$((running - 1))
  flush_finished
done

wall=$((SECONDS - wall_start))
failed=()
total=0
table=("| Task | Result | Seconds |" "| --- | --- | ---: |")
for index in "${!tasks[@]}"; do
  status=$(cat "${work}/${index}.status")
  seconds=$(cat "${work}/${index}.seconds")
  total=$((total + seconds))
  if [ "${status}" -eq 0 ]; then
    result=ok
  else
    result=FAILED
    failed+=("${labels[${index}]}")
  fi
  table+=("| ${labels[${index}]} | ${result} | ${seconds} |")
done

echo ""
echo "${title}: ${#tasks[@]} task(s), ${wall}s wall, ${total}s if run one after another (jobs=${jobs})"

if [ -n "${GITHUB_STEP_SUMMARY:-}" ]; then
  {
    echo "## ${title}"
    echo ""
    printf '%s\n' "${table[@]}"
    echo ""
    echo "${wall}s wall against ${total}s serial (jobs=${jobs})."
  } >>"${GITHUB_STEP_SUMMARY}"
fi

if [ "${#failed[@]}" -gt 0 ]; then
  echo "failed: ${failed[*]}" >&2
  exit 1
fi
