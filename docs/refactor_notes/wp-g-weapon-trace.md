# WP-G: weapon_trace split

`weapon_trace.cpp` (1183 lines) mixed frame maths, baked BPAT socket sampling,
authored-pose fallbacks and the contact search. It is now four units in
`game/systems/combat_actions/`:

| File                            | Owns                                                                                                                                                           |
| ------------------------------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `weapon_trace_sampling.{h,cpp}` | `AttackerFrame`, presented (soldier-carrier) frame, trace window clamp, steered/solved swing, mounted spear, authored-pose fallback, `sample_segment_in_frame` |
| `weapon_trace_baked.{h,cpp}`    | Action to BPAT clip mapping, socket matrix interpolation, sword/spear baked segments                                                                           |
| `weapon_trace.cpp`              | Public API, target-local samples, contact scoring, contact search                                                                                              |

## Invariants kept

- Sampling order in `sample_segment_in_frame`: window reject, clamp, steered
  solved swing (unless mounted), baked socket, mounted spear, mounted stop,
  authored pose. Mounted actions that find no segment must stop there and never
  reach the authored pose.
- `find_weapon_trace_contact` (swept) slices spans over 0.025 by recursing through the
  public overload (as before), then samples one segment, then falls back to the
  static search when no segment is valid.
- Both searches share one scan (`scan_for_best_contact`) and differ only in the
  per-soldier scoring lambda. Entity iteration order, hint bias, ignored id and
  ignored slot checks, and strict `<` best-score replacement are unchanged. The
  lambdas are templates parameters, so the hot path allocates nothing new.
- `contact_speed` is filled only by the swept search, from the tip travel over the
  slice duration.

## Left as is

`combat_action_definition.cpp` stays one cohesive constexpr table; its lookup and
timing helpers were not split.

Tests: `tests/systems/weapon_trace_test.cpp` (frames, window, determinism, static
and swept contact, ignore lists, hint bias, slicing).
