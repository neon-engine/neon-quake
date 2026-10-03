// The extension: what it brings to the engine, and where it starts. So far
// it starts, and says whether the data of the game is there.

#include <string>

#include <neon/extension/neon-extension.hpp>

namespace quake
{
  /// Where the data of the game is looked for: the folder `id1` in the
  /// folder of the extension, next to the runtime. It is the player's own
  /// copy and never part of this repository.
  const std::string first_pak = "extensions://quake/id1/pak0.pak";

  class Quake final : public neon::extension::Extension
  {
  public:
    bool Initialize(neon::extension::World &world) override
    {
      if (world.FileExists(first_pak))
      {
        world.Info("The data of the game is at " + first_pak);
      } else
      {
        world.Warn("The data of the game is not there: " + first_pak + " cannot be read. See README.md");
      }
      return true;
    }
  };
} // quake

NEON_EXTENSION(quake::Quake)
