#pragma once

#include <cstddef>
#include <span>

namespace Game::Save {

inline constexpr int k_snapshot_version = 2;

enum class FieldClass {

  AuthoritativeSerialized,

  DerivedRebuilt,

  PresentationOnly,

  CampaignLevel
};

struct FieldSpec {

  const char* name;
  FieldClass classification;

  const char* rationale;
};

[[nodiscard]] auto field_class_name(FieldClass classification) -> const char*;

[[nodiscard]] auto fields() -> std::span<const FieldSpec>;

[[nodiscard]] auto find(const char* name) -> const FieldSpec*;

} // namespace Game::Save
