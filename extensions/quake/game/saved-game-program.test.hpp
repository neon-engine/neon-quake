#ifndef QUAKE_SAVED_GAME_PROGRAM_TEST_HPP
#define QUAKE_SAVED_GAME_PROGRAM_TEST_HPP

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
  /// A small program for the tests of saved games: globals of every kind,
  /// some marked to be saved and some not, and fields of every kind, laid
  /// out as a compiler lays them out, with a first field of no type and
  /// the parts of a vector named a second time.
  class SavedGameProgram
  {
  public:
    ProgsBuilder builder;

    /// The number of the function `thing_think`.
    std::int32_t think = 0;

    SavedGameProgram()
    {
      builder.Name(builder.Integer(), "self", ProgsType::Entity, true);
      builder.Name(builder.Float(), "time", ProgsType::Float, true);
      builder.Name(builder.Integer(), "mapname", ProgsType::String, true);
      builder.Name(builder.Float(), "killed_monsters", ProgsType::Float, true);
      // marked, and of a kind that is not saved
      builder.Name(builder.Vector(), "v_forward", ProgsType::Vector, true);
      // not marked: a constant of the program
      builder.Name(builder.Float(7.0f), "seven", ProgsType::Float);

      // what a compiler puts first
      builder.field_definitions.push_back({.type = 0, .offset = 0, .name = 0});
      builder.entity_fields = 1;

      builder.Field("classname", ProgsType::String);
      const std::uint16_t origin = builder.Field("origin", ProgsType::Vector);
      std::uint16_t part = origin;
      for (const std::string_view name : {"origin_x", "origin_y", "origin_z"})
      {
        builder.field_definitions.push_back({
          .type = static_cast<std::uint16_t>(ProgsType::Float), .offset = part++, .name = builder.String(name),
        });
      }
      builder.Field("health", ProgsType::Float);
      builder.Field("owner", ProgsType::Entity);
      builder.Field("think", ProgsType::Function);
      builder.Field("message", ProgsType::String);
      builder.Field("which", ProgsType::Field);

      think = builder.Builtin("thing_think", 1);
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

#endif //QUAKE_SAVED_GAME_PROGRAM_TEST_HPP
