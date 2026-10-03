# Included from the Makefile; run make from the repo root.
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

## Run the macOS (clang + libc++), GLSL, Windows and include-graph passes.
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
