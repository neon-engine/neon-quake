#include "saved-game-capture.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <format>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "formats/bsp-file.hpp"
#include "formats/entity-text.hpp"
#include "formats/real-data.test.hpp"
#include "level-collision.hpp"
#include "level-physics.hpp"
#include "level-running.hpp"
#include "level-spawning.hpp"
#include "level-stepping.hpp"
#include "level-touching.hpp"
#include "level-world-host.test.hpp"
#include "qc-core-builtins.hpp"
#include "qc-flag.hpp"
#include "qc-recording-host.test.hpp"
#include "qc-value-text.hpp"
#include "qc-world-builtins.hpp"
#include "saved-game-text.hpp"

// A saved game with the game code and a level of a real game: a level runs
// with the real builtins and the collision of the level, is saved as the
// text of the original, and is loaded into a second machine as a host loads
// one. Both then go on, and are looked at side by side.
namespace
{
  using quake::BspFile;
  using quake::ClientThink;
  using quake::EntityText;
  using quake::LevelCollision;
  using quake::LevelFailure;
  using quake::LevelPhysics;
  using quake::LevelRunning;
  using quake::LevelSpawning;
  using quake::LevelStepping;
  using quake::LevelTouching;
  using quake::LevelVector;
  using quake::LevelWorldHost;
  using quake::Progs;
  using quake::ProgsDefinition;
  using quake::ProgsType;
  using quake::QcCell;
  using quake::QcCoreBuiltins;
  using quake::QcFields;
  using quake::QcGlobals;
  using quake::QcMachine;
  using quake::QcRecordingHost;
  using quake::QcValueText;
  using quake::QcWorldBuiltins;
  using quake::RealData;
  using quake::SavedGame;
  using quake::SavedGameCapture;
  using quake::SavedGameRestoreReport;
  using quake::SavedGameText;
  using ::testing::IsEmpty;

  constexpr std::int32_t player = 1;

  /// The length of a frame of the original at its usual rate.
  constexpr float frame_time = 1.0f / 72.0f;

  /// By how much a float may be off that went through a text with six
  /// places after the point.
  constexpr float text_tolerance = 1e-6f;

  /// A level of the real game that runs, with everything a host has
  /// around the machine.
  struct Game
  {
    BspFile level;
    std::unique_ptr<QcMachine> machine;
    std::unique_ptr<QcFields> fields;
    std::unique_ptr<QcGlobals> globals;
    std::unique_ptr<QcRecordingHost> host;
    std::unique_ptr<QcCoreBuiltins> core;
    std::unique_ptr<LevelCollision> collision;
    std::unique_ptr<LevelRunning> running;
    std::unique_ptr<LevelTouching> touching;
    std::unique_ptr<LevelStepping> stepping;
    std::unique_ptr<LevelPhysics> physics;
    std::unique_ptr<QcWorldBuiltins> world;
    std::unique_ptr<LevelWorldHost> world_host;

    /// Starts a level as for a new game: the builtins registered, the
    /// entities handed to the game code, then two frames of a tenth of a
    /// second. No player is let in.
    bool Start(const Progs &progs, const std::string_view name, const int skill)
    {
      const std::string path = std::format("maps/{}.bsp", name);
      std::string error;
      if (!level.Read(RealData::Get().GetBytes(path), error))
      {
        ADD_FAILURE() << path << ": " << error;
        return false;
      }
      EntityText text;
      EXPECT_TRUE(text.Read(level.entities, error)) << path << ": " << error;

      machine = std::make_unique<QcMachine>(progs);
      fields = std::make_unique<QcFields>(machine->GetProgs());
      globals = std::make_unique<QcGlobals>(machine->GetProgs());
      host = std::make_unique<QcRecordingHost>();

      core = std::make_unique<QcCoreBuiltins>(*host);
      core->Register(*machine);
      core->SeedRandom(1);
      core->GetVariables().Set("skill", std::to_string(skill));
      core->GetModels().Add(path);

      collision = std::make_unique<LevelCollision>(*machine);
      EXPECT_TRUE(collision->Build(level, error)) << path << ": " << error;
      running = std::make_unique<LevelRunning>(*machine);
      touching = std::make_unique<LevelTouching>(*collision, *running);
      stepping = std::make_unique<LevelStepping>(*collision, *touching, [this] { return core->NextRandom(); });
      physics = std::make_unique<LevelPhysics>(*collision, *touching);
      running->SetMover(physics.get());
      world = std::make_unique<QcWorldBuiltins>(*collision, *stepping);
      world->Register(*machine);
      world_host = std::make_unique<LevelWorldHost>(level, *collision);
      world_host->Register(*machine);

      LevelSpawning spawning(*machine);
      spawning.Spawn(text.entities, {.map_name = std::string(name), .model_name = path, .skill = skill});

      running->Advance(0.1f);
      running->Advance(0.1f);
      return true;
    }

    /// Lets time pass with the player in, a frame at a time.
    void Run(const float seconds) const
    {
      const int frames = static_cast<int>(seconds / frame_time);
      for (int frame = 0; frame < frames; frame++)
      {
        running->RunClientThink(player, ClientThink::Before);
        running->Advance(frame_time);
        running->RunClientThink(player, ClientThink::After);
      }
    }

    /// The failures that are the machine's: a run it stopped itself, not
    /// one the game code ended with `error`.
    [[nodiscard]] std::vector<std::string> GetMachineFailures() const
    {
      std::vector<std::string> messages;
      for (const LevelFailure &failure : running->GetFailures())
      {
        if (failure.error.message.starts_with(QcCoreBuiltins::error_start)) { continue; }

        messages.push_back(std::format(
          "entity {} ({}), {}: stopped in {}: {}",
          failure.entity, failure.classname, failure.function, failure.error.function_name, failure.error.message));
      }
      return messages;
    }

    [[nodiscard]] std::int32_t CountLiving() const
    {
      std::int32_t living = 0;
      for (std::int32_t entity = 0; entity < machine->GetEntityCount(); entity++)
      {
        living += machine->IsEntityFree(entity) ? 0 : 1;
      }
      return living;
    }

    /// The first entity of a classname that is not free, or 0.
    [[nodiscard]] std::int32_t Find(const std::string_view classname) const
    {
      for (std::int32_t entity = 1; entity < machine->GetEntityCount(); entity++)
      {
        if (!machine->IsEntityFree(entity) && fields->classname.GetText(*machine, entity) == classname)
        {
          return entity;
        }
      }
      return 0;
    }
  };

  /// Whether the cells of a value of one machine are those of another: a
  /// string by its text, a float as near as a text of six places has it,
  /// anything else bit for bit. `through_text` is for a string that went
  /// through the file, which has `'` for a quote.
  bool IsSameValue(
    const QcMachine &a, const QcMachine &b, const ProgsType type, const std::span<const QcCell> cells_a,
    const std::span<const QcCell> cells_b)
  {
    if (type == ProgsType::String)
    {
      std::string text(a.GetString(cells_a[0].AsInteger()));
      std::ranges::replace(text, '"', '\'');
      return text.substr(0, SavedGameText::max_token_length) == b.GetString(cells_b[0].AsInteger());
    }
    if (type == ProgsType::Float || type == ProgsType::Vector)
    {
      for (std::size_t i = 0; i < QcValueText::GetSize(type); i++)
      {
        if (!(std::fabs(cells_a[i].AsFloat() - cells_b[i].AsFloat()) <= text_tolerance)) { return false; }
      }
      return true;
    }
    return cells_a[0].bits == cells_b[0].bits;
  }

  class SavedGameRealDataTest : public ::testing::Test
  {
  protected:
    Progs _progs;

    void SetUp() override
    {
      const RealData &data = RealData::Get();
      if (!data.IsThere()) { GTEST_SKIP() << data.GetProblem(); }

      std::string error;
      ASSERT_TRUE(_progs.Read(data.GetBytes("progs.dat"), error)) << error;
    }
  };

  TEST_F(SavedGameRealDataTest, SavesALevelThatRunsAndLoadsItIntoAnotherMachineThatGoesOnAlike)
  {
    constexpr std::string_view name = "lq_e1m1";
    constexpr int skill = 1;

    // A game that runs: the player is let in, and a few seconds pass.
    Game first;
    ASSERT_TRUE(first.Start(_progs, name, skill));
    ASSERT_TRUE(first.running->ConnectClient(player, "player"));
    // the numbers a new player came in with, which the game code left
    SavedGame head;
    for (std::size_t i = 0; i < head.parms.size(); i++) { head.parms[i] = first.globals->parms[i].Get(*first.machine); }
    first.Run(3.0f);
    EXPECT_THAT(first.GetMachineFailures(), IsEmpty());

    // To save: the host's own part, then the machine's, then the text.
    head.comment = SavedGameText::MakeComment(
      first.fields->message.GetText(*first.machine, 0),
      static_cast<int>(first.globals->killed_monsters.Get(*first.machine)),
      static_cast<int>(first.globals->total_monsters.Get(*first.machine)));
    head.skill = skill;
    head.map_name = name;
    head.time = first.running->GetTime();
    for (std::size_t i = 0; i < head.light_styles.size(); i++)
    {
      head.light_styles[i] = first.core->GetLightStyle(static_cast<std::int32_t>(i));
    }
    const SavedGame saved = SavedGameCapture::Capture(*first.machine, head);
    const std::string text = SavedGameText::Write(saved);

    EXPECT_EQ(saved.comment.size(), SavedGameText::comment_length);
    EXPECT_EQ(saved.entities.size(), static_cast<std::size_t>(first.machine->GetEntityCount()));
    EXPECT_GT(saved.globals.size(), 50u);
    std::cout << "saved game of " << name << ": " << text.size() << " bytes, " << saved.entities.size()
      << " entities, " << saved.globals.size() << " globals, comment " << saved.comment << "\n";

    // To load: the text, the level as for a new game without a player, the
    // styles, and the machine's part.
    SavedGame read;
    std::string error;
    ASSERT_TRUE(SavedGameText::Read(text, read, error)) << error;
    EXPECT_EQ(read.map_name, name);
    EXPECT_EQ(read.skill, skill);
    EXPECT_EQ(read.comment, saved.comment);
    ASSERT_EQ(read.entities.size(), saved.entities.size());

    Game second;
    ASSERT_TRUE(second.Start(_progs, read.map_name, read.skill));
    for (std::size_t i = 0; i < read.light_styles.size(); i++)
    {
      EXPECT_TRUE(second.core->RestoreLightStyle(static_cast<std::int32_t>(i), read.light_styles[i]));
    }
    SavedGameRestoreReport report;
    ASSERT_TRUE(SavedGameCapture::Restore(read, *second.machine, error, &report)) << error;
    EXPECT_EQ(report.unknown_globals, 0u);
    EXPECT_EQ(report.unknown_fields, 0u);

    // The models and sounds have the numbers they had, since the game code
    // named them again when the level was started.
    EXPECT_EQ(second.core->GetModels().GetNames(), first.core->GetModels().GetNames());
    EXPECT_EQ(second.core->GetSounds().GetNames(), first.core->GetSounds().GetNames());

    // Every saved global, and every field of every entity, is what it was.
    const Progs &progs = first.machine->GetProgs();
    std::size_t globals_compared = 0;
    for (const ProgsDefinition &definition : progs.global_definitions)
    {
      const ProgsType type = definition.GetType();
      if (!definition.IsSaved() || (type != ProgsType::String && type != ProgsType::Float && type != ProgsType::Entity))
      {
        continue;
      }
      const std::array a = {QcCell::OfInteger(first.machine->GetInteger(definition.offset))};
      const std::array b = {QcCell::OfInteger(second.machine->GetInteger(definition.offset))};
      EXPECT_TRUE(IsSameValue(*first.machine, *second.machine, type, a, b)) << progs.GetString(definition.name);
      globals_compared++;
    }
    EXPECT_EQ(globals_compared, saved.globals.size());

    ASSERT_GE(second.machine->GetEntityCount(), first.machine->GetEntityCount());
    std::size_t fields_compared = 0;
    std::size_t floats_not_bit_for_bit = 0;
    for (std::int32_t entity = 0; entity < first.machine->GetEntityCount(); entity++)
    {
      ASSERT_EQ(second.machine->IsEntityFree(entity), first.machine->IsEntityFree(entity)) << entity;
      if (first.machine->IsEntityFree(entity)) { continue; }

      const std::span<const QcCell> a = first.machine->GetEntity(entity);
      const std::span<const QcCell> b = second.machine->GetEntity(entity);
      for (const ProgsDefinition &definition : progs.field_definitions)
      {
        const ProgsType type = definition.GetType();
        const std::size_t size = QcValueText::GetSize(type);
        if (type == ProgsType::Void || type > ProgsType::Function || definition.offset + size > a.size()) { continue; }

        const auto cells_a = a.subspan(definition.offset, size);
        const auto cells_b = b.subspan(definition.offset, size);
        EXPECT_TRUE(IsSameValue(*first.machine, *second.machine, type, cells_a, cells_b))
          << "entity " << entity << ", field " << progs.GetString(definition.name) << ": "
          << QcValueText::Print(*first.machine, type, cells_a) << " and "
          << QcValueText::Print(*second.machine, type, cells_b);
        fields_compared++;
        if (type == ProgsType::Float && cells_a[0].bits != cells_b[0].bits) { floats_not_bit_for_bit++; }
      }
    }
    for (std::int32_t entity = first.machine->GetEntityCount(); entity < second.machine->GetEntityCount(); entity++)
    {
      EXPECT_TRUE(second.machine->IsEntityFree(entity)) << entity;
    }
    EXPECT_GT(fields_compared, 10000u);
    std::cout << fields_compared << " fields compared, " << floats_not_bit_for_bit
      << " floats came back near and not bit for bit\n";

    // Only now is every entity linked, as the original does while it
    // loads: that gives a box in the world, `absmin` and `absmax`, also to
    // what the game code never placed and so had none when it was saved.
    for (std::int32_t entity = 0; entity < second.machine->GetEntityCount(); entity++)
    {
      if (!second.machine->IsEntityFree(entity)) { second.collision->Link(entity); }
    }
    second.running->SetTime(read.time);

    // Both go on for two seconds, with the same numbers of `random`, which
    // a saved game does not hold.
    first.core->SeedRandom(7);
    second.core->SeedRandom(7);
    second.running->ClearFailures();
    first.Run(2.0f);
    second.Run(2.0f);
    EXPECT_THAT(first.GetMachineFailures(), IsEmpty());
    EXPECT_THAT(second.GetMachineFailures(), IsEmpty());

    EXPECT_EQ(
      second.globals->killed_monsters.Get(*second.machine), first.globals->killed_monsters.Get(*first.machine));
    EXPECT_EQ(second.CountLiving(), first.CountLiving());
    EXPECT_NEAR(second.running->GetTime(), first.running->GetTime(), 1e-5);

    // the player, a monster, a door, and something to pick up
    for (const std::string_view classname : {"player", "monster_army", "door", "item_health"})
    {
      const std::int32_t entity = first.Find(classname);
      ASSERT_NE(entity, 0) << classname;
      EXPECT_EQ(second.Find(classname), entity) << classname;

      const LevelVector a = first.fields->origin.Get(*first.machine, entity);
      const LevelVector b = second.fields->origin.Get(*second.machine, entity);
      for (std::size_t i = 0; i < a.size(); i++) { EXPECT_NEAR(b[i], a[i], 0.01f) << classname << " " << i; }
    }
  }
} // namespace
