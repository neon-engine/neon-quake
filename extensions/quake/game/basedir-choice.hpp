#ifndef QUAKE_BASEDIR_CHOICE_HPP
#define QUAKE_BASEDIR_CHOICE_HPP

#include <string>
#include <vector>

#include "basedir.hpp"

namespace quake
{
  /// Which copy of the game's data the game starts with, or that the
  /// player is asked.
  struct BasedirChoice
  {
    /// The index of the copy in the list, or -1 for none.
    int chosen = -1;

    /// Whether the player picks one from a menu.
    bool asks = false;

    /// Why the game cannot start, when there is no copy at all.
    std::string problem;

    /// Something to say that does not stop the game: a name that was
    /// remembered and is not there any more.
    std::string warning;

    /// The copy that is there alone, the one `remembered` names, or the one
    /// the player picks; `asked` has the player pick whatever there is.
    [[nodiscard]] static BasedirChoice Of(
      const std::vector<Basedir> &basedirs, const std::string &remembered, bool asked = false);
  };
} // quake

#endif //QUAKE_BASEDIR_CHOICE_HPP
