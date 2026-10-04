#ifndef QUAKE_PARTICLE_VIEW_HPP
#define QUAKE_PARTICLE_VIEW_HPP

#include <array>
#include <span>

#include <neon/extension/neon-extension.hpp>

#include "formats/palette.hpp"
#include "game/level-vector.hpp"
#include "game/particle.hpp"

namespace quake
{
  /// Draws the particles of the game: the sparks of a shot, blood, the
  /// smoke behind a rocket, the burst of an explosion.
  ///
  /// A particle is a small square of one colour of the palette that faces
  /// whoever looks at it. All of them are one mesh on one entity, made anew
  /// in every frame that is drawn from the particles that live, which is
  /// cheaper than an entity for each: there may be thousands.
  ///
  /// A square is as large as the original draws it, three quarters of a
  /// unit, and grows a little with its distance so that a far one does not
  /// vanish between the pixels.
  class ParticleView
  {
    neon::extension::Entity _entity = 0;

    // whether the entity shows a mesh that is to be taken away when no
    // particle lives
    bool _has_mesh = false;

    // The colours of the palette as the engine multiplies them in: in
    // linear light.
    std::array<NeonColor, 256> _colours{};
    bool _has_colours = false;

    /// How long a side of a square is, in units of the game.
    static constexpr float size = 0.75f;

  public:
    /// Shows the particles as seen from `eyes`, by a camera that looks along
    /// `forward` with `right` and `up` as its sides, all in the space of the
    /// game. `parent` is what the entity of the particles stands under, and
    /// goes with.
    void Show(
      const neon::extension::World &world,
      const Palette &palette,
      neon::extension::Entity parent,
      std::span<const Particle> particles,
      const LevelVector &eyes,
      const LevelVector &forward,
      const LevelVector &right,
      const LevelVector &up);

    /// Forgets the entity, which went with the level it stood under.
    void Clear();
  };
} // quake

#endif //QUAKE_PARTICLE_VIEW_HPP
