#ifndef QUAKE_PROGS_BUILDER_TEST_HPP
#define QUAKE_PROGS_BUILDER_TEST_HPP

#include <bit>
#include <cstdint>
#include <initializer_list>
#include <string>
#include <string_view>
#include <vector>

#include "progs-definition.hpp"
#include "progs-function.hpp"
#include "progs-opcode.hpp"
#include "progs-statement.hpp"
#include "progs.hpp"

namespace quake
{
  /// Puts a `progs.dat` together in memory, for the tests: there is no
  /// compiler of QuakeC to make one with, so a test writes the statements,
  /// globals, and functions of its program by hand and gets the bytes of the
  /// file from Build().
  ///
  /// It starts as the compiler starts a program: the empty string at offset
  /// 0, the 28 reserved globals, a function 0 that is nothing, and a
  /// statement 0 that ends a run. Everything is public, so that a test can
  /// also make a program that is wrong.
  class ProgsBuilder
  {
  public:
    std::int32_t version = Progs::version;
    std::int32_t crc = 0;
    std::int32_t entity_fields = 0;
    std::vector<ProgsStatement> statements;
    std::vector<ProgsDefinition> global_definitions;
    std::vector<ProgsDefinition> field_definitions;
    std::vector<ProgsFunction> functions;
    std::string strings;
    std::vector<std::uint32_t> globals;

    ProgsBuilder()
    {
      strings.push_back('\0');
      globals.resize(static_cast<std::size_t>(Progs::reserved_globals), 0);
      functions.emplace_back();
      Emit(ProgsOpcode::Done);
    }

    /// Adds a string and gives the offset it starts at.
    std::int32_t String(const std::string_view text)
    {
      const auto offset = static_cast<std::int32_t>(strings.size());
      strings.append(text);
      strings.push_back('\0');
      return offset;
    }

    /// Adds a global of one cell that holds an integer: the number of an
    /// entity or of a function, the offset of a field or of a string.
    std::uint16_t Integer(const std::int32_t value = 0)
    {
      globals.push_back(static_cast<std::uint32_t>(value));
      return static_cast<std::uint16_t>(globals.size() - 1);
    }

    std::uint16_t Float(const float value = 0.0f)
    {
      globals.push_back(std::bit_cast<std::uint32_t>(value));
      return static_cast<std::uint16_t>(globals.size() - 1);
    }

    /// Adds a global of three cells and gives the offset of the first.
    std::uint16_t Vector(const float x = 0.0f, const float y = 0.0f, const float z = 0.0f)
    {
      const std::uint16_t offset = Float(x);
      Float(y);
      Float(z);
      return offset;
    }

    /// Adds a global that holds a string, which is added too.
    std::uint16_t StringGlobal(const std::string_view text)
    {
      return Integer(String(text));
    }

    /// Gives a global a name.
    void Name(const std::uint16_t offset, const std::string_view name, const ProgsType type, const bool saved = false)
    {
      const auto bits = static_cast<std::uint16_t>(
        static_cast<std::uint16_t>(type) | (saved ? ProgsDefinition::save_global : 0));
      global_definitions.push_back({.type = bits, .offset = offset, .name = String(name)});
    }

    /// Adds a field to every entity, of three cells for a vector and one
    /// otherwise, and gives its offset in an entity.
    std::uint16_t Field(const std::string_view name, const ProgsType type)
    {
      const auto offset = static_cast<std::uint16_t>(entity_fields);
      field_definitions.push_back({.type = static_cast<std::uint16_t>(type), .offset = offset, .name = String(name)});
      entity_fields += type == ProgsType::Vector ? 3 : 1;
      return offset;
    }

    /// The number the next statement gets.
    [[nodiscard]] std::int32_t Here() const
    {
      return static_cast<std::int32_t>(statements.size());
    }

    /// Adds a statement and gives its number.
    std::int32_t Emit(
      const ProgsOpcode opcode,
      const std::uint16_t a = 0,
      const std::uint16_t b = 0,
      const std::uint16_t c = 0)
    {
      statements.push_back({.opcode = static_cast<std::uint16_t>(opcode), .a = a, .b = b, .c = c});
      return Here() - 1;
    }

    /// An operand that jumps from the next statement added to statement
    /// `target`.
    [[nodiscard]] std::uint16_t JumpTo(const std::int32_t target) const
    {
      return static_cast<std::uint16_t>(target - Here());
    }

    /// The number the next function gets.
    [[nodiscard]] std::int32_t NextFunction() const
    {
      return static_cast<std::int32_t>(functions.size());
    }

    /// Adds a function that starts at a statement, with its locals, of which
    /// the first are its parameters of the given sizes. Gives its number.
    std::int32_t Function(
      const std::string_view name,
      const std::int32_t first_statement,
      const std::int32_t first_local = 0,
      const std::int32_t locals_count = 0,
      const std::initializer_list<std::uint8_t> parameter_sizes = {})
    {
      ProgsFunction function;
      function.first_statement = first_statement;
      function.first_local = first_local;
      function.locals_count = locals_count;
      function.name = String(name);
      function.file = String("test.qc");
      function.parameters_count = static_cast<std::int32_t>(parameter_sizes.size());
      std::size_t i = 0;
      for (const std::uint8_t size : parameter_sizes) { function.parameter_sizes[i++] = size; }
      functions.push_back(function);
      return NextFunction() - 1;
    }

    /// Adds a function that is builtin `number` of the engine.
    std::int32_t Builtin(const std::string_view name, const std::int32_t number)
    {
      return Function(name, -number);
    }

    /// The bytes of the file: the header, then the statements, the global
    /// and field definitions, the functions, the strings, and the globals.
    [[nodiscard]] std::vector<std::uint8_t> Build() const
    {
      std::vector<std::uint8_t> bytes;
      const auto u16 = [&bytes](const std::uint16_t value)
      {
        bytes.push_back(static_cast<std::uint8_t>(value & 0xff));
        bytes.push_back(static_cast<std::uint8_t>(value >> 8));
      };
      const auto u32 = [&bytes](const std::uint32_t value)
      {
        for (int shift = 0; shift < 32; shift += 8)
        {
          bytes.push_back(static_cast<std::uint8_t>(value >> shift & 0xff));
        }
      };
      const auto i32 = [&u32](const std::int32_t value) { u32(static_cast<std::uint32_t>(value)); };
      const auto count = [](const auto &table) { return static_cast<std::int32_t>(table.size()); };

      const std::int32_t statements_offset = static_cast<std::int32_t>(Progs::header_size);
      const std::int32_t global_definitions_offset = statements_offset + count(statements) * 8;
      const std::int32_t field_definitions_offset = global_definitions_offset + count(global_definitions) * 8;
      const std::int32_t functions_offset = field_definitions_offset + count(field_definitions) * 8;
      const std::int32_t strings_offset = functions_offset + count(functions) * 36;
      const std::int32_t globals_offset = strings_offset + count(strings);

      i32(version);
      i32(crc);
      i32(statements_offset);
      i32(count(statements));
      i32(global_definitions_offset);
      i32(count(global_definitions));
      i32(field_definitions_offset);
      i32(count(field_definitions));
      i32(functions_offset);
      i32(count(functions));
      i32(strings_offset);
      i32(count(strings));
      i32(globals_offset);
      i32(count(globals));
      i32(entity_fields);

      for (const ProgsStatement &statement : statements)
      {
        u16(statement.opcode);
        u16(statement.a);
        u16(statement.b);
        u16(statement.c);
      }
      for (const auto *definitions : {&global_definitions, &field_definitions})
      {
        for (const ProgsDefinition &definition : *definitions)
        {
          u16(definition.type);
          u16(definition.offset);
          i32(definition.name);
        }
      }
      for (const ProgsFunction &function : functions)
      {
        i32(function.first_statement);
        i32(function.first_local);
        i32(function.locals_count);
        i32(function.profile);
        i32(function.name);
        i32(function.file);
        i32(function.parameters_count);
        bytes.insert(bytes.end(), function.parameter_sizes.begin(), function.parameter_sizes.end());
      }
      bytes.insert(bytes.end(), strings.begin(), strings.end());
      for (const std::uint32_t global : globals) { u32(global); }
      return bytes;
    }
  };
} // quake

#endif //QUAKE_PROGS_BUILDER_TEST_HPP
