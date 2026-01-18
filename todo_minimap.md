# Minimap TODO

- Avoid invalidating the minimap every frame; only emit `minimap_image_changed` when content actually changes.
- Replace hard-coded 225° minimap orientation with map/camera yaw so north and camera alignment are accurate.
- Use owner/registry player colors instead of fixed minimap team colors.
- Respect fog-of-war when rendering units (hide enemies outside visible/revealed areas).
- Render missing terrain features such as forests and terrain rivers (the `terrain` river paths).
- Reduce duplication by unifying minimap fog logic with `fog_of_war_mask` where possible.
- Improve the cartographic style of the minimap drawing (linework, labels, texture balance, overall beauty).
- Investigate and fix the persistent RGB split artifact near the river (bottom-left), likely deterministic render/data issue.
- Do not render small elements like fire camps on the minimap.
