#ifndef QUAKE_PARTICLE_TRAIL_HPP
#define QUAKE_PARTICLE_TRAIL_HPP

namespace quake
{
  /// What a moving thing leaves behind, with the numbers the original has
  /// for it. Which one a model leaves is in the flags of its file.
  enum class ParticleTrail
  {
    /// Fire that turns to smoke and rises, behind a rocket.
    Rocket = 0,
    /// Smoke that rises, behind a grenade.
    Smoke = 1,
    /// Blood that falls, behind a piece of a body.
    Blood = 2,
    /// Two green lines that part, behind what a wizard spits.
    WizardTracer = 3,
    /// Half as much blood, behind what a zombie throws.
    SlightBlood = 4,
    /// Two yellow lines that part, behind what a knight of hell throws.
    KnightTracer = 5,
    /// A purple cloud that stays, behind the ball of a vore.
    Voor = 6,
  };
} // quake

#endif //QUAKE_PARTICLE_TRAIL_HPP
