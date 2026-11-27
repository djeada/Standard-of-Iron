# Support bundle

Settings → Diagnostics → **Export support bundle** writes one zip that describes
the machine the game is running on. It is the file a bug report should carry: it
answers "which build, which graphics adapter, which settings, what did the log
say at the end" without asking the reporter to find four things in four places.

Nothing is uploaded. There is no network code on this path at all: the archive is
written where the reporter points it, and they attach it to the issue
themselves.

## What is in it

| File               | What it holds                                                                              |
| ------------------ | ------------------------------------------------------------------------------------------ |
| `README.txt`       | What the bundle is, what is inside, and what was deliberately left out                     |
| `system.txt`       | Build, operating system, CPU and memory, graphics adapter and tier, display, storage state |
| `settings.ini`     | The saved preferences, sorted, without paths or account identifiers                        |
| `logs/recent.log`  | The last 800 lines this run wrote to the console                                           |
| `logs/startup.log` | Only when the startup supervisor wrote one, tail first                                     |
| `screenshot.png`   | Only when the reporter ticks the box; the window as it looked when they exported           |

Saved games, replay files and anything that names a person are never included.

## What is redacted

Every text file goes through one pass before it is written (`redact_text` in
`app/diagnostics/support_bundle.cpp`):

- the home directory becomes `<home>`, the app data, save and export directories
  become `<path>`, with the most specific prefix shortened first;
- the account name and the machine name become `<user>` and `<host>`;
- GitHub, AWS, OpenAI-style, Slack and JWT tokens become `<secret>`, as does the
  whole value of an `authorization` header, whatever scheme follows it, and the
  value of `token=`, `password=` and `api_key=`;
- e-mail addresses become `<email>`, and IPv4/IPv6 addresses become `<ip>`,
  except loopback and the unspecified address, which say nothing about the
  reporter and explain a local-server bug.

Numbers that are not addresses survive untouched: `OpenGL 4.1`, `Qt 6.8.2`,
`12:34:56`, `1920x1080`. The suite pins that in
`tests/app/support_bundle_test.cpp`, because a redaction pass that eats a version
number makes the bundle useless.

## Limits

A bundle has to be attachable to an issue, so the archive is capped: 128 KiB per
file, 4 MiB in total, 32 files. An oversized text file keeps its **tail** - the
end of a log is what matters - and says so on its first line, with the number of
bytes dropped. A screenshot is never cut: it is scaled down until it fits the
per-file cap, and dropped whole - with a line saying so in `README.txt` - when
even that is not enough, because a truncated PNG does not decode. Entries are
stored, not compressed: every unzipper reads a stored archive without a
compression library, which is worth more here than the bytes.

## Where it lives

| Piece                 | Path                                                    |
| --------------------- | ------------------------------------------------------- |
| Redaction and archive | `app/diagnostics/support_bundle.{h,cpp}`                |
| Report and settings   | `app/diagnostics/support_report.{h,cpp}`                |
| QML-facing service    | `app/diagnostics/support_bundle_service.{h,cpp}`        |
| Recent log ring       | `app/bootstrap/log_handler.{h,cpp}`                     |
| Panel                 | the `Diagnostics` section of `ui/qml/SettingsPanel.qml` |
| Tests                 | `tests/app/support_bundle_test.cpp`                     |

The service takes the executable's version strings through `set_build_info` and
the window through `set_screenshot_provider`; both are wired in `main.cpp`. That
is why the archive code sits in `app_core` and can be tested without a window, a
GL context or a build stamp.
