# Included from the Makefile; run make from the repo root.
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
	translations/app_pt_br.ts translations/app_ar.ts translations/app_tr.ts \
	translations/app_pl.ts translations/app_ru.ts
TS_SOURCE_DIRS := ui app game scene render main.cpp
# lupdate only parses code, so player-visible text authored in assets/ (mission
# briefings, objective lines, map and unit names) is mirrored into a generated
# stub it can read. Without this the catalogues silently miss a few hundred
# strings and the coverage gate below still reports success.
TS_ASSET_STUB := translations/asset_strings_generated.cpp
# The number heuristic fills patterns like %1/%2 with junk ("%1% {1/%2?}")
# instead of leaving them empty, and that junk blocks the source-language
# seeder, so the entry stays unfinished and trips translations-check forever.
#
# Qt dropped that heuristic after 6.4: on a newer lupdate the same flag is
# rejected outright ("Invalid heuristic name passed to -disable-heuristic"),
# which failed the gate on any machine with a current Qt while passing on an
# older one. Ask lupdate what it accepts rather than assuming.
LUPDATE_NUMBER_HEURISTIC := $(shell $(LUPDATE) -help 2>&1 \
	| grep -q -- 'disable-heuristic.*number' && echo '-disable-heuristic number')
LUPDATE_FLAGS := -no-obsolete -locations none $(LUPDATE_NUMBER_HEURISTIC)

## Rescan sources for translatable strings and refresh the .ts catalogues.
translations:
	@echo "$(BOLD)$(BLUE)Extracting player-visible strings from assets...$(RESET)"
	@$(PYTHON) scripts/extract-asset-strings.py
	@echo "$(BOLD)$(BLUE)Updating translation catalogues...$(RESET)"
	@$(LUPDATE) $(TS_SOURCE_DIRS) $(TS_ASSET_STUB) $(LUPDATE_FLAGS) -ts $(TS_FILES)
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
	$(LUPDATE) $(TS_SOURCE_DIRS) $(TS_ASSET_STUB) $(LUPDATE_FLAGS) -ts $$probe >/dev/null && \
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
