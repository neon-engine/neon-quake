#ifndef QUAKE_DEMO_SCORE_HPP
#define QUAKE_DEMO_SCORE_HPP

#include <cstdint>
#include <string>

namespace quake
{
  /// One player of the scoreboard of a demo.
  struct DemoScore
  {
    /// Empty for a place nobody plays in.
    std::string name;

    std::int32_t frags = 0;

    /// The colour of the shirt in the high four bits, that of the
    /// trousers in the low four.
    std::int32_t colors = 0;
  };
} // quake

#endif //QUAKE_DEMO_SCORE_HPP
