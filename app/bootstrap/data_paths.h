#pragma once

namespace App::Bootstrap {

auto data_paths_requested_from_argv(int argc, char* argv[]) -> bool;
auto print_data_paths(int argc, char* argv[]) -> int;

} // namespace App::Bootstrap
