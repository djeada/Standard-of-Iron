# Renderer Modernization Roadmap

## 1. Core Humanoid Platform
- **Refactor Interface**: extract a single public `humanoid::Rig` facade (header + implementation) that owns animation state sampling, limb chain solvers, and attachment hooks. Expose high-level APIs (`configure_pose`, `attach_item`, `apply_nation_overrides`) so troop renderers stop inheriting and poking internals.
- **Reusable Motion Modules**: split current ad-hoc logic into composable blocks (stances, locomotion profiles, melee/ranged actions, kneel/brace, sit/mount). Each module should be data-driven (JSON or C++ tables) for easy tuning.
- **State Machine Support**: provide a light animation-state descriptor (idle, walk, attack, defend, mount, death, celebratory). Ensure transitions interpolate positions/rotations for smoothness and allow parameterized variations (e.g., weapon length).
- **Attachment Slots**: define explicit mount points (hands, back, hip, shoulders, head) that expose transforms per frame. Allow multiple attachments per slot with layering order and blending (for mixing robes + armor + insignia).
- **Procedural IK Utilities**: move elbow/knee solving, aim constraints, and recoil damping to self-contained helpers reusable by troops and potential future animations (jumping, climbing).

## 2. Horse / Mount Base
- **Mount Rig API**: mirror the humanoid facade with a `mount::Rig` that handles gait blending (walk/trot/gallop), turning lean, rearing, and idle behaviors.
- **Rider Coupling**: provide helper hooks so humanoid rigs can query saddle pose, stirrup offsets, and adapt posture dynamically (e.g., lance couching vs. bow drawing).
- **Attachment Points**: expose tack points (saddle, reins, banner poles) and allow nation/unit styles to plug in custom ornaments.

## 3. Troop Renderer Architecture
- **Factory Pipeline**: standardize the build sequence: (1) instantiate base rig (humanoid or mount), (2) apply troop-class template (weapon set, armor layout, emblem spec), (3) apply nation overrides (palette, insignia, attachments), (4) apply runtime context (player tint, formation stance).
- **Data-Driven Styles**: move style definitions (colors, attachments, proportions) into dedicated descriptive files (YAML/JSON). Provide validation + fallback logic to keep runtime robust.
- **Shader Coordination**: ensure renderers tag draw calls with material profiles so the shader pipeline can swap variants (metallic, cloth, emissive). Reserve IDs for future nation-specific shaders.
- **LOD & Instancing**: add LOD metadata to troop classes (mesh complexity, animation detail) and make renderers respect instancing hints for large formations.

## 4. Nation Customization Layer
- **Style Registry**: keep the per-nation style registries but back them with data assets so designers can add nations without recompiling. Include hierarchical overrides (base → troop → nation → scenario).
- **Palette Blending Controls**: expose blend weights and masks (e.g., keep helmets nation-colored but tunics team-colored). Document best practices to keep units readable.
- **Accessory Library**: support shared accessory descriptors (e.g., Roman crest types, Carthaginian cloaks) that can be reused across troop classes.

## 5. Animation & Interaction Enhancements
- **Motion Variance**: integrate procedural noise (micro head turns, breathing, shield settling) to reduce uniformity. Allow specifying variance ranges per troop/nation.
- **Contextual Poses**: provide dedicated pose presets for formation states (testudo, shield wall, skirmish) and environmental interactions (boarding ships, using siege gear).
- **Death / Impact Hooks**: expose hooks for physics impulses so unit renderers can react to ragdoll or impact systems without bespoke code.

## 6. Tooling & Workflow
- **Visualizer Tool**: create a debug UI (Qt/ImGui) to preview rig states, swap styles, and validate nation overrides live. Essential for rapid iteration.
- **Unit Test Harness**: add render snapshot tests (headless GL) to ensure future refactors do not break silhouettes or tint blending.
- **Documentation**: author developer docs describing the layering (base rig → troop template → nation override → runtime tint) with examples for adding new troops or nations.

## 7. Incremental Adoption Plan
- **Phase 1**: extract current humanoid renderer logic into the `humanoid::Rig` without changing behavior; migrate archer/swordsman/spearman to the facade.
- **Phase 2**: move horse logic into `mount::Rig`, refactor mounted knight to use the new API, and unify rider coupling.
- **Phase 3**: shift style data into external assets, update registries to hot-load from disk.
- **Phase 4**: implement advanced animation modules (stance library, IK helpers, contextual poses) and roll out across troop classes.
- **Phase 5**: integrate visualization tooling and automated tests to lock in quality.

This roadmap keeps the renderer stack modular, testable, and ready for additional troop types or playable factions while maintaining strong separation between base rigs, troop templates, and nation flavor.
