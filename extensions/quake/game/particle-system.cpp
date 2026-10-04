#include "particle-system.hpp"

#include <algorithm>
#include <array>

namespace quake
{
  // Helpers of ParticleSystem: the colours and the numbers the original
  // has in its code.
  namespace
  {
    /// The colours a particle walks through, as places in the palette:
    /// those of `Explode`, those of `Explode2`, and those of `Fire`, which
    /// has six.
    constexpr std::array<std::uint8_t, 8> explode_ramp = {0x6f, 0x6d, 0x6b, 0x69, 0x67, 0x65, 0x63, 0x61};
    constexpr std::array<std::uint8_t, 8> explode2_ramp = {0x6f, 0x6e, 0x6d, 0x6c, 0x6b, 0x6a, 0x68, 0x66};
    constexpr std::array<std::uint8_t, 6> fire_ramp = {0x6d, 0x6b, 6, 5, 4, 3};

    /// How many colours a kind walks through in a second.
    constexpr float fire_ramp_speed = 5.0f;
    constexpr float explode_ramp_speed = 10.0f;
    constexpr float explode2_ramp_speed = 15.0f;

    /// By how much of its velocity a particle gets faster or slower in a
    /// second: `Explode`, `Blob`, and `Blob2` by the first, `Explode2` by
    /// the second.
    constexpr float velocity_change = 4.0f;
    constexpr float explode2_slowdown = 1.0f;

    /// Which part of how fast things of the level fall a particle falls
    /// by.
    constexpr float gravity_scale = 0.05f;

    /// How far a particle of a trail is from the one before it, and how
    /// much of the way one particle stands for.
    constexpr float trail_step = 1.0f;
    constexpr float trail_spacing = 3.0f;

    /// How fast a particle of a tracer goes to the side.
    constexpr float tracer_speed = 30.0f;

    /// The colour a ramp is at. A ramp is never under zero.
    template<std::size_t Size>
    std::uint8_t ColourAt(const std::array<std::uint8_t, Size> &ramp, const float position)
    {
      return ramp[std::min(static_cast<std::size_t>(position), Size - 1)];
    }

    /// A direction made one unit long. One of no length is left as it is.
    LevelVector Normalized(const LevelVector &vector)
    {
      const float length = Length(vector);
      return length != 0.0f ? Scaled(vector, 1.0f / length) : vector;
    }

    /// A number as a colour of the palette, which has 256.
    std::uint8_t ToColour(const std::int32_t value)
    {
      return static_cast<std::uint8_t>(value & 255);
    }
  }

  ParticleSystem::ParticleSystem(const ParticleSettings &settings) : _limit(settings.limit), _random(settings.seed)
  {
    _particles.reserve(_limit);
  }

  Particle *ParticleSystem::Add()
  {
    if (_particles.size() >= _limit) { return nullptr; }
    return &_particles.emplace_back();
  }

  void ParticleSystem::Scatter(Particle &particle, const LevelVector &origin)
  {
    for (std::size_t axis = 0; axis < 3; ++axis)
    {
      particle.position[axis] = origin[axis] + static_cast<float>(_random.Next() % 32 - 16);
      particle.velocity[axis] = static_cast<float>(_random.Next() % 512 - 256);
    }
  }

  void ParticleSystem::Clear()
  {
    _particles.clear();
  }

  void ParticleSystem::Advance(float dt, const float gravity)
  {
    dt = std::max(dt, 0.0f);
    _time += dt;
    std::erase_if(_particles, [this](const Particle &particle) { return particle.dies_at < _time; });

    const float fall = dt * gravity * gravity_scale;
    const float change = dt * velocity_change;
    for (Particle &particle: _particles)
    {
      particle.position = Sum(particle.position, Scaled(particle.velocity, dt));

      // a particle whose colours ran out keeps the last one and is gone
      // in the next call
      switch (particle.kind)
      {
        case ParticleKind::Static:
          break;
        case ParticleKind::Fire:
          particle.ramp += dt * fire_ramp_speed;
          if (particle.ramp >= static_cast<float>(fire_ramp.size())) { particle.dies_at = -1.0f; }
          else { particle.colour = ColourAt(fire_ramp, particle.ramp); }
          particle.velocity[2] += fall;
          break;
        case ParticleKind::Explode:
          particle.ramp += dt * explode_ramp_speed;
          if (particle.ramp >= static_cast<float>(explode_ramp.size())) { particle.dies_at = -1.0f; }
          else { particle.colour = ColourAt(explode_ramp, particle.ramp); }
          particle.velocity = Sum(particle.velocity, Scaled(particle.velocity, change));
          particle.velocity[2] -= fall;
          break;
        case ParticleKind::Explode2:
          particle.ramp += dt * explode2_ramp_speed;
          if (particle.ramp >= static_cast<float>(explode2_ramp.size())) { particle.dies_at = -1.0f; }
          else { particle.colour = ColourAt(explode2_ramp, particle.ramp); }
          particle.velocity = Difference(particle.velocity, Scaled(particle.velocity, dt * explode2_slowdown));
          particle.velocity[2] -= fall;
          break;
        case ParticleKind::Blob:
          particle.velocity = Sum(particle.velocity, Scaled(particle.velocity, change));
          particle.velocity[2] -= fall;
          break;
        case ParticleKind::Blob2:
          particle.velocity[0] -= particle.velocity[0] * change;
          particle.velocity[1] -= particle.velocity[1] * change;
          particle.velocity[2] -= fall;
          break;
        case ParticleKind::Gravity:
        case ParticleKind::SlowGravity:
          particle.velocity[2] -= fall;
          break;
      }
    }
  }

  void ParticleSystem::RunEffect(const LevelVector &origin, const LevelVector &direction, const std::int32_t colour,
                                 const std::int32_t count)
  {
    if (count == explosion_count)
    {
      Explosion(origin);
      return;
    }

    for (std::int32_t i = 0; i < count; ++i)
    {
      Particle *particle = Add();
      if (particle == nullptr) { return; }

      particle->dies_at = _time + 0.1f * static_cast<float>(_random.Next() % 5);
      particle->colour = ToColour((colour & ~7) + (_random.Next() & 7));
      particle->kind = ParticleKind::SlowGravity;
      for (std::size_t axis = 0; axis < 3; ++axis)
      {
        particle->position[axis] = origin[axis] + static_cast<float>((_random.Next() & 15) - 8);
        particle->velocity[axis] = direction[axis] * 15.0f;
      }
    }
  }

  void ParticleSystem::Explosion(const LevelVector &origin)
  {
    for (std::int32_t i = 0; i < explosion_count; ++i)
    {
      Particle *particle = Add();
      if (particle == nullptr) { return; }

      // it dies by its colours long before this
      particle->dies_at = _time + 5.0f;
      particle->colour = explode_ramp[0];
      particle->ramp = static_cast<float>(_random.Next() & 3);
      particle->kind = i % 2 == 1 ? ParticleKind::Explode : ParticleKind::Explode2;
      Scatter(*particle, origin);
    }
  }

  void ParticleSystem::Explosion2(const LevelVector &origin, const std::int32_t colour_start,
                                  const std::int32_t colour_length)
  {
    // the original divides by a length of zero
    const std::int32_t length = std::max(colour_length, 1);
    for (std::int32_t i = 0; i < 512; ++i)
    {
      Particle *particle = Add();
      if (particle == nullptr) { return; }

      particle->dies_at = _time + 0.3f;
      particle->colour = ToColour(colour_start + i % length);
      particle->kind = ParticleKind::Blob;
      Scatter(*particle, origin);
    }
  }

  void ParticleSystem::BlobExplosion(const LevelVector &origin)
  {
    for (std::int32_t i = 0; i < 1024; ++i)
    {
      Particle *particle = Add();
      if (particle == nullptr) { return; }

      // one bit of the number: it is 1 second or 1.4, nothing between
      particle->dies_at = _time + 1.0f + static_cast<float>(_random.Next() & 8) * 0.05f;
      if (i % 2 == 1)
      {
        particle->kind = ParticleKind::Blob;
        particle->colour = ToColour(66 + _random.Next() % 6);
      }
      else
      {
        particle->kind = ParticleKind::Blob2;
        particle->colour = ToColour(150 + _random.Next() % 6);
      }
      Scatter(*particle, origin);
    }
  }

  void ParticleSystem::LavaSplash(const LevelVector &origin)
  {
    for (std::int32_t i = -16; i < 16; ++i)
    {
      for (std::int32_t j = -16; j < 16; ++j)
      {
        Particle *particle = Add();
        if (particle == nullptr) { return; }

        particle->dies_at = _time + 2.0f + static_cast<float>(_random.Next() & 31) * 0.02f;
        particle->colour = ToColour(224 + (_random.Next() & 7));
        particle->kind = ParticleKind::SlowGravity;

        // where it is on the square is also where it flies: outwards, and
        // mostly up
        LevelVector direction;
        direction[0] = static_cast<float>(j * 8 + (_random.Next() & 7));
        direction[1] = static_cast<float>(i * 8 + (_random.Next() & 7));
        direction[2] = 256.0f;

        particle->position[0] = origin[0] + direction[0];
        particle->position[1] = origin[1] + direction[1];
        particle->position[2] = origin[2] + static_cast<float>(_random.Next() & 63);

        const float speed = static_cast<float>(50 + (_random.Next() & 63));
        particle->velocity = Scaled(Normalized(direction), speed);
      }
    }
  }

  void ParticleSystem::TeleportSplash(const LevelVector &origin)
  {
    for (std::int32_t i = -16; i < 16; i += 4)
    {
      for (std::int32_t j = -16; j < 16; j += 4)
      {
        for (std::int32_t k = -24; k < 32; k += 4)
        {
          Particle *particle = Add();
          if (particle == nullptr) { return; }

          particle->dies_at = _time + 0.2f + static_cast<float>(_random.Next() & 7) * 0.02f;
          particle->colour = ToColour(7 + (_random.Next() & 7));
          particle->kind = ParticleKind::SlowGravity;

          // the original has the first two the other way round here than
          // in the place of the particle
          const LevelVector direction = {static_cast<float>(j * 8), static_cast<float>(i * 8),
                                         static_cast<float>(k * 8)};

          particle->position[0] = origin[0] + static_cast<float>(i + (_random.Next() & 3));
          particle->position[1] = origin[1] + static_cast<float>(j + (_random.Next() & 3));
          particle->position[2] = origin[2] + static_cast<float>(k + (_random.Next() & 3));

          const float speed = static_cast<float>(50 + (_random.Next() & 63));
          particle->velocity = Scaled(Normalized(direction), speed);
        }
      }
    }
  }

  void ParticleSystem::Trail(const LevelVector &start, const LevelVector &end, const ParticleTrail trail)
  {
    if (trail < ParticleTrail::Rocket || trail > ParticleTrail::Voor) { return; }

    const LevelVector way = Difference(end, start);
    const LevelVector direction = Normalized(way);
    const LevelVector step = Scaled(direction, trail_step);
    float left = Length(way);
    LevelVector place = start;

    /// A place within a few units of where the trail is at.
    const auto jitter = [this, &place](Particle &particle, const bool wide)
    {
      for (std::size_t axis = 0; axis < 3; ++axis)
      {
        const std::int32_t offset = wide ? (_random.Next() & 15) - 8 : _random.Next() % 6 - 3;
        particle.position[axis] = place[axis] + static_cast<float>(offset);
      }
    };

    while (left > 0.0f)
    {
      left -= trail_spacing;

      Particle *particle = Add();
      if (particle == nullptr) { return; }

      particle->dies_at = _time + 2.0f;
      switch (trail)
      {
        case ParticleTrail::Rocket:
          particle->ramp = static_cast<float>(_random.Next() & 3);
          particle->colour = ColourAt(fire_ramp, particle->ramp);
          particle->kind = ParticleKind::Fire;
          jitter(*particle, false);
          break;
        case ParticleTrail::Smoke:
          // further through the colours: it starts as smoke
          particle->ramp = static_cast<float>((_random.Next() & 3) + 2);
          particle->colour = ColourAt(fire_ramp, particle->ramp);
          particle->kind = ParticleKind::Fire;
          jitter(*particle, false);
          break;
        case ParticleTrail::Blood:
          particle->kind = ParticleKind::Gravity;
          particle->colour = ToColour(67 + (_random.Next() & 3));
          jitter(*particle, false);
          break;
        case ParticleTrail::WizardTracer:
        case ParticleTrail::KnightTracer:
        {
          particle->dies_at = _time + 0.5f;
          particle->kind = ParticleKind::Static;
          // four particles of one colour, then four of the other
          const std::int32_t first = trail == ParticleTrail::WizardTracer ? 52 : 230;
          particle->colour = ToColour(first + static_cast<std::int32_t>((_tracer_count & 4u) << 1));
          ++_tracer_count;

          particle->position = place;
          const float side = _tracer_count % 2u == 1u ? tracer_speed : -tracer_speed;
          particle->velocity = {side * direction[1], -side * direction[0], 0.0f};
          break;
        }
        case ParticleTrail::SlightBlood:
          particle->kind = ParticleKind::Gravity;
          particle->colour = ToColour(67 + (_random.Next() & 3));
          jitter(*particle, false);
          left -= trail_spacing;
          break;
        case ParticleTrail::Voor:
          particle->colour = ToColour(9 * 16 + 8 + (_random.Next() & 3));
          particle->kind = ParticleKind::Static;
          particle->dies_at = _time + 0.3f;
          jitter(*particle, true);
          break;
      }

      place = Sum(place, step);
    }
  }
} // quake
