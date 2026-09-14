# Audio Licences

Where every `.ogg` under `assets/audio/` came from, and under what licence. For how the files are used, see [AUDIO_SYSTEM.md](AUDIO_SYSTEM.md).

[`THIRD_PARTY_LICENSES.md`](../THIRD_PARTY_LICENSES.md) ships inside every release package and stays authoritative for attribution wording. This page answers the narrower question "where did this file come from, and may we ship it".

## Where the rows come from

Every track in `assets/audio/audio_manifest.json` carries a `provenance` block, and the tables below are rendered from those blocks by `python3 scripts/audio_provenance.py --doc`. `make audio-check` fails when a track has no provenance, when an `.ogg` ships without a manifest entry, or when this page no longer matches the manifest. To add or change a file, edit its provenance block and re-render; never edit the tables by hand.

The blocks were written from the record that produced each file:

| Files                                    | Record                                                                                         |
| ---------------------------------------- | ---------------------------------------------------------------------------------------------- |
| Synthesised cues and beds                | `RECIPES` in `tools/audio_synth/cues.py`, and `tools/audio_synth/ambience.py`                  |
| Recorded beds                            | `BEDS` in `tools/audio_field/sources.py` (`build_beds.py --list`)                              |
| Sliced one-shots                         | `TAKES` in `tools/audio_field/oneshots.py` (`build_oneshots.py --list`)                        |
| Composed battle cues                     | `CUES` in `tools/audio_field/battle.py` (`build_battle.py --list`)                             |
| ElevenLabs cues                          | `PLAN`, or `UNNAMED_IMPORTS` for imports older than it, in `tools/audio_import/import_cues.py` |
| ElevenLabs music                         | `BATCHES` in `tools/audio_import/import_music.py`                                              |
| Wolf howls and growls, death cry, voices | `THIRD_PARTY_LICENSES.md`; no recipe for these exists in the tree                              |

`TDC` in the tables is The Designer's Choice UCS Collection by Nicholas A. Judy (`archive.org/details/Designers-Choice-Collection-<VOLUME>`). A sliced one-shot lists every recording its builder may take hits from; a composed cue lists every recording layered into it.

## Obligations

Only `sfx/wildlife/wolf_growl_low.ogg` and `sfx/wildlife/wolf_pack_attack.ogg` need attribution. It is in `THIRD_PARTY_LICENSES.md`, which the AppImage, macOS and Windows packages copy next to the binary:

> Growl recordings from Faragó T, Pongrácz P, Miklósi Á, Huber L, Virányi Z,
> Range F (2010), "Dogs' Expectation about Signalers' Body Size by Virtue of
> Their Growls", PLOS ONE, DOI 10.1371/journal.pone.0015175, supporting audio
> S2 and S3, used under [CC BY 2.5](https://creativecommons.org/licenses/by/2.5).
> Pitched down, trimmed, layered and loudness-normalised for this project.

Replacing either file, or dropping that paragraph from `THIRD_PARTY_LICENSES.md` while either still ships, is what would break the licence.

The ElevenLabs renders were generated under a licence held by the project author that permits commercial use. Everything else is the project's own work, CC0 or public domain. No shipped audio restricts commercial distribution.

## Known gaps

- Six ElevenLabs cues were imported before render names were recorded. `UNNAMED_IMPORTS` in `import_cues.py` records the PR, the date and what each replaced; the render names themselves are gone.
- The voices have no in-tree record beyond the statement in `THIRD_PARTY_LICENSES.md` that the project author recorded all thirty-one.

## Per file

<!-- Rendered by scripts/audio_provenance.py --doc from the manifest's provenance blocks. Edit those, not the tables below. -->

<!-- prettier-ignore-start -->

### Summary

| Licence | Files |
| --- | ---: |
| ElevenLabs licence held by the project author; commercial use permitted | 74 |
| Own work (MIT) | 68 |
| CC0 1.0 | 67 |
| Public Domain Mark 1.0 | 4 |
| Public Domain Mark 1.0 + CC0 1.0 | 4 |
| Public domain | 3 |
| CC BY 2.5 (attribution required) | 2 |
| CC0 1.0 + Public Domain Mark 1.0 | 1 |
| Public Domain Mark 1.0 + Public domain | 1 |

### `ambience/`

| File | Origin | Licence |
| --- | --- | --- |
| `alpine_mountain_pass.ogg` | Cut by `tools/audio_field/build_beds.py` from radio aporee ::: maps, Pic du Canigou, France, 'Sound of Wind on top of the Canigou Mountain' | Public Domain Mark 1.0 |
| `battlefield_dry_wind_distant_march_01.ogg` | Cut by `tools/audio_field/build_beds.py` from radio aporee ::: maps, 'dry hillside, parched wind'; TDC FOOTSTEPS, '…MCU_Running, Rocky Road' | Public Domain Mark 1.0 + CC0 1.0 |
| `battlefield_dry_wind_distant_march_02.ogg` | Cut by `tools/audio_field/build_beds.py` from radio aporee ::: maps, 'dry hillside, parched wind'; TDC FOOTSTEPS, '…CU_Footsteps, Rocky Surface' | Public Domain Mark 1.0 + CC0 1.0 |
| `burning_village_aftermath.ogg` | Synthesised by `tools/audio_synth/ambience.py` | Own work (MIT) |
| `camp_fire_night.ogg` | Cut by `tools/audio_field/build_beds.py` from Wikimedia Commons, File:Dry grass burning in open fireplace.ogg, by ezwa | Public domain |
| `carthage_war_camp_01.ogg` | Synthesised by `tools/audio_synth/ambience.py` | Own work (MIT) |
| `carthage_war_camp_02.ogg` | Synthesised by `tools/audio_synth/ambience.py` | Own work (MIT) |
| `desert_army_march.ogg` | Cut by `tools/audio_field/build_beds.py` from radio aporee ::: maps, Cafe Tissardmine, Morocco, 'Desert Wind'; TDC FOOTSTEPS, '…MCU_Running, Rocky Road' | Public Domain Mark 1.0 + CC0 1.0 |
| `forest_ambush.ogg` | Cut by `tools/audio_field/build_beds.py` from radio aporee ::: maps, Planina Razor, Tolmin, Slovenia, 'Birds in forest'; TDC WIND, 'CU_Thru Trees, Rustling, Faint Crickets' | Public Domain Mark 1.0 + CC0 1.0 |
| `mediterranean_city_market.ogg` | Synthesised by `tools/audio_synth/ambience.py` | Own work (MIT) |
| `mediterranean_harbor.ogg` | Synthesised by `tools/audio_synth/ambience.py` | Own work (MIT) |
| `mediterranean_plains.ogg` | Cut by `tools/audio_field/build_beds.py` from radio aporee ::: maps, Dörflis, Naturpark Haßberge, Germany, 'Wild Meadow Summer'; radio aporee ::: maps, Pic du Canigou, France, 'Sound of Wind on top of the Canigou Mountain' | Public Domain Mark 1.0 |
| `mountain_camp_night.ogg` | Cut by `tools/audio_field/build_beds.py` from radio aporee ::: maps, Pod Lipą, Borsuki, Poland, '(835a AB) midnight crickets'; Wikimedia Commons, File:Dry grass burning in open fireplace.ogg, by ezwa | Public Domain Mark 1.0 + Public domain |
| `river_crossing.ogg` | Cut by `tools/audio_field/build_beds.py` from radio aporee ::: maps, Joneliškės, Lithuania, 'river Viesa' | Public Domain Mark 1.0 |
| `roman_army_camp_01.ogg` | Synthesised by `tools/audio_synth/ambience.py` | Own work (MIT) |
| `roman_army_camp_02.ogg` | Synthesised by `tools/audio_synth/ambience.py` | Own work (MIT) |
| `roman_road.ogg` | Cut by `tools/audio_field/build_beds.py` from TDC FOOTSTEPS, '…CU_Footsteps, Rocky Surface'; radio aporee ::: maps, 'dry hillside, parched wind' | CC0 1.0 + Public Domain Mark 1.0 |
| `siege_camp.ogg` | Synthesised by `tools/audio_synth/ambience.py` | Own work (MIT) |
| `storm.ogg` | Cut by `tools/audio_field/build_beds.py` from Own recording, Karlsruhe, 16 July 2026 | Own work (MIT) |
| `weather_rain.ogg` | Cut by `tools/audio_field/build_beds.py` from TDC RAIN, 'CU_Raining' | CC0 1.0 |
| `weather_snow.ogg` | Cut by `tools/audio_field/build_beds.py` from TDC WIND, 'CU_Blizzard, Old Recording' | CC0 1.0 |

### `music/base/`

| File | Origin | Licence |
| --- | --- | --- |
| `base_ancient_peak_fires.ogg` | ElevenLabs render `Ancient_Peak_Fires_2026-08-28T210704`, batch `2026-08-punic` of `tools/audio_import/import_music.py` | ElevenLabs licence held by the project author; commercial use permitted |
| `base_ancient_preparations.ogg` | ElevenLabs render `Ancient_Preparations_2026-08-28T205218`, batch `2026-08-punic` of `tools/audio_import/import_music.py` | ElevenLabs licence held by the project author; commercial use permitted |
| `base_campfire_shadows_carthage.ogg` | ElevenLabs render `Campfire_Shadows_of_Carthage_2026-08-28T204014`, batch `2026-08-punic` of `tools/audio_import/import_music.py` | ElevenLabs licence held by the project author; commercial use permitted |
| `base_echoes_ancient_outpost.ogg` | ElevenLabs render `Echoes_of_the_Ancient_Outpost_2026-08-28T210704`, batch `2026-08-punic` of `tools/audio_import/import_music.py` | ElevenLabs licence held by the project author; commercial use permitted |
| `base_hearth_and_harbor.ogg` | ElevenLabs render `Hearth_and_Harbor_of_Old_2026-08-28T204417`, batch `2026-08-punic` of `tools/audio_import/import_music.py` | ElevenLabs licence held by the project author; commercial use permitted |
| `base_legion_at_dusk.ogg` | ElevenLabs render `Echoes_of_the_Legion_at_Dusk_2026-08-28T205219`, batch `2026-08-punic` of `tools/audio_import/import_music.py` | ElevenLabs licence held by the project author; commercial use permitted |
| `base_march_of_the_old_gods.ogg` | ElevenLabs render `March_of_the_Old_Gods_2026-09-01T083738`, batch `2026-09-iron` of `tools/audio_import/import_music.py` | ElevenLabs licence held by the project author; commercial use permitted |
| `base_sentinels_of_the_peak.ogg` | ElevenLabs render `Sentinels_of_the_Peak_2026-08-28T210721`, batch `2026-08-punic` of `tools/audio_import/import_music.py` | ElevenLabs licence held by the project author; commercial use permitted |
| `base_sunlight_olive_groves.ogg` | ElevenLabs render `Sunlight_Over_the_Olive_Groves_2026-08-28T204417`, batch `2026-08-punic` of `tools/audio_import/import_music.py` | ElevenLabs licence held by the project author; commercial use permitted |
| `base_uneasy_rest_punic_camp.ogg` | ElevenLabs render `Uneasy_Rest_at_the_Punic_Camp_2026-08-28T204959`, batch `2026-08-punic` of `tools/audio_import/import_music.py` | ElevenLabs licence held by the project author; commercial use permitted |
| `base_uneasy_rest_punic_camp_alt.ogg` | ElevenLabs render `Uneasy_Rest_at_the_Punic_Camp_2026-08-28T204014`, batch `2026-08-punic` of `tools/audio_import/import_music.py` | ElevenLabs licence held by the project author; commercial use permitted |

### `music/campaign/`

| File | Origin | Licence |
| --- | --- | --- |
| `campaign_crossing_of_the_alps.ogg` | ElevenLabs render `The_Crossing_of_the_Alps_2026-08-28T204617`, batch `2026-08-punic` of `tools/audio_import/import_music.py` | ElevenLabs licence held by the project author; commercial use permitted |
| `campaign_hannibals_ascent.ogg` | ElevenLabs render `Hannibals_Ascent_2026-08-28T204617`, batch `2026-08-punic` of `tools/audio_import/import_music.py` | ElevenLabs licence held by the project author; commercial use permitted |

### `music/combat/`

| File | Origin | Licence |
| --- | --- | --- |
| `combat_carthaginian_dust.ogg` | ElevenLabs render `Carthaginian_Dust_2026-08-28T205446`, batch `2026-08-punic` of `tools/audio_import/import_music.py` | ElevenLabs licence held by the project author; commercial use permitted |
| `combat_cavalry_of_carthage.ogg` | ElevenLabs render `Cavalry_of_Carthage_2026-08-28T205444`, batch `2026-08-punic` of `tools/audio_import/import_music.py` | ElevenLabs licence held by the project author; commercial use permitted |
| `combat_dust_of_cannae.ogg` | ElevenLabs render `The_Dust_of_Cannae_2026-08-28T210020`, batch `2026-08-punic` of `tools/audio_import/import_music.py` | ElevenLabs licence held by the project author; commercial use permitted |
| `combat_dust_of_trasimene.ogg` | ElevenLabs render `Dust_of_Trasimene_2026-08-28T210020`, batch `2026-08-punic` of `tools/audio_import/import_music.py` | ElevenLabs licence held by the project author; commercial use permitted |
| `combat_last_defensive_wall.ogg` | ElevenLabs render `The_Last_Defensive_Wall_2026-08-28T210210`, batch `2026-08-punic` of `tools/audio_import/import_music.py` | ElevenLabs licence held by the project author; commercial use permitted |
| `combat_shield_wall_at_dusk.ogg` | ElevenLabs render `The_Shield_Wall_at_Dusk_2026-08-28T210210`, batch `2026-08-punic` of `tools/audio_import/import_music.py` | ElevenLabs licence held by the project author; commercial use permitted |
| `combat_siege_at_dawn.ogg` | ElevenLabs render `Siege_at_Dawn_2026-09-01T083842`, batch `2026-09-iron` of `tools/audio_import/import_music.py` | ElevenLabs licence held by the project author; commercial use permitted |

### `music/events/`

| File | Origin | Licence |
| --- | --- | --- |
| `skeletons_awaken.ogg` | ElevenLabs render `Guardians_of_the_Forest_Throne_2026-08-29T075035`, batch `2026-08-punic` of `tools/audio_import/import_music.py` | ElevenLabs licence held by the project author; commercial use permitted |

### `music/menu/`

| File | Origin | Licence |
| --- | --- | --- |
| `main_theme_iron_kingdom.ogg` | ElevenLabs render `Iron_Kingdom_2026-09-01T083636`, batch `2026-09-iron` of `tools/audio_import/import_music.py` | ElevenLabs licence held by the project author; commercial use permitted |
| `main_theme_standard_of_iron.ogg` | ElevenLabs render `Standard_of_Iron_Main_Theme_2026-08-28T210251`, batch `2026-08-punic` of `tools/audio_import/import_music.py` | ElevenLabs licence held by the project author; commercial use permitted |
| `main_theme_standard_of_iron_alt.ogg` | ElevenLabs render `Standard_of_Iron_Main_Theme_ALT`, batch `2026-08-punic` of `tools/audio_import/import_music.py` | ElevenLabs licence held by the project author; commercial use permitted |

### `music/stingers/`

| File | Origin | Licence |
| --- | --- | --- |
| `defeat_echo.ogg` | ElevenLabs render `Echo_of_Defeat_2026-08-28T210512`, batch `2026-08-punic` of `tools/audio_import/import_music.py` | ElevenLabs licence held by the project author; commercial use permitted |
| `defeat_quiet_field.ogg` | ElevenLabs render `The_Quiet_Field_2026-08-28T210427`, batch `2026-08-punic` of `tools/audio_import/import_music.py` | ElevenLabs licence held by the project author; commercial use permitted |
| `defeat_tragic_silence_field.ogg` | ElevenLabs render `Tragic_Silence_on_the_Field_2026-08-28T210427`, batch `2026-08-punic` of `tools/audio_import/import_music.py` | ElevenLabs licence held by the project author; commercial use permitted |
| `retreat_signal_into_wind.ogg` | ElevenLabs render `Signal_Into_Wind_2026-08-28T210511`, batch `2026-08-punic` of `tools/audio_import/import_music.py` | ElevenLabs licence held by the project author; commercial use permitted |
| `victory_carthage_triumph.ogg` | ElevenLabs render `Triumph_of_Carthage_2026-08-28T210547`, batch `2026-08-punic` of `tools/audio_import/import_music.py` | ElevenLabs licence held by the project author; commercial use permitted |
| `victory_fanfare.ogg` | ElevenLabs render `The_Gates_of_Carthage_2026-08-28T210547`, batch `2026-08-punic` of `tools/audio_import/import_music.py` | ElevenLabs licence held by the project author; commercial use permitted |

### `sfx/alerts/`

| File | Origin | Licence |
| --- | --- | --- |
| `commander_message.ogg` | ElevenLabs render `The_Commanders_Folly`, imported by `tools/audio_import/import_cues.py` | ElevenLabs licence held by the project author; commercial use permitted |
| `enemy_reinforcements_warning.ogg` | ElevenLabs render `Horn_of_the_Enemy`, imported by `tools/audio_import/import_cues.py` | ElevenLabs licence held by the project author; commercial use permitted |
| `enemy_spotted_horn.ogg` | Composed by `tools/audio_field/build_battle.py` from Wikimedia Commons, 'File:Hunting horn tone.ogg', by Alon-De-Lon | CC0 1.0 |
| `low_resources_click.ogg` | ElevenLabs render `The_Demand_of_the_Builders`, imported by `tools/audio_import/import_cues.py` | ElevenLabs licence held by the project author; commercial use permitted |
| `objective_complete.ogg` | ElevenLabs render imported in #1366; its name was not kept (`UNNAMED_IMPORTS`) | ElevenLabs licence held by the project author; commercial use permitted |
| `objective_failed.ogg` | ElevenLabs render `Objective_Failed`, imported by `tools/audio_import/import_cues.py` | ElevenLabs licence held by the project author; commercial use permitted |
| `population_limit_horn.ogg` | ElevenLabs render `No_Peasants_Remain`, imported by `tools/audio_import/import_cues.py` | ElevenLabs licence held by the project author; commercial use permitted |
| `reinforcements_arrived.ogg` | Composed by `tools/audio_field/build_battle.py` from Wikimedia Commons, 'File:Hunting horn tone.ogg', by Alon-De-Lon | CC0 1.0 |
| `unit_lost.ogg` | ElevenLabs render `Unit_Lost_Signal`, imported by `tools/audio_import/import_cues.py` | ElevenLabs licence held by the project author; commercial use permitted |

### `sfx/build/`

| File | Origin | Licence |
| --- | --- | --- |
| `building_burning.ogg` | ElevenLabs render `Timber_Structure_Inferno`, imported by `tools/audio_import/import_cues.py` | ElevenLabs licence held by the project author; commercial use permitted |
| `building_destroyed.ogg` | Synthesised by `tools/audio_synth/cues.py`, recipe `build.building_destroyed` | Own work (MIT) |
| `construction_complete.ogg` | ElevenLabs render imported in #1366; its name was not kept (`UNNAMED_IMPORTS`) | ElevenLabs licence held by the project author; commercial use permitted |
| `construction_started.ogg` | ElevenLabs render imported in #1366; its name was not kept (`UNNAMED_IMPORTS`) | ElevenLabs licence held by the project author; commercial use permitted |
| `gate_close.ogg` | Synthesised by `tools/audio_synth/cues.py`, recipe `build.gate_close` | Own work (MIT) |
| `gate_open.ogg` | Synthesised by `tools/audio_synth/cues.py`, recipe `build.gate_open` | Own work (MIT) |
| `placement_begin.ogg` | Synthesised by `tools/audio_synth/cues.py`, recipe `build.placement_begin` | Own work (MIT) |
| `placement_confirmed.ogg` | ElevenLabs render `Hammers_and_Nails`, imported by `tools/audio_import/import_cues.py` | ElevenLabs licence held by the project author; commercial use permitted |
| `placement_rejected.ogg` | ElevenLabs render imported in #1366; its name was not kept (`UNNAMED_IMPORTS`) | ElevenLabs licence held by the project author; commercial use permitted |
| `unit_queued.ogg` | Synthesised by `tools/audio_synth/cues.py`, recipe `build.unit_queued` | Own work (MIT) |
| `unit_queued_v2.ogg` | Synthesised by `tools/audio_synth/cues.py`, recipe `build.unit_queued` | Own work (MIT) |
| `unit_ready_bell.ogg` | ElevenLabs render `Resonance_in_the_Courtyard`, imported by `tools/audio_import/import_cues.py` | ElevenLabs licence held by the project author; commercial use permitted |

### `sfx/combat/`

| File | Origin | Licence |
| --- | --- | --- |
| `ability_refused.ogg` | Composed by `tools/audio_field/build_battle.py` from TDC METAL, 'Metal, Clang, Dull, Quiet' | CC0 1.0 |
| `aftermath_battlefield.ogg` | Composed by `tools/audio_field/build_battle.py` from TDC CROWDS, 'CU_Crowd Applause, Cheering, Yelling, Whooping'; TDC WIND, 'CU_Thru Trees, Rustling, Faint Crickets' | CC0 1.0 |
| `armour_hit_01.ogg` | Sliced by `tools/audio_field/build_oneshots.py` from TDC METAL, 'Metal, Clang, Thin 01'; TDC METAL, 'Metal, Clang, Thin 02'; TDC METAL, 'Metal, Clank, Thin' | CC0 1.0 |
| `armour_hit_02.ogg` | Sliced by `tools/audio_field/build_oneshots.py` from TDC METAL, 'Metal, Clang, Thin 01'; TDC METAL, 'Metal, Clang, Thin 02'; TDC METAL, 'Metal, Clank, Thin' | CC0 1.0 |
| `armour_hit_03.ogg` | Sliced by `tools/audio_field/build_oneshots.py` from TDC METAL, 'Metal, Clang, Thin 01'; TDC METAL, 'Metal, Clang, Thin 02'; TDC METAL, 'Metal, Clank, Thin' | CC0 1.0 |
| `army_march_dirt_mass.ogg` | Composed by `tools/audio_field/build_battle.py` from TDC FOOTSTEPS, 'MCU_Running, Rocky Road'; TDC FOOTSTEPS, 'MCU_Footsteps, On Grass' | CC0 1.0 |
| `army_retreat_panic.ogg` | Composed by `tools/audio_field/build_battle.py` from TDC CROWDS, 'CU_Crowd Applause, Cheering, Yelling, Whooping'; TDC FOOTSTEPS, 'MCU_Running, Rocky Road' | CC0 1.0 |
| `arrow_impact_01.ogg` | Sliced by `tools/audio_field/build_oneshots.py` from TDC WOOD, 'CU_Board Drop 03'; TDC WOOD, 'CU_Board Drop 04'; TDC WOOD, 'CU_Board Drop 05' | CC0 1.0 |
| `arrows_many_overhead.ogg` | Composed by `tools/audio_field/build_battle.py` from TDC SWOOSHES, 'MCU_Swishes, Medium Low'; TDC SWOOSHES, 'CU_Fly By, Short' | CC0 1.0 |
| `arrows_overhead_dark.ogg` | Composed by `tools/audio_field/build_battle.py` from TDC SWOOSHES, 'CU_Swishes, Big, Low' | CC0 1.0 |
| `battlefield_crowd_chaos.ogg` | Composed by `tools/audio_field/build_battle.py` from TDC CROWDS, 'CU_Crowd Applause, Cheering, Yelling, Whooping'; TDC FOOTSTEPS, 'MCU_Running, Rocky Road' | CC0 1.0 |
| `battlefield_distant_mass_01.ogg` | Composed by `tools/audio_field/build_battle.py` from TDC CROWDS, 'CU_Crowd Applause, Cheering, Yelling, Whooping'; TDC WIND, 'CU_Thru Trees, Rustling, Faint Crickets' | CC0 1.0 |
| `blade_clash_01.ogg` | Sliced by `tools/audio_field/build_oneshots.py` from TDC WEAPONS, 'Sword, Hits, Scrapes, Shings' | CC0 1.0 |
| `blade_clash_02.ogg` | Sliced by `tools/audio_field/build_oneshots.py` from TDC WEAPONS, 'Sword, Hits, Scrapes, Shings' | CC0 1.0 |
| `blade_clash_03.ogg` | Sliced by `tools/audio_field/build_oneshots.py` from TDC WEAPONS, 'Sword, Hits, Scrapes, Shings' | CC0 1.0 |
| `bow_draw_creak.ogg` | Composed by `tools/audio_field/build_battle.py` from TDC WOOD, 'CU_Floorboard, Creak' | CC0 1.0 |
| `bow_full_draw_seat.ogg` | Composed by `tools/audio_field/build_battle.py` from TDC METAL, 'CU_Bolt, Drop'; TDC WOOD, 'CU_Floorboard, Creak' | CC0 1.0 |
| `bow_hold_strain.ogg` | Composed by `tools/audio_field/build_battle.py` from TDC WOOD, 'CU_Ship, Creaking, Sound Design' | CC0 1.0 |
| `bow_release_single.ogg` | Composed by `tools/audio_field/build_battle.py` from TDC SWOOSHES, 'CU_Stick, Small, Swishes, X4'; TDC METAL, 'CU_Bolt, Drop' | CC0 1.0 |
| `carthage_prepare_battle.ogg` | Composed by `tools/audio_field/build_battle.py` from TDC CROWDS, 'CU_Crowd Applause, Cheering, Yelling, Whooping'; Wikimedia Commons, 'File:Hunting horn tone.ogg', by Alon-De-Lon | CC0 1.0 |
| `charge_roar.ogg` | Composed by `tools/audio_field/build_battle.py` from TDC CROWDS, 'CU_Crowd Applause, Cheering, Yelling, Whooping'; TDC FOOTSTEPS, 'MCU_Running, Rocky Road' | CC0 1.0 |
| `dodge_roll.ogg` | Composed by `tools/audio_field/build_battle.py` from TDC CLOTH, 'CU_Swish, Impact, Fight'; TDC FIGHT, 'CU_Bodyfall, On Grass' | CC0 1.0 |
| `elephant_charge_carthage.ogg` | Composed by `tools/audio_field/build_battle.py` from TDC ANIMALS, 'CU_Elephant Trumpet'; TDC FOOTSTEPS, 'MCU_Running, Rocky Road' | CC0 1.0 |
| `elephant_panic.ogg` | Composed by `tools/audio_field/build_battle.py` from TDC ANIMALS, 'CU_Elephant Trumpet' | CC0 1.0 |
| `elephant_trumpet_charge.ogg` | ElevenLabs render `Charge_of_the_War_Elephant`, imported by `tools/audio_import/import_cues.py` | ElevenLabs licence held by the project author; commercial use permitted |
| `elephant_trumpet_panic.ogg` | ElevenLabs render `Distressed_War_Elephant`, imported by `tools/audio_import/import_cues.py` | ElevenLabs licence held by the project author; commercial use permitted |
| `gladius_shield_impacts_close.ogg` | Composed by `tools/audio_field/build_battle.py` from TDC FIGHT, 'CU_Smacks, Rapid'; TDC METAL, 'Metal, Clang, Thin 01' | CC0 1.0 |
| `guard_break.ogg` | Composed by `tools/audio_field/build_battle.py` from TDC WOOD, 'CU_Stick, Small, Breaks, X3'; TDC METAL, 'Metal, Clang, Thin 01' | CC0 1.0 |
| `guard_raise.ogg` | Composed by `tools/audio_field/build_battle.py` from TDC CLOTH, 'CU_Swish, Impact, Fight'; TDC METAL, 'CU_Bolt, Drop' | CC0 1.0 |
| `heal_bind_wound.ogg` | Composed by `tools/audio_field/build_battle.py` from TDC CLOTH, 'CU_Baseball Mitt, Velcro, Slow' | CC0 1.0 |
| `heal_magic_shimmer.ogg` | ElevenLabs render `Magical_Healing_Sounds`, imported by `tools/audio_import/import_cues.py` | ElevenLabs licence held by the project author; commercial use permitted |
| `horse_gallop_close_pass.ogg` | Composed by `tools/audio_field/build_battle.py` from Wikimedia Commons, 'Six Horses Galloping By', Freesound Community via Pixabay | CC0 1.0 |
| `human_death_cry.ogg` | Internet Archive, `male_scream` | Public Domain Mark 1.0 |
| `human_death_cry_v2.ogg` | ElevenLabs render imported in #1363; its name was not kept (`UNNAMED_IMPORTS`) | ElevenLabs licence held by the project author; commercial use permitted |
| `human_death_cry_v3.ogg` | ElevenLabs render imported in #1363; its name was not kept (`UNNAMED_IMPORTS`) | ElevenLabs licence held by the project author; commercial use permitted |
| `javelin_throw_whoosh.ogg` | Composed by `tools/audio_field/build_battle.py` from TDC SWOOSHES, 'CU_Swishes, Big, Low'; TDC SWOOSHES, 'MCU_Cloth, Swoosh' | CC0 1.0 |
| `jump_effort.ogg` | Composed by `tools/audio_field/build_battle.py` from TDC CLOTH, 'CU_Swish, Impact, Fight'; TDC FOOTSTEPS, 'MCU_Running, Rocky Road' | CC0 1.0 |
| `land_thud.ogg` | Composed by `tools/audio_field/build_battle.py` from TDC FIGHT, 'CU_Bodyfall, On Grass' | CC0 1.0 |
| `land_thud_v2.ogg` | Composed by `tools/audio_field/build_battle.py` from TDC FIGHT, 'CU_Bodyfall, On Grass' | CC0 1.0 |
| `lock_on_tick.ogg` | Composed by `tools/audio_field/build_battle.py` from TDC METAL, 'CU_Small, Tin, Drop' | CC0 1.0 |
| `perfect_guard.ogg` | Composed by `tools/audio_field/build_battle.py` from TDC METAL, 'Metal, Clang, Thin 01' | CC0 1.0 |
| `roman_shield_wall_impact.ogg` | Composed by `tools/audio_field/build_battle.py` from TDC FIGHT, 'CU_Bodyfall, On Grass'; TDC METAL, 'Metal, Clang, Dull, Quiet' | CC0 1.0 |
| `roman_war_horns_orders.ogg` | Composed by `tools/audio_field/build_battle.py` from Wikimedia Commons, 'File:Hunting horn tone.ogg', by Alon-De-Lon | CC0 1.0 |
| `second_wind.ogg` | Composed by `tools/audio_field/build_battle.py` from TDC CLOTH, 'CU_Swish, Impact, Fight'; TDC METAL, 'Metal, Clang, Thin 01' | CC0 1.0 |
| `shield_bash.ogg` | Composed by `tools/audio_field/build_battle.py` from TDC METAL, 'Metal, Clang 01'; TDC CLOTH, 'CU_Glove Slap' | CC0 1.0 |
| `shield_bash_v2.ogg` | Composed by `tools/audio_field/build_battle.py` from TDC METAL, 'Metal, Clang 01'; TDC CLOTH, 'CU_Glove Slap' | CC0 1.0 |
| `shield_block.ogg` | Composed by `tools/audio_field/build_battle.py` from TDC METAL, 'Metal, Clang, Thin 01'; TDC FIGHT, 'CU_Boxing Glove Hits' | CC0 1.0 |
| `siege_launch.ogg` | Composed by `tools/audio_field/build_battle.py` from TDC WOOD, 'CU_Ship, Creaking, Sound Design'; TDC SWOOSHES, 'CU_Rope, Twirling Swishes'; TDC WOOD, 'CU_Stick, Small, Breaks, X3' | CC0 1.0 |
| `soldiers_victory_cheer.ogg` | Composed by `tools/audio_field/build_battle.py` from TDC CROWDS, 'CU_Crowd Applause, Cheering, Yelling, Whooping' | CC0 1.0 |
| `spear_impact_01.ogg` | Sliced by `tools/audio_field/build_oneshots.py` from TDC WOOD, 'CU_Board Drop 01'; TDC WOOD, 'CU_Board Drop 02' | CC0 1.0 |
| `spear_impact_02.ogg` | Sliced by `tools/audio_field/build_oneshots.py` from TDC WOOD, 'CU_Board Drop 01'; TDC WOOD, 'CU_Board Drop 02' | CC0 1.0 |
| `spearmen_formation_advance.ogg` | Composed by `tools/audio_field/build_battle.py` from TDC FOOTSTEPS, 'MCU_Footsteps, On Grass'; TDC MUSICAL, 'CU_Drum, Snare, Military Marching Band' | CC0 1.0 |
| `stagger.ogg` | ElevenLabs render `Armored_Soldier_Stagger`, imported by `tools/audio_import/import_cues.py` | ElevenLabs licence held by the project author; commercial use permitted |
| `stone_impact_01.ogg` | Sliced by `tools/audio_field/build_oneshots.py` from TDC ROCKS, 'CU_Small Stones, Kicked, X4' | CC0 1.0 |
| `sword_hit_01.ogg` | Sliced by `tools/audio_field/build_oneshots.py` from TDC WEAPONS, 'CU_Sword, Hits' | CC0 1.0 |
| `vanguard_rush.ogg` | Composed by `tools/audio_field/build_battle.py` from TDC CROWDS, 'CU_Crowd Applause, Cheering, Yelling, Whooping'; TDC FOOTSTEPS, 'MCU_Running, Rocky Road' | CC0 1.0 |

### `sfx/economy/`

| File | Origin | Licence |
| --- | --- | --- |
| `income_tick.ogg` | ElevenLabs render `Economy_Income_Tick`, imported by `tools/audio_import/import_cues.py` | ElevenLabs licence held by the project author; commercial use permitted |

### `sfx/movement/`

| File | Origin | Licence |
| --- | --- | --- |
| `footstep_grass_01.ogg` | Sliced by `tools/audio_field/build_oneshots.py` from TDC FOOTSTEPS, 'MCU_Footsteps, On Grass' | CC0 1.0 |
| `footstep_grass_02.ogg` | Sliced by `tools/audio_field/build_oneshots.py` from TDC FOOTSTEPS, 'MCU_Footsteps, On Grass' | CC0 1.0 |
| `footstep_grass_03.ogg` | Sliced by `tools/audio_field/build_oneshots.py` from TDC FOOTSTEPS, 'MCU_Footsteps, On Grass' | CC0 1.0 |
| `footstep_grass_04.ogg` | Sliced by `tools/audio_field/build_oneshots.py` from TDC FOOTSTEPS, 'MCU_Footsteps, On Grass' | CC0 1.0 |
| `footstep_run_01.ogg` | Sliced by `tools/audio_field/build_oneshots.py` from TDC FOOTSTEPS, 'MCU_Running, Rocky Road' | CC0 1.0 |
| `footstep_run_02.ogg` | Sliced by `tools/audio_field/build_oneshots.py` from TDC FOOTSTEPS, 'MCU_Running, Rocky Road' | CC0 1.0 |
| `footstep_run_03.ogg` | Sliced by `tools/audio_field/build_oneshots.py` from TDC FOOTSTEPS, 'MCU_Running, Rocky Road' | CC0 1.0 |
| `footstep_run_04.ogg` | Sliced by `tools/audio_field/build_oneshots.py` from TDC FOOTSTEPS, 'MCU_Running, Rocky Road' | CC0 1.0 |
| `footstep_stone_01.ogg` | Sliced by `tools/audio_field/build_oneshots.py` from TDC FOOTSTEPS, 'CU_Footsteps, Rocky Surface' | CC0 1.0 |
| `footstep_stone_02.ogg` | Sliced by `tools/audio_field/build_oneshots.py` from TDC FOOTSTEPS, 'CU_Footsteps, Rocky Surface' | CC0 1.0 |
| `footstep_stone_03.ogg` | Sliced by `tools/audio_field/build_oneshots.py` from TDC FOOTSTEPS, 'CU_Footsteps, Rocky Surface' | CC0 1.0 |
| `footstep_stone_04.ogg` | Sliced by `tools/audio_field/build_oneshots.py` from TDC FOOTSTEPS, 'CU_Footsteps, Rocky Surface' | CC0 1.0 |
| `hooves_gallop.ogg` | ElevenLabs render `Galloping_Hooves`, imported by `tools/audio_import/import_cues.py` | ElevenLabs licence held by the project author; commercial use permitted |
| `hooves_walk.ogg` | ElevenLabs render `HoresForest_Path_Walk`, imported by `tools/audio_import/import_cues.py` | ElevenLabs licence held by the project author; commercial use permitted |

### `sfx/orders/`

| File | Origin | Licence |
| --- | --- | --- |
| `attack_horn_stab.ogg` | ElevenLabs render `Roman_War_Horn_Blast`, imported by `tools/audio_import/import_cues.py` | ElevenLabs licence held by the project author; commercial use permitted |
| `commander_rally.ogg` | ElevenLabs render `The_Commanders_Rally`, imported by `tools/audio_import/import_cues.py` | ElevenLabs licence held by the project author; commercial use permitted |
| `formation_pole_shift.ogg` | Synthesised by `tools/audio_synth/cues.py`, recipe `order.formation` | Own work (MIT) |
| `formation_pole_shift_v2.ogg` | Synthesised by `tools/audio_synth/cues.py`, recipe `order.formation` | Own work (MIT) |
| `formation_standard_planted.ogg` | ElevenLabs render `Standard_Pole_Placement`, imported by `tools/audio_import/import_cues.py` | ElevenLabs licence held by the project author; commercial use permitted |
| `gate_bolt_slide.ogg` | Synthesised by `tools/audio_synth/cues.py`, recipe `order.gate_mode` | Own work (MIT) |
| `guard_spear_taps.ogg` | ElevenLabs render `Guard_Spear_Taps`, imported by `tools/audio_import/import_cues.py` | ElevenLabs licence held by the project author; commercial use permitted |
| `hold_shields_plant.ogg` | ElevenLabs render `Shields_Planted`, imported by `tools/audio_import/import_cues.py` | ElevenLabs licence held by the project author; commercial use permitted |
| `move_kit_shuffle.ogg` | Synthesised by `tools/audio_synth/cues.py`, recipe `order.move` | Own work (MIT) |
| `move_kit_shuffle_v2.ogg` | Synthesised by `tools/audio_synth/cues.py`, recipe `order.move` | Own work (MIT) |
| `move_kit_shuffle_v3.ogg` | Synthesised by `tools/audio_synth/cues.py`, recipe `order.move` | Own work (MIT) |
| `patrol_horn_two_note.ogg` | ElevenLabs render `The_Patrol_Horn`, imported by `tools/audio_import/import_cues.py` | ElevenLabs licence held by the project author; commercial use permitted |
| `rally_banner_peg.ogg` | Synthesised by `tools/audio_synth/cues.py`, recipe `order.rally_set` | Own work (MIT) |
| `run_kit_rattle.ogg` | Synthesised by `tools/audio_synth/cues.py`, recipe `order.run` | Own work (MIT) |
| `run_kit_rattle_v2.ogg` | Synthesised by `tools/audio_synth/cues.py`, recipe `order.run` | Own work (MIT) |
| `stop_drum.ogg` | ElevenLabs render `Damped_War_Drum_Stroke`, imported by `tools/audio_import/import_cues.py` | ElevenLabs licence held by the project author; commercial use permitted |

### `sfx/state/`

| File | Origin | Licence |
| --- | --- | --- |
| `commander_enter.ogg` | Synthesised by `tools/audio_synth/cues.py`, recipe `state.commander_enter` | Own work (MIT) |
| `commander_exit.ogg` | Synthesised by `tools/audio_synth/cues.py`, recipe `state.commander_exit` | Own work (MIT) |
| `load_complete.ogg` | Synthesised by `tools/audio_synth/cues.py`, recipe `state.load_complete` | Own work (MIT) |
| `save_complete.ogg` | Synthesised by `tools/audio_synth/cues.py`, recipe `state.save_complete` | Own work (MIT) |
| `speed_notch.ogg` | Synthesised by `tools/audio_synth/cues.py`, recipe `state.speed_change` | Own work (MIT) |

### `sfx/ui/`

| File | Origin | Licence |
| --- | --- | --- |
| `back_cancel.ogg` | ElevenLabs render `Wooden_Box_Closure`, imported by `tools/audio_import/import_cues.py` | ElevenLabs licence held by the project author; commercial use permitted |
| `click_confirm.ogg` | ElevenLabs render `Stiff_Leather_Tap`, imported by `tools/audio_import/import_cues.py` | ElevenLabs licence held by the project author; commercial use permitted |
| `command_accept.ogg` | ElevenLabs render `Spear_and_Shield_Impact`, imported by `tools/audio_import/import_cues.py` | ElevenLabs licence held by the project author; commercial use permitted |
| `command_refuse.ogg` | ElevenLabs render `Command_Refuse`, imported by `tools/audio_import/import_cues.py` | ElevenLabs licence held by the project author; commercial use permitted |
| `command_refuse_v2.ogg` | ElevenLabs render `No_No_No`, imported by `tools/audio_import/import_cues.py` | ElevenLabs licence held by the project author; commercial use permitted |
| `confirm_seal.ogg` | ElevenLabs render `Bronze_Seal_Press`, imported by `tools/audio_import/import_cues.py` | ElevenLabs licence held by the project author; commercial use permitted |
| `deselect.ogg` | ElevenLabs render `Leather_Lift_Brush`, imported by `tools/audio_import/import_cues.py` | ElevenLabs licence held by the project author; commercial use permitted |
| `error_thud.ogg` | ElevenLabs render `Shield_Thud_Error`, imported by `tools/audio_import/import_cues.py` | ElevenLabs licence held by the project author; commercial use permitted |
| `error_thud_v2.ogg` | ElevenLabs render `Leather_Shield_Impact`, imported by `tools/audio_import/import_cues.py` | ElevenLabs licence held by the project author; commercial use permitted |
| `hover_brush.ogg` | ElevenLabs render `Fingertip_and_Parchment`, imported by `tools/audio_import/import_cues.py` | ElevenLabs licence held by the project author; commercial use permitted |
| `notification.ogg` | Synthesised by `tools/audio_synth/cues.py`, recipe `ui.notification` | Own work (MIT) |
| `panel_close.ogg` | Synthesised by `tools/audio_synth/cues.py`, recipe `ui.panel_close` | Own work (MIT) |
| `panel_open.ogg` | Synthesised by `tools/audio_synth/cues.py`, recipe `ui.panel_open` | Own work (MIT) |
| `select_group.ogg` | Synthesised by `tools/audio_synth/cues.py`, recipe `ui.select_group` | Own work (MIT) |
| `select_group_v2.ogg` | Synthesised by `tools/audio_synth/cues.py`, recipe `ui.select_group` | Own work (MIT) |
| `select_group_v3.ogg` | Synthesised by `tools/audio_synth/cues.py`, recipe `ui.select_group` | Own work (MIT) |
| `select_unit.ogg` | ElevenLabs render `Shield_Boss_Knock`, imported by `tools/audio_import/import_cues.py` | ElevenLabs licence held by the project author; commercial use permitted |
| `tab_slide.ogg` | Synthesised by `tools/audio_synth/cues.py`, recipe `ui.tab_switch` | Own work (MIT) |
| `tab_slide_v2.ogg` | Synthesised by `tools/audio_synth/cues.py`, recipe `ui.tab_switch` | Own work (MIT) |
| `toggle_latch.ogg` | ElevenLabs render `Toggle_Latch_UI_Click`, imported by `tools/audio_import/import_cues.py` | ElevenLabs licence held by the project author; commercial use permitted |

### `sfx/undead/`

| File | Origin | Licence |
| --- | --- | --- |
| `skeletons_rise.ogg` | ElevenLabs render `Rise_of_the_Ossuary`, imported by `tools/audio_import/import_cues.py` | ElevenLabs licence held by the project author; commercial use permitted |

### `sfx/wildlife/`

| File | Origin | Licence |
| --- | --- | --- |
| `bird_chirp.ogg` | ElevenLabs render `Morning_Bird_Chirp`, imported by `tools/audio_import/import_cues.py` | ElevenLabs licence held by the project author; commercial use permitted |
| `sheep_bleat.ogg` | ElevenLabs render `Pastoral_Sheep_Bleat`, imported by `tools/audio_import/import_cues.py` | ElevenLabs licence held by the project author; commercial use permitted |
| `wolf_bite_snap.ogg` | Composed by `tools/audio_field/build_battle.py` from TDC FIGHT, 'CU_Smacks, Rapid'; TDC ANIMALS, 'CU_Aggresive Dog Barks and Snarls, Distant Wind Chimes' | CC0 1.0 |
| `wolf_growl_low.ogg` | Faragó et al. 2010, PLOS ONE, supporting audio S3 | CC BY 2.5 (attribution required) |
| `wolf_howl_distant.ogg` | Wikimedia Commons, `File:Wolf howls.ogg`, 13.35–17.55 s | Public domain |
| `wolf_howl_near.ogg` | Wikimedia Commons, `File:Wolf howls.ogg`, 20.05–23.65 s | Public domain |
| `wolf_pack_attack.ogg` | Faragó et al. 2010, PLOS ONE, supporting audio S3 and S2 | CC BY 2.5 (attribution required) |
| `wolf_snarl_bark.ogg` | Composed by `tools/audio_field/build_battle.py` from TDC ANIMALS, 'CU_Aggresive Dog Barks and Snarls, Distant Wind Chimes' | CC0 1.0 |

### `voices/carthage/`

| File | Origin | Licence |
| --- | --- | --- |
| `archer.ogg` | Recorded by the project author | Own work (MIT) |
| `baal_cultist.ogg` | Recorded by the project author | Own work (MIT) |
| `ballista.ogg` | Recorded by the project author | Own work (MIT) |
| `catapult.ogg` | Recorded by the project author | Own work (MIT) |
| `elephant.ogg` | Recorded by the project author | Own work (MIT) |
| `hannibal.ogg` | Recorded by the project author | Own work (MIT) |
| `hanno_the_great.ogg` | Recorded by the project author | Own work (MIT) |
| `hasdrubal_barca.ogg` | Recorded by the project author | Own work (MIT) |
| `healer.ogg` | Recorded by the project author | Own work (MIT) |
| `horse_archer.ogg` | Recorded by the project author | Own work (MIT) |
| `horse_spearman.ogg` | Recorded by the project author | Own work (MIT) |
| `horse_swordsman.ogg` | Recorded by the project author | Own work (MIT) |
| `infantry.ogg` | Recorded by the project author | Own work (MIT) |
| `numidians.ogg` | Recorded by the project author | Own work (MIT) |
| `spearman.ogg` | Recorded by the project author | Own work (MIT) |
| `swordsman.ogg` | Recorded by the project author | Own work (MIT) |

### `voices/commanders/`

| File | Origin | Licence |
| --- | --- | --- |
| `marcus_claudius_marcellus.ogg` | Recorded by the project author | Own work (MIT) |
| `publius_cornelius_scipio.ogg` | Recorded by the project author | Own work (MIT) |
| `quintus_fabius_maximus.ogg` | Recorded by the project author | Own work (MIT) |

### `voices/roman/`

| File | Origin | Licence |
| --- | --- | --- |
| `archer.ogg` | Recorded by the project author | Own work (MIT) |
| `ballista.ogg` | Recorded by the project author | Own work (MIT) |
| `builder.ogg` | Recorded by the project author | Own work (MIT) |
| `catapult.ogg` | Recorded by the project author | Own work (MIT) |
| `fire_legionary.ogg` | Recorded by the project author | Own work (MIT) |
| `healer.ogg` | Recorded by the project author | Own work (MIT) |
| `horse_archer.ogg` | Recorded by the project author | Own work (MIT) |
| `horse_spearman.ogg` | Recorded by the project author | Own work (MIT) |
| `horse_swordsman.ogg` | Recorded by the project author | Own work (MIT) |
| `praetorian_guard.ogg` | Recorded by the project author | Own work (MIT) |
| `spearman.ogg` | Recorded by the project author | Own work (MIT) |
| `swordsman.ogg` | Recorded by the project author | Own work (MIT) |

<!-- prettier-ignore-end -->
