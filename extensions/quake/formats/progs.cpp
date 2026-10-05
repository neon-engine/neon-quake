#include "progs.hpp"

#include <format>
#include <utility>

#include "byte-reader.hpp"

namespace quake
{
  // Helpers of Progs: the sizes of the entries of the file, and the checks
  // that Read() makes of each table.
  namespace
  {
    constexpr std::size_t statement_size = 8;
    constexpr std::size_t definition_size = 8;
    constexpr std::size_t function_size = 36;
    constexpr std::size_t global_size = 4;

    /// Whether a table of `count` entries of `entry_size` bytes at `offset`
    /// lies inside a file of `file_size` bytes, saying what is wrong when
    /// not. The numbers are as the header has them, so they may be negative.
    bool check_table(
      const std::string_view what,
      const std::int32_t offset,
      const std::int32_t count,
      const std::size_t entry_size,
      const std::size_t file_size,
      std::string &error)
    {
      if (offset < 0 || count < 0)
      {
        error = std::format("The table of {} has a negative offset or count: {} and {}", what, offset, count);
        return false;
      }

      // in 64 bits, so that no product or sum of what a file says wraps
      const std::uint64_t end =
        static_cast<std::uint64_t>(offset) + static_cast<std::uint64_t>(count) * entry_size;
      if (end > file_size)
      {
        error = std::format(
          "The table of {} ends at byte {}, outside the file of {} bytes", what, end, file_size);
        return false;
      }
      return true;
    }

    std::vector<ProgsDefinition> read_definitions(
      ByteReader &reader,
      const std::int32_t offset,
      const std::int32_t count)
    {
      std::vector<ProgsDefinition> definitions(static_cast<std::size_t>(count));
      reader.Seek(static_cast<std::size_t>(offset));
      for (ProgsDefinition &definition : definitions)
      {
        definition.type = reader.ReadU16();
        definition.offset = reader.ReadU16();
        definition.name = reader.ReadI32();
      }
      return definitions;
    }

    const ProgsDefinition *find_definition(
      const Progs &progs,
      const std::vector<ProgsDefinition> &definitions,
      const std::string_view name)
    {
      for (const ProgsDefinition &definition : definitions)
      {
        if (progs.GetString(definition.name) == name) { return &definition; }
      }
      return nullptr;
    }
  }

  bool Progs::Read(const std::span<const std::uint8_t> bytes, std::string &error)
  {
    if (bytes.size() < header_size)
    {
      error = std::format("The file has {} bytes, fewer than the {} of a header", bytes.size(), header_size);
      return false;
    }

    ByteReader reader(bytes);
    Progs read;
    ProgsHeader &h = read.header;
    h.version = reader.ReadI32();
    h.crc = reader.ReadI32();
    h.statements_offset = reader.ReadI32();
    h.statements_count = reader.ReadI32();
    h.global_definitions_offset = reader.ReadI32();
    h.global_definitions_count = reader.ReadI32();
    h.field_definitions_offset = reader.ReadI32();
    h.field_definitions_count = reader.ReadI32();
    h.functions_offset = reader.ReadI32();
    h.functions_count = reader.ReadI32();
    h.strings_offset = reader.ReadI32();
    h.strings_size = reader.ReadI32();
    h.globals_offset = reader.ReadI32();
    h.globals_count = reader.ReadI32();
    h.entity_fields = reader.ReadI32();

    if (h.version != version)
    {
      error = std::format("The file is of version {}, and only version {} is known", h.version, version);
      return false;
    }

    const std::size_t size = bytes.size();
    if (!check_table("statements", h.statements_offset, h.statements_count, statement_size, size, error) ||
        !check_table(
          "global definitions",
          h.global_definitions_offset,
          h.global_definitions_count,
          definition_size,
          size,
          error) ||
        !check_table(
          "field definitions", h.field_definitions_offset, h.field_definitions_count, definition_size, size, error) ||
        !check_table("functions", h.functions_offset, h.functions_count, function_size, size, error) ||
        !check_table("strings", h.strings_offset, h.strings_size, 1, size, error) ||
        !check_table("globals", h.globals_offset, h.globals_count, global_size, size, error))
    {
      return false;
    }

    if (h.entity_fields < 0)
    {
      error = std::format("The file says an entity has {} fields", h.entity_fields);
      return false;
    }
    if (h.globals_count < reserved_globals)
    {
      error = std::format(
        "The file has {} globals, fewer than the {} every program starts with", h.globals_count, reserved_globals);
      return false;
    }

    const auto strings = reader.BytesAt(
      static_cast<std::size_t>(h.strings_offset),
      static_cast<std::size_t>(h.strings_size));
    read.strings.assign(strings.begin(), strings.end());
    if (!read.strings.empty() && read.strings.back() != '\0')
    {
      // with it, a string that starts inside the table also ends inside it
      error = "The strings do not end with a zero";
      return false;
    }

    read.statements.resize(static_cast<std::size_t>(h.statements_count));
    reader.Seek(static_cast<std::size_t>(h.statements_offset));
    for (ProgsStatement &statement : read.statements)
    {
      statement.opcode = reader.ReadU16();
      statement.a = reader.ReadU16();
      statement.b = reader.ReadU16();
      statement.c = reader.ReadU16();
    }

    read.global_definitions = read_definitions(reader, h.global_definitions_offset, h.global_definitions_count);
    read.field_definitions = read_definitions(reader, h.field_definitions_offset, h.field_definitions_count);

    read.functions.resize(static_cast<std::size_t>(h.functions_count));
    reader.Seek(static_cast<std::size_t>(h.functions_offset));
    for (ProgsFunction &function : read.functions)
    {
      function.first_statement = reader.ReadI32();
      function.first_local = reader.ReadI32();
      function.locals_count = reader.ReadI32();
      function.profile = reader.ReadI32();
      function.name = reader.ReadI32();
      function.file = reader.ReadI32();
      function.parameters_count = reader.ReadI32();
      for (std::uint8_t &parameter_size : function.parameter_sizes) { parameter_size = reader.ReadU8(); }
    }

    read.globals.resize(static_cast<std::size_t>(h.globals_count));
    reader.Seek(static_cast<std::size_t>(h.globals_offset));
    for (std::uint32_t &global : read.globals) { global = reader.ReadU32(); }

    if (!reader.IsGood())
    {
      error = "The file ends before its tables do";
      return false;
    }

    for (std::size_t i = 0; i < read.global_definitions.size(); i++)
    {
      const ProgsDefinition &definition = read.global_definitions[i];
      if (!read.HasString(definition.name))
      {
        error = std::format("The name of global definition {} is at {}, outside the strings", i, definition.name);
        return false;
      }
      if (definition.offset >= h.globals_count)
      {
        error = std::format(
          "Global definition {} ({}) is at offset {}, outside the {} globals",
          i, read.GetString(definition.name), definition.offset, h.globals_count);
        return false;
      }
    }

    for (std::size_t i = 0; i < read.field_definitions.size(); i++)
    {
      const ProgsDefinition &definition = read.field_definitions[i];
      if (!read.HasString(definition.name))
      {
        error = std::format("The name of field definition {} is at {}, outside the strings", i, definition.name);
        return false;
      }
      if (definition.offset >= h.entity_fields)
      {
        error = std::format(
          "Field definition {} ({}) is at offset {}, outside the {} fields of an entity",
          i, read.GetString(definition.name), definition.offset, h.entity_fields);
        return false;
      }
    }

    for (std::size_t i = 0; i < read.functions.size(); i++)
    {
      const ProgsFunction &function = read.functions[i];
      if (!read.HasString(function.name) || !read.HasString(function.file))
      {
        error = std::format(
          "The name or the file of function {} is at {} and {}, outside the strings", i, function.name, function.file);
        return false;
      }

      const std::string_view name = read.GetString(function.name);
      if (function.first_statement >= h.statements_count)
      {
        error = std::format(
          "Function {} ({}) starts at statement {}, outside the {} statements",
          i, name, function.first_statement, h.statements_count);
        return false;
      }
      if (function.first_local < 0 || function.locals_count < 0 ||
          static_cast<std::int64_t>(function.first_local) + function.locals_count > h.globals_count)
      {
        error = std::format(
          "Function {} ({}) has {} locals from offset {}, outside the {} globals",
          i, name, function.locals_count, function.first_local, h.globals_count);
        return false;
      }
      if (function.parameters_count > ProgsFunction::max_parameters)
      {
        error = std::format(
          "Function {} ({}) takes {} parameters, more than {}",
          i, name, function.parameters_count, ProgsFunction::max_parameters);
        return false;
      }

      // a builtin has no locals for its parameters, it reads them where the
      // caller put them
      if (function.IsBuiltin()) { continue; }

      std::int32_t cells = 0;
      for (std::int32_t parameter = 0; parameter < function.parameters_count; parameter++)
      {
        cells += function.parameter_sizes[static_cast<std::size_t>(parameter)];
      }
      // The parameters are not held to the locals: id's qcc counted none
      // for a function declared ahead of its body (see ProgsFunction). They
      // only have to be in the globals.
      if (static_cast<std::int64_t>(function.first_local) + cells > h.globals_count)
      {
        error = std::format(
          "Function {} ({}) has parameters of {} cells from offset {}, outside the {} globals",
          i, name, cells, function.first_local, h.globals_count);
        return false;
      }
    }

    *this = std::move(read);
    return true;
  }

  bool Progs::HasString(const std::int32_t offset) const
  {
    return offset >= 0 && static_cast<std::size_t>(offset) < strings.size();
  }

  std::string_view Progs::GetString(const std::int32_t offset) const
  {
    if (!HasString(offset)) { return {}; }

    // stops at the first zero, or at the end of the strings when a program
    // that was not read from a file has none
    const auto start = static_cast<std::size_t>(offset);
    const std::size_t end = strings.find('\0', start);
    return std::string_view(strings).substr(start, end == std::string::npos ? std::string::npos : end - start);
  }

  std::optional<std::int32_t> Progs::FindFunction(const std::string_view name) const
  {
    for (std::size_t i = 0; i < functions.size(); i++)
    {
      if (GetString(functions[i].name) == name) { return static_cast<std::int32_t>(i); }
    }
    return std::nullopt;
  }

  const ProgsDefinition *Progs::FindGlobal(const std::string_view name) const
  {
    return find_definition(*this, global_definitions, name);
  }

  const ProgsDefinition *Progs::FindField(const std::string_view name) const
  {
    return find_definition(*this, field_definitions, name);
  }
} // quake
