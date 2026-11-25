#pragma once

#include "ownership_constants.h"

namespace Engine::Core {

inline constexpr int k_owner_everyone = 0;

class LocalAudience {
public:
  LocalAudience() = default;

  explicit LocalAudience(int local_owner_id)
      : m_local_owner_id(local_owner_id) {}

  void set_local_owner_id(int owner_id) { m_local_owner_id = owner_id; }

  [[nodiscard]] auto local_owner_id() const -> int { return m_local_owner_id; }

  [[nodiscard]] auto is_spectating() const -> bool {
    return m_local_owner_id == k_owner_everyone;
  }

  [[nodiscard]] auto is_local(int owner_id) const -> bool {
    return !is_spectating() && owner_id == m_local_owner_id;
  }

  [[nodiscard]] auto includes(int owner_id) const -> bool {
    return is_spectating() || owner_id == k_owner_everyone ||
           Game::Core::is_neutral_owner(owner_id) || owner_id == m_local_owner_id;
  }

  // A line spoken to one player ("the vein is yours"). Unlike includes(), a
  // spectator is nobody's addressee, so owned messages never reach them.
  [[nodiscard]] auto addresses(int owner_id) const -> bool {
    return owner_id == k_owner_everyone || Game::Core::is_neutral_owner(owner_id) ||
           is_local(owner_id);
  }

  [[nodiscard]] auto involves(int first_owner_id, int second_owner_id) const -> bool {
    return is_spectating() || first_owner_id == m_local_owner_id ||
           second_owner_id == m_local_owner_id;
  }

private:
  int m_local_owner_id{k_owner_everyone};
};

} // namespace Engine::Core
