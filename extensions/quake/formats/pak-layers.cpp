#include "pak-layers.hpp"

#include <algorithm>
#include <iterator>
#include <ranges>
#include <utility>

namespace quake
{
  void PakLayers::Add(PakFile pak)
  {
    _paks.push_back(std::move(pak));
  }

  std::size_t PakLayers::GetCount() const
  {
    return _paks.size();
  }

  const PakFile *PakLayers::FindPak(const std::string_view name) const
  {
    // from the last added to the first, so that a later pak wins
    for (const PakFile &pak : std::views::reverse(_paks))
    {
      if (pak.Has(name)) { return &pak; }
    }
    return nullptr;
  }

  bool PakLayers::Has(const std::string_view name) const
  {
    return FindPak(name) != nullptr;
  }

  std::span<const std::uint8_t> PakLayers::GetBytes(const std::string_view name) const
  {
    const PakFile *pak = FindPak(name);
    if (pak == nullptr) { return {}; }
    return pak->GetBytes(name);
  }

  std::vector<std::string> PakLayers::ListNames(const std::string_view folder) const
  {
    std::vector<std::string> names;
    for (const PakFile &pak : _paks)
    {
      std::vector<std::string> of_pak = pak.ListNames(folder);
      std::ranges::move(of_pak, std::back_inserter(names));
    }

    // a name that several paks have is listed once
    std::ranges::sort(names);
    const auto repeated = std::ranges::unique(names);
    names.erase(repeated.begin(), repeated.end());
    return names;
  }
} // quake
