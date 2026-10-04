#ifndef QUAKE_LEVEL_PROGRAM_TEST_HPP
#define QUAKE_LEVEL_PROGRAM_TEST_HPP

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

#include <gtest/gtest.h>

#include "formats/progs-builder.test.hpp"
#include "formats/progs.hpp"
#include "formats/qc-machine.hpp"

namespace quake
{
  /// A small program for the tests of this folder: the globals and fields
  /// that starting and running a level work with, and functions that are
  /// builtins, so that a test says in C++ what the game code does.
  class LevelProgram
  {
    std::int32_t _builtins = 0;

  public:
    ProgsBuilder builder;

    LevelProgram()
    {
      for (const std::string_view name : {"self", "other", "world"})
      {
        builder.Name(builder.Integer(), name, ProgsType::Entity);
      }
      for (const std::string_view name : {
             "time", "frametime", "serverflags", "deathmatch", "coop", "force_retouch", "parm1",
           })
      {
        builder.Name(builder.Float(), name, ProgsType::Float);
      }
      builder.Name(builder.Integer(), "mapname", ProgsType::String);
      builder.Name(builder.Vector(), "v_forward", ProgsType::Vector);

      for (const std::string_view name : {"classname", "model", "netname", "target"})
      {
        builder.Field(name, ProgsType::String);
      }
      for (const std::string_view name : {"origin", "angles", "velocity", "avelocity"})
      {
        builder.Field(name, ProgsType::Vector);
      }
      for (const std::string_view name : {
             "modelindex", "movetype", "solid", "ltime", "nextthink", "spawnflags", "health", "light_lev",
           })
      {
        builder.Field(name, ProgsType::Float);
      }
      builder.Field("think", ProgsType::Function);
      builder.Field("owner", ProgsType::Entity);
      builder.Field("light", ProgsType::Function);
    }

    /// Adds a function that is a builtin, and gives the number of the
    /// builtin, to register what it does with.
    std::int32_t Function(const std::string_view name)
    {
      builder.Builtin(name, ++_builtins);
      return _builtins;
    }

    /// The same for a function the engine finds by a global of its name,
    /// such as `StartFrame`.
    std::int32_t EngineFunction(const std::string_view name)
    {
      const std::int32_t function = builder.NextFunction();
      const std::int32_t builtin = Function(name);
      builder.Name(builder.Integer(function), name, ProgsType::Function);
      return builtin;
    }

    /// A machine with the program, read as a file is.
    [[nodiscard]] QcMachine Make() const
    {
      Progs progs;
      std::string error;
      EXPECT_TRUE(progs.Read(builder.Build(), error)) << error;
      return QcMachine(std::move(progs));
    }
  };
} // quake

#endif //QUAKE_LEVEL_PROGRAM_TEST_HPP
