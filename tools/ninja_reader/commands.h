// Copyright 2026 dywoq - MIT License
// https://github.com/djeada/Standard-of-Iron

#ifndef COMMANDS_H
#define COMMANDS_H

#include <span>

namespace NinjaReader::Commands {

[[nodiscard]] int help();
[[nodiscard]] int read(const std::span<char*>& args);

} // namespace NinjaReader::Commands

#endif
