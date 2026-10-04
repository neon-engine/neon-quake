#include "qc-console-variables.hpp"

#include <cstdlib>
#include <format>
#include <utility>

namespace quake
{
  QcConsoleVariables::QcConsoleVariables()
  {
    // what the game code of the original asks for, and what the original has
    // them at when nobody chose otherwise
    constexpr std::pair<std::string_view, std::string_view> defaults[] = {
      // the game that is played
      {"skill", "1"},
      {"deathmatch", "0"},
      {"coop", "0"},
      {"teamplay", "0"},
      {"fraglimit", "0"},
      {"timelimit", "0"},
      {"samelevel", "0"},
      {"noexit", "0"},
      // whether the whole game is there, and not the first episode alone
      {"registered", "0"},
      // what the game code keeps for itself, some of it over a change of level
      {"temp1", "0"},
      {"gamecfg", "0"},
      {"scratch1", "0"},
      {"scratch2", "0"},
      {"scratch3", "0"},
      {"scratch4", "0"},
      {"saved1", "0"},
      {"saved2", "0"},
      {"saved3", "0"},
      {"saved4", "0"},
      {"savedgamecfg", "0"},
      // how things move
      {"sv_gravity", "800"},
      {"sv_maxspeed", "320"},
      {"sv_maxvelocity", "2000"},
      {"sv_friction", "4"},
      {"sv_stopspeed", "100"},
      {"sv_accelerate", "10"},
      {"sv_aim", "0.93"},
      {"sv_nostep", "0"},
    };
    for (const auto &[name, text] : defaults) { Set(name, text); }
  }

  bool QcConsoleVariables::Has(const std::string_view name) const
  {
    return _texts.contains(name);
  }

  std::string_view QcConsoleVariables::GetText(const std::string_view name) const
  {
    const auto found = _texts.find(name);
    return found == _texts.end() ? std::string_view() : std::string_view(found->second);
  }

  float QcConsoleVariables::GetFloat(const std::string_view name) const
  {
    const auto found = _texts.find(name);
    return found == _texts.end() ? 0.0f : std::strtof(found->second.c_str(), nullptr);
  }

  void QcConsoleVariables::Set(const std::string_view name, const std::string_view text)
  {
    const auto found = _texts.find(name);
    if (found == _texts.end())
    {
      _texts.emplace(name, text);
      return;
    }
    found->second = text;
  }

  void QcConsoleVariables::SetFloat(const std::string_view name, const float value)
  {
    Set(name, std::format("{}", value));
  }

  std::size_t QcConsoleVariables::GetCount() const
  {
    return _texts.size();
  }
} // quake
