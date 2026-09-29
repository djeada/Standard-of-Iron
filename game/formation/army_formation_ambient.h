#pragma once

namespace Game::Formation {

class ArmyFormationRegistry;

[[nodiscard]] auto ambient_formation_registry() -> ArmyFormationRegistry&;

} // namespace Game::Formation
