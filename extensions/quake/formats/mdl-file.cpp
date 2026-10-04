#include "mdl-file.hpp"

#include <utility>

#include "byte-reader.hpp"

namespace quake
{
  // Helpers of MdlFile: one reader per part of the file, and the checks they
  // share.
  namespace
  {
    /// How many bytes the file has of each thing.
    constexpr std::size_t header_size = 84;
    constexpr std::size_t texture_coordinate_size = 12;
    constexpr std::size_t triangle_size = 16;
    constexpr std::size_t packed_vertex_size = 4;
    constexpr std::size_t name_size = 16;

    /// Whether `count` things of `size` bytes each are left to read. Asked
    /// before a list is made, so that a count that lies is refused without
    /// making room for it.
    bool has_room(const ByteReader &reader, const std::size_t count, const std::size_t size)
    {
      const std::size_t left = reader.GetSize() - reader.GetPosition();
      return size == 0 || count <= left / size;
    }

    /// Whether a count of the file is between 1 and the most there may be,
    /// saying so in `error` when not.
    bool check_count(const std::int32_t count, const std::int32_t most, const std::string &what, std::string &error)
    {
      if (count >= 1 && count <= most) { return true; }

      error = "The " + what + ", " + std::to_string(count) + ", is not between 1 and " + std::to_string(most);
      return false;
    }

    MdlVector read_vector(ByteReader &reader)
    {
      MdlVector vector;
      vector.x = reader.ReadF32();
      vector.y = reader.ReadF32();
      vector.z = reader.ReadF32();
      return vector;
    }

    MdlPackedVertex read_packed_vertex(ByteReader &reader)
    {
      MdlPackedVertex vertex;
      vertex.position[0] = reader.ReadU8();
      vertex.position[1] = reader.ReadU8();
      vertex.position[2] = reader.ReadU8();
      vertex.normal_index = reader.ReadU8();
      return vertex;
    }

    /// The times of a group, which follow one another without anything
    /// between.
    bool read_times(ByteReader &reader, const std::size_t count, std::vector<float> &times)
    {
      if (!has_room(reader, count, 4)) { return false; }

      times.reserve(count);
      for (std::size_t i = 0; i < count; i++)
      {
        times.push_back(reader.ReadF32());
      }
      return true;
    }

    bool read_header(ByteReader &reader, MdlHeader &header, std::string &error)
    {
      if (!has_room(reader, 1, header_size))
      {
        error = "The file ends before its header";
        return false;
      }

      if (reader.ReadU32() != MdlFile::magic)
      {
        error = "The file does not start with IDPO";
        return false;
      }
      if (const std::int32_t version = reader.ReadI32(); version != MdlFile::version)
      {
        error = "The version is " + std::to_string(version) + ", not " + std::to_string(MdlFile::version);
        return false;
      }

      header.scale = read_vector(reader);
      header.translate = read_vector(reader);
      header.bounding_radius = reader.ReadF32();
      header.eye_position = read_vector(reader);
      header.skin_count = reader.ReadI32();
      header.skin_width = reader.ReadI32();
      header.skin_height = reader.ReadI32();
      header.vertex_count = reader.ReadI32();
      header.triangle_count = reader.ReadI32();
      header.frame_count = reader.ReadI32();
      header.sync_type = reader.ReadI32();
      header.flags = reader.ReadU32();
      header.size = reader.ReadF32();

      return check_count(header.skin_count, MdlFile::most_skins, "number of skins", error) &&
        check_count(header.skin_width, MdlFile::most_skin_side, "width of the skins", error) &&
        check_count(header.skin_height, MdlFile::most_skin_side, "height of the skins", error) &&
        check_count(header.vertex_count, MdlFile::most_vertices, "number of vertices", error) &&
        check_count(header.triangle_count, MdlFile::most_triangles, "number of triangles", error) &&
        check_count(header.frame_count, MdlFile::most_frames, "number of frames", error);
    }

    bool read_skin(
      ByteReader &reader,
      const MdlHeader &header,
      const std::size_t number,
      MdlSkin &skin,
      std::string &error)
    {
      const std::string which = "skin " + std::to_string(number);
      const std::string cut_short = "The file ends before the end of " + which;

      if (!has_room(reader, 1, 4))
      {
        error = cut_short;
        return false;
      }
      skin.is_group = reader.ReadI32() != 0;

      std::size_t picture_count = 1;
      if (skin.is_group)
      {
        if (!has_room(reader, 1, 4))
        {
          error = cut_short;
          return false;
        }
        const std::int32_t count = reader.ReadI32();
        if (!check_count(count, MdlFile::most_in_group, "number of pictures of " + which, error)) { return false; }

        picture_count = static_cast<std::size_t>(count);
        if (!read_times(reader, picture_count, skin.times))
        {
          error = cut_short;
          return false;
        }
      }

      const std::size_t picture_size =
        static_cast<std::size_t>(header.skin_width) * static_cast<std::size_t>(header.skin_height);
      if (!has_room(reader, picture_count, picture_size))
      {
        error = cut_short;
        return false;
      }

      skin.pictures.reserve(picture_count);
      for (std::size_t i = 0; i < picture_count; i++)
      {
        const auto pixels = reader.ReadBytes(picture_size);
        skin.pictures.emplace_back(pixels.begin(), pixels.end());
      }
      return true;
    }

    /// A pose: its bounding box, its name, and a place for every vertex.
    /// `which` names it in what is said of a mistake.
    bool read_pose(
      ByteReader &reader,
      const std::size_t vertex_count,
      const std::string &which,
      MdlPose &pose,
      std::string &error)
    {
      if (!has_room(reader, 1, 2 * packed_vertex_size + name_size))
      {
        error = "The file ends before the end of " + which;
        return false;
      }
      pose.minimum = read_packed_vertex(reader);
      pose.maximum = read_packed_vertex(reader);
      pose.name = reader.ReadFixedString(name_size);

      if (!has_room(reader, vertex_count, packed_vertex_size))
      {
        error = "The file ends before the end of " + which;
        return false;
      }

      pose.vertices.reserve(vertex_count);
      for (std::size_t i = 0; i < vertex_count; i++)
      {
        const MdlPackedVertex vertex = read_packed_vertex(reader);
        if (vertex.normal_index >= MdlFile::normal_count)
        {
          error = "Vertex " + std::to_string(i) + " of " + which + " names normal " +
            std::to_string(vertex.normal_index) + " of " + std::to_string(MdlFile::normal_count);
          return false;
        }
        pose.vertices.push_back(vertex);
      }
      return true;
    }

    bool read_frame(
      ByteReader &reader,
      const MdlHeader &header,
      const std::size_t number,
      MdlFrame &frame,
      std::string &error)
    {
      const auto vertex_count = static_cast<std::size_t>(header.vertex_count);
      const std::string which = "frame " + std::to_string(number);
      const std::string cut_short = "The file ends before the end of " + which;

      if (!has_room(reader, 1, 4))
      {
        error = cut_short;
        return false;
      }
      frame.is_group = reader.ReadI32() != 0;

      if (!frame.is_group)
      {
        MdlPose pose;
        if (!read_pose(reader, vertex_count, which, pose, error)) { return false; }

        frame.minimum = pose.minimum;
        frame.maximum = pose.maximum;
        frame.poses.push_back(std::move(pose));
        return true;
      }

      if (!has_room(reader, 1, 4 + 2 * packed_vertex_size))
      {
        error = cut_short;
        return false;
      }
      const std::int32_t count = reader.ReadI32();
      if (!check_count(count, MdlFile::most_in_group, "number of poses of " + which, error)) { return false; }

      const auto pose_count = static_cast<std::size_t>(count);
      frame.minimum = read_packed_vertex(reader);
      frame.maximum = read_packed_vertex(reader);
      if (!read_times(reader, pose_count, frame.times))
      {
        error = cut_short;
        return false;
      }

      // every pose has at least its bounding box and its name, which keeps
      // a count that lies from making room for poses that are not there
      if (!has_room(reader, pose_count, 2 * packed_vertex_size + name_size))
      {
        error = cut_short;
        return false;
      }
      frame.poses.reserve(pose_count);
      for (std::size_t i = 0; i < pose_count; i++)
      {
        MdlPose pose;
        if (!read_pose(reader, vertex_count, "pose " + std::to_string(i) + " of " + which, pose, error))
        {
          return false;
        }
        frame.poses.push_back(std::move(pose));
      }
      return true;
    }
  }

  bool MdlFile::Read(const std::span<const std::uint8_t> bytes, std::string &error)
  {
    ByteReader reader(bytes);

    MdlHeader header;
    if (!read_header(reader, header, error)) { return false; }

    std::vector<MdlSkin> skins;
    for (std::size_t i = 0; i < static_cast<std::size_t>(header.skin_count); i++)
    {
      MdlSkin skin;
      if (!read_skin(reader, header, i, skin, error)) { return false; }
      skins.push_back(std::move(skin));
    }

    const auto vertex_count = static_cast<std::size_t>(header.vertex_count);
    if (!has_room(reader, vertex_count, texture_coordinate_size))
    {
      error = "The file ends before the end of its texture coordinates";
      return false;
    }
    std::vector<MdlTextureCoordinate> texture_coordinates;
    texture_coordinates.reserve(vertex_count);
    for (std::size_t i = 0; i < vertex_count; i++)
    {
      MdlTextureCoordinate coordinate;
      coordinate.on_seam = reader.ReadI32() != 0;
      coordinate.s = reader.ReadI32();
      coordinate.t = reader.ReadI32();
      texture_coordinates.push_back(coordinate);
    }

    const auto triangle_count = static_cast<std::size_t>(header.triangle_count);
    if (!has_room(reader, triangle_count, triangle_size))
    {
      error = "The file ends before the end of its triangles";
      return false;
    }
    std::vector<MdlTriangle> triangles;
    triangles.reserve(triangle_count);
    for (std::size_t i = 0; i < triangle_count; i++)
    {
      MdlTriangle triangle;
      triangle.faces_front = reader.ReadI32() != 0;
      for (std::int32_t &vertex : triangle.vertices)
      {
        vertex = reader.ReadI32();
        if (vertex < 0 || vertex >= header.vertex_count)
        {
          error = "Triangle " + std::to_string(i) + " names vertex " + std::to_string(vertex) + " of " +
            std::to_string(header.vertex_count);
          return false;
        }
      }
      triangles.push_back(triangle);
    }

    std::vector<MdlFrame> frames;
    for (std::size_t i = 0; i < static_cast<std::size_t>(header.frame_count); i++)
    {
      MdlFrame frame;
      if (!read_frame(reader, header, i, frame, error)) { return false; }
      frames.push_back(std::move(frame));
    }

    // every read was asked for before it was made, so this cannot be, and
    // is asked all the same
    if (!reader.IsGood())
    {
      error = "The file ends before what it announced";
      return false;
    }

    _header = header;
    _skins = std::move(skins);
    _texture_coordinates = std::move(texture_coordinates);
    _triangles = std::move(triangles);
    _frames = std::move(frames);
    return true;
  }

  const MdlHeader &MdlFile::GetHeader() const
  {
    return _header;
  }

  const std::vector<MdlSkin> &MdlFile::GetSkins() const
  {
    return _skins;
  }

  const std::vector<MdlTextureCoordinate> &MdlFile::GetTextureCoordinates() const
  {
    return _texture_coordinates;
  }

  const std::vector<MdlTriangle> &MdlFile::GetTriangles() const
  {
    return _triangles;
  }

  const std::vector<MdlFrame> &MdlFile::GetFrames() const
  {
    return _frames;
  }

  const MdlPose *MdlFile::FindPose(const std::size_t frame, const std::size_t pose) const
  {
    if (frame >= _frames.size() || pose >= _frames[frame].poses.size()) { return nullptr; }
    return &_frames[frame].poses[pose];
  }
} // quake
