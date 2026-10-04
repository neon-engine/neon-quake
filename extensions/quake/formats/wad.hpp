#ifndef QUAKE_WAD_HPP
#define QUAKE_WAD_HPP

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "picture.hpp"
#include "wad-lump.hpp"

namespace quake
{
  /// An archive of the kind `gfx.wad` is, called WAD2: lumps of bytes, each
  /// with a name and a letter for what it holds, found through a directory.
  ///
  /// The file starts with `WAD2`, the number of lumps, and where the
  /// directory is, four bytes each. The directory has 32 bytes for each
  /// lump: where it is, its size in the file, its size unpacked, a byte for
  /// what it holds, a byte for how it is packed, two bytes of nothing, and
  /// 16 bytes of name.
  ///
  /// A wad copies nothing: it keeps the bytes it was read from in sight and
  /// hands out parts of them, so those bytes have to stay where they are
  /// for as long as the wad and what it handed out are in use.
  ///
  /// Textures with smaller copies, `WadLumpType::MipTexture`, are not read
  /// here. Their bytes are handed out like any others, for the reader of
  /// the level format.
  class Wad final
  {
    std::span<const std::uint8_t> _bytes;
    std::vector<WadLump> _lumps;

  public:
    /// The name of the lump with the letters of the console. It says it is
    /// a mip texture, and is 128 by 128 pixels without a width and a height
    /// in front.
    static constexpr std::string_view characters_name = "conchars";

    /// How many pixels each side of the letters of the console has.
    static constexpr std::int32_t characters_side = 128;

    /// Reads the directory of a wad from the bytes of its file.
    ///
    /// Returns false, says in `error` what was wrong, and stays as it was,
    /// when the bytes are not a wad: too few for the start, something else
    /// than `WAD2` in front, a number that is less than nothing, a
    /// directory that is not all inside the file, or a lump that is not.
    bool Read(std::span<const std::uint8_t> bytes, std::string &error);

    /// Every lump, in the order of the directory.
    [[nodiscard]] const std::vector<WadLump> &GetLumps() const;

    /// The lump of a name, upper and lower case being one, or `nullptr`
    /// when there is none. Of several lumps with one name it is the first.
    [[nodiscard]] const WadLump *Find(std::string_view name) const;

    /// The bytes of a lump as they are in the file, without copying them.
    /// Empty for a lump that is not inside the file, which one that
    /// GetLumps() or Find() gave never is.
    [[nodiscard]] std::span<const std::uint8_t> GetBytes(const WadLump &lump) const;

    /// Reads the lump of a name as a picture: one of the status bar, which
    /// has its width and height in front as a `.lmp` file has, or the
    /// letters of the console, which are known by their name.
    ///
    /// Returns false, says in `error` what was wrong, and leaves `picture`
    /// as it was, when there is no such lump, when it is packed, when it
    /// holds something else than a picture, or when its bytes are not one.
    bool ReadPicture(std::string_view name, Picture &picture, std::string &error) const;
  };
} // quake

#endif //QUAKE_WAD_HPP
