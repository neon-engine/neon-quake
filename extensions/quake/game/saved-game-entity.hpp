#ifndef QUAKE_SAVED_GAME_ENTITY_HPP
#define QUAKE_SAVED_GAME_ENTITY_HPP

#include <string>
#include <utility>
#include <vector>

namespace quake
{
  /// One entity of a `SavedGame`: the fields that are not zero, each with
  /// its name and its value as text, in the order the program has them.
  struct SavedGameEntity
  {
    /// Whether the entity was free. A free one has no fields.
    ///
    /// The text of a saved game has no word for it: a free entity is a
    /// block without fields, and so is one that is not free and has every
    /// field zero, which therefore comes back free, as in the original.
    bool free = false;

    std::vector<std::pair<std::string, std::string>> pairs;
  };
} // quake

#endif //QUAKE_SAVED_GAME_ENTITY_HPP
