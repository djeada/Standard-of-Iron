# Standard of Iron - Makefile
# Provides standard targets for building, running, and managing the project

# Default target
.DEFAULT_GOAL := help

# Configuration
BUILD_DIR := build
DEBUG_BUILD_DIR := build-debug
BUILD_TIDY_DIR := build-tidy
BINARY_NAME := standard_of_iron
MAP_EDITOR_BINARY := map_editor
ARENA_BINARY := arena_app
DEFAULT_LANG ?= en
MESH_OUTPUT_DIR ?= dist/mesh
SUPPORTED_MESH_SPECIES := horse elephant
MESH_SPECIES ?= $(word 2,$(MAKECMDGOALS))
MESH_PART ?= full

# Clang-tidy auto-fixer (git-only by default; --all scans whole project)
CLANG_TIDY_FIXER := scripts/run-clang-tidy-fixes.sh

# Optional knobs (override on the command line)
# e.g. make tidy CLANG_TIDY_JOBS=2 CLANG_TIDY_AUTO_FIX_CHECKS="-*,bugprone-*"
CLANG_TIDY_JOBS ?=
CLANG_TIDY_AUTO_FIX_CHECKS ?=
CLANG_TIDY_FIX_PATHS ?=
# Base for git diff (fallback is origin/main inside the script if unset)
CLANG_TIDY_GIT_BASE ?=

# ---- Formatting / linting driver ----
# scripts/format.py is the single entry point: the Makefile, the pre-commit
# hooks and CI all call it so they can never disagree about what "formatted"
# means. Tool versions are pinned in tools/versions.env.
PYTHON ?= python3
FORMAT_DRIVER := $(PYTHON) scripts/format.py
FORMAT_JOBS ?= $(shell command -v nproc >/dev/null 2>&1 && nproc || echo 4)
# Base revision used by the *-changed targets and by CI.
FORMAT_BASE ?= origin/main
# Extra flags forwarded to the driver (e.g. FORMAT_ARGS="--verbose").
FORMAT_ARGS ?=
# Allow the lint step to skip comment stripping (set to empty to skip).
LINT_STRIP ?= --strip-comments
# Extra GoogleTest flags (e.g. TEST_ARGS="--gtest_filter=SaveLoadServiceTest.*").
TEST_ARGS ?=

# Colors for output
BOLD := \033[1m
GREEN := \033[32m
BLUE := \033[34m
YELLOW := \033[33m
RED := \033[31m
RESET := \033[0m

# Help target - shows available commands
.PHONY: help
help:
	@echo "$(BOLD)Standard of Iron - Build System$(RESET)"
	@echo ""
	@echo "$(BOLD)Available targets:$(RESET)"
	@echo "  $(GREEN)install$(RESET)       - Install all dependencies"
	@echo "  $(GREEN)configure$(RESET)     - Configure build with CMake"
	@echo "  $(GREEN)build$(RESET)         - Build the project"
	@echo "  $(GREEN)mesh$(RESET)          - Render creature mesh comparison sheet (use: make mesh horse)"
	@echo "  $(GREEN)build-tidy$(RESET)    - Build with clang-tidy static analysis enabled"
	@echo "  $(GREEN)debug$(RESET)         - Build with debug symbols, GDB support, and runtime tracing"
	@echo "  $(GREEN)release$(RESET)       - Build optimized release version"
	@echo "  $(GREEN)run$(RESET)           - Run the main application (includes map pipeline)"
	@echo "  $(GREEN)arena$(RESET)         - Run the arena playground tool"
	@echo "  $(GREEN)run-map-pipeline$(RESET) - Run map preprocessing pipeline only"
	@echo "  $(GREEN)editor$(RESET)        - Run the map editor"
	@echo "  $(GREEN)clean$(RESET)         - Clean build directory"
	@echo "  $(GREEN)rebuild$(RESET)       - Clean and build"
	@echo "  $(GREEN)test$(RESET)          - Build only test binaries, then run them"
	@echo "  $(GREEN)test-only$(RESET)     - Run existing test binaries without building"
	@echo "  $(GREEN)validate-content$(RESET) - Validate mission and campaign JSON files"
	@echo "  $(GREEN)audio-preview$(RESET) - Render before/after WAVs of the decode-time mastering"
	@echo "  $(GREEN)audio-report$(RESET)  - List missing/placeholder sounds into docs/AUDIO_WISHLIST.md"
	@echo "  $(GREEN)audio-check$(RESET)   - Fail when cues, manifest and audio files disagree"
	@echo "  $(GREEN)audio-field-ambience$(RESET) - Rebuild recorded ambience beds from their sources"
	@echo "  $(GREEN)audio-battle$(RESET)  - Rebuild composed battle cues from their CC0 sources"
	@echo "  $(GREEN)translations$(RESET)  - Refresh .ts/.qm catalogues and translator CSVs"
	@echo "  $(GREEN)translations-check$(RESET) - Fail when assets gained untranslated player text"
	@echo "  $(GREEN)test-validator$(RESET) - Run validator integration tests"
	@echo "  $(GREEN)check-deps$(RESET)    - Check if dependencies are installed"
	@echo "  $(GREEN)dev$(RESET)           - Set up development environment (install + configure + build)"
	@echo "  $(GREEN)all$(RESET)           - Full build (configure + build)"
	@echo ""
	@echo "$(BOLD)Formatting (also strips comments via remove-comments.sh):$(RESET)"
	@echo "  $(GREEN)format$(RESET)        - Format, apply Ruff fixes (including unsafe), and verify quality"
	@echo "  $(GREEN)format-check$(RESET)  - Verify formatting, change nothing (CI gate)"
	@echo "  $(GREEN)format-changed$(RESET) - Format only files changed vs FORMAT_BASE"
	@echo "  $(GREEN)format-check-changed$(RESET) - Fast check of changed files only"
	@echo "  $(GREEN)format-doctor$(RESET) - Report installed vs pinned tool versions"
	@echo "  $(GREEN)format-bootstrap$(RESET) - Install the pinned formatting toolchain"
	@echo ""
	@echo "$(BOLD)Linting and quality (also strips comments via remove-comments.sh):$(RESET)"
	@echo "  $(GREEN)lint$(RESET)          - clang-tidy, qmllint, Ruff, ShellCheck, yamllint, JSON"
	@echo "  $(GREEN)lint-fix$(RESET)      - Apply the linters' automated fixes (explicit)"
	@echo "  $(GREEN)lint-changed$(RESET)  - Lint only files changed vs FORMAT_BASE"
	@echo "  $(GREEN)quality$(RESET)       - lint + format-check + quality markers"
	@echo "  $(GREEN)validate$(RESET)      - quality + build + test + content validation"
	@echo "  $(GREEN)portability$(RESET)   - macOS/Windows checks run from Linux"
	@echo "  $(GREEN)hooks-install$(RESET) - Install the pre-commit git hooks"
	@echo "  $(GREEN)tidy$(RESET)          - Run clang-tidy fixes on changed files"
	@echo "  $(GREEN)tidy-all$(RESET)      - Run clang-tidy fixes on the whole project"
	@echo "  $(RED)strip-comments$(RESET) - DESTRUCTIVE: delete comments (needs STRIP_COMMENTS_CONFIRM=1)"
	@echo ""
	@echo "$(BOLD)Examples:$(RESET)"
	@echo "  make install    # Install dependencies"
	@echo "  make dev        # Complete development setup"
	@echo "  make debug      # Build for debugging with GDB"
	@echo "  make run        # Build and run the game"
	@echo "  make mesh horse # Render a four-view horse mesh comparison sheet"
	@echo "  DEFAULT_LANG=de make build   # Build with German as default language"
	@echo "  FORMAT_BASE=origin/develop make format-check-changed"
	@echo "  make strip-comments DRY_RUN=1 # Preview the destructive rewrite"

# Install dependencies
.PHONY: install
install:
	@echo "$(BOLD)$(BLUE)Installing dependencies...$(RESET)"
	@bash scripts/setup-deps.sh --yes
	@echo "$(GREEN)✓ Dependencies installed successfully$(RESET)"

# Check if dependencies are installed
.PHONY: check-deps
check-deps:
	@echo "$(BOLD)$(BLUE)Checking dependencies...$(RESET)"
	@bash scripts/setup-deps.sh --dry-run

# Create build directory
build-dir:
	@mkdir -p $(BUILD_DIR)

# Configure build with CMake
.PHONY: configure
configure: build-dir
	@echo "$(BOLD)$(BLUE)Configuring build with CMake...$(RESET)"
	@if [ -f "$(BUILD_DIR)/CMakeCache.txt" ]; then \
		CACHED_SRC=$$(grep '^CMAKE_HOME_DIRECTORY:INTERNAL=' "$(BUILD_DIR)/CMakeCache.txt" | cut -d= -f2-); \
		if [ "$$CACHED_SRC" != "$$(pwd)" ]; then \
			echo "$(YELLOW)Build cache points at $$CACHED_SRC; resetting stale build tree$(RESET)"; \
			find "$(BUILD_DIR)" -mindepth 1 -maxdepth 1 -exec rm -rf {} +; \
		fi; \
	fi
	@if [ ! -f "$(BUILD_DIR)/_deps/googletest-src/googletest/include/gtest/gtest.h" ] || \
		[ ! -f "$(BUILD_DIR)/_deps/googletest-src/googletest/include/gtest/gtest-assertion-result.h" ] || \
		[ ! -f "$(BUILD_DIR)/_deps/googletest-src/googlemock/include/gmock/gmock.h" ]; then \
		echo "$(YELLOW)Detected incomplete googletest cache; refreshing build/_deps/googletest-*$(RESET)"; \
		rm -rf "$(BUILD_DIR)/_deps/googletest-src" \
		       "$(BUILD_DIR)/_deps/googletest-build" \
		       "$(BUILD_DIR)/_deps/googletest-subbuild"; \
	fi
	@cd $(BUILD_DIR) && cmake -DCMAKE_BUILD_TYPE=Release -DENABLE_CLANG_TIDY=OFF -DDEFAULT_LANG=$(DEFAULT_LANG) ..
	@echo "$(GREEN)✓ Configuration complete$(RESET)"

# Build the project
.PHONY: build
build: run-map-pipeline configure
	@echo "$(BOLD)$(BLUE)Building project...$(RESET)"
	@cd $(BUILD_DIR) && make -j$$(nproc)
	@$(MAKE) bake-bpat
	@echo "$(GREEN)✓ Build complete$(RESET)"

# Build only the runnable application and runtime assets.
.PHONY: build-app
build-app: run-map-pipeline configure
	@echo "$(BOLD)$(BLUE)Building Standard of Iron...$(RESET)"
	@cd $(BUILD_DIR) && cmake --build . -j$$(nproc) --target $(BINARY_NAME)
	@$(MAKE) bake-bpat
	@echo "$(GREEN)✓ Application build complete$(RESET)"

# Bake creature animation textures (BPAT) into assets/creatures/.
# Runs after build so the bpat_baker binary exists. Idempotent.
.PHONY: bake-bpat
bake-bpat:
	@echo "$(BOLD)$(BLUE)Baking creature animation textures (BPAT)...$(RESET)"
	@$(BUILD_DIR)/bin/bpat_baker assets/creatures
	@echo "$(GREEN)✓ BPAT baking complete$(RESET)"

# Build with clang-tidy enabled
.PHONY: build-tidy
build-tidy:
	@echo "$(BOLD)$(BLUE)Configuring build with clang-tidy enabled...$(RESET)"
	@mkdir -p $(BUILD_TIDY_DIR)
	@cd $(BUILD_TIDY_DIR) && cmake -DENABLE_CLANG_TIDY=ON -DDEFAULT_LANG=$(DEFAULT_LANG) ..
	@echo "$(BOLD)$(BLUE)Building with clang-tidy analysis...$(RESET)"
	@cd $(BUILD_TIDY_DIR) && make -j$$(nproc)
	@echo "$(GREEN)✓ Build with clang-tidy complete$(RESET)"

# Build everything (alias for build)
.PHONY: all
all: build

.PHONY: horse elephant
horse elephant:
	@:

.PHONY: mesh
mesh: configure
	@case "$(MESH_SPECIES)" in \
		horse|elephant) ;; \
		*) echo "$(RED)Usage: make mesh [horse|elephant] MESH_PART=<full|torso|legs|neck_head>$(RESET)"; exit 2 ;; \
	esac
	@echo "$(BOLD)$(BLUE)Rendering $(MESH_SPECIES) mesh comparison views ($(MESH_PART))...$(RESET)"
	@mkdir -p "$(MESH_OUTPUT_DIR)"
	@cmake --build $(BUILD_DIR) -j$$(nproc) --target mesh_preview
	@QT_QPA_PLATFORM=offscreen "$(BUILD_DIR)/bin/mesh_preview" "$(MESH_SPECIES)" "$(MESH_OUTPUT_DIR)" "$(MESH_PART)"
	@echo "$(GREEN)✓ Mesh sheet written under $(MESH_OUTPUT_DIR)$(RESET)"

# Run map pipeline preprocessing
.PHONY: run-map-pipeline
run-map-pipeline:
	@echo "$(BOLD)$(BLUE)Running map pipeline preprocessing...$(RESET)"
	@bash scripts/run-map-pipeline.sh $(if $(map_pipeline_rebuild),--rebuild,)
	@echo "$(GREEN)✓ Map pipeline complete$(RESET)"

# Run the main application
.PHONY: run
run: build-app
	@echo "$(BOLD)$(BLUE)Running Standard of Iron...$(RESET)"
	@python3 scripts/purge-stale-saves.py "$(CURDIR)"
	@cd $(BUILD_DIR) && \
	BIN_PATH="./bin/$(BINARY_NAME)"; \
	if [ ! -x "$$BIN_PATH" ]; then \
		echo "$(RED)$(BINARY_NAME) not found at $$BIN_PATH$(RESET)"; \
		exit 127; \
	fi; \
	PLATFORM="$$(uname -s)"; \
	DEFAULT_QPA="offscreen"; \
	case "$$PLATFORM" in \
		Darwin) DEFAULT_QPA="cocoa" ;; \
		MINGW*|MSYS*|CYGWIN*) DEFAULT_QPA="windows" ;; \
		*) \
			if [ -n "$$WAYLAND_DISPLAY" ]; then \
				DEFAULT_QPA="wayland"; \
			elif [ -n "$$DISPLAY" ]; then \
				DEFAULT_QPA="xcb"; \
			fi ;; \
	esac; \
	if [ -z "$$QT_QPA_PLATFORM" ]; then \
		echo "$(YELLOW)QT_QPA_PLATFORM not set; defaulting to $$DEFAULT_QPA$(RESET)"; \
		QT_QPA_PLATFORM="$$DEFAULT_QPA" "$${BIN_PATH}"; \
	else \
		"$${BIN_PATH}"; \
	fi

# Run with xvfb for headless environments (software rasterization)
.PHONY: run-headless
run-headless: build
	@echo "$(BOLD)$(BLUE)Running Standard of Iron under xvfb...$(RESET)"
	@if ! command -v xvfb-run >/dev/null 2>&1; then \
	  echo "$(YELLOW)xvfb-run not found. Installing...$(RESET)"; \
	  sudo apt-get update -y >/dev/null 2>&1 && sudo apt-get install -y xvfb >/dev/null 2>&1; \
	fi
	@cd $(BUILD_DIR) && \
	BIN_PATH="./bin/$(BINARY_NAME)"; \
	if [ ! -x "$$BIN_PATH" ]; then \
		echo "$(RED)$(BINARY_NAME) not found at $$BIN_PATH$(RESET)"; \
		exit 127; \
	fi; \
	xvfb-run -s "-screen 0 1280x720x24" "$$BIN_PATH"

# Run the map editor
.PHONY: editor
editor: run-map-pipeline configure
	@echo "$(BOLD)$(BLUE)Building Map Editor...$(RESET)"
	@cmake --build $(BUILD_DIR) -j$$(nproc) --target $(MAP_EDITOR_BINARY)
	@echo "$(BOLD)$(BLUE)Running Map Editor...$(RESET)"
	@cd $(BUILD_DIR) && ./bin/$(MAP_EDITOR_BINARY)

.PHONY: arena
arena: run-map-pipeline configure
	@echo "$(BOLD)$(BLUE)Building Arena playground...$(RESET)"
	@cmake --build $(BUILD_DIR) -j$$(nproc) --target $(ARENA_BINARY)
	@echo "$(BOLD)$(BLUE)Running Arena playground...$(RESET)"
	@cd $(BUILD_DIR) && \
	BIN_PATH="./bin/$(ARENA_BINARY)"; \
	if [ ! -x "$$BIN_PATH" ]; then \
		echo "$(RED)$(ARENA_BINARY) not found at $$BIN_PATH$(RESET)"; \
		exit 127; \
	fi; \
	PLATFORM="$$(uname -s)"; \
	DEFAULT_QPA="offscreen"; \
	case "$$PLATFORM" in \
		Darwin) DEFAULT_QPA="cocoa" ;; \
		MINGW*|MSYS*|CYGWIN*) DEFAULT_QPA="windows" ;; \
		*) \
			if [ -n "$$WAYLAND_DISPLAY" ]; then \
				DEFAULT_QPA="wayland"; \
			elif [ -n "$$DISPLAY" ]; then \
				DEFAULT_QPA="xcb"; \
			fi ;; \
	esac; \
	if [ -z "$$QT_QPA_PLATFORM" ]; then \
		echo "$(YELLOW)QT_QPA_PLATFORM not set; defaulting to $$DEFAULT_QPA$(RESET)"; \
		QT_QPA_PLATFORM="$$DEFAULT_QPA" "$${BIN_PATH}"; \
	else \
		"$${BIN_PATH}"; \
	fi

# Clean build directory
.PHONY: clean
clean:
	@echo "$(BOLD)$(YELLOW)Cleaning build directory...$(RESET)"
	@rm -rf $(BUILD_DIR) $(DEBUG_BUILD_DIR) $(BUILD_TIDY_DIR)
	@echo "$(GREEN)✓ Clean complete$(RESET)"

# Rebuild (clean + build)
.PHONY: rebuild
rebuild: clean build

# Development setup (install + configure + build)
.PHONY: dev
dev: install build
	@echo "$(GREEN)✓ Development environment ready!$(RESET)"
	@echo "$(BOLD)You can now run:$(RESET)"
	@echo "  make run      # Run the game"
	@echo "  make arena    # Run the arena playground"
	@echo "  make editor   # Run the map editor"

# Build only the binaries required by the test suite.
.PHONY: test-build
test-build: configure
	@echo "$(BOLD)$(BLUE)Building test binaries...$(RESET)"
	@cmake --build $(BUILD_DIR) -j$$(nproc) --target soi_test_binaries
	@echo "$(GREEN)✓ Test binaries built$(RESET)"

# Incrementally build and run tests.
.PHONY: test
test: test-build
	@$(MAKE) --no-print-directory test-only

# Run tests without configuring or compiling.
#
# scripts/run-tests.sh owns the suite list so the Makefile and the CI workflows
# cannot disagree about what running the tests means.
.PHONY: test-only
test-only:
	@echo "$(BOLD)$(BLUE)Running tests...$(RESET)"
	@bash scripts/run-tests.sh $(BUILD_DIR) --gtest_brief=1 $(TEST_ARGS)

# Re-render the synthesised cue sounds and re-register them. The recipes in
# tools/audio_synth are the source of truth for these files, so edit a recipe
# and run this rather than hand-editing an .ogg. Needs ffmpeg with libvorbis.
.PHONY: audio-assets
audio-assets:
	@echo "$(BOLD)$(BLUE)Synthesising cue sounds...$(RESET)"
	@$(PYTHON) tools/audio_synth/synthesize_cues.py
	@$(PYTHON) tools/audio_synth/register_cues.py
	@$(MAKE) --no-print-directory audio-ambience
	@$(MAKE) --no-print-directory audio-report
	@echo "$(GREEN)✓ Cue sounds rendered and registered$(RESET)"

## Re-render the synthesised ambience beds at the mixer's sample rate.
.PHONY: audio-ambience
audio-ambience:
	@echo "$(BOLD)$(BLUE)Synthesising ambience beds...$(RESET)"
	@$(PYTHON) tools/audio_synth/synthesize_ambience.py
	@echo "$(GREEN)✓ Ambience beds rendered$(RESET)"

# The nature beds are cut from public-domain recordings rather than generated,
# so they are committed and this is not part of audio-assets: it needs a network
# and it re-downloads tens of megabytes. Run it when a source or a window in
# tools/audio_field/sources.py changes.
## Rebuild the recorded ambience beds from their public-domain sources.
.PHONY: audio-field-ambience
audio-field-ambience:
	@echo "$(BOLD)$(BLUE)Rebuilding recorded ambience beds...$(RESET)"
	@$(PYTHON) tools/audio_field/build_beds.py
	@echo "$(GREEN)✓ Recorded ambience beds rebuilt$(RESET)"

# Same deal as the beds above: committed output, network needed, run it when a
# recipe in tools/audio_field/battle.py changes. These replaced the AudioCraft
# cues whose model licence forbade selling the game.
## Rebuild the composed battle cues from their CC0 sources.
.PHONY: audio-battle
audio-battle:
	@echo "$(BOLD)$(BLUE)Rebuilding composed battle cues...$(RESET)"
	@$(PYTHON) tools/audio_field/build_battle.py
	@echo "$(GREEN)✓ Composed battle cues rebuilt$(RESET)"

# Rewrite docs/AUDIO_WISHLIST.md from the cue catalog, the manifest and the
# assets on disk. Run it any time you want the current list of missing sounds.
.PHONY: audio-preview
audio-preview:
	@echo "$(BOLD)$(BLUE)Rendering audio mastering preview...$(RESET)"
	@cmake --build $(BUILD_DIR) -j$$(nproc) --target audio_master_preview
	@$(BUILD_DIR)/bin/audio_master_preview --out artifacts/audio_preview $(AUDIO_PREVIEW_ARGS)

## Audit the cue catalogue, the manifest and the files on disk.
.PHONY: audio-report
audio-report:
	@echo "$(BOLD)$(BLUE)Auditing game audio...$(RESET)"
	@$(PYTHON) scripts/audio_report.py
	@echo "$(GREEN)✓ Audio report written to docs/AUDIO_WISHLIST.md$(RESET)"

# Same audit as a gate: fails when a cue, a manifest entry and a file disagree.
.PHONY: audio-check
audio-check:
	@echo "$(BOLD)$(BLUE)Checking game audio wiring...$(RESET)"
	@$(PYTHON) scripts/audio_report.py --stdout --check > /dev/null
	@echo "$(GREEN)✓ Audio cue, manifest and asset links are consistent$(RESET)"

# Validate mission and campaign content
.PHONY: validate-content
validate-content: build
	@echo "$(BOLD)$(BLUE)Validating mission and campaign content...$(RESET)"
	@if [ -f "$(BUILD_DIR)/bin/content_validator" ]; then \
		$(BUILD_DIR)/bin/content_validator assets; \
		if [ $$? -eq 0 ]; then \
			echo "$(GREEN)✓ Content validation passed$(RESET)"; \
		else \
			echo "$(RED)✗ Content validation failed$(RESET)"; \
			exit 1; \
		fi \
	else \
		echo "$(RED)Content validator not found. Build may have failed.$(RESET)"; \
		exit 1; \
	fi

# Test validator with integration tests
.PHONY: test-validator
test-validator: build
	@echo "$(BOLD)$(BLUE)Running validator integration tests...$(RESET)"
	@if [ -f "tests/validator_integration_test.sh" ]; then \
		bash tests/validator_integration_test.sh; \
	else \
		echo "$(RED)Validator integration test not found.$(RESET)"; \
		exit 1; \
	fi

## Assert a published reel never opens on a black thumbnail frame. Needs ffmpeg, not a build.
.PHONY: test-promo-first-frame
test-promo-first-frame:
	@echo "$(BOLD)$(BLUE)Running promo first-frame tests...$(RESET)"
	@bash tests/promo_first_frame_test.sh

# ---- Formatting (also strips comments via remove-comments.sh) ----
.PHONY: format format-check format-changed format-check-changed \
	format-staged format-doctor format-bootstrap clean-format-trash

## Format every tracked file, apply Ruff fixes including unsafe fixes, then run the complete non-compiler quality gate.
format: clean-format-trash
	@$(FORMAT_DRIVER) --all --strip-comments --fix --strict --jobs $(FORMAT_JOBS) $(FORMAT_ARGS)
	@$(FORMAT_DRIVER) --all --lint --fix --unsafe-fixes --jobs $(FORMAT_JOBS) --build-dir $(BUILD_DIR) $(FORMAT_ARGS)
	@$(MAKE) --no-print-directory quality LINT_STRIP=

## Verify formatting without writing anything (CI gate).
format-check:
	@$(FORMAT_DRIVER) --all --check --jobs $(FORMAT_JOBS) $(FORMAT_ARGS)

## Format only what changed against FORMAT_BASE (default origin/main).
format-changed: clean-format-trash
	@$(FORMAT_DRIVER) --changed $(FORMAT_BASE) --fix --jobs $(FORMAT_JOBS) $(FORMAT_ARGS)

## Fast pull-request gate: check only the changed files.
format-check-changed:
	@$(FORMAT_DRIVER) --changed $(FORMAT_BASE) --check --jobs $(FORMAT_JOBS) $(FORMAT_ARGS)

## Format the files currently staged in the index.
format-staged:
	@$(FORMAT_DRIVER) --staged --fix --jobs $(FORMAT_JOBS) $(FORMAT_ARGS)

## Report which formatters are installed and whether they match the pins.
format-doctor:
	@$(FORMAT_DRIVER) --doctor

## Install the pinned toolchain (pip/npm) and print system-package hints.
format-bootstrap:
	@$(FORMAT_DRIVER) --bootstrap $(FORMAT_ARGS)

## Remove editor and qmlformat backup droppings.
clean-format-trash:
	@find . \
		\( -path "./.git" -o -path "./$(BUILD_DIR)" -o -path "./$(BUILD_DIR)/*" -o -path "./$(DEBUG_BUILD_DIR)" -o -path "./$(DEBUG_BUILD_DIR)/*" -o -path "./$(BUILD_TIDY_DIR)" -o -path "./$(BUILD_TIDY_DIR)/*" -o -path "./third_party" -o -path "./third_party/*" \) -prune -o \
		-type f \( -name "*~" -o -name ".#*" -o -name "#*#" \) -print -exec rm -f {} +

# ---- Linting (also strips comments via remove-comments.sh) ----
.PHONY: lint lint-fix lint-changed lint-deep

## Run every linter: clang-tidy, qmllint, Ruff, ShellCheck, yamllint, JSON.
lint:
	@$(FORMAT_DRIVER) --all $(LINT_STRIP) --lint --jobs $(FORMAT_JOBS) --build-dir $(BUILD_DIR) $(FORMAT_ARGS)

## Apply the linters' automated fixes. Explicit and separate from `format`.
lint-fix:
	@$(FORMAT_DRIVER) --all --lint --fix --jobs $(FORMAT_JOBS) --build-dir $(BUILD_DIR) $(FORMAT_ARGS)

## Lint only what changed against FORMAT_BASE (pull-request gate).
lint-changed:
	@$(FORMAT_DRIVER) --changed $(FORMAT_BASE) --lint --jobs $(FORMAT_JOBS) --build-dir $(BUILD_DIR) $(FORMAT_ARGS)

## Nightly lane: whole-project clang-tidy, advisory findings become failures.
lint-deep:
	@$(FORMAT_DRIVER) --all --lint --deep --fail-on-advisory --jobs $(FORMAT_JOBS) --build-dir $(BUILD_DIR) $(FORMAT_ARGS)

# ---- Destructive source transformations (never run by `format`) ----
.PHONY: strip-comments format-strip-comments

## DESTRUCTIVE: delete comments from tracked sources. Review the diff.
strip-comments:
	@echo "$(BOLD)$(RED)This rewrites source files and deletes comments.$(RESET)"
	@echo "$(YELLOW)Set STRIP_COMMENTS_CONFIRM=1 to proceed, or run with DRY_RUN=1.$(RESET)"
	@if [ "$(DRY_RUN)" = "1" ]; then \
		$(FORMAT_DRIVER) --all --strip-comments --dry-run; \
	elif [ "$(STRIP_COMMENTS_CONFIRM)" = "1" ]; then \
		$(FORMAT_DRIVER) --all --strip-comments; \
	else \
		echo "$(RED)Refusing to strip comments without STRIP_COMMENTS_CONFIRM=1$(RESET)"; \
		exit 1; \
	fi

# Backwards-compatible alias for the old target name.
format-strip-comments: strip-comments

# ---- Translations ----
# lupdate rescans every qsTr()/tr()/QT_TR_NOOP in the UI and engine sources and
# rewrites the .ts catalogues. `-locations none` keeps the diffs free of the line
# numbers that churn on every unrelated edit.
#
# Compiling .ts to .qm is not done here: CMake runs lrelease into the build tree
# on every build, so the embedded catalogue can never lag the .ts it came from.
.PHONY: translations translations-check

LUPDATE ?= $(shell command -v lupdate 2>/dev/null || echo /usr/lib/qt6/bin/lupdate)
TS_FILES := translations/app_en.ts translations/app_de.ts translations/app_es.ts \
	translations/app_pt_br.ts translations/app_ar.ts
TS_SOURCE_DIRS := ui app game scene render main.cpp
# lupdate only parses code, so player-visible text authored in assets/ (mission
# briefings, objective lines, map and unit names) is mirrored into a generated
# stub it can read. Without this the catalogues silently miss a few hundred
# strings and the coverage gate below still reports success.
TS_ASSET_STUB := translations/asset_strings_generated.cpp

## Rescan sources for translatable strings and refresh the .ts catalogues.
translations:
	@echo "$(BOLD)$(BLUE)Extracting player-visible strings from assets...$(RESET)"
	@$(PYTHON) scripts/extract-asset-strings.py
	@echo "$(BOLD)$(BLUE)Updating translation catalogues...$(RESET)"
	@$(LUPDATE) $(TS_SOURCE_DIRS) $(TS_ASSET_STUB) -no-obsolete -locations none -ts $(TS_FILES)
	@$(PYTHON) scripts/seed-source-translations.py
	@bash scripts/ts2csv.sh > /dev/null
	@echo "$(GREEN)✓ Catalogues and translator CSVs updated (.qm build on next compile)$(RESET)"

## Fail if any UI string is missing from the catalogues or left untranslated.
## Rescans into a scratch copy so it never rewrites the tracked catalogues.
translations-check:
	@echo "$(BOLD)$(BLUE)Checking translation coverage...$(RESET)"
	@$(PYTHON) scripts/extract-asset-strings.py --check
	@tmp=$$(mktemp -d) && trap 'rm -rf "$$tmp"' EXIT && \
	cp $(TS_FILES) "$$tmp/" && \
	probe=""; for ts in $(TS_FILES); do probe="$$probe $$tmp/$$(basename $$ts)"; done && \
	$(LUPDATE) $(TS_SOURCE_DIRS) $(TS_ASSET_STUB) -no-obsolete -locations none -ts $$probe >/dev/null && \
	for ts in $(TS_FILES); do \
		if ! diff -q "$$ts" "$$tmp/$$(basename $$ts)" >/dev/null; then \
			echo "$(RED)$$ts is stale. Run 'make translations'.$(RESET)"; \
			diff -u "$$ts" "$$tmp/$$(basename $$ts)" | head -40; \
			exit 1; \
		fi; \
	done
	@if grep -q 'type="unfinished"' $(TS_FILES); then \
		echo "$(RED)Untranslated strings remain:$(RESET)"; \
		grep -l 'type="unfinished"' $(TS_FILES); \
		exit 1; \
	fi
	@echo "$(GREEN)✓ Every UI string is translated$(RESET)"

# ---- Cross-platform portability ----
#
# The game is developed on Linux/GCC/Mesa and shipped on macOS/AppleClang and
# Windows/MSVC. These two targets look, from Linux, for the constructs that
# only the other two toolchains reject.
#
# portability-lint needs clang, libc++-dev and glslang-tools. Without them the
# passes skip; CI passes --require-all so a missing tool fails there instead.
#
# portability-build is the same warning set applied to a real compile, which is
# the only way to reach code behind #ifdefs and templates that the lint's
# syntax-only pass still covers but a reader might doubt. It builds into
# BUILD_STRICT_DIR so it never disturbs the incremental build in build/.
.PHONY: portability portability-lint portability-build

BUILD_STRICT_DIR := build-strict

## Run the macOS (clang + libc++), GLSL and Windows portability passes.
portability-lint:
	@echo "$(BOLD)$(BLUE)Checking cross-platform portability...$(RESET)"
	@$(PYTHON) scripts/check-portability.py --build-dir $(BUILD_DIR)

## Compile the whole project with the portability warning set promoted to errors.
portability-build:
	@echo "$(BOLD)$(BLUE)Building with SOI_STRICT_WARNINGS...$(RESET)"
	@cmake -S . -B $(BUILD_STRICT_DIR) -G Ninja -DCMAKE_BUILD_TYPE=Release \
		-DSOI_STRICT_WARNINGS=ON -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
	@cmake --build $(BUILD_STRICT_DIR) -j$$(nproc)
	@echo "$(GREEN)✓ Strict build clean$(RESET)"

## Both portability gates.
portability: portability-lint portability-build

# ---- Aggregate gates ----
.PHONY: quality validate hooks-install

## Everything that does not need a compiler: comment stripping, linting, formatting, markers.
quality:
	@$(MAKE) --no-print-directory lint FORMAT_ARGS="$(FORMAT_ARGS) --strict"
	@$(MAKE) --no-print-directory format-check FORMAT_ARGS="$(FORMAT_ARGS) --strict"
	@echo "$(BOLD)$(BLUE)Checking quality markers...$(RESET)"
	@git ls-files -z '*.c' '*.cc' '*.cpp' '*.cxx' '*.h' '*.hpp' '*.py' '*.sh' \
		| xargs -0 -r $(PYTHON) scripts/check-quality-markers.py
	@echo "$(BOLD)$(BLUE)Checking QML typography...$(RESET)"
	@$(PYTHON) scripts/check-typography.py
	@$(PYTHON) scripts/validate_qrc_resources.py
	@echo "$(GREEN)✓ Quality checks passed$(RESET)"

## Full local pre-push gate: quality + build + tests + content validation.
validate: quality build test validate-content
	@echo "$(BOLD)$(BLUE)Validating shader uniforms...$(RESET)"
	@$(PYTHON) scripts/validate_shader_uniforms.py
	@echo "$(BOLD)$(BLUE)Validating OpenGL requirements...$(RESET)"
	@$(PYTHON) scripts/validate_opengl_requirements.py
	@echo "$(GREEN)✓ Validation complete$(RESET)"

## Install the git hooks defined in .pre-commit-config.yaml.
hooks-install:
	@if command -v pre-commit >/dev/null 2>&1; then \
		pre-commit install --install-hooks; \
		echo "$(GREEN)✓ Git hooks installed$(RESET)"; \
	else \
		echo "$(RED)pre-commit not found. Run 'make format-bootstrap' first.$(RESET)"; \
		exit 1; \
	fi

# ---- clang-tidy auto-fixer (heavier, explicit) ----
.PHONY: tidy tidy-all

tidy:
	@echo "$(BOLD)$(BLUE)Running clang-tidy fixes on changed files (vs $${CLANG_TIDY_GIT_BASE:-origin/main})...$(RESET)"
	@bash $(CLANG_TIDY_FIXER) \
		--nice \
		--build-dir="$(BUILD_DIR)" \
		--default-lang="$(DEFAULT_LANG)" \
		$(if $(CLANG_TIDY_JOBS),--jobs="$(CLANG_TIDY_JOBS)") \
		$(if $(CLANG_TIDY_FIX_PATHS),--paths="$(CLANG_TIDY_FIX_PATHS)") \
		$(if $(CLANG_TIDY_AUTO_FIX_CHECKS),--checks="$(CLANG_TIDY_AUTO_FIX_CHECKS)")

tidy-all:
	@echo "$(BOLD)$(BLUE)Running clang-tidy fixes on ALL source files...$(RESET)"
	@bash $(CLANG_TIDY_FIXER) \
		--all \
		--nice \
		--build-dir="$(BUILD_DIR)" \
		--default-lang="$(DEFAULT_LANG)" \
		$(if $(CLANG_TIDY_JOBS),--jobs="$(CLANG_TIDY_JOBS)") \
		$(if $(CLANG_TIDY_FIX_PATHS),--paths="$(CLANG_TIDY_FIX_PATHS)") \
		$(if $(CLANG_TIDY_AUTO_FIX_CHECKS),--checks="$(CLANG_TIDY_AUTO_FIX_CHECKS)")

# Debug build
.PHONY: debug
debug:
	@echo "$(BOLD)$(BLUE)Configuring debug build with GDB support and runtime tracing...$(RESET)"
	@mkdir -p $(DEBUG_BUILD_DIR)
	@cd $(DEBUG_BUILD_DIR) && cmake -DCMAKE_BUILD_TYPE=Debug -DENABLE_CLANG_TIDY=OFF -DDEFAULT_LANG=$(DEFAULT_LANG) ..
	@echo "$(BOLD)$(BLUE)Building debug version...$(RESET)"
	@cd $(DEBUG_BUILD_DIR) && make -j$$(nproc)
	@echo "$(GREEN)✓ Debug build complete$(RESET)"
	@echo "$(BOLD)Debug Info:$(RESET)"
	@echo "  Debug symbols: $(GREEN)Enabled (-g3 -ggdb3)$(RESET)"
	@echo "  Optimizations: $(YELLOW)Disabled (-O0)$(RESET)"
	@echo "  Frame pointers: $(GREEN)Preserved$(RESET)"
	@echo "  Inlining: $(YELLOW)Disabled$(RESET)"
	@echo ""
	@echo "$(BOLD)Run with GDB:$(RESET)"
	@echo "  cd $(DEBUG_BUILD_DIR) && gdb ./bin/$(BINARY_NAME)"
	@echo "  cd $(DEBUG_BUILD_DIR) && gdb --args ./bin/$(BINARY_NAME) [args]"

# Release build
.PHONY: release
release: build-dir
	@echo "$(BOLD)$(BLUE)Configuring release build...$(RESET)"
	@cd $(BUILD_DIR) && cmake -DCMAKE_BUILD_TYPE=Release -DENABLE_CLANG_TIDY=OFF ..
	@cd $(BUILD_DIR) && make -j$$(nproc)
	@echo "$(GREEN)✓ Release build complete$(RESET)"

# Show build info
.PHONY: info
info:
	@echo "$(BOLD)Project Information:$(RESET)"
	@echo "  Build directory: $(BUILD_DIR)"
	@echo "  Binary name: $(BINARY_NAME)"
	@echo "  Map editor: $(MAP_EDITOR_BINARY)"
	@echo "  CMake version: $$(cmake --version | head -1)"
	@echo "  GCC version: $$(gcc --version | head -1)"
	@if [ -f "$(BUILD_DIR)/$(BINARY_NAME)" ]; then \
		echo "  Binary built: $(GREEN)✓$(RESET)"; \
	else \
		echo "  Binary built: $(RED)✗$(RESET)"; \
	fi

# Quick start for new developers
.PHONY: quickstart
quickstart:
	@echo "$(BOLD)$(GREEN)Quick Start Guide:$(RESET)"
	@echo "1. Install dependencies: $(BLUE)make install$(RESET)"
	@echo "2. Build the project: $(BLUE)make build$(RESET)"
	@echo "3. Run the game: $(BLUE)make run$(RESET)"
	@echo "4. Run the arena playground: $(BLUE)make arena$(RESET)"
	@echo ""
	@echo "Or use the shortcut: $(BLUE)make dev$(RESET)"
