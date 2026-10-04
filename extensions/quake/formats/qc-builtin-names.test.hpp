#ifndef QUAKE_QC_BUILTIN_NAMES_TEST_HPP
#define QUAKE_QC_BUILTIN_NAMES_TEST_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace quake
{
  /// The name the game code of the original knows a builtin of the engine
  /// by, for its number. Empty for a number the original has no builtin of.
  ///
  /// A compiler that makes the file small leaves the names of the builtins
  /// out of it, so a real `progs.dat` only has their numbers, and the tests
  /// that list or stand in for the builtins take the names from here.
  [[nodiscard]] constexpr std::string_view QcBuiltinName(const std::int32_t number)
  {
    // the name of builtin 0, which there is not, first
    constexpr std::array<std::string_view, 79> names = {
      "", "makevectors", "setorigin", "setmodel", "setsize", "", "break", "random", "sound", "normalize",
      "error", "objerror", "vlen", "vectoyaw", "spawn", "remove", "traceline", "checkclient", "find",
      "precache_sound", "precache_model", "stuffcmd", "findradius", "bprint", "sprint", "dprint", "ftos",
      "vtos", "coredump", "traceon", "traceoff", "eprint", "walkmove", "", "droptofloor", "lightstyle",
      "rint", "floor", "ceil", "", "checkbottom", "pointcontents", "", "fabs", "aim", "cvar", "localcmd",
      "nextent", "particle", "ChangeYaw", "", "vectoangles", "WriteByte", "WriteChar", "WriteShort",
      "WriteLong", "WriteCoord", "WriteAngle", "WriteString", "WriteEntity", "", "", "", "", "", "", "",
      "movetogoal", "precache_file", "makestatic", "changelevel", "", "cvar_set", "centerprint",
      "ambientsound", "precache_model2", "precache_sound2", "precache_file2", "setspawnparms",
    };
    if (number < 0 || static_cast<std::size_t>(number) >= names.size()) { return {}; }

    return names[static_cast<std::size_t>(number)];
  }
} // quake

#endif //QUAKE_QC_BUILTIN_NAMES_TEST_HPP
