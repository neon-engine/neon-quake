#include "mip-texture.hpp"

#include <utility>

#include "byte-reader.hpp"

namespace quake
{
  // Helpers of MipTexture: what the letters of a name are compared with.
  namespace
  {
    /// A letter as its small form, so that `SKY1` is the sky too. Archives
    /// of textures are not careful about the case of names.
    char to_lower(const char letter)
    {
      return letter >= 'A' && letter <= 'Z' ? static_cast<char>(letter - 'A' + 'a') : letter;
    }
  }

  bool MipTexture::Read(const std::span<const std::uint8_t> bytes, std::string &error)
  {
    if (bytes.size() < header_size)
    {
      error = "it has " + std::to_string(bytes.size()) + " bytes, fewer than the " + std::to_string(header_size) +
        " before its pictures";
      return false;
    }

    ByteReader reader(bytes);
    MipTexture texture;
    texture.name = reader.ReadFixedString(16);
    texture.width = reader.ReadU32();
    texture.height = reader.ReadU32();

    if (texture.width == 0 || texture.height == 0)
    {
      error = "\"" + texture.name + "\" is " + std::to_string(texture.width) + " by " +
        std::to_string(texture.height) + " pixels";
      return false;
    }

    // where each size starts, counted from the name
    std::array<std::uint32_t, level_count> offsets{};
    for (std::uint32_t &offset : offsets) { offset = reader.ReadU32(); }

    for (std::size_t level = 0; level < level_count; level++)
    {
      // a tool that writes only the full size leaves the others at zero
      if (level > 0 && offsets[level] == 0) { continue; }

      const std::size_t level_width = texture.width >> level;
      const std::size_t level_height = texture.height >> level;

      // a count too large for a number of the machine cannot be in the bytes either
      if (level_height != 0 && level_width > bytes.size() / level_height)
      {
        error = "\"" + texture.name + "\" is " + std::to_string(texture.width) + " by " +
          std::to_string(texture.height) + " pixels, more than its " + std::to_string(bytes.size()) + " bytes hold";
        return false;
      }

      const auto picture = reader.BytesAt(offsets[level], level_width * level_height);
      if (!reader.IsGood())
      {
        error = "size " + std::to_string(level) + " of \"" + texture.name + "\", " + std::to_string(level_width) +
          " by " + std::to_string(level_height) + " pixels from byte " + std::to_string(offsets[level]) +
          ", lies outside its " + std::to_string(bytes.size()) + " bytes";
        return false;
      }
      texture.pixels[level].assign(picture.begin(), picture.end());
    }

    *this = std::move(texture);
    return true;
  }

  bool MipTexture::IsLiquid() const
  {
    // a name that starts with `*` is a liquid: `*lava`, `*slime`, `*tele`,
    // and every other one water
    return !name.empty() && name[0] == '*';
  }

  bool MipTexture::IsSky() const
  {
    // a name that starts with `sky` is the sky
    return name.size() >= 3 && to_lower(name[0]) == 's' && to_lower(name[1]) == 'k' && to_lower(name[2]) == 'y';
  }

  bool MipTexture::HasHoles() const
  {
    // a name that starts with `{` has holes where its pixels are colour 255
    return !name.empty() && name[0] == '{';
  }

  int MipTexture::GetAnimationFrame() const
  {
    // a name that starts with `+` is a frame of an animation, and its second
    // letter says which: `+0` to `+9` are the frames shown in turn, `+a` to
    // `+j` those of the second animation of the same name
    if (name.size() < 2 || name[0] != '+') { return -1; }

    const char frame = to_lower(name[1]);
    if (frame >= '0' && frame <= '9') { return frame - '0'; }
    if (frame >= 'a' && frame <= 'j') { return frame - 'a'; }
    return -1;
  }

  bool MipTexture::IsAlternateAnimation() const
  {
    if (name.size() < 2 || name[0] != '+') { return false; }

    const char frame = to_lower(name[1]);
    return frame >= 'a' && frame <= 'j';
  }
} // quake
