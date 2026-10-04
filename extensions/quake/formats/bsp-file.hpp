#ifndef QUAKE_BSP_FILE_HPP
#define QUAKE_BSP_FILE_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "bsp-clip-node.hpp"
#include "bsp-edge.hpp"
#include "bsp-face.hpp"
#include "bsp-format.hpp"
#include "bsp-leaf.hpp"
#include "bsp-lump.hpp"
#include "bsp-model.hpp"
#include "bsp-node.hpp"
#include "bsp-plane.hpp"
#include "bsp-texture-info.hpp"
#include "bsp-vector.hpp"
#include "mip-texture.hpp"

namespace quake
{
  /// The parts of a level, in the order its header lists them.
  enum class BspLumpKind
  {
    Entities,
    Planes,
    Textures,
    Vertices,
    Visibility,
    Nodes,
    TextureInfos,
    Faces,
    Lighting,
    ClipNodes,
    Leaves,
    LeafFaces,
    Edges,
    FaceEdges,
    Models,
  };

  /// A level of the game, `maps/*.bsp`, as its file has it: version 29, the
  /// one of the original game, or one of the forms that lift its limits,
  /// `BSP2` and `2PSB`.
  ///
  /// The forms have the same lumps, with numbers of different widths. They
  /// are read into the same entries, which are as wide as the widest form,
  /// so that what uses a level does not mind which form its file had.
  ///
  /// This is the file and nothing made from it. Every number keeps the
  /// meaning, the units, and the axes of the game, with Z pointing up. What
  /// a renderer draws is made from it by `BspMesh`.
  ///
  /// A level that was read is whole: every lump lies inside the file, and
  /// every number that names an entry of another lump names one that is
  /// there, so that what uses the level may follow them without asking.
  struct BspFile
  {
    /// What the file of a level of the original game starts with.
    static constexpr std::int32_t version = 29;

    /// What the files of the wider forms start with in the place of a
    /// version: the letters `BSP2`, and the letters `2PSB`.
    static constexpr std::int32_t bsp2_magic = 0x32505342;
    static constexpr std::int32_t bsp2_rmq_magic = 0x42535032;

    static constexpr std::size_t lump_count = 15;

    /// How many bytes the header takes: the version, and the place and size
    /// of every lump.
    static constexpr std::size_t header_size = 4 + lump_count * 8;

    /// The form the file was written in.
    BspFormat format = BspFormat::Version29;

    /// Where each lump was in the file.
    std::array<BspLump, lump_count> lumps{};

    /// The things in the level, as the text `EntityText` reads.
    std::string entities;

    std::vector<BspPlane> planes;

    /// The textures of the walls. One that the level names but does not
    /// carry is empty.
    std::vector<std::optional<MipTexture>> textures;

    std::vector<BspVector> vertices;

    /// Which leaves are seen from which, packed as the game packs them. It
    /// is kept as it is.
    std::vector<std::uint8_t> visibility;

    std::vector<BspNode> nodes;

    std::vector<BspTextureInfo> texture_infos;

    std::vector<BspFace> faces;

    /// The lightmaps of all faces, one byte for each sample, from dark to
    /// twice as bright as the texture.
    std::vector<std::uint8_t> lighting;

    std::vector<BspClipNode> clip_nodes;

    std::vector<BspLeaf> leaves;

    /// The faces of the leaves: each entry is the number of a face.
    std::vector<std::uint32_t> leaf_faces;

    std::vector<BspEdge> edges;

    /// The edges of the faces. A positive entry is an edge walked from its
    /// first vertex to its second, a negative one is edge `-entry` walked
    /// the other way.
    std::vector<std::int32_t> face_edges;

    std::vector<BspModel> models;

    /// Takes the level from the bytes of its file. Returns false, says what
    /// was wrong in `error`, and stays as it was when they are not a level
    /// of a form that is read, when a lump lies outside them or does not hold whole
    /// entries, or when an entry names something that is not there.
    bool Read(std::span<const std::uint8_t> bytes, std::string &error);

    [[nodiscard]] const BspLump &GetLump(BspLumpKind kind) const;
  };
} // quake

#endif //QUAKE_BSP_FILE_HPP
