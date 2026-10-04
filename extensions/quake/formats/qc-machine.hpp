#ifndef QUAKE_QC_MACHINE_HPP
#define QUAKE_QC_MACHINE_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "progs.hpp"
#include "qc-cell.hpp"
#include "qc-error.hpp"
#include "qc-limits.hpp"

namespace quake
{
  /// The machine that runs the game code of a `Progs`.
  ///
  /// It keeps what the program works on: the globals, the entities, and the
  /// strings made while it runs. It knows nothing of the game: what a
  /// builtin does, and what the fields of an entity mean, is for the host
  /// that owns the machine, which registers the builtins with SetBuiltin().
  ///
  /// Nothing a program does reads or writes outside the machine's memory.
  /// A program that tries, or that never ends, is stopped: Call() returns
  /// false and GetError() says why and where. What reaches the machine from
  /// the host with an offset or a number that is out of range gives zero, or
  /// is refused.
  class QcMachine final
  {
  public:
    /// A function of the engine that the program calls by a number. It
    /// reads its parameters and sets what it returns through the machine
    /// it is given.
    using Builtin = std::function<void(QcMachine &)>;

  private:
    Progs _progs;
    QcLimits _limits;
    /// The globals, and how many the program has: two spare cells follow
    /// them, see the constructor.
    std::vector<QcCell> _globals;
    std::size_t _global_count = 0;

    /// The fields of every entity, one block of `entity_fields` cells after
    /// another, and for each entity whether it is free.
    std::vector<QcCell> _entity_cells;
    std::vector<std::uint8_t> _entity_free;

    /// The strings made while running, which never move once made, and the
    /// offset each was given.
    std::deque<std::string> _temporary_strings;
    std::unordered_map<std::string, std::int32_t> _temporary_offsets;

    /// A call that is under way: which function, the statement to go on
    /// with when it returns, and where its locals were put aside.
    struct Frame
    {
      std::int32_t function = 0;
      std::int32_t return_statement = 0;
      std::size_t saved_locals_at = 0;
    };

    std::vector<Frame> _frames;

    /// What the locals of the functions under way held before they were
    /// called, the newest call last.
    std::vector<QcCell> _saved_locals;

    QcError _error;
    bool _failed = false;

    /// Where the globals and fields are that State works on, found by their
    /// names once, when the machine is made.
    struct StatePlaces
    {
      bool found = false;
      std::uint16_t self = 0;
      std::uint16_t time = 0;
      std::uint16_t next_think = 0;
      std::uint16_t frame = 0;
      std::uint16_t think = 0;
    };

    StatePlaces _state_places;

    std::unordered_map<std::int32_t, Builtin> _builtins;

    /// With how many parameters the call that is under way was made.
    std::int32_t _argument_count = 0;

    /// How many Call() are inside one another: a builtin may call back in.
    int _runs = 0;

    /// The statement that is running, for the error.
    std::int32_t _statement = -1;

    [[nodiscard]] bool HasGlobals(std::int32_t offset, std::int32_t count) const;

    /// Makes ready for a Call(): one from outside starts without the error
    /// of the run before. False when a run under way was stopped already.
    bool Begin();

    /// Stops the run with an error. Returns false, for the caller to return.
    bool Fail(std::string message);

    /// Starts a function of statements: puts its locals aside and copies
    /// the parameters of the call into them.
    bool Enter(std::int32_t function, std::int32_t return_statement);

    /// Ends the function on top: gives its locals back what they held
    /// before, and returns the statement to go on with.
    std::int32_t Leave();

    /// Calls the builtin a function stands for. An error when the host
    /// registered none of its number.
    bool CallBuiltin(std::int32_t function);

    /// Runs a function of statements until it returns.
    bool Run(std::int32_t function);

    /// The place of `count` cells of a field in an entity, among the cells
    /// of all entities, as a program names them. Stops the run when there
    /// is no such entity or the field is not inside it.
    bool FindField(std::int32_t entity, std::int32_t field, std::size_t count, std::size_t &place);

    /// The same from a pointer, which Address made of an entity and a field.
    bool FindPointer(std::int32_t pointer, std::size_t count, std::size_t &place);

    /// What State does: the entity in the global `self` shows a frame now,
    /// and thinks next with a function, a tenth of a second from `time`.
    bool SetState(QcCell frame, QcCell think);

    /// Takes the text of a string a program names, stopping the run when
    /// there is no such string.
    bool ReadString(std::int32_t offset, std::string_view &text);

  public:
    /// Where the value a function returns is, among the globals.
    static constexpr std::int32_t return_offset = 1;

    /// Where the first parameter of a call is. Each has `parameter_size`
    /// cells, whatever it holds.
    static constexpr std::int32_t first_parameter_offset = 4;
    static constexpr std::int32_t parameter_size = 3;
    static constexpr std::int32_t max_parameters = ProgsFunction::max_parameters;

    /// Takes a program that Read() accepted. The world, entity 0, is there
    /// from the start.
    explicit QcMachine(Progs progs, const QcLimits &limits = {});

    [[nodiscard]] const Progs &GetProgs() const;

    // Running.

    /// Runs a function to its end, with the parameters that were put in the
    /// parameter globals before. Returns false when the run was stopped by
    /// an error, which GetError() then holds. A builtin may call this while
    /// it is called itself.
    bool Call(std::int32_t function);

    /// The same for a function by its name. A name that is not known is an
    /// error.
    bool Call(std::string_view name);

    /// Whether the last run was stopped by an error.
    [[nodiscard]] bool HasFailed() const;

    /// Why the last run was stopped, and where.
    [[nodiscard]] const QcError &GetError() const;

    /// Stops the run that is under way with an error of the host's, for a
    /// builtin that is called with what it cannot work with. The first
    /// error of a run is the one that is kept.
    void Stop(std::string message);

    // The builtins, and what one works with while it is called.

    /// Registers the builtin of a number, from 1 up, in the place of the one
    /// that was there. A builtin must not replace itself while it runs.
    bool SetBuiltin(std::int32_t number, Builtin builtin);

    /// With how many parameters the program made the call that is under
    /// way, which a builtin that takes any number asks. For a call from the
    /// host, the number the function says it takes.
    [[nodiscard]] std::int32_t GetArgumentCount() const;

    // The parameters of a call, numbered from 0 to 7: set by the host
    // before Call(), read by a builtin. A string, an entity, a function,
    // and a field are integers. A parameter that there is not reads as
    // zero and is not set.

    [[nodiscard]] float GetParameterFloat(std::int32_t parameter) const;

    [[nodiscard]] std::int32_t GetParameterInteger(std::int32_t parameter) const;

    [[nodiscard]] std::array<float, 3> GetParameterVector(std::int32_t parameter) const;

    /// The text of a parameter that is a string.
    [[nodiscard]] std::string_view GetParameterString(std::int32_t parameter) const;

    bool SetParameterFloat(std::int32_t parameter, float value);

    bool SetParameterInteger(std::int32_t parameter, std::int32_t value);

    bool SetParameterVector(std::int32_t parameter, const std::array<float, 3> &value);

    /// Sets a parameter to a string made now, see AddString().
    bool SetParameterString(std::int32_t parameter, std::string_view text);

    // What a call returns: set by a builtin, read by the host after Call().

    [[nodiscard]] float GetReturnFloat() const;

    [[nodiscard]] std::int32_t GetReturnInteger() const;

    [[nodiscard]] std::array<float, 3> GetReturnVector() const;

    [[nodiscard]] std::string_view GetReturnString() const;

    void SetReturnFloat(float value);

    void SetReturnInteger(std::int32_t value);

    void SetReturnVector(const std::array<float, 3> &value);

    /// Returns a string made now, see AddString().
    void SetReturnString(std::string_view text);

    // The globals. A read outside them gives zero, a write is refused.

    [[nodiscard]] std::int32_t GetGlobalCount() const;

    [[nodiscard]] float GetFloat(std::int32_t offset) const;

    [[nodiscard]] std::int32_t GetInteger(std::int32_t offset) const;

    [[nodiscard]] std::array<float, 3> GetVector(std::int32_t offset) const;

    bool SetFloat(std::int32_t offset, float value);

    bool SetInteger(std::int32_t offset, std::int32_t value);

    bool SetVector(std::int32_t offset, const std::array<float, 3> &value);

    // The strings.

    /// Whether an offset names a string: one of the file, or one made while
    /// running.
    [[nodiscard]] bool HasString(std::int32_t offset) const;

    /// The string at an offset. Empty when there is none. It stays where it
    /// is for as long as the machine lives.
    [[nodiscard]] std::string_view GetString(std::int32_t offset) const;

    /// Keeps a string made while running, the result of a builtin for one,
    /// and gives the offset to put in a cell for it.
    ///
    /// The strings of the file have the offsets from zero up, so these get
    /// the negative ones. The same text gets the same offset again. They
    /// are kept until the machine goes, since a cell anywhere may hold one.
    std::int32_t AddString(std::string_view text);

    // The entities.

    /// How many entities there are, free ones among them.
    [[nodiscard]] std::int32_t GetEntityCount() const;

    /// Makes an entity with every field zero, in the place of a free one
    /// when there is one, and gives its number. Nothing when the limit is
    /// reached.
    std::optional<std::int32_t> CreateEntity();

    /// Frees an entity and sets its fields to zero. The world is never
    /// freed. The program may still read and write a free entity, as in the
    /// original; its number is given out again by CreateEntity().
    bool FreeEntity(std::int32_t entity);

    /// Whether an entity is free. An entity that there is not is free too.
    [[nodiscard]] bool IsEntityFree(std::int32_t entity) const;

    /// The fields of an entity, to read and to write. Empty when there is no
    /// such entity. It holds until the next CreateEntity().
    [[nodiscard]] std::span<QcCell> GetEntity(std::int32_t entity);
  };
} // quake

#endif //QUAKE_QC_MACHINE_HPP
