#!/usr/bin/env bash
# Prints the version this build should call itself.
#
# All three packaging workflows and the humans reading them go through this
# script, so there is one answer to "what version is this" instead of four that
# drift. The order matters:
#
#   1. a v* tag being built  -> that tag, minus the leading v
#   2. otherwise             -> the project() version in CMakeLists.txt
#
# The second case is what a nightly or a workflow_dispatch build gets. It is
# deliberately the same number the binary reports through
# QCoreApplication::applicationVersion(), because a package whose file name
# disagrees with the About panel inside it is worse than no version at all.
#
# usage: scripts/release-version.sh

set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

ref="${GITHUB_REF_NAME:-}"
if [[ "${ref}" =~ ^v[0-9]+\.[0-9]+\.[0-9]+ ]]; then
  echo "${ref#v}"
  exit 0
fi

version="$(sed -n 's/^project(StandardOfIron VERSION \([0-9.]*\).*/\1/p' \
  "${repo_root}/CMakeLists.txt" | head -1)"

if [ -z "${version}" ]; then
  echo "error: could not read the project version from CMakeLists.txt" >&2
  exit 1
fi

echo "${version}"
