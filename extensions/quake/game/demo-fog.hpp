#ifndef QUAKE_DEMO_FOG_HPP
#define QUAKE_DEMO_FOG_HPP

namespace quake
{
  /// The fog of a level, as FitzQuake has a server set it.
  struct DemoFog
  {
    /// How thick, from 0 to 1.
    float density = 0.0f;

    /// The colour, each from 0 to 1.
    float red = 0.0f;
    float green = 0.0f;
    float blue = 0.0f;

    /// Over how many seconds the fog that was turns into this one.
    float seconds = 0.0f;
  };
} // quake

#endif //QUAKE_DEMO_FOG_HPP
