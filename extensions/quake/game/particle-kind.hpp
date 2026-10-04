#ifndef QUAKE_PARTICLE_KIND_HPP
#define QUAKE_PARTICLE_KIND_HPP

namespace quake
{
  /// How a particle changes while it lives, with the names the original
  /// has for it. A second of the game is meant by "a second", and
  /// "gravity" is a twentieth of how fast things of the level fall, see
  /// ParticleSystem::Advance().
  enum class ParticleKind
  {
    /// Keeps its velocity and its colour until its time is over.
    Static,
    /// Falls. The original meant it to fall faster than `SlowGravity`, but
    /// the code for that was never compiled in: the two are the same.
    Gravity,
    /// Falls.
    SlowGravity,
    /// Rises, and walks the colours of fire and smoke, five a second. It
    /// dies after the sixth.
    Fire,
    /// Gets faster, falls, and walks the colours of an explosion, ten a
    /// second. It dies after the eighth.
    Explode,
    /// Gets slower, falls, and walks other colours of an explosion,
    /// fifteen a second. It dies after the eighth.
    Explode2,
    /// Gets faster and falls.
    Blob,
    /// Gets slower sideways and falls.
    Blob2,
  };
} // quake

#endif //QUAKE_PARTICLE_KIND_HPP
