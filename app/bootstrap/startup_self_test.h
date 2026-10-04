#pragma once

namespace App::Bootstrap {

auto validate_release_campaign_map_resources() -> bool;

auto check_audio_manifest() -> int;
auto check_release_defaults() -> int;

} // namespace App::Bootstrap
