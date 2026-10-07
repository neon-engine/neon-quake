#ifndef QUAKE_BASEDIR_LIST_HPP
#define QUAKE_BASEDIR_LIST_HPP

#include <functional>
#include <string>
#include <string_view>
#include <vector>

#include "basedir.hpp"

namespace quake
{
  /// The copies of the game's data under `assets://basedirs/`.
  struct BasedirList
  {
    /// Where they are looked for.
    static constexpr std::string_view folder = "assets://basedirs/";

    /// The name of a copy whose `id1` is in `assets://basedirs/` itself,
    /// which has no folder of its own to be named after.
    static constexpr std::string_view default_name = "default";

    /// Finds them from the folders in `assets://basedirs/` and, through
    /// `folders_in`, the folders in each of those, so that this needs no
    /// file system. A folder counts when it holds `id1` in any case. An
    /// `id1` in `assets://basedirs/` itself is the only copy there is, the
    /// others then left out: the player has said which. In the order of
    /// the names.
    [[nodiscard]] static std::vector<Basedir> Find(
      const std::vector<std::string> &folders,
      const std::function<std::vector<std::string>(const std::string &folder)> &folders_in);
  };
} // quake

#endif //QUAKE_BASEDIR_LIST_HPP
