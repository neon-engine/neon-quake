#ifndef QUAKE_QC_BUILTIN_NUMBER_HPP
#define QUAKE_QC_BUILTIN_NUMBER_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace quake
{
  /// The number the game code of the original calls each builtin of the
  /// engine by. The numbers are those of the original, with the gaps it has,
  /// since a `progs.dat` names a builtin by its number alone.
  enum class QcBuiltinNumber : std::int32_t
  {
    MakeVectors = 1,
    SetOrigin = 2,
    SetModel = 3,
    SetSize = 4,
    Break = 6,
    Random = 7,
    Sound = 8,
    Normalize = 9,
    Error = 10,
    ObjError = 11,
    VLen = 12,
    VecToYaw = 13,
    Spawn = 14,
    Remove = 15,
    TraceLine = 16,
    CheckClient = 17,
    Find = 18,
    PrecacheSound = 19,
    PrecacheModel = 20,
    StuffCmd = 21,
    FindRadius = 22,
    BPrint = 23,
    SPrint = 24,
    DPrint = 25,
    FToS = 26,
    VToS = 27,
    CoreDump = 28,
    TraceOn = 29,
    TraceOff = 30,
    EPrint = 31,
    WalkMove = 32,
    DropToFloor = 34,
    LightStyle = 35,
    RInt = 36,
    Floor = 37,
    Ceil = 38,
    CheckBottom = 40,
    PointContents = 41,
    FAbs = 43,
    Aim = 44,
    CVar = 45,
    LocalCmd = 46,
    NextEnt = 47,
    Particle = 48,
    ChangeYaw = 49,
    VecToAngles = 51,
    WriteByte = 52,
    WriteChar = 53,
    WriteShort = 54,
    WriteLong = 55,
    WriteCoord = 56,
    WriteAngle = 57,
    WriteString = 58,
    WriteEntity = 59,
    MoveToGoal = 67,
    PrecacheFile = 68,
    MakeStatic = 69,
    ChangeLevel = 70,
    CVarSet = 72,
    CenterPrint = 73,
    AmbientSound = 74,
    PrecacheModel2 = 75,
    PrecacheSound2 = 76,
    PrecacheFile2 = 77,
    SetSpawnParms = 78,
  };

  /// The name the game code of the original knows a builtin of the engine
  /// by, for its number. Empty for a number the original has no builtin of.
  ///
  /// A compiler that makes the file small leaves the names of the builtins
  /// out of it, so a real `progs.dat` only has their numbers, and whoever
  /// lists the builtins or says which one is missing takes the names from
  /// here.
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

  /// The same for a builtin named by the enum.
  [[nodiscard]] constexpr std::string_view QcBuiltinName(const QcBuiltinNumber number)
  {
    return QcBuiltinName(static_cast<std::int32_t>(number));
  }
} // quake

#endif //QUAKE_QC_BUILTIN_NUMBER_HPP
