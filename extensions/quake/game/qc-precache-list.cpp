#include "qc-precache-list.hpp"

#include <algorithm>
#include <cstddef>

namespace quake
{
  QcPrecacheList::QcPrecacheList()
  {
    // number 0, which is no file
    _names.emplace_back();
  }

  std::int32_t QcPrecacheList::Add(const std::string_view name)
  {
    if (const std::optional<std::int32_t> number = Find(name)) { return *number; }

    _names.emplace_back(name);
    return GetCount() - 1;
  }

  std::optional<std::int32_t> QcPrecacheList::Find(const std::string_view name) const
  {
    // A level names a few hundred files, each once or a few times, so the
    // list is searched as it is.
    const auto found = std::ranges::find(_names, name);
    if (found == _names.end()) { return std::nullopt; }

    return static_cast<std::int32_t>(found - _names.begin());
  }

  std::string_view QcPrecacheList::GetName(const std::int32_t number) const
  {
    if (number < 0 || number >= GetCount()) { return {}; }

    return _names[static_cast<std::size_t>(number)];
  }

  std::int32_t QcPrecacheList::GetCount() const
  {
    return static_cast<std::int32_t>(_names.size());
  }

  const std::vector<std::string> &QcPrecacheList::GetNames() const
  {
    return _names;
  }
} // quake
