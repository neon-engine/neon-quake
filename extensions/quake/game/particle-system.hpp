#ifndef QUAKE_PARTICLE_SYSTEM_HPP
#define QUAKE_PARTICLE_SYSTEM_HPP

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "level-vector.hpp"
#include "particle-random.hpp"
#include "particle-settings.hpp"
#include "particle-trail.hpp"
#include "particle.hpp"

namespace quake
{
  /// The particles of a level: every spark, puff of blood, explosion,
  /// trail of a rocket, splash of lava, and flash of a teleporter of the
  /// original is made of small squares of one colour that move, fall,
  /// change colour, and die. This makes them and moves them, with the
  /// numbers of the original. It draws nothing: a host asks for the
  /// particles that live, in every frame, and draws them as it likes.
  ///
  /// The system has a clock of its own, which Advance() moves. A particle
  /// is gone in the first Advance() after its time is over. As in the
  /// original, a particle whose colours ran out is still handed out in the
  /// frame they ran out in, with the last of its colours.
  ///
  /// There are never more particles than the limit of the settings. An
  /// effect that finds the system full stops making particles, as in the
  /// original.
  ///
  /// The glow of particles around an entity, `R_EntityParticles` of the
  /// original, is left out: it needs the table of 162 directions of the
  /// original, which is not in this repository.
  class ParticleSystem final
  {
    std::vector<Particle> _particles;
    std::size_t _limit;
    ParticleRandom _random;
    float _time = 0.0f;

    /// Counts the particles of tracers, over all calls: by it a tracer
    /// changes its side and its colour.
    std::uint32_t _tracer_count = 0;

    /// A new particle with nothing set, or nothing when the system is
    /// full.
    [[nodiscard]] Particle *Add();

    /// A random place in a box around a place, and a random velocity, as
    /// the parts of an explosion start with.
    void Scatter(Particle &particle, const LevelVector &origin);

  public:
    /// How many particles the original makes for an explosion. An effect
    /// asked for with this count is an explosion, see RunEffect().
    static constexpr std::int32_t explosion_count = 1024;

    explicit ParticleSystem(const ParticleSettings &settings = {});

    /// The particles that live, in no order a host can count on. The span
    /// holds until the next call that is not `const`.
    [[nodiscard]] std::span<const Particle> GetParticles() const { return _particles; }

    /// The most particles there are at a time.
    [[nodiscard]] std::size_t GetLimit() const { return _limit; }

    /// The clock particles die by: the sum of what Advance() was given.
    [[nodiscard]] float GetTime() const { return _time; }

    /// Removes every particle, as for a new level. The clock and the
    /// random numbers go on.
    void Clear();

    /// Lets `dt` seconds pass: removes the particles whose time is over,
    /// and moves and changes the others by their kind. `gravity` is how
    /// fast things of the level fall, `sv_gravity`, 800 in the original.
    /// Particles fall by a twentieth of it. A `dt` under zero is taken as
    /// zero.
    void Advance(float dt, float gravity);

    /// A puff at a place: `count` particles in a box of 16 units around
    /// `origin`, of the eight colours of the palette `colour` is in, that
    /// go 15 times `direction` in a second and fall. It is what the game
    /// code's `particle` makes, and with it a nail or a shot that hits a
    /// wall, and blood.
    ///
    /// A `count` of `explosion_count` makes the particles of Explosion()
    /// instead. The original's message has one byte for the count, and
    /// 255 in it means that count: a host that reads the message turns
    /// 255 into `explosion_count`.
    void RunEffect(const LevelVector &origin, const LevelVector &direction, std::int32_t colour, std::int32_t count);

    /// The explosion of a rocket: 1024 particles that fly out of a box of
    /// 32 units, through the colours of fire, for up to 0.8 seconds.
    void Explosion(const LevelVector &origin);

    /// An explosion of colours the game code picks: 512 particles that
    /// go through `colour_length` colours of the palette from
    /// `colour_start` on, one after the other, and are gone after 0.3
    /// seconds.
    void Explosion2(const LevelVector &origin, std::int32_t colour_start, std::int32_t colour_length);

    /// The explosion of a spawn, the blue blob: 1024 particles, half of
    /// them purple ones that fly out, half of them blue ones that stay
    /// close, for 1 or 1.4 seconds.
    void BlobExplosion(const LevelVector &origin);

    /// The splash of the lava a boss rises out of: 1024 particles on a
    /// square of 256 units around `origin` that fly up and outwards, for 2
    /// to 2.6 seconds.
    void LavaSplash(const LevelVector &origin);

    /// The flash of a teleporter: 896 particles in a box of 32 by 32 by 56
    /// units around `origin` that fly outwards, for 0.2 to 0.34 seconds.
    void TeleportSplash(const LevelVector &origin);

    /// What a moving thing leaves behind on its way from `start` to
    /// `end`: the way of one frame. One particle is made for every 3
    /// units of the way, for every 6 with `SlightBlood`.
    ///
    /// As in the original, the particles are not spread over the whole
    /// way. The first is at `start`, and each next one is 1 unit further
    /// towards `end`: they lie on the first third of the way. A host calls
    /// this in every frame with a short way, so it does not show.
    ///
    /// The particles of a tracer go to the side, each to the other side
    /// than the one before it, also from one call to the next.
    ///
    /// A trail that is none of the kinds makes nothing.
    void Trail(const LevelVector &start, const LevelVector &end, ParticleTrail trail);
  };
} // quake

#endif //QUAKE_PARTICLE_SYSTEM_HPP
