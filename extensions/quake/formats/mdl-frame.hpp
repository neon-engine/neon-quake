#ifndef QUAKE_MDL_FRAME_HPP
#define QUAKE_MDL_FRAME_HPP

#include <vector>

#include "mdl-packed-vertex.hpp"
#include "mdl-pose.hpp"

namespace quake
{
  /// One frame of a model, which is what the game code names by a number:
  /// a pose, or a group of poses that plays by itself, as the flame of a
  /// torch does.
  struct MdlFrame
  {
    /// Whether the file has this frame as a group, which may hold one pose
    /// only.
    bool is_group = false;

    /// The smallest and the largest place of any vertex in any pose of the
    /// frame.
    MdlPackedVertex minimum;
    MdlPackedVertex maximum;

    /// One pose, or the poses of the group in their order.
    std::vector<MdlPose> poses;

    /// For a group, the time in seconds since the start of the group at
    /// which each pose ends, one for each pose. Empty otherwise.
    std::vector<float> times;
  };
} // quake

#endif //QUAKE_MDL_FRAME_HPP
