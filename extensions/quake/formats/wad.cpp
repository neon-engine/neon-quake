#include "wad.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <utility>

#include "byte-reader.hpp"
#include "picture-reader.hpp"

namespace quake
{
  // Helpers of Wad: the sizes of the parts of the file, and how names are
  // compared.
  namespace
  {
    /// What a wad starts with.
    constexpr std::array<std::uint8_t, 4> magic = {'W', 'A', 'D', '2'};

    /// How many bytes come before anything else: the four letters, the
    /// number of lumps, and the place of the directory.
    constexpr std::size_t header_size = 12;

    /// How many bytes the directory has for each lump.
    constexpr std::size_t entry_size = 32;

    /// How many bytes of an entry are the name.
    constexpr std::size_t name_size = 16;

    /// Whether two names are the same, upper and lower case being one.
    bool same_name(const std::string_view one, const std::string_view other)
    {
      if (one.size() != other.size()) { return false; }

      const auto lower = [](const char letter)
      {
        return letter >= 'A' && letter <= 'Z' ? static_cast<char>(letter - 'A' + 'a') : letter;
      };
      for (std::size_t i = 0; i < one.size(); i++)
      {
        if (lower(one[i]) != lower(other[i])) { return false; }
      }
      return true;
    }
  }

  bool Wad::Read(const std::span<const std::uint8_t> bytes, std::string &error)
  {
    ByteReader reader(bytes);
    const auto letters = reader.ReadBytes(magic.size());
    const std::int32_t count = reader.ReadI32();
    const std::int32_t directory = reader.ReadI32();
    if (!reader.IsGood())
    {
      error = "a wad starts with " + std::to_string(header_size) + " bytes, and there are " +
        std::to_string(bytes.size());
      return false;
    }

    if (!std::ranges::equal(letters, magic))
    {
      error = "a wad starts with the letters WAD2, and this does not";
      return false;
    }

    if (count < 0)
    {
      error = "the wad says it has " + std::to_string(count) + " lumps";
      return false;
    }

    // the room for the directory is counted in entries, so that no size is
    // multiplied or added that could wrap around
    const bool starts_inside = directory >= 0 && static_cast<std::size_t>(directory) <= bytes.size();
    if (!starts_inside ||
      static_cast<std::size_t>(count) > (bytes.size() - static_cast<std::size_t>(directory)) / entry_size)
    {
      error = "the directory of the wad, " + std::to_string(count) + " lumps at byte " +
        std::to_string(directory) + ", is not inside its " + std::to_string(bytes.size()) + " bytes";
      return false;
    }

    std::vector<WadLump> lumps;
    lumps.reserve(static_cast<std::size_t>(count));

    reader.Seek(static_cast<std::size_t>(directory));
    for (std::int32_t i = 0; i < count; i++)
    {
      const std::int32_t offset = reader.ReadI32();
      const std::int32_t size_on_disk = reader.ReadI32();
      const std::int32_t size = reader.ReadI32();

      WadLump lump;
      lump.type = static_cast<WadLumpType>(reader.ReadU8());
      lump.compression = reader.ReadU8();
      reader.Skip(2);
      lump.name = reader.ReadFixedString(name_size);

      if (offset < 0 || size_on_disk < 0 || size < 0)
      {
        error = "lump " + std::to_string(i) + " of the wad, `" + lump.name + "`, says it is " +
          std::to_string(size_on_disk) + " bytes at byte " + std::to_string(offset) + ", " +
          std::to_string(size) + " unpacked: none of these can be less than nothing";
        return false;
      }

      lump.offset = static_cast<std::size_t>(offset);
      lump.size_on_disk = static_cast<std::size_t>(size_on_disk);
      lump.size = static_cast<std::size_t>(size);

      if (lump.offset > bytes.size() || lump.size_on_disk > bytes.size() - lump.offset)
      {
        error = "lump " + std::to_string(i) + " of the wad, `" + lump.name + "`, " +
          std::to_string(size_on_disk) + " bytes at byte " + std::to_string(offset) + ", is not inside its " +
          std::to_string(bytes.size()) + " bytes";
        return false;
      }

      lumps.push_back(std::move(lump));
    }

    if (!reader.IsGood())
    {
      error = "the directory of the wad is cut short";
      return false;
    }

    _bytes = bytes;
    _lumps = std::move(lumps);
    return true;
  }

  const std::vector<WadLump> &Wad::GetLumps() const
  {
    return _lumps;
  }

  const WadLump *Wad::Find(const std::string_view name) const
  {
    for (const WadLump &lump : _lumps)
    {
      if (same_name(lump.name, name)) { return &lump; }
    }
    return nullptr;
  }

  std::span<const std::uint8_t> Wad::GetBytes(const WadLump &lump) const
  {
    ByteReader reader(_bytes);
    return reader.BytesAt(lump.offset, lump.size_on_disk);
  }

  bool Wad::ReadPicture(const std::string_view name, Picture &picture, std::string &error) const
  {
    const WadLump *lump = Find(name);
    if (lump == nullptr)
    {
      error = "the wad has no lump `" + std::string(name) + "`";
      return false;
    }

    if (lump->compression != 0)
    {
      error = "lump `" + lump->name + "` of the wad is packed, in the way " +
        std::to_string(lump->compression) + ", which is not read";
      return false;
    }

    std::string why;
    bool read = false;
    if (same_name(lump->name, characters_name))
    {
      // known by their name: the letter of what they hold says mip texture
      read = read_raw_picture(GetBytes(*lump), characters_side, characters_side, picture, why);
    } else if (lump->type == WadLumpType::StatusBarPicture)
    {
      read = read_picture(GetBytes(*lump), picture, why);
    } else
    {
      error = "lump `" + lump->name + "` of the wad is not a picture that is read here: it holds " +
        std::string(lump->DescribeType()) + ", type " + std::to_string(static_cast<int>(lump->type));
      return false;
    }

    if (!read)
    {
      error = "lump `" + lump->name + "` of the wad: " + why;
      return false;
    }
    return true;
  }
} // quake
