#ifndef QUAKE_GAME_CODE_HPP
#define QUAKE_GAME_CODE_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include <neon/extension/neon-extension.hpp>

#include "formats/bsp-file.hpp"
#include "formats/qc-machine.hpp"
#include "game-data.hpp"
#include "hud-view.hpp"
#include "game/frame-limit-delay.hpp"
#include "game/level-collision.hpp"
#include "game/level-physics.hpp"
#include "game/level-running.hpp"
#include "game/level-stepping.hpp"
#include "game/level-axes.hpp"
#include "game/level-touching.hpp"
#include "game/menu.hpp"
#include "game/particle-system.hpp"
#include "game/player-command.hpp"
#include "game/player-movement.hpp"
#include "game/qc-core-builtins.hpp"
#include "game/qc-fields.hpp"
#include "game/qc-globals.hpp"
#include "game/qc-host.hpp"
#include "game/qc-world-builtins.hpp"
#include "game/saved-game.hpp"
#include "game/server-message-listener.hpp"
#include "game/server-message-reader.hpp"
#include "game/status-bar.hpp"
#include "game/view-tint.hpp"
#include "level-view.hpp"
#include "model-view.hpp"
#include "demo-show.hpp"
#include "light-view.hpp"
#include "particle-view.hpp"
#include "sound-view.hpp"
#include "sprite-view.hpp"

namespace quake
{
  /// The game code of the game, `progs.dat`, run for a level that is shown:
  /// what the engine of the original was to it, on Neon Engine.
  ///
  /// The game code owns the game. It says what every entity of a level is,
  /// where it is, and which model it shows, in the fields of its entities.
  /// This reads those fields after every step and makes the world of the
  /// engine agree: a door stands where its `origin` says, a monster shows
  /// the frame its `frame` says.
  ///
  /// What the game code asks that needs the walls of a level, a line traced
  /// through it, a monster that walks, an item that falls, is answered by
  /// the collision of the game itself, `LevelCollision` and what is built
  /// on it, with the hulls of the level as the original has them.
  ///
  /// The player is an entity of the game code as every other, and is moved
  /// as the original moves one: steered by what is pressed, `PlayerMovement`,
  /// and walked, swum, and dropped through the level by that collision.
  /// That is what gives the game its pace. The engine is left the eyes: a
  /// camera that is put where the player looks from, in every frame that
  /// is drawn.
  class GameCode final : public QcHost, public ServerMessageListener
  {
    using Vector = std::array<float, 3>;

    /// What there is of the game code while a level runs, in the order it
    /// is made.
    struct Level
    {
      BspFile file;
      QcMachine machine;
      QcGlobals globals;
      QcFields fields;
      QcCoreBuiltins builtins;
      LevelRunning running;

      // what the entities collide with, and what moves them through it
      LevelCollision collision;
      LevelTouching touching;
      LevelStepping stepping;
      LevelPhysics physics;
      QcWorldBuiltins world_builtins;

      // what steers the player
      PlayerMovement movement;

      // the sparks, the blood, and the smoke that fly
      ParticleSystem particles;

      Level(Progs progs, QcHost &host);
    };

    /// What the engine shows for an entity of the game code.
    struct Shown
    {
      neon::extension::Entity entity = 0;

      /// The model it shows, as the game code names it. Empty for nothing.
      std::string model;

      /// Whether the entity of the engine is a model of the level, which the
      /// level view made, and not one made here.
      bool is_part = false;

      /// Whether it is a model of the kind ModelView shows.
      bool is_alias = false;

      /// The skin its entity was made to show.
      std::int32_t skin = 0;

      /// Whether it is a sprite, which SpriteView shows and turns.
      bool is_sprite = false;

      Vector origin{};
      Vector angles{};
      bool is_placed = false;

      /// Where it was shown when the game code last moved it, and how many
      /// steps ago that was. Until `glide_steps` have passed it is shown on
      /// its way from there to where it is, see Interpolate().
      Vector from_origin{};
      Vector from_angles{};
      int steps_since = 0;
      int glide_steps = 1;

      /// Whether it is shown where it is, and nothing is left to glide.
      bool is_settled = true;

      /// Whether it turns where it lies, as a weapon to pick up does.
      bool spins = false;
    };

    std::unique_ptr<Level> _level;
    std::vector<Shown> _shown;

    // What is shown on top of the world: the status bar, and what draws it.
    StatusBar _status_bar;
    HudView _hud;

    // The menu of the game, what it sets, and whether the mouse looks the
    // other way up and down.
    Menu _menu;
    MenuOptions _options{.always_run = true};
    double _menu_time = 0.0;

    // whether the game was started before: the first time, its menu is open,
    // as the original greets a player with it
    bool _was_started = false;

    /// How many games can be kept, as the menu of the original has places.
    static constexpr std::size_t save_slots = 12;

    // The games that were saved, each as the text the original writes for
    // one, and what the menu calls each. Each is a file of the player's as
    // well, which the next run of the game reads.
    std::array<std::string, save_slots> _saves;
    std::array<std::string, save_slots> _save_names;

    // The saved game that is being gone back to, while its level starts.
    std::unique_ptr<SavedGame> _loading;

    // the numbers the player came into the level with, as the game code had
    // them when the player came in: what a saved game keeps of the player
    std::array<float, 16> _came_with{};

    /// Where the saved games and the settings of the menu are kept: files
    /// of the player's, which the next run of the game finds.
    static constexpr std::string_view saves_folder = "user://saves/";
    static constexpr std::string_view options_file = "user://quake.cfg";

    /// The file of a place of the menu.
    [[nodiscard]] static std::string PathOfSave(std::size_t slot);

    /// Reads the saved games a run before left, for the menu to offer.
    void ReadSaves();

    /// Reads and writes what the menu sets.
    void ReadOptions();

    void WriteOptions();

    /// Makes what the options say so: how loud the sounds and the music
    /// are, and how bright the game is shown.
    void ApplyOptions();

    /// Reads the impulses the environment names for a run without a
    /// player, see SteerPlayer().
    void ReadScriptedImpulses();

    /// Reads how the window is shown now and which sizes its display
    /// offers, and makes what the player never chose what it is now.
    void ReadVideo();

    /// Writes the video settings where the engine reads them before it
    /// shows its window, so that the game comes up as it was left and is
    /// not seen to change, see VideoSettingsFile.
    void WriteVideoSettings();

    /// Shows the window as the video settings say: its mode, its size, and
    /// whether a frame waits for the screen. Only what differs from how it
    /// is shown is changed. The frame limit is asked for, and made so a
    /// second after it was asked for last or when its slider is left, see
    /// FrameLimitDelay.
    void ApplyVideo();

    /// The frame limit the options say, as the engine takes it: 0 for
    /// none, or a number from 30 to 300.
    [[nodiscard]] static int FrameLimitOf(const MenuOptions &options);

    /// Adds the pictures of the menu to a list, while it is open.
    void AddMenu(std::vector<HudPicture> &pictures);

    /// Plays the recording behind the menu on by a frame, and shows the
    /// menu over it.
    void ShowTitle();

    /// Ends the recording that plays and starts the one whose turn it is.
    /// Returns false when none of them can be played.
    bool PlayNextDemo();

    /// Lights what the game code has flash and glow in this step: the shot
    /// of a gun, a rocket, who carries a strong or a weak light.
    void ShowLights();

    /// Keeps the game as it is in a place of the menu.
    void Save(std::size_t slot);

    /// Goes back to the game kept in a place of the menu.
    void Load(std::size_t slot);

    /// Does what the menu asks for: plays its sounds, takes its settings,
    /// starts a game, leaves.
    void Act(const std::vector<MenuAction> &actions);

    /// What the menu is told of the game: whether one runs, and what is
    /// saved.
    [[nodiscard]] MenuGame DescribeGame();

    // the colour laid over what the player sees: a hit, a pickup, water
    ViewTint _tint;

    /// A line the game code printed for the player, and when.
    struct Message
    {
      std::string text;
      double at = 0.0;
    };

    // The last lines the game code printed, shown for a few seconds in the
    // upper left; and what it put in the middle of the screen, and when.
    std::vector<Message> _messages;
    std::string _center_text;
    double _center_at = 0.0;

    /// A bolt of lightning between two places: the entity of the game code
    /// it comes from, the model it is made of, where it starts and ends,
    /// when it goes, and the entities of the engine that show its pieces.
    struct Beam
    {
      std::int32_t owner = 0;
      std::string model;
      Vector start{};
      Vector end{};
      double ends_at = 0.0;
      std::vector<neon::extension::Entity> pieces;
    };

    // the bolts that are there; one of an entity takes the place of the one
    // that entity had
    std::vector<Beam> _beams;

    /// For how long a bolt is there, in seconds, and how long a piece of it
    /// is, in units of the game, as in the original.
    static constexpr double beam_seconds = 0.2;
    static constexpr float beam_piece = 30.0f;

    /// Shows the pieces of a bolt from where it starts to where it ends.
    void ShowBeam(Beam &beam);

    /// Takes the pieces of a bolt out of the world.
    void HideBeam(Beam &beam);

    /// Takes away the bolts whose time is over, and has one that comes from
    /// the player start where the player is now.
    void UpdateBeams();

    // puts what the game code writes for the player together into messages
    ServerMessageReader _reader{*this};

    // The text the game code shows at the end of an episode, letter after
    // letter, when it started, and whether the picture of an end stands
    // over it. Empty for none.
    std::string _finale_text;
    double _finale_at = 0.0;
    bool _finale_has_picture = false;

    // whether the game code said that the level is over
    bool _is_over = false;

    // the time of the level at which it was over, while its counts are shown
    float _completed_time = -1.0f;

    /// For how long a line the game code printed is shown, in seconds, and
    /// how many are shown at once.
    static constexpr double message_seconds = 4.0;
    static constexpr std::size_t most_messages = 4;

    // what draws the particles of the level
    ParticleView _particle_view;

    // The recording that plays behind the menu until a game is started,
    // the names of those the data has, and whose turn it is next.
    std::unique_ptr<DemoShow> _demo;
    std::vector<std::string> _demo_names;
    std::size_t _next_demo = 0;

    // impulses the environment asked for, which are given one in a step
    std::vector<float> _scripted_impulses;

    // the sizes the display offers, and how the window is shown now
    std::vector<MenuSize> _display_sizes;
    int _shown_window_mode = 0;
    int _shown_window_width = 0;
    int _shown_window_height = 0;
    bool _shown_vertical_sync = true;

    // the frame limit that waits for its slider to be left alone, and the
    // time it is counted in: the seconds of every frame since the start
    FrameLimitDelay _frame_limit_delay;
    double _played_time = 0.0;

    // whether the camera is told the effect of a view in a liquid, and
    // whether it was told anything yet
    bool _is_under = false;
    bool _has_told_effects = false;

    // the lights of explosions, of shots, and of what glows as it flies
    LightView _lights;

    // the models of the level that an entity which went left behind for good
    std::set<std::size_t> _static_parts;

    // how many entities were made to show one of the game code, which
    // numbers their names
    std::uint64_t _made = 0;

    // the entity everything the game code shows stands under, so that it
    // goes with the level
    neon::extension::Entity _root = 0;

    // The level that runs, and the one the game code asked for, which is
    // gone to once the step it asked in is over. Empty for none.
    std::string _map;
    std::string _wanted_map;

    // The numbers the player came into the level with, which a player who
    // died starts it with again, and those the player leaves it with. Empty
    // for those of a new player.
    std::vector<float> _start_parms;
    std::vector<float> _wanted_parms;
    float _server_flags = 0.0f;


    // what the game is shown with, which outlive a level
    const neon::extension::World *_world = nullptr;
    const GameData *_data = nullptr;
    LevelView *_view = nullptr;
    ModelView *_models = nullptr;
    SoundView *_sounds = nullptr;
    SpriteView *_sprites = nullptr;

    NeonField _position_field{};
    NeonField _rotation_field{};

    // The entity of the scene the camera stands under, once it is there:
    // the eyes of the player.
    neon::extension::Entity _player = 0;

    // Where the player looks, in degrees as the game counts them: up and
    // down, where down is more, and around. It is the player's own, turned
    // in every frame that is drawn, and the game code is told in every step.
    float _view_pitch = 0.0f;
    float _view_yaw = 0.0f;

    // Where the eyes of the player were after the step before and after the
    // last, in the space of the game, between which they are shown; and how
    // high they are shown, which lags behind the step of a stair.
    Vector _eyes_before{};
    Vector _eyes{};
    float _shown_height = 0.0f;
    bool _has_eyes = false;

    // how long the frame that is drawn took, in seconds
    float _frame_time = 0.0f;

    // what the game code printed of a line that has not ended yet
    std::string _line;

    // The camera of the player, and the entity under it that shows the
    // weapon in the player's hands.
    neon::extension::Entity _camera = 0;
    neon::extension::Entity _weapon = 0;
    std::string _weapon_model;

    // What the player asked for since the last step: the number of a
    // weapon, which the game code is handed once.
    float _impulse = 0.0f;

    /// A thing a tour stops at: what it is, and where.
    struct TourStop
    {
      std::string name;
      std::int32_t entity = 0;
      Vector origin{};
    };

    // A tour of a level, for checking by eye that everything a level shows
    // looks as it should: asked for with the environment variable
    // QUAKE_TOUR, it flies the player to one of every kind of thing, a few
    // steps each, so that a run that saves those frames has a picture of
    // each. See README.md.
    bool _tours = false;
    std::vector<TourStop> _tour;
    std::vector<TourStop> _static_stops;
    std::int64_t _steps = 0;

    /// How many steps the level settles before a tour starts, and how many
    /// it stops at each thing.
    static constexpr std::int64_t tour_start = 30;
    static constexpr std::int64_t tour_stop_steps = 6;

    // how many failures of the game code were said already
    std::size_t _failures_said = 0;

    /// The entity of the game code the one player is, the first after the
    /// world, as in the original.
    static constexpr std::int32_t player_entity = 1;

    /// How fast the player asks to go, in units of the game a second:
    /// forward, sideways, and up or down in water. They are what the
    /// original asks for with its run key held; the game holds a player
    /// down to its own speed, 320.
    static constexpr float forward_speed = 400.0f;
    static constexpr float side_speed = 350.0f;
    static constexpr float up_speed = 200.0f;

    /// Degrees the view turns for what `look` gives, a pixel of the mouse.
    static constexpr float look_speed = 0.14f;

    /// How far the player looks up and down, as the original has it.
    static constexpr float most_pitch_up = -70.0f;
    static constexpr float most_pitch_down = 80.0f;

    /// How the view bobs as the player walks, as the original has it: how
    /// far for the speed, how long a stride takes in seconds, and what part
    /// of a stride goes up.
    static constexpr float bob_amount = 0.02f;
    static constexpr float bob_cycle = 0.6f;
    static constexpr float bob_up = 0.5f;

    /// How far the view leans into a sidestep, in degrees, the speed at
    /// which it leans all the way, and how far it lies over when the player
    /// is dead.
    static constexpr float roll_angle = 2.0f;
    static constexpr float roll_speed = 200.0f;
    static constexpr float dead_roll = 80.0f;

    /// How fast the eyes follow a step up a stair, in units a second, and
    /// how far they may lag behind it, as in the original.
    static constexpr float eye_rise_speed = 80.0f;
    static constexpr float most_eye_lag = 12.0f;

    /// The least light the weapon in the player's hands has, of 255, as in
    /// the original.
    static constexpr float least_weapon_light = 24.0f;

    /// How many steps of the world a stride of a monster is shown over. The
    /// game code moves a monster every tenth of a second.
    static constexpr double stride_time = 0.1;

    /// How far, in units of the game, something may move in a step and
    /// still be shown on its way: further is a jump to another place, as
    /// through a teleporter.
    static constexpr float glide_reach = 128.0f;

    // how long a step of the world is, as last told, in seconds
    float _step = 1.0f / 60.0f;

    /// What the game code is handed to go to the next weapon the player
    /// has, and to the one before.
    static constexpr float next_weapon_impulse = 10.0f;
    static constexpr float previous_weapon_impulse = 12.0f;

    /// Registers the builtins that need the level or the engine.
    void RegisterBuiltins();

    /// What `setmodel` does: the entity names a model and takes the size of
    /// one of the level.
    void SetModel(std::int32_t entity, std::int32_t name_offset, std::string_view name);

    /// What `makestatic` does: what the entity shows stays for the rest of
    /// the level, and the entity itself goes.
    void MakeStatic(std::int32_t entity);

    /// A name for the entity of the engine that shows an entity of the game
    /// code, which no other has or had.
    std::string MakeName(std::int32_t entity);

    /// Takes away what the engine shows for an entity.
    void Hide(Shown &shown);

    /// Makes what the engine shows for an entity agree with its fields.
    void Show(std::int32_t entity);

    /// Where an entity is shown at a moment between two steps, `blend` of
    /// the way from the last to the next: on its way from where it was to
    /// where the game code has it.
    void FindShownPlace(const Shown &shown, float blend, Vector &origin, Vector &angles) const;

    /// Puts the entity of the engine where an entity of the game code is
    /// shown.
    void Place(const Shown &shown, const Vector &origin, const Vector &angles) const;

    /// Leads the player from one kind of thing a level shows to the next,
    /// when a tour was asked for, and says in the log which is looked at in
    /// which step. True while it leads.
    bool LeadTour();

    /// Tells the game code what the player holds down and where the player
    /// looks, and steers the player by it for a step.
    void SteerPlayer(float dt);

    /// Where the eyes of the player are, in the space of the game.
    void FindEyes(Vector &eyes) const;

    /// Keeps where the eyes are after a step, and where they were before.
    void NoteEyes();

    /// Plays a sound of the game at a place of the game, as loud as it is.
    void PlaySoundAt(const std::string &name, const Vector &place);

    /// Shows what is on top of the world for the player: the status bar,
    /// the lines the game code printed, the words in the middle of the
    /// screen, or the counts of a level that is over.
    void ShowHud();

    /// Puts the camera of the scene where the player looks from, `blend`
    /// of the way between the last two steps, looking where the player
    /// looks.
    void ShowView(float blend);

    /// Shows the weapon the game code has in the player's hands, with the
    /// frame it is at, in front of the camera.
    void ShowWeapon();

    /// Whether the player is dead, or looks at a level that is over: the
    /// game code has the player as anything but walking then.
    [[nodiscard]] bool IsPlayerHeld();

    /// Takes away everything the game code shows, and the game code of the
    /// level itself.
    void Stop();

    /// Starts the game code for the level `_map`, which the level view
    /// shows already, with the numbers the player brings.
    bool Run(std::string &error);

    /// Goes to the level the game code asked for.
    void GoToWantedLevel();

    /// Says the failures of the game code that were not said yet.
    void SayFailures();

  public:
    // QcHost
    void PrintToAll(std::string_view text) override;

    void PrintToClient(std::int32_t client, std::string_view text) override;

    void PrintToCenter(std::int32_t client, std::string_view text) override;

    void Error(std::int32_t self, std::string_view text) override;

    void ObjectError(std::int32_t entity, std::string_view text) override;

    void EntityRemoved(std::int32_t entity) override;

    void ChangeLevel(std::string_view level) override;

    void LightStyleSet(std::int32_t style, std::string_view text) override;

    void ClientCommand(std::int32_t client, std::string_view text) override;

    void WriteMessage(QcMessageDestination destination, std::int32_t client, const QcMessageValue &value) override;

    // ServerMessageListener: what the game code tells the side of the
    // player, which is here too
    void PointEffect(const ServerMessageTarget &target, const TempEntityPoint &effect) override;

    void ColoredExplosion(const ServerMessageTarget &target, const TempEntityExplosion &explosion) override;

    void BeamEffect(const ServerMessageTarget &target, const TempEntityBeam &beam) override;

    void IntermissionStarted(const ServerMessageTarget &target) override;

    void FinaleStarted(const ServerMessageTarget &target, std::string_view text) override;

    void CutsceneStarted(const ServerMessageTarget &target, std::string_view text) override;

    void MusicTrackSet(const ServerMessageTarget &target, std::int32_t track, std::int32_t loop_track) override;

    void CenterTextPrinted(const ServerMessageTarget &target, std::string_view text) override;

    void TextPrinted(const ServerMessageTarget &target, std::string_view text) override;

    void ServerCommand(std::string_view text) override;

    /// Greets a player as the original does: with the menu, over the
    /// recordings the data names, one after the other, until a game is
    /// started from the menu. Returns false when the data has no recording
    /// that can be played; a level is then started in its place.
    bool StartTitle(
      const neon::extension::World &world,
      const GameData &data,
      LevelView &view,
      ModelView &models,
      SoundView &sounds,
      SpriteView &sprites);

    /// Starts the game code for a level the level view shows already: reads
    /// `progs.dat`, hands it the entities of the level, and lets the one
    /// player in. Returns false, and says why in `error`, when the data has
    /// no game code or it cannot be read; the level is then shown without.
    bool Start(
      const neon::extension::World &world,
      const GameData &data,
      LevelView &view,
      ModelView &models,
      SoundView &sounds,
      SpriteView &sprites,
      const std::string &map,
      std::string &error);

    /// Takes what the player did in this frame, once in every frame that
    /// is drawn: the view is turned by `look`, and the number of a weapon,
    /// or the wish for the next one or the one before, is kept for the game
    /// code's next step. `frame_time` is how long the frame took.
    void ReadInput(const neon::extension::World &world, float frame_time);

    /// Shows what moves on its way between two steps of the world, once in
    /// every frame that is drawn: `blend` is how far the frame lies between
    /// the last step and the next. It is what keeps the game fluid however
    /// many frames are drawn in a second. A monster walks in ten strides a
    /// second, as the game code moves it, and is shown gliding from one to
    /// the next.
    void Interpolate(const neon::extension::World &world, float blend);

    /// Whether a level runs.
    [[nodiscard]] bool IsRunning() const;

    /// Lets a step of time pass for the game: the player is read, the game
    /// code runs, and the world of the engine is made to agree.
    void Advance(const neon::extension::World &world, float dt);
  };
} // quake

#endif //QUAKE_GAME_CODE_HPP
