#ifndef QUAKE_BSP_ENTITY_HPP
#define QUAKE_BSP_ENTITY_HPP

#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace quake
{
  /// A thing of a level as its text describes it: the world, a light, a
  /// monster, a door. It is keys with their values, in the order they were
  /// written, and what they mean is for the game code to say.
  struct BspEntity
  {
    std::vector<std::pair<std::string, std::string>> pairs;

    /// The value of a key, or nothing when the entity does not have it. Of a
    /// key written twice it is the first.
    [[nodiscard]] const std::string *Find(const std::string_view key) const
    {
      for (const auto &[name, value] : pairs)
      {
        if (name == key) { return &value; }
      }
      return nullptr;
    }
  };
} // quake

#endif //QUAKE_BSP_ENTITY_HPP
