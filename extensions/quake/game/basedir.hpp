#ifndef QUAKE_BASEDIR_HPP
#define QUAKE_BASEDIR_HPP

#include <string>

namespace quake
{
  /// A copy of the game's data the player brought: a folder under
  /// `assets://basedirs/` that holds an `id1`, as the original's -basedir
  /// names the folder `id1` is in. The name is that of the folder, such as
  /// `steam` or `librequake`, and is also where the saved games and the
  /// options of this copy are kept, `user://<name>/`.
  struct Basedir
  {
    std::string name;

    /// The folder `id1` of it, as a virtual path ending with a slash, in
    /// the case it has on disk: `assets://basedirs/steam/id1/`.
    std::string data_folder;

    /// Where what the player keeps of this copy goes: `user://steam/`.
    [[nodiscard]] std::string UserFolder() const
    {
      return "user://" + name + "/";
    }
  };
} // quake

#endif //QUAKE_BASEDIR_HPP
