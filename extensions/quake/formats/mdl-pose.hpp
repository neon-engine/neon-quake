#ifndef QUAKE_MDL_POSE_HPP
#define QUAKE_MDL_POSE_HPP

#include <string>
#include <vector>

#include "mdl-packed-vertex.hpp"

namespace quake
{
  /// The model standing one way: a place for every one of its vertices.
  struct MdlPose
  {
    /// The smallest and the largest place of any vertex, packed as they are.
    MdlPackedVertex minimum;
    MdlPackedVertex maximum;

    /// The name the artist gave it, such as `run3`, of 16 bytes at most.
    std::string name;

    /// As many as the model has vertices, in their order.
    std::vector<MdlPackedVertex> vertices;
  };
} // quake

#endif //QUAKE_MDL_POSE_HPP
