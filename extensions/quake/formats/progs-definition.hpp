#ifndef QUAKE_PROGS_DEFINITION_HPP
#define QUAKE_PROGS_DEFINITION_HPP

#include <cstdint>

namespace quake
{
  /// What a global or a field of the game code holds.
  enum class ProgsType : std::uint16_t
  {
    Void = 0,
    String = 1,
    Float = 2,
    Vector = 3,
    Entity = 4,
    Field = 5,
    Function = 6,
    Pointer = 7,
  };

  /// The name and type of a global, or of a field of every entity, and where
  /// it is. Eight bytes in the file.
  struct ProgsDefinition
  {
    /// The bit of `type` that says a global is written into a saved game.
    static constexpr std::uint16_t save_global = 1 << 15;

    /// The type, a number of `ProgsType`, with `save_global` in bit 15.
    std::uint16_t type = 0;

    /// For a global, its offset among the globals. For a field, its offset
    /// among the cells of an entity.
    std::uint16_t offset = 0;

    /// Where the name starts in the strings.
    std::int32_t name = 0;

    /// The type without the bit of the saved game.
    [[nodiscard]] ProgsType GetType() const
    {
      return static_cast<ProgsType>(type & ~save_global);
    }

    /// Whether a saved game keeps this global.
    [[nodiscard]] bool IsSaved() const
    {
      return (type & save_global) != 0;
    }
  };
} // quake

#endif //QUAKE_PROGS_DEFINITION_HPP
