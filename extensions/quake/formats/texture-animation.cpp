#include "texture-animation.hpp"

#include <array>
#include <cmath>
#include <string_view>

namespace quake
{
  // Helpers of TextureAnimation, for this file alone.
  namespace
  {
    char to_lower(const char letter)
    {
      return letter >= 'A' && letter <= 'Z' ? static_cast<char>(letter - 'A' + 'a') : letter;
    }

    /// Whether two names are the same but for large and small letters.
    bool is_same_name(const std::string_view one, const std::string_view other)
    {
      if (one.size() != other.size()) { return false; }

      for (std::size_t i = 0; i < one.size(); i++)
      {
        if (to_lower(one[i]) != to_lower(other[i])) { return false; }
      }
      return true;
    }
  }

  TextureAnimation TextureAnimation::Find(
    const std::span<const std::optional<MipTexture>> textures,
    const std::int32_t texture)
  {
    TextureAnimation found;
    if (texture < 0 || static_cast<std::size_t>(texture) >= textures.size() || !textures[texture].has_value() ||
        textures[texture]->GetAnimationFrame() < 0)
    {
      found.frames.push_back(texture);
      return found;
    }

    // every texture of the same name after its first two letters, by the
    // place its second letter gives it in its run
    constexpr std::int32_t none = -1;
    std::array<std::int32_t, 10> first;
    std::array<std::int32_t, 10> second;
    first.fill(none);
    second.fill(none);

    const MipTexture &asked = *textures[texture];
    const std::string_view name = std::string_view(asked.name).substr(2);
    for (std::size_t other = 0; other < textures.size(); other++)
    {
      if (!textures[other].has_value()) { continue; }

      const MipTexture &candidate = *textures[other];
      const int frame = candidate.GetAnimationFrame();
      if (frame < 0 || frame >= 10 || !is_same_name(std::string_view(candidate.name).substr(2), name)) { continue; }

      std::array<std::int32_t, 10> &run = candidate.IsAlternateAnimation() ? second : first;
      if (run[static_cast<std::size_t>(frame)] == none) { run[static_cast<std::size_t>(frame)] = static_cast<std::int32_t>(other); }
    }

    // a run with a gap is shown without it
    const bool asked_second = asked.IsAlternateAnimation();
    for (const std::int32_t frame : asked_second ? second : first)
    {
      if (frame != none) { found.frames.push_back(frame); }
    }
    for (const std::int32_t frame : asked_second ? first : second)
    {
      if (frame != none) { found.alternate.push_back(frame); }
    }
    return found;
  }

  std::size_t TextureAnimation::ChooseFrame(const std::size_t count, const double time)
  {
    if (count == 0 || time <= 0.0) { return 0; }

    return static_cast<std::size_t>(std::floor(time * frames_per_second)) % count;
  }
} // quake
