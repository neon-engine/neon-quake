#ifndef QUAKE_GAME_SHADERS_HPP
#define QUAKE_GAME_SHADERS_HPP

namespace quake
{
  /// The shaders of the game, as a material names them. They are compiled
  /// with the game from `extensions/quake/shaders/` and lie with the assets
  /// of the extension.
  struct GameShaders
  {
    /// A wall, a model, a sprite, a picture: its texture pixel by pixel,
    /// times its light.
    static constexpr auto surface = "extensions://quake/assets/shaders/surface";

    /// Water, slime, lava, a teleporter: the texture swims.
    static constexpr auto liquid = "extensions://quake/assets/shaders/liquid";

    /// The sky: two layers that drift over a dome.
    static constexpr auto sky = "extensions://quake/assets/shaders/sky";
  };
} // quake

#endif //QUAKE_GAME_SHADERS_HPP
