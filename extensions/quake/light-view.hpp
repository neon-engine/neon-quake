#ifndef QUAKE_LIGHT_VIEW_HPP
#define QUAKE_LIGHT_VIEW_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include <neon/extension/neon-extension.hpp>

namespace quake
{
  /// The lights of a moment: an explosion that lights a room and fades, the
  /// flash of a shot, the glow a rocket carries along.
  ///
  /// The level keeps its light in pictures, which do not know of them. Each
  /// is a point light of the engine, which the shaders of the game read: its
  /// colour, and how far it reaches as the light's `constant`, see
  /// `light_of_the_moment` in quake.glsl. As in the original there are at
  /// most 32 at once, each known by a key: the number of the entity it
  /// belongs to, so that an entity has one light and moves it along, or 0
  /// for a light of its own.
  class LightView
  {
    /// One light, in the units and axes of the game.
    struct Light
    {
      neon::extension::Entity entity = 0;
      std::int32_t key = 0;
      std::array<float, 3> place{};

      /// How far it reaches, and how much of that it loses in a second.
      float radius = 0.0f;
      float decay = 0.0f;

      /// The time of the game it goes out at.
      double ends_at = 0.0;

      /// Whether the engine has to be told where it is and how far it
      /// reaches.
      bool has_changed = false;
    };

    std::vector<Light> _lights;
    double _time = 0.0;

    // an entity of the engine is known by its name, so each gets a number
    std::int64_t _made = 0;

    // what makes the lights flicker, as the original's dice do
    std::uint32_t _dice = 1;

  public:
    /// How many lights there are at most. One more takes the place of the
    /// first.
    static constexpr std::size_t most = 32;

    /// Lights a light at a place of the game for a number of seconds. Its
    /// reach, in units of the game, shrinks by `decay` in a second. A key
    /// that is not 0 takes the place of the light of that key.
    void Flash(std::int32_t key, const std::array<float, 3> &place, float radius, float seconds, float decay = 0.0f);

    /// A number from 0 to 31, another each time: what the original adds to
    /// the reach of a light so that it flickers.
    [[nodiscard]] float Flicker();

    /// Fades the lights to a time of the game, in seconds, takes away those
    /// that went out, and tells the engine of the others, under `parent`.
    void Update(const neon::extension::World &world, neon::extension::Entity parent, double time);

    /// How much light the lights of the moment give at a place of the game,
    /// in the numbers the light of a level is counted in for a model: what
    /// is left of the reach of each.
    [[nodiscard]] float FindLight(const std::array<float, 3> &place) const;

    /// Forgets every light, when the level they stand in goes: their
    /// entities went with it.
    void Forget();
  };
} // quake

#endif //QUAKE_LIGHT_VIEW_HPP
