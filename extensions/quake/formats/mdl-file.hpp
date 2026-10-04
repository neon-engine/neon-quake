#ifndef QUAKE_MDL_FILE_HPP
#define QUAKE_MDL_FILE_HPP

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "mdl-frame.hpp"
#include "mdl-header.hpp"
#include "mdl-pose.hpp"
#include "mdl-skin.hpp"
#include "mdl-texture-coordinate.hpp"
#include "mdl-triangle.hpp"

namespace quake
{
  /// A model of the game, `progs/*.mdl`: a monster, a weapon, an item. What
  /// the file holds, as it holds it, and nothing made of it yet; `MdlMesh`
  /// makes of it what a renderer draws.
  ///
  /// The file is a header, the skins, where on the skin each vertex is, the
  /// triangles, and the frames, each of which places every vertex anew. The
  /// vertices and triangles are the same in every frame, only the places
  /// change.
  ///
  /// What was read can be trusted: every count is what its list holds, every
  /// triangle names vertices the model has, every pose has a place for every
  /// vertex, every picture has the size of the header.
  class MdlFile
  {
    MdlHeader _header;
    std::vector<MdlSkin> _skins;
    std::vector<MdlTextureCoordinate> _texture_coordinates;
    std::vector<MdlTriangle> _triangles;
    std::vector<MdlFrame> _frames;

  public:
    /// The four letters a model starts with, `IDPO`, read as a number.
    static constexpr std::uint32_t magic = 0x4f504449;

    /// The only version there is of the format.
    static constexpr std::int32_t version = 6;

    /// How many directions the table of normals of the original game has. A
    /// vertex that names another one is refused.
    static constexpr std::int32_t normal_count = 162;

    /// The most a model may have of each thing. The original game allowed
    /// far less, and the ports that followed more and more; these are past
    /// what any model has, and only keep a file that lies from asking for
    /// more memory than there is.
    static constexpr std::int32_t most_skins = 1024;
    static constexpr std::int32_t most_skin_side = 8192;
    static constexpr std::int32_t most_vertices = 65536;
    static constexpr std::int32_t most_triangles = 131072;
    static constexpr std::int32_t most_frames = 65536;
    static constexpr std::int32_t most_in_group = 65536;

    /// Takes the model from the bytes of the file. Returns false, says in
    /// `error` what was wrong, and stays as it was, when the bytes are not a
    /// model: another magic or version, a count that is negative or past the
    /// most there may be, a file that ends before what it announced, a
    /// triangle that names a vertex that is not there, a vertex that names a
    /// normal that is not there. Bytes after the last frame are left alone.
    bool Read(std::span<const std::uint8_t> bytes, std::string &error);

    [[nodiscard]] const MdlHeader &GetHeader() const;

    [[nodiscard]] const std::vector<MdlSkin> &GetSkins() const;

    /// One for each vertex.
    [[nodiscard]] const std::vector<MdlTextureCoordinate> &GetTextureCoordinates() const;

    [[nodiscard]] const std::vector<MdlTriangle> &GetTriangles() const;

    [[nodiscard]] const std::vector<MdlFrame> &GetFrames() const;

    /// A pose of a frame: `pose` is 0 for a frame that is not a group, and
    /// the place in the group otherwise. Null when there is no such frame or
    /// no such pose in it.
    [[nodiscard]] const MdlPose *FindPose(std::size_t frame, std::size_t pose = 0) const;
  };
} // quake

#endif //QUAKE_MDL_FILE_HPP
