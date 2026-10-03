#pragma once

namespace App::Bootstrap {

// Every campaign-map resource must be embedded in the binary and non-empty.
auto validate_release_campaign_map_resources() -> bool;

} // namespace App::Bootstrap
