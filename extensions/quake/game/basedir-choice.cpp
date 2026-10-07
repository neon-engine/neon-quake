#include "basedir-choice.hpp"

#include <algorithm>

#include "basedir-list.hpp"

namespace quake
{
  BasedirChoice BasedirChoice::Of(
    const std::vector<Basedir> &basedirs, const std::string &remembered, const bool asked)
  {
    BasedirChoice choice;

    if (basedirs.empty())
    {
      choice.problem = "There is no data of the game. Put the folder id1 of Quake, or of LibreQuake, in a folder of "
                       "its own under assets/basedirs/ next to the game, such as assets/basedirs/steam/id1";
      return choice;
    }

    // an id1 straight under assets://basedirs/ is the player's say, over
    // whatever was chosen before
    if (basedirs.size() == 1 && basedirs.front().name == BasedirList::default_name && !asked)
    {
      choice.chosen = 0;
      return choice;
    }

    if (asked)
    {
      choice.asks = true;
      return choice;
    }

    if (!remembered.empty())
    {
      const auto named = std::ranges::find(basedirs, remembered, &Basedir::name);
      if (named != basedirs.end())
      {
        choice.chosen = static_cast<int>(named - basedirs.begin());
        return choice;
      }
      choice.warning = "The data " + remembered + " that was chosen before is not under assets/basedirs/ any more";
    }

    if (basedirs.size() == 1)
    {
      choice.chosen = 0;
      return choice;
    }

    choice.asks = true;
    return choice;
  }
} // quake
