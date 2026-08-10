#!/usr/bin/env bash
# Runs, on this machine, every gate the release pipeline runs, in the order the
# pipeline runs them.
#
# A tagged release costs three platform builds and roughly forty minutes before
# it tells you that a formatter, a test or the packaged self-test was unhappy.
# Everything here is reproducible on Linux, so the answer to "will the tag go
# green" should come from a terminal, not from a tag push and a wait.
#
# What this cannot cover: the macOS and Windows *packaging* steps (macdeployqt,
# hdiutil, windeployqt) and the packaged self-test on those platforms. The
# portability stage below is the closest proxy -- it re-parses every translation
# unit with Clang/libc++ and puts every shader through glslangValidator, which
# is what usually breaks over there.
#
#   usage: scripts/verify-release.sh [--build-dir DIR] [--only STAGE[,STAGE...]]
#          scripts/verify-release.sh --list
#
# Exits non-zero if any stage fails, and prints a summary naming the failures.

set -uo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT" || exit 2

BUILD_DIR="build-verify"
ONLY=""

# 'portability' sits after 'configure' on purpose: its Clang/libc++ pass reads
# compile_commands.json, so it needs a configured tree. This is the same reason
# the pipeline splits it between quality.yml and build-linux.yml.
STAGES=(quality map configure portability tests content package)

usage() {
  sed -n '2,20p' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'
  echo "stages: ${STAGES[*]}"
}

while [ $# -gt 0 ]; do
  case "$1" in
    --build-dir)
      BUILD_DIR="${2:?--build-dir needs a path}"
      shift 2
      ;;
    --only)
      ONLY="${2:?--only needs a stage list}"
      shift 2
      ;;
    --list)
      echo "${STAGES[*]}"
      exit 0
      ;;
    -h | --help)
      usage
      exit 0
      ;;
    *)
      echo "unknown argument: $1" >&2
      usage >&2
      exit 2
      ;;
  esac
done

# Several suites locate their fixtures as applicationDirPath()/../../assets --
# that is, they assume the binary sits at <repo>/<build-dir>/bin. A build tree
# anywhere else (or nested deeper) makes them fail on missing assets, which
# looks exactly like broken content. Refuse that up front instead of reporting
# a dozen phantom failures.
case "$BUILD_DIR" in
  */*)
    echo "error: --build-dir must be a single directory name directly under the" >&2
    echo "       repository root: the test suites resolve assets as" >&2
    echo "       <build-dir>/bin/../../assets. Got: $BUILD_DIR" >&2
    exit 2
    ;;
esac

if [ -t 1 ]; then
  BOLD=$'\033[1m'
  RED=$'\033[31m'
  GREEN=$'\033[32m'
  YELLOW=$'\033[33m'
  RESET=$'\033[0m'
else
  BOLD="" RED="" GREEN="" YELLOW="" RESET=""
fi

declare -a RESULT_NAMES=()
declare -a RESULT_STATES=()
FAILED=0

wanted() {
  [ -z "$ONLY" ] && return 0
  case ",${ONLY}," in
    *",$1,"*) return 0 ;;
    *) return 1 ;;
  esac
}

# Each check is run even when an earlier one in the same stage failed: one run
# should report every problem, not send you round the loop once per problem.
run_check() {
  local name="$1"
  shift
  printf '%s>> %s%s\n' "$BOLD" "$name" "$RESET"
  if "$@"; then
    RESULT_NAMES+=("$name")
    RESULT_STATES+=("pass")
    return 0
  fi
  RESULT_NAMES+=("$name")
  RESULT_STATES+=("FAIL")
  FAILED=1
  printf '%s   FAILED: %s%s\n' "$RED" "$name" "$RESET"
  return 1
}

skip_check() {
  RESULT_NAMES+=("$1")
  RESULT_STATES+=("skip: $2")
  printf '%s>> %s -- skipped (%s)%s\n' "$YELLOW" "$1" "$2" "$RESET"
}

# ---- quality: mirrors .github/workflows/quality.yml (quality job) ----------
stage_quality() {
  run_check "format --check" python3 scripts/format.py --all --check --strict
  run_check "lint" python3 scripts/format.py --all --lint --strict
  run_check "quality markers" bash -c \
    "git ls-files -z '*.c' '*.cc' '*.cpp' '*.cxx' '*.h' '*.hpp' '*.py' '*.sh' \
       | xargs -0 -r python3 scripts/check-quality-markers.py"
  run_check "shader uniforms" python3 scripts/validate_shader_uniforms.py
  run_check "opengl requirements" python3 scripts/validate_opengl_requirements.py
}

# ---- portability: quality.yml (portability job) + build-linux.yml apple pass -
stage_portability() {
  if command -v glslangValidator >/dev/null 2>&1; then
    run_check "portability glsl+windows" \
      python3 scripts/check-portability.py --only glsl,windows --require-all
  else
    run_check "portability windows" \
      python3 scripts/check-portability.py --only windows --require-all
    skip_check "portability glsl" "glslangValidator not installed"
  fi

  # The Clang/libc++ pass is the one that predicts macOS compile failures. It
  # reads the compile database, so it is useless without a configured tree.
  if [ ! -f "${BUILD_DIR}/compile_commands.json" ]; then
    skip_check "portability apple" "no ${BUILD_DIR}/compile_commands.json (run the configure stage)"
  elif command -v clang++ >/dev/null 2>&1; then
    run_check "portability apple (clang/libc++)" \
      python3 scripts/check-portability.py --only apple --build-dir "$BUILD_DIR"
  else
    skip_check "portability apple" "clang++ not installed"
  fi
}

# ---- map: the step that used to take the release down -----------------------
stage_map() {
  run_check "campaign map assets" python3 scripts/generate-campaign-map.py
}

# ---- configure + build ------------------------------------------------------
stage_configure() {
  run_check "configure" cmake -S . -B "$BUILD_DIR" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DDEFAULT_LANG=en \
    -DENABLE_GENERATED_CAMPAIGN_MAP_ASSETS=ON \
    -DBUILD_TESTING=ON
  run_check "build (game + tests + validator)" cmake --build "$BUILD_DIR" \
    --parallel --target standard_of_iron soi_test_binaries content_validator
}

# ---- tests: scripts/run-tests.sh, same as every workflow --------------------
stage_tests() {
  run_check "unit tests" env QT_QPA_PLATFORM=offscreen \
    bash scripts/run-tests.sh "$BUILD_DIR" --gtest_brief=1
}

# ---- content: pr.yml translation + validator lanes --------------------------
stage_content() {
  run_check "translation coverage" make translations-check
  run_check "content validation" "${ROOT}/${BUILD_DIR}/bin/content_validator" assets
  run_check "validator integration" bash tests/validator_integration_test.sh
}

# ---- package: the packaged self-test, the gate that fails on macOS ----------
# Runs the built binary rather than an AppImage: the markers asserted here are
# exactly the ones the Linux, macOS and Windows jobs grep for, so a stall in the
# loading handshake shows up here instead of forty minutes into a tag.
stage_package() {
  local binary="${BUILD_DIR}/bin/standard_of_iron"
  if [ ! -x "$binary" ]; then
    skip_check "packaged self-test" "no binary at $binary (run the configure stage)"
    return
  fi

  local runner=()
  if [ -z "${DISPLAY:-}" ] && command -v xvfb-run >/dev/null 2>&1; then
    runner=(xvfb-run --auto-servernum --)
  fi

  local log="${BUILD_DIR}/release_self_test.log"
  printf '%s>> release self-test (software GL)%s\n' "$BOLD" "$RESET"
  # LIBGL_ALWAYS_SOFTWARE forces llvmpipe, which is what the Linux job runs on
  # and the harshest timing case available here -- a handshake that depends on
  # frames arriving quickly fails here first.
  if LIBGL_ALWAYS_SOFTWARE=1 QT_OPENGL=desktop QT_LOGGING_RULES="qt.rhi.*=false" \
    timeout 600s "${runner[@]}" "$binary" --release-self-test >"$log" 2>&1; then
    :
  fi

  local marker
  local missing=()
  for marker in SOI_RENDERER_SELF_TEST SOI_MISSION_SELF_TEST \
    SOI_GRAPHICS_DEFAULT_SELF_TEST SOI_CAMPAIGN_MAP_SELF_TEST \
    SOI_CREATURE_ASSET_SELF_TEST SOI_AUDIO_SELF_TEST SOI_GL_FLOOR \
    SOI_GL_TIER_41 SOI_GL_TIER_45; do
    grep -q "${marker}: PASS" "$log" || missing+=("$marker")
  done

  if [ ${#missing[@]} -eq 0 ]; then
    RESULT_NAMES+=("release self-test")
    RESULT_STATES+=("pass")
  else
    RESULT_NAMES+=("release self-test")
    RESULT_STATES+=("FAIL")
    FAILED=1
    printf '%s   missing PASS markers: %s%s\n' "$RED" "${missing[*]}" "$RESET"
    printf '   the tail of %s:\n' "$log"
    grep -E "SOI_[A-Z_0-9]+:" "$log" | tail -20 | sed 's/^/     /'
  fi

  # OpenGL 3.3 Core is the floor on every platform; macOS caps at 4.1, so
  # anything gained from 4.3+ must stay optional. Both other jobs assert this.
  printf '%s>> renderer self-test with GPU crowd culling off%s\n' "$BOLD" "$RESET"
  local fallback_log="${BUILD_DIR}/renderer_fallback.log"
  SOI_RENDER_DISABLE_GPU_CROWD_CULL=1 LIBGL_ALWAYS_SOFTWARE=1 QT_OPENGL=desktop \
    timeout 600s "${runner[@]}" "$binary" --renderer-self-test \
    >"$fallback_log" 2>&1
  if grep -q "SOI_RENDERER_SELF_TEST: PASS" "$fallback_log"; then
    RESULT_NAMES+=("3.3 fallback self-test")
    RESULT_STATES+=("pass")
  else
    RESULT_NAMES+=("3.3 fallback self-test")
    RESULT_STATES+=("FAIL")
    FAILED=1
    printf '%s   no PASS in %s%s\n' "$RED" "$fallback_log" "$RESET"
  fi
}

for stage in "${STAGES[@]}"; do
  wanted "$stage" || continue
  printf '\n%s===== %s =====%s\n' "$BOLD" "$stage" "$RESET"
  "stage_${stage}"
done

printf '\n%s===== summary =====%s\n' "$BOLD" "$RESET"
for index in "${!RESULT_NAMES[@]}"; do
  state="${RESULT_STATES[$index]}"
  case "$state" in
    pass) printf '  %s%-6s%s %s\n' "$GREEN" "pass" "$RESET" "${RESULT_NAMES[$index]}" ;;
    FAIL) printf '  %s%-6s%s %s\n' "$RED" "FAIL" "$RESET" "${RESULT_NAMES[$index]}" ;;
    *) printf '  %s%-6s%s %s (%s)\n' "$YELLOW" "skip" "$RESET" \
      "${RESULT_NAMES[$index]}" "${state#skip: }" ;;
  esac
done

if [ "$FAILED" -ne 0 ]; then
  printf '\n%sRELEASE VERIFY: FAIL%s\n' "$RED" "$RESET"
  exit 1
fi
printf '\n%sRELEASE VERIFY: PASS%s\n' "$GREEN" "$RESET"
