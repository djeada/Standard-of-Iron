#pragma once

struct EntityCache {
  int player_troop_count = 0;
  bool player_barracks_alive = false;
  bool enemy_barracks_alive = false;
  int enemy_barracks_count = 0;

  void reset() {
    player_troop_count = 0;
    player_barracks_alive = false;
    enemy_barracks_alive = false;
    enemy_barracks_count = 0;
  }
};
