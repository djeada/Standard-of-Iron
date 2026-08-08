# Security Policy

## Supported versions

Only the most recent release receives fixes. While the project is at 0.x, that
means the latest `v0.x.y` tag on the
[Releases page](https://github.com/djeada/Standard-of-Iron/releases).

## Reporting a vulnerability

Report privately through
[GitHub Security Advisories](https://github.com/djeada/Standard-of-Iron/security/advisories/new)
rather than in a public issue, so a fix can ship before the details are
widely known.

Please include the version, the platform, and enough detail to reproduce the
problem. You should get an acknowledgement within a week.

## What is in scope

Standard of Iron is a single-player game. It has no network play, no accounts,
and no telemetry, so the interesting attack surface is narrower than for most
applications — it is the untrusted files the game will happily read:

- **Save databases** (`SQLite`) — a malformed or hostile save that causes
  memory corruption rather than a clean error.
- **Map, mission and campaign JSON** — the same, for content a player may have
  downloaded from someone else.
- **Assets** loaded from `assets/` next to the binary, including the baked
  creature formats (`.bpat`, `.bpsm`, `.bprm`) and Ogg Vorbis audio.
- **The packaged releases themselves** — a package that ships something it
  should not, or a build pipeline that could be induced to.

## What is not a vulnerability

- A crash on a file you deliberately corrupted yourself, where the crash is a
  clean assertion or an error message.
- Anything requiring an attacker who already has write access to the game's
  installation directory or the player's save folder.
- Cheating in single-player. Editing your own save to give yourself an army is
  a feature of owning a computer.
