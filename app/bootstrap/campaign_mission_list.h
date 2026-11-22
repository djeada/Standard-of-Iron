#pragma once

namespace App::Bootstrap {

auto campaign_mission_list_requested_from_argv(int argc, char* argv[]) -> bool;
auto print_campaign_mission_list(int argc, char* argv[]) -> int;

} // namespace App::Bootstrap
