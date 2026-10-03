# ---- Audio ----
# Included from the Makefile; every target here is run from the repo root.
#
# Re-render the synthesised cue sounds and re-register them. The recipes in
# tools/audio_synth are the source of truth for these files, so edit a recipe
# and run this rather than hand-editing an .ogg. Needs ffmpeg with libvorbis.
.PHONY: audio-assets
audio-assets:
	@echo "$(BOLD)$(BLUE)Synthesising cue sounds...$(RESET)"
	@$(PYTHON) tools/audio_synth/synthesize_cues.py
	@$(PYTHON) tools/audio_synth/register_cues.py
	@$(MAKE) --no-print-directory audio-report
	@echo "$(GREEN)✓ Cue sounds rendered and registered$(RESET)"

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

.PHONY: audio-preview
audio-preview:
	@echo "$(BOLD)$(BLUE)Rendering audio mastering preview...$(RESET)"
	@cmake --build $(BUILD_DIR) -j$$(nproc) --target audio_master_preview
	@$(BUILD_DIR)/bin/audio_master_preview --out artifacts/audio_preview $(AUDIO_PREVIEW_ARGS)

## Audit the cue catalogue, the manifest and the files on disk.
# Writes artifacts/audio/AUDIO_WISHLIST.md; run it any time you want the current
# list of missing sounds.
.PHONY: audio-report
audio-report:
	@echo "$(BOLD)$(BLUE)Auditing game audio...$(RESET)"
	@$(PYTHON) scripts/audio_report.py
	@echo "$(GREEN)✓ Audio report written to artifacts/audio/AUDIO_WISHLIST.md$(RESET)"

# Same audit as a gate: fails when a cue, a manifest entry and a file disagree.
.PHONY: audio-check
audio-check:
	@echo "$(BOLD)$(BLUE)Checking game audio wiring...$(RESET)"
	@$(PYTHON) scripts/audio_report.py --stdout --check > /dev/null
	@$(PYTHON) scripts/audio_validate.py
	@$(PYTHON) scripts/audio_provenance.py --check
	@echo "$(GREEN)✓ Audio cue, manifest and asset links are consistent$(RESET)"

## Propose an import for anything dropped in "new sfx/". Writes nothing.
# Pass AUDIO_IMPORT_ARGS=--apply once the printed proposal is what you want.
.PHONY: audio-import
audio-import:
	@echo "$(BOLD)$(BLUE)Reading new sound effects...$(RESET)"
	@$(PYTHON) scripts/audio_import.py $(AUDIO_IMPORT_ARGS)

## Decode every shipped clip and report leading silence and boundary clicks.
# Reports only: an asset is never rewritten by a script.
.PHONY: audio-scan
audio-scan:
	@echo "$(BOLD)$(BLUE)Scanning shipped audio for silence and clicks...$(RESET)"
	@$(PYTHON) scripts/audio_validate.py --scan
