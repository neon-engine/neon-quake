#ifndef QUAKE_MDL_TEST_MODEL_HPP
#define QUAKE_MDL_TEST_MODEL_HPP

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include "mdl-file.hpp"

namespace quake
{
  /// A model for the tests of the format, which make the bytes they read
  /// themselves: what a file would hold, and the bytes of such a file.
  ///
  /// The counts of the header are written as they stand, whatever the lists
  /// hold, so that a test can make a file that lies.
  struct MdlTestModel
  {
    std::uint32_t magic = MdlFile::magic;
    std::int32_t version = MdlFile::version;
    MdlHeader header;
    std::vector<MdlSkin> skins;
    std::vector<MdlTextureCoordinate> texture_coordinates;
    std::vector<MdlTriangle> triangles;
    std::vector<MdlFrame> frames;

    /// Where the counts of the header are in the bytes, for a test that
    /// changes one after the bytes were made.
    static constexpr std::size_t skin_count_at = 48;
    static constexpr std::size_t skin_width_at = 52;
    static constexpr std::size_t skin_height_at = 56;
    static constexpr std::size_t vertex_count_at = 60;
    static constexpr std::size_t triangle_count_at = 64;
    static constexpr std::size_t frame_count_at = 68;
    static constexpr std::size_t header_size = 84;

    static void PutU32(std::vector<std::uint8_t> &bytes, const std::uint32_t value)
    {
      for (int shift = 0; shift < 32; shift += 8)
      {
        bytes.push_back(static_cast<std::uint8_t>(value >> shift));
      }
    }

    static void PutI32(std::vector<std::uint8_t> &bytes, const std::int32_t value)
    {
      PutU32(bytes, static_cast<std::uint32_t>(value));
    }

    static void PutF32(std::vector<std::uint8_t> &bytes, const float value)
    {
      std::uint32_t bits;
      std::memcpy(&bits, &value, sizeof(bits));
      PutU32(bytes, bits);
    }

    static void PutVector(std::vector<std::uint8_t> &bytes, const MdlVector &vector)
    {
      PutF32(bytes, vector.x);
      PutF32(bytes, vector.y);
      PutF32(bytes, vector.z);
    }

    static void PutPackedVertex(std::vector<std::uint8_t> &bytes, const MdlPackedVertex &vertex)
    {
      bytes.insert(bytes.end(), vertex.position.begin(), vertex.position.end());
      bytes.push_back(vertex.normal_index);
    }

    static void PutPose(std::vector<std::uint8_t> &bytes, const MdlPose &pose)
    {
      PutPackedVertex(bytes, pose.minimum);
      PutPackedVertex(bytes, pose.maximum);
      for (std::size_t i = 0; i < 16; i++)
      {
        bytes.push_back(i < pose.name.size() ? static_cast<std::uint8_t>(pose.name[i]) : 0);
      }
      for (const MdlPackedVertex &vertex : pose.vertices)
      {
        PutPackedVertex(bytes, vertex);
      }
    }

    /// Writes a number over the four bytes at a place.
    static void Overwrite(std::vector<std::uint8_t> &bytes, const std::size_t at, const std::int32_t value)
    {
      for (std::size_t i = 0; i < 4; i++)
      {
        bytes[at + i] = static_cast<std::uint8_t>(static_cast<std::uint32_t>(value) >> (8 * i));
      }
    }

    /// The bytes of the file of this model.
    [[nodiscard]] std::vector<std::uint8_t> ToBytes() const
    {
      std::vector<std::uint8_t> bytes;
      PutU32(bytes, magic);
      PutI32(bytes, version);
      PutVector(bytes, header.scale);
      PutVector(bytes, header.translate);
      PutF32(bytes, header.bounding_radius);
      PutVector(bytes, header.eye_position);
      PutI32(bytes, header.skin_count);
      PutI32(bytes, header.skin_width);
      PutI32(bytes, header.skin_height);
      PutI32(bytes, header.vertex_count);
      PutI32(bytes, header.triangle_count);
      PutI32(bytes, header.frame_count);
      PutI32(bytes, header.sync_type);
      PutU32(bytes, header.flags);
      PutF32(bytes, header.size);

      for (const MdlSkin &skin : skins)
      {
        PutI32(bytes, skin.is_group ? 1 : 0);
        if (skin.is_group)
        {
          PutI32(bytes, static_cast<std::int32_t>(skin.pictures.size()));
          for (const float time : skin.times) { PutF32(bytes, time); }
        }
        for (const auto &picture : skin.pictures)
        {
          bytes.insert(bytes.end(), picture.begin(), picture.end());
        }
      }

      for (const MdlTextureCoordinate &coordinate : texture_coordinates)
      {
        // the file has 32 for a vertex on the seam, not 1
        PutI32(bytes, coordinate.on_seam ? 0x20 : 0);
        PutI32(bytes, coordinate.s);
        PutI32(bytes, coordinate.t);
      }

      for (const MdlTriangle &triangle : triangles)
      {
        PutI32(bytes, triangle.faces_front ? 1 : 0);
        for (const std::int32_t vertex : triangle.vertices) { PutI32(bytes, vertex); }
      }

      for (const MdlFrame &frame : frames)
      {
        PutI32(bytes, frame.is_group ? 1 : 0);
        if (frame.is_group)
        {
          PutI32(bytes, static_cast<std::int32_t>(frame.poses.size()));
          PutPackedVertex(bytes, frame.minimum);
          PutPackedVertex(bytes, frame.maximum);
          for (const float time : frame.times) { PutF32(bytes, time); }
        }
        for (const MdlPose &pose : frame.poses)
        {
          PutPose(bytes, pose);
        }
      }
      return bytes;
    }

    /// A picture of the size of the skins whose pixels count up from
    /// `first`.
    [[nodiscard]] std::vector<std::uint8_t> MakePicture(const std::uint8_t first) const
    {
      std::vector<std::uint8_t> picture;
      const int count = header.skin_width * header.skin_height;
      for (int i = 0; i < count; i++)
      {
        picture.push_back(static_cast<std::uint8_t>(first + i));
      }
      return picture;
    }

    /// A pose of the tetrahedron whose top is `height` up, and whose
    /// vertices name the normals `normal`, `normal` + 1, and so on.
    static MdlPose MakePose(const std::string &name, const std::uint8_t height, const std::uint8_t normal)
    {
      MdlPose pose;
      pose.minimum.position = {0, 0, 0};
      pose.maximum.position = {10, 10, height};
      pose.name = name;
      pose.vertices = {
        {{0, 0, 0}, normal},
        {{10, 0, 0}, static_cast<std::uint8_t>(normal + 1)},
        {{0, 10, 0}, static_cast<std::uint8_t>(normal + 2)},
        {{0, 0, height}, static_cast<std::uint8_t>(normal + 3)},
      };
      return pose;
    }

    /// A tetrahedron with one skin of 8 by 4 and two frames: a corner at
    /// the origin and one along each axis, 10 away, the one on z 20 away in
    /// the second frame. The corner at the origin is on the seam, and the
    /// triangle that lies in the plane of y and z is of the back.
    static MdlTestModel MakeTetrahedron()
    {
      MdlTestModel model;
      model.header.scale = {1.0f, 1.0f, 1.0f};
      model.header.bounding_radius = 20.0f;
      model.header.eye_position = {1.0f, 2.0f, 3.0f};
      model.header.skin_count = 1;
      model.header.skin_width = 8;
      model.header.skin_height = 4;
      model.header.vertex_count = 4;
      model.header.triangle_count = 4;
      model.header.frame_count = 2;
      model.header.sync_type = 1;
      model.header.flags = 8;
      model.header.size = 2.5f;

      MdlSkin skin;
      skin.pictures.push_back(model.MakePicture(0));
      model.skins.push_back(skin);

      model.texture_coordinates = {{true, 3, 0}, {false, 0, 3}, {false, 2, 3}, {false, 1, 1}};

      // each goes around clockwise seen from outside
      model.triangles = {
        {true, {0, 1, 2}},
        {true, {0, 3, 1}},
        {false, {0, 2, 3}},
        {true, {1, 3, 2}},
      };

      for (int i = 0; i < 2; i++)
      {
        MdlFrame frame;
        frame.poses.push_back(MakePose(i == 0 ? "stand1" : "stand2", i == 0 ? 10 : 20, i == 0 ? 0 : 100));
        frame.minimum = frame.poses[0].minimum;
        frame.maximum = frame.poses[0].maximum;
        model.frames.push_back(frame);
      }
      return model;
    }

    /// The tetrahedron with a second skin that is a group of two pictures,
    /// and a third frame that is a group of two poses.
    static MdlTestModel MakeWithGroups()
    {
      MdlTestModel model = MakeTetrahedron();

      MdlSkin skin;
      skin.is_group = true;
      skin.pictures = {model.MakePicture(50), model.MakePicture(100)};
      skin.times = {0.25f, 0.5f};
      model.skins.push_back(skin);
      model.header.skin_count = 2;

      MdlFrame frame;
      frame.is_group = true;
      frame.minimum.position = {0, 0, 0};
      frame.maximum.position = {10, 10, 40};
      frame.poses = {MakePose("flame1", 30, 10), MakePose("flame2", 40, 20)};
      frame.times = {0.1f, 0.2f};
      model.frames.push_back(frame);
      model.header.frame_count = 3;
      return model;
    }
  };
} // quake

#endif //QUAKE_MDL_TEST_MODEL_HPP
