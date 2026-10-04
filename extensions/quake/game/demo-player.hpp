#ifndef QUAKE_DEMO_PLAYER_HPP
#define QUAKE_DEMO_PLAYER_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "demo-client-data.hpp"
#include "demo-entity-state.hpp"
#include "demo-entity.hpp"
#include "demo-file.hpp"
#include "demo-intermission.hpp"
#include "demo-listener.hpp"
#include "demo-message-handler.hpp"
#include "demo-message-reader.hpp"
#include "demo-score.hpp"
#include "demo-server-info.hpp"
#include "demo-static-sound.hpp"
#include "level-vector.hpp"
#include "player-stats.hpp"

namespace quake
{
  /// Plays a recorded game: the client of the original, as far as showing
  /// a recording needs one. It keeps what a client keeps of what a server
  /// sent, and a clock, and says for the time of the clock what there is
  /// to show.
  ///
  /// A running game needs none of this, as there the host shows the
  /// entities of the game code themselves. A recording has no game code:
  /// only what a server once sent a client.
  ///
  /// ```
  /// DemoPlayer player(listener);
  /// std::string problem;
  /// if (!player.Open(bytes_of_demo1, problem)) { return; }
  ///
  /// // every frame
  /// player.Advance(frame_seconds);
  /// for (const DemoEntity &entity : player.GetEntities())
  /// {
  ///   Show(entity.number, player.GetModelName(entity.state.model), entity.state);
  /// }
  /// if (player.IsOver()) { /* the next recording */ }
  /// ```
  ///
  /// What happens at a moment, a sound or an explosion, is told to the
  /// listener from within Advance(). The first thing told is that a level
  /// began, with which the host loads the map.
  ///
  /// The clock follows the original. A block of the recording is read
  /// when the clock is past the time of the server the block before it
  /// had, so that there is always a block ahead of the clock, and what
  /// moves is put between the last two blocks. While the client is still
  /// joining the level, block after block is read at once.
  class DemoPlayer final : DemoMessageHandler
  {
  public:
    /// How many numbers of the status bar a client keeps.
    static constexpr std::size_t stat_count = 32;

    /// How many styles of light there may be.
    static constexpr std::size_t light_style_count = 256;

    // Which number of the status bar is which.
    static constexpr std::size_t stat_health = 0;
    static constexpr std::size_t stat_weapon_model = 2;
    static constexpr std::size_t stat_ammo = 3;
    static constexpr std::size_t stat_armor = 4;
    static constexpr std::size_t stat_weapon_frame = 5;
    static constexpr std::size_t stat_shells = 6;
    static constexpr std::size_t stat_nails = 7;
    static constexpr std::size_t stat_rockets = 8;
    static constexpr std::size_t stat_cells = 9;
    static constexpr std::size_t stat_active_weapon = 10;
    static constexpr std::size_t stat_total_secrets = 11;
    static constexpr std::size_t stat_total_monsters = 12;
    static constexpr std::size_t stat_secrets = 13;
    static constexpr std::size_t stat_monsters = 14;

  private:
    /// What is kept of one entity of the server.
    struct Slot
    {
      /// What the entity is when an update says nothing else.
      DemoEntityState baseline{};

      /// What the last update made of it. Its origin and angles are those
      /// that are shown. A model of zero is an entity that is not shown.
      DemoEntityState state{};

      /// Where the last two updates have it, the newer one first.
      std::array<LevelVector, 2> origins{};
      std::array<LevelVector, 2> angles{};

      /// The time of the server of the last update, if there was one.
      float message_time = 0.0f;
      bool was_updated = false;

      /// Whether there is no update before the last to move it from.
      bool is_new = false;

      bool moves_in_steps = false;
      float frame_finish_time = 0.0f;

      // An entity that moves in steps is moved from the step before to
      // the last one, from the moment on the clock the last one came.
      LevelVector step_from_origin{};
      LevelVector step_from_angles{};
      LevelVector step_to_origin{};
      LevelVector step_to_angles{};
      float step_time = 0.0f;
    };

    DemoListener &_listener;

    /// The bytes of the recording, which the blocks of the file are in.
    std::vector<std::uint8_t> _bytes;
    DemoFile _file;
    DemoMessageReader _reader;
    std::size_t _next_block = 0;

    bool _is_open = false;
    bool _is_over = false;
    std::string _problem;

    // The clock, and the times of the server of the last two blocks, the
    // newer one first.
    float _time = 0.0f;
    std::array<float, 2> _message_times{};

    /// How far the client is with joining the level.
    std::int32_t _signon = 0;

    // What the last level said of itself.
    bool _has_level = false;
    DemoServerInfo _info;

    std::vector<Slot> _slots;
    std::vector<DemoEntity> _entities;
    std::vector<DemoEntityState> _static_entities;
    std::vector<DemoStaticSound> _static_sounds;

    // The view: where the one who recorded looked with the last two
    // blocks, the newer one first, and between them for the clock.
    std::int32_t _view_entity = 0;
    std::array<LevelVector, 2> _block_view_angles{};
    LevelVector _view_angles{};

    DemoClientData _client_data;
    std::array<std::int32_t, stat_count> _stats{};
    std::array<std::string, light_style_count> _light_styles;
    std::vector<DemoScore> _scores;

    bool _is_paused = false;
    DemoIntermission _intermission = DemoIntermission::None;
    float _completed_time = 0.0f;

    /// Forgets everything of a level.
    void ClearLevel();

    /// The slot of an entity, made when there is none yet.
    Slot &SlotOf(std::int32_t entity);

    /// How far the clock is between the last two blocks, from 0 to 1. A
    /// clock that ran away from them is put back, as in the original.
    float LerpPoint();

    /// Works out what is shown for the clock.
    void Relink();

    /// Starts one of the screens over the level.
    void StartIntermission(DemoIntermission intermission);

    // The messages, as DemoMessageReader hands them over. The angles a
    // server sets the view to are not among them: where the one who
    // recorded looked comes with every block, and wins, as in the original.

    void HandleServerInfo(const DemoServerInfo &info) override;

    void HandleTime(float seconds) override;

    void HandleSignon(std::int32_t stage) override;

    void HandlePause(bool is_paused) override;

    void HandleDisconnect() override;

    void HandleBaseline(std::int32_t entity, const DemoEntityState &state) override;

    void HandleEntityUpdate(const DemoEntityUpdate &update) override;

    void HandleStaticEntity(const DemoEntityState &state) override;

    void HandleViewEntity(std::int32_t entity) override;

    void HandleClientData(const DemoClientData &data) override;

    void HandleStat(std::int32_t stat, std::int32_t value) override;

    void HandleDamage(const DemoDamage &damage) override;

    void HandleBonusFlash() override;

    void HandleSound(const DemoSound &sound) override;

    void HandleStopSound(std::int32_t entity, std::int32_t channel) override;

    void HandleStaticSound(const DemoStaticSound &sound) override;

    void HandleMusicTrack(std::int32_t track, std::int32_t loop_track) override;

    void HandlePointEffect(const TempEntityPoint &effect) override;

    void HandleColoredExplosion(const TempEntityExplosion &explosion) override;

    void HandleBeamEffect(const TempEntityBeam &beam) override;

    void HandleParticles(const DemoParticles &particles) override;

    void HandleLightStyle(std::int32_t style, std::string_view text) override;

    void HandleSkybox(std::string_view name) override;

    void HandleFog(const DemoFog &fog) override;

    void HandlePrint(std::string_view text) override;

    void HandleCenterPrint(std::string_view text) override;

    void HandleStuffText(std::string_view text) override;

    void HandlePlayerName(std::int32_t player, std::string_view name) override;

    void HandlePlayerFrags(std::int32_t player, std::int32_t frags) override;

    void HandlePlayerColors(std::int32_t player, std::int32_t colors) override;

    void HandleKilledMonster() override;

    void HandleFoundSecret() override;

    void HandleIntermission() override;

    void HandleFinale(std::string_view text) override;

    void HandleCutscene(std::string_view text) override;

    void HandleSellScreen() override;

  public:
    /// How far a client is with joining a level when it has joined: the
    /// first update of an entity came.
    static constexpr std::int32_t signed_on = 4;

    /// The listener has to outlive this.
    explicit DemoPlayer(DemoListener &listener);

    DemoPlayer(const DemoPlayer &) = delete;

    DemoPlayer &operator=(const DemoPlayer &) = delete;

    /// Takes the bytes of a `.dem` to play, in place of what it played
    /// before. The bytes are copied. Nothing is read of the messages yet,
    /// and the listener is not told anything: that is for Advance().
    ///
    /// False, with the reason in `problem`, for bytes `DemoFile` refuses.
    /// Nothing is open then.
    bool Open(std::span<const std::uint8_t> bytes, std::string &problem);

    /// Forgets the recording and everything of it.
    void Close();

    /// Moves the clock on by seconds, reads every block whose time has
    /// come, telling the listener of what happens in them, and works out
    /// what is shown now. The first call reads all the blocks with which
    /// a client joins the level.
    ///
    /// Nothing happens when nothing is open, or the recording is over.
    void Advance(float seconds);

    // How the playing goes.

    [[nodiscard]] bool IsOpen() const;

    /// Whether the recording is at its end: the server went, the file has
    /// no block left, or a message could not be read, see HasFailed().
    /// What was shown last can still be asked for.
    [[nodiscard]] bool IsOver() const;

    /// Whether the recording ended for a message that could not be read.
    [[nodiscard]] bool HasFailed() const;

    /// What could not be read. Empty when nothing failed.
    [[nodiscard]] const std::string &GetProblem() const;

    /// The clock: the time of the server that is shown, in seconds since
    /// the level began.
    [[nodiscard]] float GetTime() const;

    /// Whether the client has joined the level, so that there are
    /// entities to show.
    [[nodiscard]] bool IsSignedOn() const;

    [[nodiscard]] bool IsPaused() const;

    /// Which screen is over the level, and the time of the clock it came.
    [[nodiscard]] DemoIntermission GetIntermission() const;

    [[nodiscard]] float GetCompletedTime() const;

    // The level.

    /// Whether a server said which level this is.
    [[nodiscard]] bool HasLevel() const;

    /// Everything the server said of the level at its start.
    [[nodiscard]] const DemoServerInfo &GetServerInfo() const;

    /// The name of the level for a reader, in letters of the console.
    [[nodiscard]] const std::string &GetLevelName() const;

    /// The name of the file of the level, such as `maps/e1m1.bsp`. Empty
    /// when there is no level.
    [[nodiscard]] std::string_view GetMapModelName() const;

    /// The names of the models and of the sounds, by index. Index 0 is
    /// none.
    [[nodiscard]] std::span<const std::string> GetModelNames() const;

    [[nodiscard]] std::span<const std::string> GetSoundNames() const;

    /// The name of one model or sound. Empty for an index there is no
    /// name for.
    [[nodiscard]] std::string_view GetModelName(std::int32_t index) const;

    [[nodiscard]] std::string_view GetSoundName(std::int32_t index) const;

    /// How the lights of a style flicker, a letter for each tenth of a
    /// second. Empty for a style the server said nothing of.
    [[nodiscard]] std::string_view GetLightStyle(std::size_t style) const;

    // What is shown.

    /// The entities that are shown now, each where it is for the clock:
    /// between its last two updates, or at the last one when it came
    /// from too far to have moved there. The level itself is not one of
    /// them. The entity the player sees through is, and a host that shows
    /// the view from its eyes leaves it out.
    ///
    /// An entity the last packet had no update for is gone, and is not
    /// here: a host takes away what it showed for a number that is here
    /// no more. The list holds until the next call of Advance().
    [[nodiscard]] std::span<const DemoEntity> GetEntities() const;

    /// One of the entities that are shown, by its number. Null when it is
    /// not shown.
    [[nodiscard]] const DemoEntity *FindEntity(std::int32_t number) const;

    /// The entities that never change, in the order the server made them.
    /// The list only grows while a level lasts.
    [[nodiscard]] std::span<const DemoEntityState> GetStaticEntities() const;

    /// The sounds that go on for as long as the level does. The list only
    /// grows while a level lasts.
    [[nodiscard]] std::span<const DemoStaticSound> GetStaticSounds() const;

    // The view.

    /// The entity the player sees through. The eyes are at its origin,
    /// higher by the view height of GetClientData().
    [[nodiscard]] std::int32_t GetViewEntity() const;

    /// Where the one who recorded looked, for the clock: pitch, yaw, and
    /// roll in degrees, between those of the last two blocks.
    [[nodiscard]] const LevelVector &GetViewAngles() const;

    /// What the server said of the player last: the view height, the
    /// kick of the view, and more.
    [[nodiscard]] const DemoClientData &GetClientData() const;

    // The status bar.

    /// One number of the status bar. Zero for one there is not.
    [[nodiscard]] std::int32_t GetStat(std::size_t stat) const;

    /// The numbers of the status bar as the status bar and the screens
    /// between levels take them. The time is that of the clock. The share
    /// the armour takes is not known to a client and is zero.
    [[nodiscard]] PlayerStats GetStats() const;

    /// The model of the weapon in the hands, as an index into the names
    /// of the models, and its frame. A model of zero is no weapon.
    [[nodiscard]] std::int32_t GetWeaponModel() const;

    [[nodiscard]] std::int32_t GetWeaponFrame() const;

    /// The players of the scoreboard, as many as the server is for.
    [[nodiscard]] std::span<const DemoScore> GetScores() const;
  };
} // quake

#endif //QUAKE_DEMO_PLAYER_HPP
