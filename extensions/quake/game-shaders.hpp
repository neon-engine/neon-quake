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
    static constexpr auto surface = "extensions://quake/shaders/surface";

    /// Water, slime, lava, a teleporter: the texture swims.
    static constexpr auto liquid = "extensions://quake/shaders/liquid";

    /// A wall and a liquid whose light flickers, pulses, or is switched:
    /// the light is worked out as they are drawn, see styles.glsl.
    static constexpr auto surface_styled = "extensions://quake/shaders/surface-styled";
    static constexpr auto liquid_styled = "extensions://quake/shaders/liquid-styled";

    /// What a camera whose eyes are in a liquid is told as an effect: the
    /// whole picture waves.
    static constexpr auto under_water = "extensions://quake/shaders/under-water";

    /// The sky: two layers that drift over a dome.
    static constexpr auto sky = "extensions://quake/shaders/sky";
  };
} // quake

#endif //QUAKE_GAME_SHADERS_HPP
