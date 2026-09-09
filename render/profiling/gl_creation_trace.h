#pragma once

#include <QJsonArray>

#include <cstddef>
#include <string>
#include <string_view>

namespace Render::Profiling {

[[nodiscard]] auto gl_creation_trace_enabled() noexcept -> bool;

void set_playable_window_open(bool open) noexcept;

[[nodiscard]] auto playable_window_open() noexcept -> bool;

void record_mesh_upload(const std::string& fingerprint) noexcept;

[[nodiscard]] auto mesh_upload_report(std::size_t max_entries) -> QJsonArray;

void record_gl_creation(std::string_view kind, std::size_t count) noexcept;

void reset_gl_creation_trace() noexcept;

[[nodiscard]] auto gl_creation_trace_report(std::size_t max_sites) -> QJsonArray;

} // namespace Render::Profiling
