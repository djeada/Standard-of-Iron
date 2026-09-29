#include "app/orders/local_command.h"

#include <utility>

#include "app/orders/order_issuer.h"
#include "game/command/command_queue.h"

namespace App::Orders {

void submit_local_command(Engine::Core::World* world, Game::Command::Payload payload) {
  Game::Command::submit(*world,
                        Game::Command::Source::LocalPlayer,
                        local_owner(world),
                        std::move(payload));
}

} // namespace App::Orders
