#ifndef QUAKE_WAD_LUMP_HPP
#define QUAKE_WAD_LUMP_HPP

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace quake
{
  /// What a lump of a wad holds, as the byte of its entry says it: a
  /// letter. A wad may say any byte; the ones the game knows have a name
  /// here, and any other is kept as the number it is.
  enum class WadLumpType : std::uint8_t
  {
    /// A palette, `@`.
    Palette = '@',

    /// A picture of the status bar, `B`: a width, a height, and the pixels,
    /// as a `.lmp` file has them.
    StatusBarPicture = 'B',

    /// A texture with its smaller copies, `D`, as a level holds them. The
    /// letters of the console, `conchars`, say they are one, and are not:
    /// they are 128 by 128 pixels and nothing else.
    MipTexture = 'D',

    /// A picture of the console, `E`.
    ConsolePicture = 'E',
  };

  /// One lump of a wad, as its entry in the directory describes it: its
  /// name, what it holds, and where its bytes are in the file.
  struct WadLump
  {
    /// The name, of 16 bytes at most. `gfx.wad` may write it in upper case,
    /// and the game asks in lower case: `Wad::Find()` takes both as one.
    std::string name;

    WadLumpType type{};

    /// Whether the bytes are packed, and how. Nothing but 0, not packed, is
    /// in use.
    std::uint8_t compression = 0;

    /// Where the bytes start, from the start of the file.
    std::size_t offset = 0;

    /// How many bytes there are in the file.
    std::size_t size_on_disk = 0;

    /// How many bytes there are once unpacked: the same, when not packed.
    std::size_t size = 0;

    /// What the lump holds in words, for a list or a message.
    [[nodiscard]] std::string_view DescribeType() const
    {
      switch (type)
      {
        case WadLumpType::Palette:
        {
          return "palette";
        }
        case WadLumpType::StatusBarPicture:
        {
          return "status bar picture";
        }
        case WadLumpType::MipTexture:
        {
          return "mip texture";
        }
        case WadLumpType::ConsolePicture:
        {
          return "console picture";
        }
      }
      return "unknown";
    }
  };
} // quake

#endif //QUAKE_WAD_LUMP_HPP
