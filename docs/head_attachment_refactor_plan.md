# Head Attachment Refactor Plan

## Overview
The goal is to redesign the humanoid head attachment system so that facial hair and helmets can be added, removed, or customised without duplicating per-unit math. The new architecture pivots around a "head frame" transform that provides a stable local coordinate space for anything mounted on the head.

---

## Step 1 – Build the HeadFrame abstraction
- [x] Extend `HumanoidPose` (or introduce a companion struct) with:
  - origin (QVector3D)
  - orthonormal basis vectors (right, up, forward)
  - uniform head radius
- [x] Populate the frame inside `HumanoidRendererBase::drawCommonBody` right after the head sphere is placed.
- [x] Add helper functions (e.g. `makeHeadLocalTransform(ctxModel, headFrame, localPos)`) that convert head-local offsets (expressed as multiples of the head radius) into world transforms with the correct orientation and translation.
- [x] Ensure immediate head attachments in the base renderer (eyes today, facial hair in Step 2) migrate to the helper and no longer multiply by world-space matrices directly.

## Step 2 – Refactor base beard rendering
- [x] Rewrite `drawFacialHair` so all tufts are created via the HeadFrame helpers.
- [x] Keep beard density logic but express every position as a head-local vector before conversion.
- [x] Guarantee that disabling facial hair (`style == None` or `coverage == 0`) simply early-returns without touching anything else.
- [x] Remove nation-specific beard overrides where possible so nations supply only style parameters, not geometry. *(Verified none exist; base renderer now covers all facial hair geometry.)*

## Step 3 – Standardise helmet attachment helpers
- [x] Introduce head-frame aware primitives in the base renderer (e.g. `makeHeadLocalTransform`, `headLocalPosition`).
- [x] Update existing helmet implementations (starting with Carthage) to call into the helpers using head-local offsets.
- [ ] Port remaining nation helmet renderers to the head-frame helpers.
  - Progress: Carthage (all done); Kingdom archer, spearman, swordsman migrated; Kingdom horse_swordsman and Roman units still pending.
- [ ] Ensure per-style toggles (`show_helmet`, etc.) become simple guard clauses that skip invoking the helpers.
- [ ] Validate that animations (kneeling, leaning, attacking) carry the helmet correctly via the head frame.

## Step 4 – Regression tests and documentation
- [ ] Compile and run the game to inspect standing, kneeling, and attack cycles across at least two nations.
- [ ] Verify that enabling/disabling facial hair or helmets is a data change only (no code edits required).
- [ ] Document the new API in `docs/rendering.md` (or equivalent) so future units follow the same pattern.
