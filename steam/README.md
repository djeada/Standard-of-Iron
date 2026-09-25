# Steam release

How a Standard of Iron release candidate becomes a Steam build, and what has
to be configured by hand in Steamworks. Issue #1491 tracks the full list. This
file covers the parts the repository owns or depends on.

```text
tag vX.Y.Z ─► Release workflow ─► GitHub release (AppImage, DMG, ZIP)
                    │
                    └─► artifacts: release-*, steam-macos-app, symbols-*
                                        │
             Steam upload workflow ◄────┘  (manual, takes the run ID)
               resolve ─► verify-macos | verify-windows ─► stage + verify ─► SteamPipe
                                                                              │
                                              beta branch "rc" (password) ◄───┘
                                                        │  install, accept (checklist below)
                                                        ▼
                                      Steamworks: promote that BuildID to default (manual)
```

Nothing is rebuilt for Steam. The depots are unpacked from the packages that a
passing Release run produced. The Steam build and the GitHub release candidate
therefore come from the same commit and contain the same files.

## Files

| Path                                       | What it is                                                          |
| ------------------------------------------ | ------------------------------------------------------------------- |
| `steam/steam.json`                         | AppID, depot IDs, the beta branch, and each depot's launch target   |
| `steam/app_build.vdf.in`                   | SteamPipe app build template                                        |
| `steam/depot_build.vdf.in`                 | SteamPipe depot build template (one per platform)                   |
| `scripts/steam-release.py`                 | `stage`, `verify`, `vdf`, `manifest`, `build-id`                    |
| `.github/workflows/steam-upload.yml`       | manual upload of a Release run to a beta branch                     |
| `scripts/sign-and-notarize-macos.sh`       | Developer ID signing, notarization and stapling (`app`, then `dmg`) |
| `dist/macos/standard_of_iron.entitlements` | hardened-runtime entitlements the Steam overlay needs               |

The IDs in `steam/steam.json` are not secret; credentials live in GitHub
secrets. App `5129960` has depot `5129961` (Windows) and depot `5129962` (Linux

- SteamOS). The macOS entry stays `0` until a Mac depot is created. The `vdf`
  step refuses to render a script for any selected platform whose ID is still
  `0`.

## Depots and launch options

One depot per OS, and no shared depot. The OS depots have almost no identical
files: each carries its own Qt build, and its own copy of `assets/` at a
different path.

| Depot   | Contents (from the Release run)                     | Launch executable          | Working dir  | OS / arch               |
| ------- | --------------------------------------------------- | -------------------------- | ------------ | ----------------------- |
| Windows | the unpacked `…-win-x64.zip`                        | `standard_of_iron.exe`     | (depot root) | Windows, 64-bit         |
| Linux   | the AppDir extracted from `…-linux-x86_64.AppImage` | `usr/bin/standard_of_iron` | `usr/bin`    | Linux + SteamOS, 64-bit |
| macOS   | `standard_of_iron.app`, signed, notarized, stapled  | `standard_of_iron.app`     | (depot root) | macOS, 64-bit           |

**Linux ships the AppDir, not the AppImage.** An AppImage needs FUSE, which the
Steam Linux Runtime container does not provide. The binary finds Qt through its
RPATH and `usr/bin/qt.conf`, so it launches directly without `AppRun`. The
v0.1.0 payload, staged this way, passed `--renderer-self-test` when launched
from `usr/bin` and when launched from `/`. Set the working directory anyway.

**macOS launches the bundle**, not `Contents/MacOS/standard_of_iron`, so that
LaunchServices applies its Info.plist. The architectures are whatever Qt
provides; `build-macos.yml` reads and asserts them, and the GitHub DMG is
`universal` when Qt is.

**Windows needs the MSVC runtime.** `windeployqt --compiler-runtime` ships
`vc_redist.x64.exe`, not the runtime DLLs. In Steamworks, under Installation →
Redistributables, tick **Visual C++ Redist 2015-2022 (x64)**. Steam then
installs it on first launch. **This is required**: `steam-release.py stage`
leaves the 18 MB installer out of the Windows depot (the GitHub ZIP keeps it),
so without the redistributable ticked, a PC that lacks the runtime cannot start
the game.

**Package size.** The baked creature caches ship zstd-compressed (241 MB →
38 MB), and Qt's Direct3D 12 shader compiler (`dxcompiler.dll`, `dxil.dll`) is
not deployed, because the game renders only through OpenGL. `verify` fails a
depot that carries either DLL again. The Mesa software renderer stays, as the
fallback for drivers below OpenGL 3.3.

Install directory: `Standard of Iron`.

## One-time setup

### Steamworks

1. Complete onboarding, pay the Steam Direct fee, and enter tax and bank
   details. Record the AppID and the three depot IDs in `steam/steam.json`
   through a normal PR.
2. Create the depots with the OS and architecture in the table above. Add them
   to the developer comp package now. Add each to the store package only when
   that platform is ready to be advertised.
3. Add the launch options from the table.
4. Create the beta branch **`rc`**, with a password.
5. In the macOS platform settings, set **64-bit binaries included**. Set **App
   bundles are notarized** only once a real upload has passed the
   `verify-macos` job and a Mac install has passed the checklist below.
6. Create a dedicated builder account. Give it only _Edit App Metadata_ and
   _Publish App Changes To Steam_, for this app only. That is enough to upload
   and set a build live on `rc`. Setting a build live on `default` needs a
   partner-site login with Steam Guard confirmation, which CI does not have.

### GitHub

| Where                   | Name                                                  | Value                                                                                       |
| ----------------------- | ----------------------------------------------------- | ------------------------------------------------------------------------------------------- |
| Repository secrets      | `MACOS_CERTIFICATE`, `MACOS_CERTIFICATE_PASSWORD`     | base64 `.p12` with a **Developer ID Application** identity                                  |
|                         | `APPLE_ID`, `APPLE_ID_PASSWORD`, `APPLE_TEAM_ID`      | notarytool credentials (app-specific password)                                              |
|                         | `MACOS_KEYCHAIN_PASSWORD`                             | optional                                                                                    |
|                         | `WINDOWS_CERTIFICATE`, `WINDOWS_CERTIFICATE_PASSWORD` | base64 `.pfx` Authenticode certificate                                                      |
| Repository variable     | `SOI_REQUIRE_SIGNING`                                 | `true` once the secrets above exist                                                         |
| Environment **`steam`** | `STEAM_USERNAME`                                      | the builder account                                                                         |
|                         | `STEAM_CONFIG_VDF`                                    | `base64 -w0 ~/Steam/config/config.vdf` from a machine where that account passed Steam Guard |

Add required reviewers to the `steam` environment, so a person approves every
upload.

Until `SOI_REQUIRE_SIGNING` is `true`, Release still publishes an ad-hoc Mac
build and an unsigned Windows build to GitHub, as it has since v0.1.0.

Steam uploads apply their own rules, whatever the variable says:

- **macOS:** `steam-upload.yml` fails unless the bundle is Developer-ID-signed,
  stapled and accepted by Gatekeeper. Steam requires notarization for new Mac
  apps, and that needs the paid Apple Developer Program. There is no free route.
- **Windows:** Steam does not require Authenticode. An unsigned executable ships
  with a warning in the run summary. A signature that is present but invalid
  fails the upload, and so does an unsigned executable when
  `require-windows-signature` is set. Signing only affects SmartScreen prompts
  outside Steam. SignPath Foundation signs open-source projects for free, and
  Azure Trusted Signing is a low-cost alternative.

Until there is an Apple certificate, upload Windows and Linux only (the
default, `platforms: windows,linux`) and do not advertise Mac support.

## Shipping a release candidate

1. Tag `vX.Y.Z`, with a CHANGELOG section. Release runs preflight; builds,
   self-tests and verifies all three platforms; and publishes to GitHub. Every
   build job also stages and verifies the Steam depot its package would
   produce, so a bad depot fails the tag rather than the upload.
2. Actions → **Steam upload** → Run workflow, with the Release run's ID from its
   URL. Leave **preview** on for the first run: it stages, verifies, renders
   the VDFs and runs SteamCMD in preview mode, without uploading. Without Steam
   credentials, it stops after rendering.
3. Run it again with **preview** off. The summary shows the **BuildID**. Keep the
   `steam-record-<run>` artifact (kept 90 days) with the release notes. It
   holds the commit, the Release run, the BuildID, the rendered VDFs, the
   SteamCMD log, and a per-file manifest of every depot.
4. Install from the `rc` branch on every advertised OS, and work through the
   acceptance checklist.
5. In Steamworks → Builds, set **that BuildID** live on `default`. This step is
   manual on purpose.

Release artifacts are kept for 14 days. To upload an older candidate, re-run
its Release workflow first.

**Symbols.** Release builds are compiled with `SOI_RELEASE_DEBUG_INFO=ON`. The
debug information is split off before packaging, and uploaded as
`symbols-windows`, `symbols-macos` and `symbols-linux`:

- Windows: the PDB.
- macOS: the dSYM.
- Linux: a `.debug` file plus the GNU build ID.

GitHub keeps these artifacts for at most 90 days, so download them for every
BuildID that reaches customers.

## Steam Cloud (Auto-Cloud)

Saves live in one SQLite database under `QStandardPaths::AppDataLocation`. The
game pins its application name in `app/core/app_identity.h`, so the path does
not depend on the executable or bundle name. `standard_of_iron --print-data-paths`
prints the exact locations on any platform, and every package build asserts
them.

| OS      | Save directory                                             | Auto-Cloud root     |
| ------- | ---------------------------------------------------------- | ------------------- |
| Windows | `%APPDATA%\standard_of_iron\saves`                         | `WinAppDataRoaming` |
| Linux   | `~/.local/share/standard_of_iron/saves` (`$XDG_DATA_HOME`) | `LinuxXdgDataHome`  |
| macOS   | `~/Library/Application Support/standard_of_iron/saves`     | `MacAppSupport`     |

Configure in Steamworks → Steam Cloud:

- Byte quota per user: **256 MB**. File count: **16**.
- Root path: `WinAppDataRoaming`, subdirectory `standard_of_iron/saves`,
  pattern `saves.sqlite`, OS **[All OSes]**, not recursive.
- Root overrides: `MacAppSupport` on macOS and `LinuxXdgDataHome` on Linux,
  both with the same subdirectory.

What syncs, and why:

- `saves.sqlite` holds all of a player's progress: slots, autosaves and
  campaign results. The database runs in WAL mode. On exit, the game closes
  both its connections (the save worker's in `SaveLoadService::shutdown`, the
  main one at static destruction), so SQLite checkpoints the WAL into this
  file. Auto-Cloud uploads only after the process has exited.
- Not synced:
    - `saves.sqlite-wal` / `-shm`: they exist only while the game runs or after
      a crash, and SQLite recovers them locally.
    - `saves.sqlite.v<N>-backup`: the pre-migration copy, a local safety net.
    - `saves.sqlite.unreadable-*`: a quarantined database; syncing it would
      spread the damage.
    - `saves/exports/`: packages the player exported to share by hand.
- Settings (`djeada/StandardOfIron.ini`: graphics, display, audio levels) are
  per machine. They live outside the synced directory on purpose.

The path has not moved since v0.1.0. Pre-Steam saves are therefore already in
the synced location, and need no migration.

## Acceptance checklist (install from the `rc` branch)

On each advertised OS, on a machine with no development tools installed:

- [ ] Fresh install; launch from the Steam client; launch again after a reboot.
- [ ] Main menu, new campaign, the tutorial's critical path, an early, a middle
      and a late campaign mission, and a skirmish.
- [ ] Manual save, quicksave and autosave, then load each after restarting.
- [ ] Settings persist. Language switch, audio, and fullscreen/windowed/
      resolution changes work.
- [ ] A large battle, first combat, a structure collapsing, weather, and a
      mission transition, with no hitch severe enough to block release.
- [ ] `standard_of_iron --print-data-paths` from the install directory prints
      the path in the table above. Nothing is written inside the install
      directory: after playing, **Verify integrity of game files** reports
      nothing.
- [ ] Uninstalling keeps the saves; reinstalling with Auto-Cloud restores
      progress. Continue a save Windows→Linux, Windows→macOS and
      macOS→Windows. Play offline, then reconnect. Play conflicting sessions
      on two machines.
- [ ] Windows: note SmartScreen and antivirus behaviour on first launch.
      Confirm the VC++ redistributable installed silently.
- [ ] macOS (Apple Silicon, and Intel if advertised): run
      `codesign --verify --deep --strict`, `xcrun stapler validate` and
      `spctl --assess --type execute -v` on the installed copy at
      `~/Library/Application Support/Steam/steamapps/common/Standard of Iron/standard_of_iron.app`.
      This proves that SteamPipe kept the framework symlinks. Check
      Gatekeeper on first open, and the overlay (Shift+Tab) if it is wanted.
- [ ] Linux and Steam Deck: the game launches under the Steam Linux Runtime,
      and the software GL fallback works.
- [ ] Record the commit, BuildID and depot manifest IDs of the accepted build
      (the `steam-record` artifact plus Steamworks → Builds).

## Store page and review

These are not repository tasks, but they decide what the build must prove:

- Advertise only what ships. The game is single-player. Do not claim
  multiplayer, controller support, achievements or Workshop until they exist.
  Supported languages must match `translations/`. A platform appears on the
  page only once its depot passes the checklist above.
- Required art: header 920×430, small 462×174, main 1232×706, vertical 748×896,
  library 600×900, hero 3840×1240, logo, and screenshots at 1920×1080 or
  larger. At least one gameplay trailer, showing only the shipping game.
  `tools/arena/promos/` and `scripts/film-game.sh` produce the footage.
- **Content Survey, AI disclosure (pre-generated).** This draft matches
  `THIRD_PARTY_LICENSES.md`. Before submitting, confirm that no other shipped
  content (art, textures, text) is AI-generated.

    > The game's music (30 tracks) and 44 of its sound effects were generated with
    > ElevenLabs under a licence that permits commercial use, then edited, levelled
    > and mixed by the developer. All voice lines are the developer's own
    > recordings; the remaining sound effects are synthesised or cut from CC0 and
    > public-domain recordings. No content is generated while the game runs.

- **Privacy.** The game makes no network requests and collects no player data.
  Everything it stores is local: saves, settings and exports.
- **Licences.** Every depot carries `LICENSE` and `THIRD_PARTY_LICENSES.md`, and
  `verify` fails without them. Every depot links Qt dynamically (LGPL v3).
- Submit the store page for review first. It must then be public as Coming
  Soon for at least two weeks. Submit the build for review several business
  days before launch.
