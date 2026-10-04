#ifndef QUAKE_DEMO_INTERMISSION_HPP
#define QUAKE_DEMO_INTERMISSION_HPP

namespace quake
{
  /// Which screen a demo shows over the level.
  enum class DemoIntermission
  {
    /// None: the level runs.
    None,
    /// The level is over, and its tally is shown.
    Tally,
    /// An episode is over, and its text is shown.
    Finale,
    /// A text over the view of a camera.
    Cutscene,
  };
} // quake

#endif //QUAKE_DEMO_INTERMISSION_HPP
