#ifndef QUAKE_QC_PRECACHE_LIST_HPP
#define QUAKE_QC_PRECACHE_LIST_HPP

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace quake
{
  /// The files of one kind, models or sounds, that the game code said it
  /// will use, in the order it named them.
  ///
  /// The place of a name in the list is the number the original gives the
  /// file, and what the game code later finds in `modelindex`. Number 0 is
  /// no file and is there from the start with an empty name. For the models
  /// the level itself is number 1, which the host adds before any game code
  /// runs. A name is in the list once.
  class QcPrecacheList
  {
    std::vector<std::string> _names;

  public:
    QcPrecacheList();

    /// Adds a name and gives its number, or the number it has when it was
    /// added before. The empty name is number 0.
    std::int32_t Add(std::string_view name);

    /// The number of a name, or nothing when it was never added.
    [[nodiscard]] std::optional<std::int32_t> Find(std::string_view name) const;

    /// The name of a number. Empty for number 0 and for one there is not.
    [[nodiscard]] std::string_view GetName(std::int32_t number) const;

    /// How many numbers there are, number 0 among them.
    [[nodiscard]] std::int32_t GetCount() const;

    /// Every name by its number, the empty one of number 0 first.
    [[nodiscard]] const std::vector<std::string> &GetNames() const;
  };
} // quake

#endif //QUAKE_QC_PRECACHE_LIST_HPP
