#include "qc-machine.hpp"

#include <algorithm>
#include <cmath>
#include <format>
#include <limits>
#include <utility>

#include "progs-opcode.hpp"

namespace quake
{
  // Helpers of QcMachine: what each opcode reads and writes among the
  // globals, the arithmetic that has to be careful, and where a parameter
  // is.
  namespace
  {
    // dividing by zero gives an infinity or not-a-number only with these
    // floats, see DivF
    static_assert(std::numeric_limits<float>::is_iec559);

    /// How many cells each of the three operands of a statement names
    /// among the globals: 0 when the operand is not a global, 3 for a
    /// vector, 1 for anything else.
    struct OperandSizes
    {
      std::uint8_t a = 0;
      std::uint8_t b = 0;
      std::uint8_t c = 0;
    };

    /// The sizes of the operands of an opcode. False when there is no such
    /// opcode.
    bool operand_sizes(const std::uint16_t opcode, OperandSizes &sizes)
    {
      switch (static_cast<ProgsOpcode>(opcode))
      {
        case ProgsOpcode::Done:
        case ProgsOpcode::Return:
        {
          // a value of any type is returned as three cells
          sizes = {3, 0, 0};
          return true;
        }
        case ProgsOpcode::MulF:
        case ProgsOpcode::DivF:
        case ProgsOpcode::AddF:
        case ProgsOpcode::SubF:
        case ProgsOpcode::EqF:
        case ProgsOpcode::EqS:
        case ProgsOpcode::EqE:
        case ProgsOpcode::EqFnc:
        case ProgsOpcode::NeF:
        case ProgsOpcode::NeS:
        case ProgsOpcode::NeE:
        case ProgsOpcode::NeFnc:
        case ProgsOpcode::Le:
        case ProgsOpcode::Ge:
        case ProgsOpcode::Lt:
        case ProgsOpcode::Gt:
        case ProgsOpcode::And:
        case ProgsOpcode::Or:
        case ProgsOpcode::BitAnd:
        case ProgsOpcode::BitOr:
        {
          sizes = {1, 1, 1};
          return true;
        }
        case ProgsOpcode::MulV:
        case ProgsOpcode::EqV:
        case ProgsOpcode::NeV:
        {
          sizes = {3, 3, 1};
          return true;
        }
        case ProgsOpcode::MulFV:
        {
          sizes = {1, 3, 3};
          return true;
        }
        case ProgsOpcode::MulVF:
        {
          sizes = {3, 1, 3};
          return true;
        }
        case ProgsOpcode::AddV:
        case ProgsOpcode::SubV:
        {
          sizes = {3, 3, 3};
          return true;
        }
        case ProgsOpcode::StoreF:
        case ProgsOpcode::StoreS:
        case ProgsOpcode::StoreEnt:
        case ProgsOpcode::StoreFld:
        case ProgsOpcode::StoreFnc:
        {
          sizes = {1, 1, 0};
          return true;
        }
        case ProgsOpcode::StoreV:
        {
          sizes = {3, 3, 0};
          return true;
        }
        case ProgsOpcode::NotF:
        case ProgsOpcode::NotS:
        case ProgsOpcode::NotEnt:
        case ProgsOpcode::NotFnc:
        {
          sizes = {1, 0, 1};
          return true;
        }
        case ProgsOpcode::NotV:
        {
          sizes = {3, 0, 1};
          return true;
        }
        case ProgsOpcode::LoadF:
        case ProgsOpcode::LoadS:
        case ProgsOpcode::LoadEnt:
        case ProgsOpcode::LoadFld:
        case ProgsOpcode::LoadFnc:
        case ProgsOpcode::Address:
        {
          // an entity, a field, and where what is read goes
          sizes = {1, 1, 1};
          return true;
        }
        case ProgsOpcode::LoadV:
        {
          sizes = {1, 1, 3};
          return true;
        }
        case ProgsOpcode::StorepF:
        case ProgsOpcode::StorepS:
        case ProgsOpcode::StorepEnt:
        case ProgsOpcode::StorepFld:
        case ProgsOpcode::StorepFnc:
        case ProgsOpcode::State:
        {
          // a value and a pointer; for State, a frame and a function
          sizes = {1, 1, 0};
          return true;
        }
        case ProgsOpcode::StorepV:
        {
          sizes = {3, 1, 0};
          return true;
        }
        case ProgsOpcode::Call0:
        case ProgsOpcode::Call1:
        case ProgsOpcode::Call2:
        case ProgsOpcode::Call3:
        case ProgsOpcode::Call4:
        case ProgsOpcode::Call5:
        case ProgsOpcode::Call6:
        case ProgsOpcode::Call7:
        case ProgsOpcode::Call8:
        case ProgsOpcode::If:
        case ProgsOpcode::IfNot:
        {
          // a function to call, or a truth to test; the second operand of
          // a test is how far to jump
          sizes = {1, 0, 0};
          return true;
        }
        case ProgsOpcode::Goto:
        {
          sizes = {0, 0, 0};
          return true;
        }
        default:
        {
          return false;
        }
      }
    }

    /// A float as the whole number the bit operations work on. The original
    /// converts without looking, which is undefined for what an integer
    /// cannot hold; here not-a-number is zero and what is too large is the
    /// largest there is.
    std::int32_t to_integer(const float value)
    {
      if (std::isnan(value)) { return 0; }
      if (value >= 2147483648.0f) { return std::numeric_limits<std::int32_t>::max(); }
      if (value <= -2147483648.0f) { return std::numeric_limits<std::int32_t>::min(); }
      return static_cast<std::int32_t>(value);
    }

    QcCell of_bool(const bool value)
    {
      return QcCell::OfFloat(value ? 1.0f : 0.0f);
    }

    /// Where a parameter is among the globals, or -1, which no global is,
    /// when there is no such parameter.
    std::int32_t parameter_offset(const std::int32_t parameter)
    {
      if (parameter < 0 || parameter >= QcMachine::max_parameters) { return -1; }

      return QcMachine::first_parameter_offset + parameter * QcMachine::parameter_size;
    }
  }

  QcMachine::QcMachine(Progs progs, const QcLimits &limits)
    : _progs(std::move(progs)),
      _limits(limits)
  {
    _globals.reserve(_progs.globals.size());
    for (const std::uint32_t bits : _progs.globals) { _globals.push_back({bits}); }

    // a program that was not read from a file may lack them, and the
    // machine reads and writes them without asking
    if (_globals.size() < static_cast<std::size_t>(Progs::reserved_globals))
    {
      _globals.resize(static_cast<std::size_t>(Progs::reserved_globals));
    }
    // Two cells more than the program has, which only statements reach.
    // The compiler copies a value of any type as a vector where it does
    // not care, when it returns one and when it passes one to a function,
    // so a float that is the last global of a program is read as three
    // cells. The original reads past its globals then; here the two cells
    // are there, and zero.
    _global_count = _globals.size();
    _globals.resize(_global_count + 2);

    _progs.header.entity_fields = std::max(_progs.header.entity_fields, 0);

    // The original has what State works on at fixed places it was compiled
    // with. Here they are found by their names, and a program without them
    // cannot use State.
    const ProgsDefinition *self = _progs.FindGlobal("self");
    const ProgsDefinition *time = _progs.FindGlobal("time");
    const ProgsDefinition *next_think = _progs.FindField("nextthink");
    const ProgsDefinition *frame = _progs.FindField("frame");
    const ProgsDefinition *think = _progs.FindField("think");
    if (self != nullptr && time != nullptr && next_think != nullptr && frame != nullptr && think != nullptr)
    {
      _state_places = {
        .found = true,
        .self = self->offset,
        .time = time->offset,
        .next_think = next_think->offset,
        .frame = frame->offset,
        .think = think->offset,
      };
    }

    // the world
    _entity_cells.resize(static_cast<std::size_t>(_progs.header.entity_fields));
    _entity_free.push_back(0);
  }

  const Progs &QcMachine::GetProgs() const
  {
    return _progs;
  }

  bool QcMachine::HasGlobals(const std::int32_t offset, const std::int32_t count) const
  {
    return offset >= 0 && static_cast<std::size_t>(offset) + static_cast<std::size_t>(count) <= _global_count;
  }

  std::int32_t QcMachine::GetGlobalCount() const
  {
    return static_cast<std::int32_t>(_global_count);
  }

  float QcMachine::GetFloat(const std::int32_t offset) const
  {
    return HasGlobals(offset, 1) ? _globals[static_cast<std::size_t>(offset)].AsFloat() : 0.0f;
  }

  std::int32_t QcMachine::GetInteger(const std::int32_t offset) const
  {
    return HasGlobals(offset, 1) ? _globals[static_cast<std::size_t>(offset)].AsInteger() : 0;
  }

  std::array<float, 3> QcMachine::GetVector(const std::int32_t offset) const
  {
    if (!HasGlobals(offset, 3)) { return {}; }

    const auto at = static_cast<std::size_t>(offset);
    return {_globals[at].AsFloat(), _globals[at + 1].AsFloat(), _globals[at + 2].AsFloat()};
  }

  bool QcMachine::SetFloat(const std::int32_t offset, const float value)
  {
    if (!HasGlobals(offset, 1)) { return false; }

    _globals[static_cast<std::size_t>(offset)] = QcCell::OfFloat(value);
    return true;
  }

  bool QcMachine::SetInteger(const std::int32_t offset, const std::int32_t value)
  {
    if (!HasGlobals(offset, 1)) { return false; }

    _globals[static_cast<std::size_t>(offset)] = QcCell::OfInteger(value);
    return true;
  }

  bool QcMachine::SetVector(const std::int32_t offset, const std::array<float, 3> &value)
  {
    if (!HasGlobals(offset, 3)) { return false; }

    const auto at = static_cast<std::size_t>(offset);
    for (std::size_t i = 0; i < 3; i++) { _globals[at + i] = QcCell::OfFloat(value[i]); }
    return true;
  }

  bool QcMachine::Fail(std::string message)
  {
    // the first error is the one that says what went wrong
    if (_failed) { return false; }

    _failed = true;
    _error = {};
    _error.message = std::move(message);
    _error.statement = _statement;
    if (!_frames.empty())
    {
      _error.function = _frames.back().function;
      _error.function_name = _progs.GetString(_progs.functions[static_cast<std::size_t>(_error.function)].name);
    }

    // every call under way ends here. The locals keep what they hold, as in
    // the original: nothing goes on running that could read them
    _frames.clear();
    _saved_locals.clear();
    return false;
  }

  void QcMachine::Stop(std::string message)
  {
    Fail(std::move(message));
  }

  bool QcMachine::HasFailed() const
  {
    return _failed;
  }

  const QcError &QcMachine::GetError() const
  {
    return _error;
  }

  std::int64_t QcMachine::GetStatementsRun() const
  {
    return _statements_run;
  }

  bool QcMachine::Begin()
  {
    if (_runs > 0) { return !_failed; }

    _failed = false;
    _error = {};
    _statement = -1;
    _frames.clear();
    _saved_locals.clear();
    return true;
  }

  bool QcMachine::Call(const std::string_view name)
  {
    if (!Begin()) { return false; }

    const std::optional<std::int32_t> function = _progs.FindFunction(name);
    if (!function) { return Fail(std::format("There is no function named {}", name)); }
    return Call(*function);
  }

  bool QcMachine::Call(const std::int32_t function)
  {
    if (!Begin()) { return false; }

    if (function <= 0 || static_cast<std::size_t>(function) >= _progs.functions.size())
    {
      return Fail(std::format(
        "There is no function {}: the program has functions 1 to {}",
        function, static_cast<std::int64_t>(_progs.functions.size()) - 1));
    }

    // a builtin that calls back in still has its own count afterwards
    const ProgsFunction &called = _progs.functions[static_cast<std::size_t>(function)];
    const std::int32_t argument_count = _argument_count;
    _argument_count = std::clamp(called.parameters_count, 0, max_parameters);

    _runs++;
    const bool done = called.IsBuiltin() ? CallBuiltin(function) : Run(function);
    _runs--;
    _argument_count = argument_count;
    return done && !_failed;
  }

  bool QcMachine::Enter(const std::int32_t function, const std::int32_t return_statement)
  {
    const ProgsFunction &entered = _progs.functions[static_cast<std::size_t>(function)];
    const std::string_view name = _progs.GetString(entered.name);

    if (_frames.size() >= _limits.stack_depth)
    {
      return Fail(std::format(
        "The stack is too deep: calling {} would be more than {} calls inside one another",
        name, _limits.stack_depth));
    }

    // Read() checks these too, but a program may be made without it
    if (entered.first_local < 0 || entered.locals_count < 0 ||
        !HasGlobals(entered.first_local, entered.locals_count))
    {
      return Fail(std::format("The locals of function {} ({}) are outside the globals", function, name));
    }
    const std::int32_t parameters = std::clamp(entered.parameters_count, 0, max_parameters);
    std::int32_t cells = 0;
    for (std::int32_t i = 0; i < parameters; i++)
    {
      const std::int32_t size = entered.parameter_sizes[static_cast<std::size_t>(i)];
      if (size > parameter_size)
      {
        return Fail(std::format(
          "Parameter {} of function {} ({}) is {} cells, more than {}", i, function, name, size, parameter_size));
      }
      cells += size;
    }

    // What a call puts aside is its locals, or its parameters where they are
    // more: id's qcc counted no locals for a function declared ahead of its
    // body (see ProgsFunction), and its parameter has to survive a call of
    // the same function further in all the same.
    const std::int32_t kept = std::max(entered.locals_count, cells);
    if (!HasGlobals(entered.first_local, kept))
    {
      return Fail(std::format("The parameters of function {} ({}) are outside the globals", function, name));
    }

    const auto locals = static_cast<std::size_t>(kept);
    if (_saved_locals.size() + locals > _limits.saved_locals)
    {
      return Fail(std::format(
        "The stack is too deep: calling {} would put more than {} locals aside", name, _limits.saved_locals));
    }

    // The locals of a function are globals, one set for all its calls. What
    // they hold now belongs to whoever called: a call of the same function
    // further out when it calls itself. So they are put aside here and
    // given back by Leave(), which is what makes recursion work.
    const auto first_local = _globals.begin() + entered.first_local;
    _frames.push_back({
      .function = function,
      .return_statement = return_statement,
      .saved_locals_at = _saved_locals.size(),
    });
    _saved_locals.insert(_saved_locals.end(), first_local, first_local + kept);

    // the parameters of the call become the first locals, one after another
    // without the gaps the parameter globals have
    auto local = static_cast<std::size_t>(entered.first_local);
    for (std::int32_t i = 0; i < parameters; i++)
    {
      const auto from = static_cast<std::size_t>(first_parameter_offset + i * parameter_size);
      for (std::size_t cell = 0; cell < entered.parameter_sizes[static_cast<std::size_t>(i)]; cell++)
      {
        _globals[local++] = _globals[from + cell];
      }
    }
    return true;
  }

  std::int32_t QcMachine::Leave()
  {
    const Frame frame = _frames.back();
    _frames.pop_back();

    const ProgsFunction &left = _progs.functions[static_cast<std::size_t>(frame.function)];
    const auto saved = _saved_locals.begin() + static_cast<std::ptrdiff_t>(frame.saved_locals_at);
    std::copy(saved, _saved_locals.end(), _globals.begin() + left.first_local);
    _saved_locals.erase(saved, _saved_locals.end());
    return frame.return_statement;
  }

  bool QcMachine::CallBuiltin(const std::int32_t function)
  {
    // in 64 bits: the lowest number there is has no positive of its size
    const ProgsFunction &called = _progs.functions[static_cast<std::size_t>(function)];
    const std::int64_t number = -static_cast<std::int64_t>(called.first_statement);

    const auto found = number <= std::numeric_limits<std::int32_t>::max()
                         ? _builtins.find(static_cast<std::int32_t>(number))
                         : _builtins.end();
    if (found == _builtins.end() || !found->second)
    {
      const std::string_view name = _progs.GetString(called.name);
      return Fail(name.empty()
                    ? std::format("Builtin {} is not registered", number)
                    : std::format("Builtin {} ({}) is not registered", number, name));
    }

    found->second(*this);
    return !_failed;
  }

  bool QcMachine::SetBuiltin(const std::int32_t number, Builtin builtin)
  {
    if (number <= 0) { return false; }

    _builtins[number] = std::move(builtin);
    return true;
  }

  std::int32_t QcMachine::GetArgumentCount() const
  {
    return _argument_count;
  }

  float QcMachine::GetParameterFloat(const std::int32_t parameter) const
  {
    return GetFloat(parameter_offset(parameter));
  }

  std::int32_t QcMachine::GetParameterInteger(const std::int32_t parameter) const
  {
    return GetInteger(parameter_offset(parameter));
  }

  std::array<float, 3> QcMachine::GetParameterVector(const std::int32_t parameter) const
  {
    return GetVector(parameter_offset(parameter));
  }

  std::string_view QcMachine::GetParameterString(const std::int32_t parameter) const
  {
    return GetString(GetParameterInteger(parameter));
  }

  bool QcMachine::SetParameterFloat(const std::int32_t parameter, const float value)
  {
    return SetFloat(parameter_offset(parameter), value);
  }

  bool QcMachine::SetParameterInteger(const std::int32_t parameter, const std::int32_t value)
  {
    return SetInteger(parameter_offset(parameter), value);
  }

  bool QcMachine::SetParameterVector(const std::int32_t parameter, const std::array<float, 3> &value)
  {
    return SetVector(parameter_offset(parameter), value);
  }

  bool QcMachine::SetParameterString(const std::int32_t parameter, const std::string_view text)
  {
    if (parameter_offset(parameter) < 0) { return false; }

    return SetParameterInteger(parameter, AddString(text));
  }

  float QcMachine::GetReturnFloat() const
  {
    return GetFloat(return_offset);
  }

  std::int32_t QcMachine::GetReturnInteger() const
  {
    return GetInteger(return_offset);
  }

  std::array<float, 3> QcMachine::GetReturnVector() const
  {
    return GetVector(return_offset);
  }

  std::string_view QcMachine::GetReturnString() const
  {
    return GetString(GetReturnInteger());
  }

  void QcMachine::SetReturnFloat(const float value)
  {
    SetFloat(return_offset, value);
  }

  void QcMachine::SetReturnInteger(const std::int32_t value)
  {
    SetInteger(return_offset, value);
  }

  void QcMachine::SetReturnVector(const std::array<float, 3> &value)
  {
    SetVector(return_offset, value);
  }

  void QcMachine::SetReturnString(const std::string_view text)
  {
    SetReturnInteger(AddString(text));
  }

  bool QcMachine::ReadString(const std::int32_t offset, std::string_view &text)
  {
    if (!HasString(offset)) { return Fail(std::format("There is no string at offset {}", offset)); }

    text = GetString(offset);
    return true;
  }

  bool QcMachine::FindField(
    const std::int32_t entity,
    const std::int32_t field,
    const std::size_t count,
    std::size_t &place)
  {
    // A free entity is not refused: the original lets a program read and
    // write one, and game code does, for an entity it removed a moment ago.
    if (entity < 0 || entity >= GetEntityCount())
    {
      return Fail(std::format("There is no entity {}: there are {}", entity, GetEntityCount()));
    }

    const auto fields = static_cast<std::size_t>(_progs.header.entity_fields);
    if (field < 0 || static_cast<std::size_t>(field) + count > fields)
    {
      return Fail(std::format(
        "A field of {} cells at offset {} is outside the {} cells of an entity", count, field, fields));
    }

    place = static_cast<std::size_t>(entity) * fields + static_cast<std::size_t>(field);
    return true;
  }

  bool QcMachine::FindPointer(const std::int32_t pointer, const std::size_t count, std::size_t &place)
  {
    // A pointer is the place of the field among the cells of all entities:
    // the number of the entity times the cells of one, plus the offset of
    // the field. A program only gets one from Address, but nothing stops it
    // from making one up, so it is taken apart and checked again.
    const std::int32_t fields = _progs.header.entity_fields;
    if (pointer < 0 || fields == 0)
    {
      return Fail(std::format("A pointer of {} points at no field of an entity", pointer));
    }
    return FindField(pointer / fields, pointer % fields, count, place);
  }

  bool QcMachine::SetState(const QcCell frame, const QcCell think)
  {
    if (!_state_places.found)
    {
      return Fail("State needs the globals self and time and the fields nextthink, frame, and think");
    }

    const std::int32_t entity = GetInteger(_state_places.self);
    std::size_t next_think_at = 0;
    std::size_t frame_at = 0;
    std::size_t think_at = 0;
    if (!FindField(entity, _state_places.next_think, 1, next_think_at) ||
        !FindField(entity, _state_places.frame, 1, frame_at) ||
        !FindField(entity, _state_places.think, 1, think_at))
    {
      return false;
    }

    _entity_cells[next_think_at] = QcCell::OfFloat(GetFloat(_state_places.time) + 0.1f);
    _entity_cells[frame_at] = frame;
    _entity_cells[think_at] = think;
    return true;
  }

  bool QcMachine::Run(const std::int32_t function)
  {
    const ProgsFunction &started = _progs.functions[static_cast<std::size_t>(function)];

    // the run ends when the function it started with returns
    const std::size_t exit_depth = _frames.size();
    if (!Enter(function, -1)) { return false; }

    const auto statements_count = static_cast<std::int32_t>(_progs.statements.size());
    const std::size_t globals_count = _global_count;
    QcCell *const g = _globals.data();
    std::int32_t next = started.first_statement;
    std::int64_t executed = 0;

    while (true)
    {
      // checked before it becomes the statement that is running, so that
      // the error names the statement that jumped
      if (next < 0 || next >= statements_count)
      {
        return Fail(std::format(
          "The program went to statement {}, outside its {} statements", next, statements_count));
      }
      _statement = next;

      if (++executed > _limits.statements)
      {
        return Fail(std::format(
          "The program ran more than {} statements and is taken to be stuck in a loop", _limits.statements));
      }

      _statements_run++;

      const ProgsStatement &statement = _progs.statements[static_cast<std::size_t>(next)];
      OperandSizes sizes;
      if (!operand_sizes(statement.opcode, sizes))
      {
        return Fail(std::format("Opcode {} is not known", statement.opcode));
      }

      // Once here, the cases below read and write their operands freely.
      // An operand has to start inside the globals; one of three cells may
      // end in the two spare cells after them, see the constructor.
      const std::size_t a = statement.a;
      const std::size_t b = statement.b;
      const std::size_t c = statement.c;
      if ((sizes.a != 0 && a >= globals_count) ||
          (sizes.b != 0 && b >= globals_count) ||
          (sizes.c != 0 && c >= globals_count))
      {
        return Fail(std::format(
          "A statement names a global outside the {} there are: its operands are {}, {}, and {}",
          globals_count, a, b, c));
      }

      next++;
      switch (static_cast<ProgsOpcode>(statement.opcode))
      {
        case ProgsOpcode::AddF:
        {
          g[c] = QcCell::OfFloat(g[a].AsFloat() + g[b].AsFloat());
          break;
        }
        case ProgsOpcode::SubF:
        {
          g[c] = QcCell::OfFloat(g[a].AsFloat() - g[b].AsFloat());
          break;
        }
        case ProgsOpcode::MulF:
        {
          g[c] = QcCell::OfFloat(g[a].AsFloat() * g[b].AsFloat());
          break;
        }
        case ProgsOpcode::DivF:
        {
          // Dividing by zero is not an error, as it is none in the
          // original, which divides without looking: the result is an
          // infinity, or not-a-number for zero by zero, and the program
          // goes on with it. Game code relies on getting away with it.
          g[c] = QcCell::OfFloat(g[a].AsFloat() / g[b].AsFloat());
          break;
        }
        case ProgsOpcode::AddV:
        case ProgsOpcode::SubV:
        {
          // every result is worked out before it is written, since the
          // operand written may be one that is read
          const float sign = static_cast<ProgsOpcode>(statement.opcode) == ProgsOpcode::AddV ? 1.0f : -1.0f;
          const float x = g[a].AsFloat() + sign * g[b].AsFloat();
          const float y = g[a + 1].AsFloat() + sign * g[b + 1].AsFloat();
          const float z = g[a + 2].AsFloat() + sign * g[b + 2].AsFloat();
          g[c] = QcCell::OfFloat(x);
          g[c + 1] = QcCell::OfFloat(y);
          g[c + 2] = QcCell::OfFloat(z);
          break;
        }
        case ProgsOpcode::MulV:
        {
          // two vectors multiplied are their dot product
          g[c] = QcCell::OfFloat(
            g[a].AsFloat() * g[b].AsFloat() +
            g[a + 1].AsFloat() * g[b + 1].AsFloat() +
            g[a + 2].AsFloat() * g[b + 2].AsFloat());
          break;
        }
        case ProgsOpcode::MulFV:
        case ProgsOpcode::MulVF:
        {
          const bool float_first = static_cast<ProgsOpcode>(statement.opcode) == ProgsOpcode::MulFV;
          const float scale = g[float_first ? a : b].AsFloat();
          const std::size_t vector = float_first ? b : a;
          const float x = scale * g[vector].AsFloat();
          const float y = scale * g[vector + 1].AsFloat();
          const float z = scale * g[vector + 2].AsFloat();
          g[c] = QcCell::OfFloat(x);
          g[c + 1] = QcCell::OfFloat(y);
          g[c + 2] = QcCell::OfFloat(z);
          break;
        }
        case ProgsOpcode::BitAnd:
        {
          g[c] = QcCell::OfFloat(static_cast<float>(to_integer(g[a].AsFloat()) & to_integer(g[b].AsFloat())));
          break;
        }
        case ProgsOpcode::BitOr:
        {
          g[c] = QcCell::OfFloat(static_cast<float>(to_integer(g[a].AsFloat()) | to_integer(g[b].AsFloat())));
          break;
        }
        case ProgsOpcode::And:
        {
          // both sides are always worked out before a statement like this
          // one: the language has no short cut
          g[c] = of_bool(g[a].AsFloat() != 0.0f && g[b].AsFloat() != 0.0f);
          break;
        }
        case ProgsOpcode::Or:
        {
          g[c] = of_bool(g[a].AsFloat() != 0.0f || g[b].AsFloat() != 0.0f);
          break;
        }
        case ProgsOpcode::Le:
        {
          g[c] = of_bool(g[a].AsFloat() <= g[b].AsFloat());
          break;
        }
        case ProgsOpcode::Ge:
        {
          g[c] = of_bool(g[a].AsFloat() >= g[b].AsFloat());
          break;
        }
        case ProgsOpcode::Lt:
        {
          g[c] = of_bool(g[a].AsFloat() < g[b].AsFloat());
          break;
        }
        case ProgsOpcode::Gt:
        {
          g[c] = of_bool(g[a].AsFloat() > g[b].AsFloat());
          break;
        }
        case ProgsOpcode::EqF:
        {
          g[c] = of_bool(g[a].AsFloat() == g[b].AsFloat());
          break;
        }
        case ProgsOpcode::NeF:
        {
          g[c] = of_bool(g[a].AsFloat() != g[b].AsFloat());
          break;
        }
        case ProgsOpcode::EqV:
        case ProgsOpcode::NeV:
        {
          const bool equal =
            g[a].AsFloat() == g[b].AsFloat() &&
            g[a + 1].AsFloat() == g[b + 1].AsFloat() &&
            g[a + 2].AsFloat() == g[b + 2].AsFloat();
          g[c] = of_bool(equal == (static_cast<ProgsOpcode>(statement.opcode) == ProgsOpcode::EqV));
          break;
        }
        case ProgsOpcode::EqS:
        case ProgsOpcode::NeS:
        {
          // strings are equal by their text, not by where they are
          std::string_view left;
          std::string_view right;
          if (!ReadString(g[a].AsInteger(), left) || !ReadString(g[b].AsInteger(), right)) { return false; }
          g[c] = of_bool((left == right) == (static_cast<ProgsOpcode>(statement.opcode) == ProgsOpcode::EqS));
          break;
        }
        case ProgsOpcode::EqE:
        case ProgsOpcode::EqFnc:
        {
          g[c] = of_bool(g[a].AsInteger() == g[b].AsInteger());
          break;
        }
        case ProgsOpcode::NeE:
        case ProgsOpcode::NeFnc:
        {
          g[c] = of_bool(g[a].AsInteger() != g[b].AsInteger());
          break;
        }
        case ProgsOpcode::NotF:
        {
          g[c] = of_bool(g[a].AsFloat() == 0.0f);
          break;
        }
        case ProgsOpcode::NotV:
        {
          g[c] = of_bool(g[a].AsFloat() == 0.0f && g[a + 1].AsFloat() == 0.0f && g[a + 2].AsFloat() == 0.0f);
          break;
        }
        case ProgsOpcode::NotS:
        {
          // no string, or one without text
          std::string_view text;
          if (!ReadString(g[a].AsInteger(), text)) { return false; }
          g[c] = of_bool(text.empty());
          break;
        }
        case ProgsOpcode::NotEnt:
        case ProgsOpcode::NotFnc:
        {
          // the world and the function that is nothing are both number 0
          g[c] = of_bool(g[a].AsInteger() == 0);
          break;
        }
        case ProgsOpcode::StoreF:
        case ProgsOpcode::StoreS:
        case ProgsOpcode::StoreEnt:
        case ProgsOpcode::StoreFld:
        case ProgsOpcode::StoreFnc:
        {
          g[b] = g[a];
          break;
        }
        case ProgsOpcode::StoreV:
        {
          const QcCell x = g[a];
          const QcCell y = g[a + 1];
          const QcCell z = g[a + 2];
          g[b] = x;
          g[b + 1] = y;
          g[b + 2] = z;
          break;
        }
        case ProgsOpcode::LoadF:
        case ProgsOpcode::LoadS:
        case ProgsOpcode::LoadEnt:
        case ProgsOpcode::LoadFld:
        case ProgsOpcode::LoadFnc:
        {
          std::size_t place = 0;
          if (!FindField(g[a].AsInteger(), g[b].AsInteger(), 1, place)) { return false; }
          g[c] = _entity_cells[place];
          break;
        }
        case ProgsOpcode::LoadV:
        {
          std::size_t place = 0;
          if (!FindField(g[a].AsInteger(), g[b].AsInteger(), 3, place)) { return false; }
          g[c] = _entity_cells[place];
          g[c + 1] = _entity_cells[place + 1];
          g[c + 2] = _entity_cells[place + 2];
          break;
        }
        case ProgsOpcode::Address:
        {
          // The original refuses the address of a field of the world once
          // the level runs. That is a rule of the game and not of the
          // machine, and is left to the host.
          std::size_t place = 0;
          if (!FindField(g[a].AsInteger(), g[b].AsInteger(), 1, place)) { return false; }
          g[c] = QcCell::OfInteger(static_cast<std::int32_t>(place));
          break;
        }
        case ProgsOpcode::StorepF:
        case ProgsOpcode::StorepS:
        case ProgsOpcode::StorepEnt:
        case ProgsOpcode::StorepFld:
        case ProgsOpcode::StorepFnc:
        {
          std::size_t place = 0;
          if (!FindPointer(g[b].AsInteger(), 1, place)) { return false; }
          _entity_cells[place] = g[a];
          break;
        }
        case ProgsOpcode::StorepV:
        {
          std::size_t place = 0;
          if (!FindPointer(g[b].AsInteger(), 3, place)) { return false; }
          _entity_cells[place] = g[a];
          _entity_cells[place + 1] = g[a + 1];
          _entity_cells[place + 2] = g[a + 2];
          break;
        }
        case ProgsOpcode::State:
        {
          if (!SetState(g[a], g[b])) { return false; }
          break;
        }
        case ProgsOpcode::If:
        case ProgsOpcode::IfNot:
        {
          // The original tests the bits of the cell and not the float, and
          // so does this: a float of minus zero, whose bits are not all
          // zero, counts as true here though it is false for NotF.
          const bool wanted = static_cast<ProgsOpcode>(statement.opcode) == ProgsOpcode::If;
          if ((g[a].bits != 0) == wanted) { next = _statement + static_cast<std::int16_t>(statement.b); }
          break;
        }
        case ProgsOpcode::Call0:
        case ProgsOpcode::Call1:
        case ProgsOpcode::Call2:
        case ProgsOpcode::Call3:
        case ProgsOpcode::Call4:
        case ProgsOpcode::Call5:
        case ProgsOpcode::Call6:
        case ProgsOpcode::Call7:
        case ProgsOpcode::Call8:
        {
          // the parameters are in their globals already: the statements
          // before this one stored them there
          const std::int32_t called = g[a].AsInteger();
          if (called == 0) { return Fail("A call of function 0, which is no function"); }
          if (called < 0 || static_cast<std::size_t>(called) >= _progs.functions.size())
          {
            return Fail(std::format(
              "A call of function {}: the program has functions 1 to {}",
              called, static_cast<std::int64_t>(_progs.functions.size()) - 1));
          }

          _argument_count = statement.opcode - static_cast<std::uint16_t>(ProgsOpcode::Call0);
          const ProgsFunction &target = _progs.functions[static_cast<std::size_t>(called)];
          if (target.IsBuiltin())
          {
            // a builtin may run the machine itself, which moves the
            // statement that is running
            const std::int32_t calling = _statement;
            if (!CallBuiltin(called)) { return false; }
            _statement = calling;
            break;
          }

          if (!Enter(called, next)) { return false; }
          next = target.first_statement;
          break;
        }
        case ProgsOpcode::Goto:
        {
          next = _statement + static_cast<std::int16_t>(statement.a);
          break;
        }
        case ProgsOpcode::Done:
        case ProgsOpcode::Return:
        {
          const QcCell x = g[a];
          const QcCell y = g[a + 1];
          const QcCell z = g[a + 2];
          g[return_offset] = x;
          g[return_offset + 1] = y;
          g[return_offset + 2] = z;

          next = Leave();
          if (_frames.size() == exit_depth) { return true; }
          break;
        }
        default:
        {
          return Fail(std::format("Opcode {} is not known", statement.opcode));
        }
      }
    }
  }

  bool QcMachine::HasString(const std::int32_t offset) const
  {
    if (offset >= 0) { return _progs.HasString(offset); }

    // the first one made is -1, the second -2, and so on
    return static_cast<std::size_t>(-(offset + 1)) < _temporary_strings.size();
  }

  std::string_view QcMachine::GetString(const std::int32_t offset) const
  {
    if (offset >= 0) { return _progs.GetString(offset); }
    if (!HasString(offset)) { return {}; }

    return _temporary_strings[static_cast<std::size_t>(-(offset + 1))];
  }

  std::int32_t QcMachine::AddString(const std::string_view text)
  {
    std::string key(text);
    if (const auto found = _temporary_offsets.find(key); found != _temporary_offsets.end()) { return found->second; }

    const std::int32_t offset = -static_cast<std::int32_t>(_temporary_strings.size()) - 1;
    _temporary_strings.push_back(key);
    _temporary_offsets.emplace(std::move(key), offset);
    return offset;
  }

  std::int32_t QcMachine::GetEntityCount() const
  {
    return static_cast<std::int32_t>(_entity_free.size());
  }

  std::optional<std::int32_t> QcMachine::CreateEntity()
  {
    // the world is never free, so the search starts after it
    for (std::size_t i = 1; i < _entity_free.size(); i++)
    {
      if (_entity_free[i] != 0)
      {
        // cleared now and not when it was freed: the program may have read
        // and written it since
        _entity_free[i] = 0;
        std::ranges::fill(GetEntity(static_cast<std::int32_t>(i)), QcCell{});
        return static_cast<std::int32_t>(i);
      }
    }

    if (GetEntityCount() >= _limits.entities) { return std::nullopt; }

    _entity_free.push_back(0);
    _entity_cells.resize(_entity_free.size() * static_cast<std::size_t>(_progs.header.entity_fields));
    return GetEntityCount() - 1;
  }

  bool QcMachine::FreeEntity(const std::int32_t entity)
  {
    if (entity <= 0 || entity >= GetEntityCount()) { return false; }

    _entity_free[static_cast<std::size_t>(entity)] = 1;
    return true;
  }

  bool QcMachine::IsEntityFree(const std::int32_t entity) const
  {
    if (entity < 0 || entity >= GetEntityCount()) { return true; }

    return _entity_free[static_cast<std::size_t>(entity)] != 0;
  }

  std::span<QcCell> QcMachine::GetEntity(const std::int32_t entity)
  {
    if (entity < 0 || entity >= GetEntityCount()) { return {}; }

    const auto fields = static_cast<std::size_t>(_progs.header.entity_fields);
    return std::span(_entity_cells).subspan(static_cast<std::size_t>(entity) * fields, fields);
  }
} // quake
