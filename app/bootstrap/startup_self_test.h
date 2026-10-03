#pragma once

namespace App::Bootstrap {

// Every campaign-map resource must be embedded in the binary and non-empty.
auto validate_release_campaign_map_resources() -> bool;

// Each check returns -1 when it passes, otherwise the process exit code.
auto check_audio_manifest() -> int;
auto check_release_defaults() -> int;

} // namespace App::Bootstrap
