#ifndef QUAKE_SAVED_GAME_HPP
#define QUAKE_SAVED_GAME_HPP

#include <array>
#include <cstddef>
#include <string>
#include <utility>
#include <vector>

#include "saved-game-entity.hpp"

namespace quake
{
  /// A game that was saved, as the file of the original holds it: a few
  /// lines of what the engine knows, then what the game code knows, its
  /// globals and every entity, by name and as text.
  ///
  /// It is only what was read or is to be written. `SavedGameText` is the
  /// file, `SavedGameCapture` takes one from a running game and puts one
  /// back.
  struct SavedGame
  {
    /// The only version the original writes.
    static constexpr int current_version = 5;

    /// How many numbers a player carries from one level to the next.
    static constexpr std::size_t parm_count = 16;

    /// How many styles of lights there are.
    static constexpr std::size_t light_style_count = 64;

    int version = current_version;

    /// What a list of saved games shows for this one: the name of the level
    /// and how many monsters were killed, see `SavedGameText::MakeComment`.
    /// It has `_` where a space is meant.
    std::string comment;

    /// The numbers the player came into the level with, which the player
    /// gets again when the level is started anew after a death.
    std::array<float, parm_count> parms{};

    /// 0 easy, 1 medium, 2 hard, 3 nightmare.
    int skill = 1;

    /// The name of the level without folder and ending, such as `e1m1`.
    std::string map_name;

    /// The time of the level in seconds.
    double time = 0.0;

    /// How the lights of each style flicker, see `LightStyles`.
    std::array<std::string, light_style_count> light_styles;

    /// The globals of the game code that are marked to be saved, each with
    /// its name and its value as text.
    std::vector<std::pair<std::string, std::string>> globals;

    /// Every entity by its number, the world first.
    std::vector<SavedGameEntity> entities;
  };
} // quake

#endif //QUAKE_SAVED_GAME_HPP
