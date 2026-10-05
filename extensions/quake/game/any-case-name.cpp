#include "any-case-name.hpp"

#include <algorithm>

namespace quake
{
  // what compares two names without the case of their letters
  namespace
  {
    char Lower(const char letter)
    {
      return letter >= 'A' && letter <= 'Z' ? static_cast<char>(letter - 'A' + 'a') : letter;
    }

    bool SameInAnyCase(const std::string_view a, const std::string_view b)
    {
      return std::ranges::equal(a, b, [](const char x, const char y) { return Lower(x) == Lower(y); });
    }
  }

  std::optional<std::string> FindAnyCaseName(const std::vector<std::string> &names, const std::string_view wanted)
  {
    if (std::ranges::find(names, wanted) != names.end()) { return std::string(wanted); }

    const auto found = std::ranges::find_if(
      names, [wanted](const std::string &name) { return SameInAnyCase(name, wanted); });
    if (found == names.end()) { return std::nullopt; }
    return *found;
  }
} // quake
