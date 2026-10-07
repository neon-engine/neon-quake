#ifndef QUAKE_BASEDIR_FILE_HPP
#define QUAKE_BASEDIR_FILE_HPP

#include <string>
#include <string_view>

namespace quake
{
  /// The copy of the game's data the player chose, kept so that the game
  /// starts with it the next time without asking:
  ///
  ///     version: 1
  ///     basedir: librequake
  ///
  /// The game writes it when the player picks one, and a person may write
  /// it by hand. Without it, or with a name that is no longer there, the
  /// game asks again when there is more than one.
  struct BasedirFile
  {
    static constexpr std::string_view path = "user://basedir.yml";

    /// The name the text gives as `basedir`, without quotes and blanks, or
    /// nothing when it gives none.
    [[nodiscard]] static std::string Read(std::string_view text);

    /// The text of the file for a name.
    [[nodiscard]] static std::string Write(std::string_view name);
  };
} // quake

#endif //QUAKE_BASEDIR_FILE_HPP
