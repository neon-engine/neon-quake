#include "particle-system.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <span>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace
{
  using quake::Length;
  using quake::LevelVector;
  using quake::Particle;
  using quake::ParticleKind;
  using quake::ParticleSettings;
  using quake::ParticleSystem;
  using quake::ParticleTrail;
  using ::testing::AllOf;
  using ::testing::Each;
  using ::testing::Field;
  using ::testing::FloatEq;
  using ::testing::FloatNear;
  using ::testing::Ge;
  using ::testing::Le;

  /// How fast things fall in the original, `sv_gravity`.
  constexpr float gravity = 800.0f;

  /// A place away from the middle of the level, so that a mixed up axis
  /// shows.
  constexpr LevelVector origin = {100.0f, -200.0f, 300.0f};

  /// Says that a particle is in a box around `origin`, given by how far
  /// it reaches to both sides on each axis.
  void ExpectInBox(const Particle &particle, const LevelVector &low, const LevelVector &high)
  {
    for (std::size_t axis = 0; axis < 3; ++axis)
    {
      EXPECT_GE(particle.position[axis], origin[axis] + low[axis]) << "axis " << axis;
      EXPECT_LE(particle.position[axis], origin[axis] + high[axis]) << "axis " << axis;
    }
  }

  std::ptrdiff_t CountOf(const std::span<const Particle> particles, const ParticleKind kind)
  {
    return std::ranges::count_if(particles, [kind](const Particle &particle) { return particle.kind == kind; });
  }

  bool Same(const std::span<const Particle> a, const std::span<const Particle> b)
  {
    return std::ranges::equal(a, b, [](const Particle &left, const Particle &right)
    {
      return left.position == right.position && left.velocity == right.velocity && left.colour == right.colour
             && left.kind == right.kind && left.ramp == right.ramp && left.dies_at == right.dies_at;
    });
  }

  /// Says that the particles are those of the explosion of a rocket.
  void ExpectExplosion(const std::span<const Particle> particles)
  {
    ASSERT_EQ(particles.size(), 1024u);
    EXPECT_EQ(CountOf(particles, ParticleKind::Explode), 512);
    EXPECT_EQ(CountOf(particles, ParticleKind::Explode2), 512);
    for (const Particle &particle: particles)
    {
      ExpectInBox(particle, {-16.0f, -16.0f, -16.0f}, {15.0f, 15.0f, 15.0f});
      EXPECT_EQ(particle.colour, 0x6f);
      EXPECT_THAT(particle.ramp, AllOf(Ge(0.0f), Le(3.0f)));
      EXPECT_THAT(particle.velocity, Each(AllOf(Ge(-256.0f), Le(255.0f))));
    }
  }

  TEST(ParticleSystemTest, StartsEmptyWithTheLimitOfPorts)
  {
    const ParticleSystem system;

    EXPECT_TRUE(system.GetParticles().empty());
    EXPECT_EQ(system.GetLimit(), 16384u);
    EXPECT_EQ(system.GetTime(), 0.0f);
  }

  TEST(ParticleSystemTest, AnEffectIsAPuffOfTheColoursAskedForThatGoesWhereItIsSent)
  {
    ParticleSystem system;

    system.RunEffect(origin, {1.0f, -2.0f, 0.5f}, 73, 20);

    const auto particles = system.GetParticles();
    ASSERT_EQ(particles.size(), 20u);
    for (const Particle &particle: particles)
    {
      ExpectInBox(particle, {-8.0f, -8.0f, -8.0f}, {7.0f, 7.0f, 7.0f});
      EXPECT_THAT(particle.colour, AllOf(Ge(72), Le(79)));
      EXPECT_EQ(particle.kind, ParticleKind::SlowGravity);
      EXPECT_EQ(particle.velocity, (LevelVector{15.0f, -30.0f, 7.5f}));
      EXPECT_THAT(particle.dies_at, AllOf(Ge(0.0f), Le(0.4001f)));
    }
  }

  TEST(ParticleSystemTest, AnEffectIsGoneAfterLessThanHalfASecond)
  {
    ParticleSystem system;
    system.RunEffect(origin, {}, 0, 50);

    system.Advance(0.41f, gravity);

    EXPECT_TRUE(system.GetParticles().empty());
  }

  TEST(ParticleSystemTest, AnEffectOfNoneOrLessMakesNothing)
  {
    ParticleSystem system;

    system.RunEffect(origin, {}, 0, 0);
    system.RunEffect(origin, {}, 0, -5);

    EXPECT_TRUE(system.GetParticles().empty());
  }

  TEST(ParticleSystemTest, AnEffectOf1024IsAnExplosion)
  {
    ParticleSystem system;

    system.RunEffect(origin, {1.0f, 0.0f, 0.0f}, 73, ParticleSystem::explosion_count);

    ExpectExplosion(system.GetParticles());
  }

  TEST(ParticleSystemTest, AnExplosionIs1024ParticlesOfTwoKindsOutOfABox)
  {
    ParticleSystem system;

    system.Explosion(origin);

    ExpectExplosion(system.GetParticles());
  }

  TEST(ParticleSystemTest, AnExplosionWalksItsColoursAndDiesWhenTheyEnd)
  {
    constexpr std::array<std::uint8_t, 8> explode = {0x6f, 0x6d, 0x6b, 0x69, 0x67, 0x65, 0x63, 0x61};
    constexpr std::array<std::uint8_t, 8> explode2 = {0x6f, 0x6e, 0x6d, 0x6c, 0x6b, 0x6a, 0x68, 0x66};
    ParticleSystem system;
    system.Explosion(origin);

    // a step that is half a colour of the one kind and three quarters of
    // one of the other, so that no sum is rounded
    constexpr float step = 0.05f;
    std::size_t before = system.GetParticles().size();
    bool saw_last_colour = false;
    for (std::int32_t i = 0; i < 16; ++i)
    {
      system.Advance(step, gravity);

      const auto particles = system.GetParticles();
      EXPECT_LE(particles.size(), before);
      before = particles.size();
      for (const Particle &particle: particles)
      {
        // one whose colours ran out is here for this one frame
        if (particle.ramp >= 8.0f) { continue; }
        const auto &ramp = particle.kind == ParticleKind::Explode ? explode : explode2;
        EXPECT_EQ(particle.colour, ramp[static_cast<std::size_t>(particle.ramp)]);
        saw_last_colour = saw_last_colour || particle.colour == 0x61;
      }
    }
    EXPECT_TRUE(saw_last_colour);

    // the slowest starts at the first colour and has ten a second
    system.Advance(step, gravity);
    EXPECT_TRUE(system.GetParticles().empty());
    EXPECT_LT(system.GetTime(), 1.0f);
  }

  TEST(ParticleSystemTest, TheOneKindOfAnExplosionGetsFasterAndTheOtherSlower)
  {
    ParticleSystem system;
    system.Explosion(origin);
    const Particle slower = system.GetParticles()[0];
    const Particle faster = system.GetParticles()[1];
    ASSERT_EQ(slower.kind, ParticleKind::Explode2);
    ASSERT_EQ(faster.kind, ParticleKind::Explode);

    system.Advance(0.1f, 0.0f);

    EXPECT_FLOAT_EQ(system.GetParticles()[0].velocity[0], slower.velocity[0] * 0.9f);
    EXPECT_FLOAT_EQ(system.GetParticles()[1].velocity[0], faster.velocity[0] * 1.4f);
    EXPECT_FLOAT_EQ(system.GetParticles()[1].position[0], faster.position[0] + faster.velocity[0] * 0.1f);
  }

  TEST(ParticleSystemTest, AnExplosionOfColoursGoesThroughThemAndIsGoneSoon)
  {
    ParticleSystem system;

    system.Explosion2(origin, 32, 5);

    const auto particles = system.GetParticles();
    ASSERT_EQ(particles.size(), 512u);
    for (std::size_t i = 0; i < particles.size(); ++i)
    {
      ExpectInBox(particles[i], {-16.0f, -16.0f, -16.0f}, {15.0f, 15.0f, 15.0f});
      EXPECT_EQ(particles[i].colour, 32 + i % 5);
      EXPECT_EQ(particles[i].kind, ParticleKind::Blob);
    }

    system.Advance(0.29f, gravity);
    EXPECT_EQ(system.GetParticles().size(), 512u);
    system.Advance(0.02f, gravity);
    EXPECT_TRUE(system.GetParticles().empty());
  }

  TEST(ParticleSystemTest, AnExplosionOfNoColoursHasTheFirst)
  {
    ParticleSystem system;

    system.Explosion2(origin, 32, 0);

    EXPECT_THAT(system.GetParticles(), Each(Field(&Particle::colour, 32)));
  }

  TEST(ParticleSystemTest, ABlobExplosionIsHalfPurpleAndHalfBlue)
  {
    ParticleSystem system;

    system.BlobExplosion(origin);

    const auto particles = system.GetParticles();
    ASSERT_EQ(particles.size(), 1024u);
    EXPECT_EQ(CountOf(particles, ParticleKind::Blob), 512);
    EXPECT_EQ(CountOf(particles, ParticleKind::Blob2), 512);
    for (const Particle &particle: particles)
    {
      ExpectInBox(particle, {-16.0f, -16.0f, -16.0f}, {15.0f, 15.0f, 15.0f});
      if (particle.kind == ParticleKind::Blob) { EXPECT_THAT(particle.colour, AllOf(Ge(66), Le(71))); }
      else { EXPECT_THAT(particle.colour, AllOf(Ge(150), Le(155))); }
      EXPECT_TRUE(particle.dies_at == 1.0f || std::abs(particle.dies_at - 1.4f) < 0.001f) << particle.dies_at;
    }
  }

  TEST(ParticleSystemTest, TheBlueOfABlobSlowsDownSidewaysAndThePurpleGetsFaster)
  {
    ParticleSystem system;
    system.BlobExplosion(origin);
    const Particle blue = system.GetParticles()[0];
    const Particle purple = system.GetParticles()[1];
    ASSERT_EQ(blue.kind, ParticleKind::Blob2);
    ASSERT_EQ(purple.kind, ParticleKind::Blob);

    system.Advance(0.1f, gravity);

    const Particle &blue_after = system.GetParticles()[0];
    EXPECT_FLOAT_EQ(blue_after.velocity[0], blue.velocity[0] * 0.6f);
    EXPECT_FLOAT_EQ(blue_after.velocity[1], blue.velocity[1] * 0.6f);
    EXPECT_FLOAT_EQ(blue_after.velocity[2], blue.velocity[2] - 4.0f);
    const Particle &purple_after = system.GetParticles()[1];
    EXPECT_FLOAT_EQ(purple_after.velocity[0], purple.velocity[0] * 1.4f);
    EXPECT_FLOAT_EQ(purple_after.velocity[2], purple.velocity[2] * 1.4f - 4.0f);
  }

  TEST(ParticleSystemTest, ALavaSplashIsASquareThatFliesUp)
  {
    ParticleSystem system;

    system.LavaSplash(origin);

    const auto particles = system.GetParticles();
    ASSERT_EQ(particles.size(), 1024u);
    for (const Particle &particle: particles)
    {
      ExpectInBox(particle, {-128.0f, -128.0f, 0.0f}, {127.0f, 127.0f, 63.0f});
      EXPECT_THAT(particle.colour, AllOf(Ge(224), Le(231)));
      EXPECT_EQ(particle.kind, ParticleKind::SlowGravity);
      EXPECT_THAT(Length(particle.velocity), AllOf(Ge(49.99f), Le(113.01f)));
      EXPECT_GT(particle.velocity[2], 40.0f);
      EXPECT_THAT(particle.dies_at, AllOf(Ge(2.0f), Le(2.6201f)));
    }
  }

  TEST(ParticleSystemTest, ATeleportSplashIsABoxThatFliesApart)
  {
    ParticleSystem system;

    system.TeleportSplash(origin);

    const auto particles = system.GetParticles();
    ASSERT_EQ(particles.size(), 896u);
    for (const Particle &particle: particles)
    {
      ExpectInBox(particle, {-16.0f, -16.0f, -24.0f}, {15.0f, 15.0f, 31.0f});
      EXPECT_THAT(particle.colour, AllOf(Ge(7), Le(14)));
      EXPECT_EQ(particle.kind, ParticleKind::SlowGravity);
      // the one in the middle has nowhere to fly
      EXPECT_THAT(Length(particle.velocity), AllOf(Ge(0.0f), Le(113.01f)));
      EXPECT_THAT(particle.dies_at, AllOf(Ge(0.2f), Le(0.3401f)));
    }

    system.Advance(0.35f, gravity);
    EXPECT_TRUE(system.GetParticles().empty());
  }

  TEST(ParticleSystemTest, ATrailHasAParticleForEveryThreeUnitsEachOneUnitFurther)
  {
    ParticleSystem system;
    const LevelVector end = {origin[0], origin[1] + 30.0f, origin[2]};

    system.Trail(origin, end, ParticleTrail::WizardTracer);

    const auto particles = system.GetParticles();
    ASSERT_EQ(particles.size(), 10u);
    for (std::size_t i = 0; i < particles.size(); ++i)
    {
      EXPECT_FLOAT_EQ(particles[i].position[0], origin[0]);
      EXPECT_FLOAT_EQ(particles[i].position[1], origin[1] + static_cast<float>(i));
      EXPECT_FLOAT_EQ(particles[i].position[2], origin[2]);
    }
  }

  TEST(ParticleSystemTest, ATrailCountsAStartedStretchOfThreeUnits)
  {
    ParticleSystem system;

    system.Trail(origin, {origin[0] + 3.5f, origin[1], origin[2]}, ParticleTrail::Blood);
    EXPECT_EQ(system.GetParticles().size(), 2u);

    system.Trail(origin, origin, ParticleTrail::Blood);
    EXPECT_EQ(system.GetParticles().size(), 2u);
  }

  TEST(ParticleSystemTest, ARocketLeavesFireAndAGrenadeSmoke)
  {
    constexpr std::array<std::uint8_t, 6> fire = {0x6d, 0x6b, 6, 5, 4, 3};
    ParticleSystem system;
    const LevelVector end = {origin[0], origin[1], origin[2] + 60.0f};

    system.Trail(origin, end, ParticleTrail::Rocket);
    system.Trail(origin, end, ParticleTrail::Smoke);

    const auto particles = system.GetParticles();
    ASSERT_EQ(particles.size(), 40u);
    for (std::size_t i = 0; i < particles.size(); ++i)
    {
      const Particle &particle = particles[i];
      const float along = static_cast<float>(i % 20);
      ExpectInBox(particle, {-3.0f, -3.0f, along - 3.0f}, {2.0f, 2.0f, along + 2.0f});
      EXPECT_EQ(particle.kind, ParticleKind::Fire);
      EXPECT_EQ(particle.velocity, LevelVector{});
      EXPECT_EQ(particle.colour, fire[static_cast<std::size_t>(particle.ramp)]);
      if (i < 20) { EXPECT_THAT(particle.ramp, AllOf(Ge(0.0f), Le(3.0f))); }
      else { EXPECT_THAT(particle.ramp, AllOf(Ge(2.0f), Le(5.0f))); }
    }
  }

  TEST(ParticleSystemTest, FireRisesThroughItsColoursAndDiesWhenTheyEnd)
  {
    ParticleSystem system;
    system.Trail(origin, {origin[0] + 3.0f, origin[1], origin[2]}, ParticleTrail::Rocket);
    ASSERT_EQ(system.GetParticles().size(), 1u);
    const Particle start = system.GetParticles()[0];

    // a tenth of a second is half a colour
    system.Advance(0.1f, gravity);
    system.Advance(0.1f, gravity);

    ASSERT_EQ(system.GetParticles().size(), 1u);
    const Particle &risen = system.GetParticles()[0];
    EXPECT_FLOAT_EQ(risen.ramp, start.ramp + 1.0f);
    EXPECT_FLOAT_EQ(risen.velocity[2], 8.0f);
    EXPECT_GT(risen.position[2], start.position[2]);

    // six colours at five a second
    for (std::int32_t i = 0; i < 12; ++i) { system.Advance(0.1f, gravity); }
    EXPECT_TRUE(system.GetParticles().empty());
  }

  TEST(ParticleSystemTest, BloodIsRedAndSlightBloodHalfAsMuch)
  {
    ParticleSystem system;
    const LevelVector end = {origin[0] + 60.0f, origin[1], origin[2]};

    system.Trail(origin, end, ParticleTrail::Blood);
    EXPECT_EQ(system.GetParticles().size(), 20u);
    system.Trail(origin, end, ParticleTrail::SlightBlood);
    EXPECT_EQ(system.GetParticles().size(), 30u);

    for (const Particle &particle: system.GetParticles())
    {
      EXPECT_EQ(particle.kind, ParticleKind::Gravity);
      EXPECT_THAT(particle.colour, AllOf(Ge(67), Le(70)));
      EXPECT_FLOAT_EQ(particle.dies_at, 2.0f);
    }
  }

  TEST(ParticleSystemTest, ATracerGoesToBothSidesInTurnAlsoFromCallToCall)
  {
    ParticleSystem system;
    // towards +x: the sides are -y and +y
    const LevelVector end = {origin[0] + 9.0f, origin[1], origin[2]};

    system.Trail(origin, end, ParticleTrail::WizardTracer);
    system.Trail(origin, end, ParticleTrail::KnightTracer);
    system.Trail(origin, end, ParticleTrail::KnightTracer);

    const auto particles = system.GetParticles();
    ASSERT_EQ(particles.size(), 9u);
    for (std::size_t i = 0; i < particles.size(); ++i)
    {
      const Particle &particle = particles[i];
      EXPECT_EQ(particle.kind, ParticleKind::Static);
      EXPECT_FLOAT_EQ(particle.dies_at, 0.5f);
      EXPECT_EQ(particle.velocity, (LevelVector{0.0f, i % 2 == 0 ? -30.0f : 30.0f, 0.0f})) << i;
      // four of the one colour, then four of the other, over all calls
      const std::uint8_t first = i < 3 ? 52 : 230;
      EXPECT_EQ(particle.colour, first + (i % 8 < 4 ? 0 : 8)) << i;
    }

    system.Advance(0.25f, gravity);
    EXPECT_FLOAT_EQ(system.GetParticles()[0].position[1], origin[1] - 7.5f);
    system.Advance(0.26f, gravity);
    EXPECT_TRUE(system.GetParticles().empty());
  }

  TEST(ParticleSystemTest, AVoorTrailIsAPurpleCloudThatStays)
  {
    ParticleSystem system;

    system.Trail(origin, {origin[0], origin[1], origin[2] + 3.0f}, ParticleTrail::Voor);

    ASSERT_EQ(system.GetParticles().size(), 1u);
    const Particle &particle = system.GetParticles()[0];
    ExpectInBox(particle, {-8.0f, -8.0f, -8.0f}, {7.0f, 7.0f, 7.0f});
    EXPECT_THAT(particle.colour, AllOf(Ge(152), Le(155)));
    EXPECT_EQ(particle.kind, ParticleKind::Static);
    EXPECT_FLOAT_EQ(particle.dies_at, 0.3f);
  }

  TEST(ParticleSystemTest, ATrailThatIsNoneOfTheKindsMakesNothing)
  {
    ParticleSystem system;

    system.Trail(origin, {}, static_cast<ParticleTrail>(7));
    system.Trail(origin, {}, static_cast<ParticleTrail>(-1));

    EXPECT_TRUE(system.GetParticles().empty());
  }

  TEST(ParticleSystemTest, AParticleFallsByATwentiethOfTheGravityOfTheLevel)
  {
    ParticleSystem system;
    system.Trail(origin, {origin[0] + 3.0f, origin[1], origin[2]}, ParticleTrail::Blood);
    const float height = system.GetParticles()[0].position[2];

    // a second, in frames of a hundredth
    for (std::int32_t i = 0; i < 100; ++i) { system.Advance(0.01f, gravity); }

    ASSERT_EQ(system.GetParticles().size(), 1u);
    const Particle &fallen = system.GetParticles()[0];
    // 40 units a second faster in every second, so 20 units in the first.
    // A frame moves by the speed of the frame before, which loses a
    // little of it.
    EXPECT_THAT(fallen.velocity[2], FloatNear(-40.0f, 0.01f));
    EXPECT_THAT(height - fallen.position[2], FloatNear(19.8f, 0.01f));
  }

  TEST(ParticleSystemTest, WithoutGravityNothingFalls)
  {
    ParticleSystem system;
    system.Trail(origin, {origin[0] + 3.0f, origin[1], origin[2]}, ParticleTrail::Blood);
    const LevelVector place = system.GetParticles()[0].position;

    system.Advance(1.0f, 0.0f);

    EXPECT_EQ(system.GetParticles()[0].position, place);
  }

  TEST(ParticleSystemTest, TimeThatGoesBackIsNoTime)
  {
    ParticleSystem system;
    system.LavaSplash(origin);
    const Particle before = system.GetParticles()[0];

    system.Advance(-1.0f, gravity);

    EXPECT_EQ(system.GetTime(), 0.0f);
    EXPECT_EQ(system.GetParticles()[0].position, before.position);
    EXPECT_EQ(system.GetParticles()[0].velocity, before.velocity);
  }

  TEST(ParticleSystemTest, ThereAreNeverMoreParticlesThanTheLimitAndTheDeadMakeRoom)
  {
    ParticleSystem system(ParticleSettings{.limit = 100});
    const Particle *storage = system.GetParticles().data();

    system.Explosion(origin);
    EXPECT_EQ(system.GetParticles().size(), 100u);
    system.LavaSplash(origin);
    system.Trail(origin, {}, ParticleTrail::Rocket);
    system.RunEffect(origin, {}, 0, 10);
    EXPECT_EQ(system.GetParticles().size(), 100u);

    // the time of an explosion is over; the lava that found no room would
    // still be there
    system.Advance(1.0f, gravity);
    system.Advance(0.01f, gravity);
    EXPECT_TRUE(system.GetParticles().empty());

    system.TeleportSplash(origin);
    EXPECT_EQ(system.GetParticles().size(), 100u);
    EXPECT_EQ(system.GetParticles().data(), storage);
  }

  TEST(ParticleSystemTest, ClearRemovesEveryParticle)
  {
    ParticleSystem system;
    system.LavaSplash(origin);

    system.Clear();

    EXPECT_TRUE(system.GetParticles().empty());
  }

  TEST(ParticleSystemTest, TheSameSeedGivesTheSameParticles)
  {
    const auto run = [](const std::uint32_t seed)
    {
      ParticleSystem system(ParticleSettings{.seed = seed});
      system.Explosion(origin);
      system.Trail(origin, {}, ParticleTrail::Rocket);
      system.Advance(0.1f, gravity);
      system.LavaSplash(origin);
      system.Advance(0.1f, gravity);
      return system;
    };

    const ParticleSystem first = run(7);
    const ParticleSystem second = run(7);
    const ParticleSystem other = run(8);

    EXPECT_FALSE(first.GetParticles().empty());
    EXPECT_TRUE(Same(first.GetParticles(), second.GetParticles()));
    EXPECT_FALSE(Same(first.GetParticles(), other.GetParticles()));
  }

  TEST(ParticleSystemTest, AFullSystemIsAdvancedInFarLessThanAFrame)
  {
    ParticleSystem system;
    // every kind that lives for a while, until there is no room
    while (system.GetParticles().size() < system.GetLimit())
    {
      system.LavaSplash(origin);
      system.BlobExplosion(origin);
      system.Explosion(origin);
      system.Trail(origin, {origin[0] + 3000.0f, origin[1], origin[2]}, ParticleTrail::Smoke);
    }
    ASSERT_EQ(system.GetParticles().size(), 16384u);

    // no time passes, so that the system stays full; the work is the same
    constexpr std::int32_t rounds = 200;
    const auto start = std::chrono::steady_clock::now();
    for (std::int32_t i = 0; i < rounds; ++i) { system.Advance(0.0f, gravity); }
    const std::chrono::duration<double, std::micro> taken = std::chrono::steady_clock::now() - start;

    ASSERT_EQ(system.GetParticles().size(), 16384u);
    const double each = taken.count() / rounds;
    std::cout << "Advance of 16384 particles: " << each << " microseconds\n";
    // a frame at 60 a second is 16667; this leaves room for a slow machine
    // and a build without optimisation
    EXPECT_LT(each, 4000.0);
  }
} // namespace
