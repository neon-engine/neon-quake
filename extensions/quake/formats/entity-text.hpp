#ifndef QUAKE_ENTITY_TEXT_HPP
#define QUAKE_ENTITY_TEXT_HPP

#include <string>
#include <string_view>
#include <vector>

#include "bsp-entity.hpp"

namespace quake
{
  /// The text a level describes its things with, the lump of entities: one
  /// block in braces for each, with its keys and values in quotes.
  ///
  /// ```
  /// {
  /// "classname" "light"
  /// "origin" "0 64 128"
  /// }
  /// ```
  ///
  /// The first entity is the world. Values stay text: a place such as
  /// `"0 64 128"` is in the units and axes of the game, and is taken apart
  /// by whoever knows what the key means.
  struct EntityText
  {
    std::vector<BspEntity> entities;

    /// Takes the entities from the text. Returns false, says in `error`
    /// what was wrong and at which line and column, and stays as it was
    /// when the text is not blocks of quoted keys and values.
    bool Read(std::string_view text, std::string &error);
  };
} // quake

#endif //QUAKE_ENTITY_TEXT_HPP
