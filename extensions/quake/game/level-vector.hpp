#ifndef QUAKE_LEVEL_VECTOR_HPP
#define QUAKE_LEVEL_VECTOR_HPP

#include <array>
#include <cmath>

namespace quake
{
  /// Three numbers as the machine hands them out: a place, a direction, or
  /// a corner of a box, in the units and axes of the game.
  using LevelVector = std::array<float, 3>;

  // The arithmetic that moving entities through a level is made of.

  [[nodiscard]] constexpr LevelVector Sum(const LevelVector &a, const LevelVector &b)
  {
    return {a[0] + b[0], a[1] + b[1], a[2] + b[2]};
  }

  [[nodiscard]] constexpr LevelVector Difference(const LevelVector &a, const LevelVector &b)
  {
    return {a[0] - b[0], a[1] - b[1], a[2] - b[2]};
  }

  [[nodiscard]] constexpr LevelVector Scaled(const LevelVector &vector, const float scale)
  {
    return {vector[0] * scale, vector[1] * scale, vector[2] * scale};
  }

  [[nodiscard]] constexpr float Dot(const LevelVector &a, const LevelVector &b)
  {
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
  }

  [[nodiscard]] constexpr LevelVector Cross(const LevelVector &a, const LevelVector &b)
  {
    return {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]};
  }

  [[nodiscard]] inline float Length(const LevelVector &vector)
  {
    return std::sqrt(Dot(vector, vector));
  }

  [[nodiscard]] constexpr bool IsZero(const LevelVector &vector)
  {
    return vector[0] == 0.0f && vector[1] == 0.0f && vector[2] == 0.0f;
  }
} // quake

#endif //QUAKE_LEVEL_VECTOR_HPP
