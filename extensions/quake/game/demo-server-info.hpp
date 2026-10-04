#ifndef QUAKE_DEMO_SERVER_INFO_HPP
#define QUAKE_DEMO_SERVER_INFO_HPP

#include <cstdint>
#include <string>
#include <vector>

#include "demo-protocol.hpp"

namespace quake
{
  /// What a server says first of a level: the protocol it speaks, the
  /// name of the level, and every model and sound the level uses. Every
  /// later message names a model or a sound by its index here.
  struct DemoServerInfo
  {
    DemoProtocol protocol{};

    /// For how many players the server is.
    std::int32_t max_clients = 0;

    /// 0 for a game played together or alone, 1 for a deathmatch. It
    /// decides which screen shows between levels.
    std::int32_t game_type = 0;

    /// The name of the level for a reader. Its bytes are letters of the
    /// console, see `HudText`.
    std::string level_name;

    /// The names of the files of the models, by index. Index 0 is no
    /// model and is empty. Index 1 is the level itself, such as
    /// `maps/e1m1.bsp`, and names such as `*3` are its doors and lifts.
    std::vector<std::string> model_names;

    /// The names of the files of the sounds under `sound/`, by index.
    /// Index 0 is no sound and is empty.
    std::vector<std::string> sound_names;
  };
} // quake

#endif //QUAKE_DEMO_SERVER_INFO_HPP
