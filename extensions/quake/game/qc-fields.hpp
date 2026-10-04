#ifndef QUAKE_QC_FIELDS_HPP
#define QUAKE_QC_FIELDS_HPP

#include <string_view>
#include <vector>

#include "formats/progs-definition.hpp"
#include "formats/progs.hpp"
#include "qc-field.hpp"

namespace quake
{
  /// The fields of an entity that the engine of the original works with,
  /// each found by its name in a program: where an entity is and how it
  /// moves, what it looks like, when it thinks, and what a player carries.
  /// The game code has many more of its own, which only it reads.
  ///
  /// As with `QcGlobals`, a field the program lacks reads as zero and is not
  /// written.
  ///
  /// ```
  /// const QcFields fields(machine.GetProgs());
  /// const std::array<float, 3> place = fields.origin.Get(machine, entity);
  /// fields.health.Set(machine, entity, 100.0f);
  /// ```
  struct QcFields
  {
    using Float = QcField<ProgsType::Float>;
    using Vector = QcField<ProgsType::Vector>;
    using String = QcField<ProgsType::String>;
    using Entity = QcField<ProgsType::Entity>;
    using Function = QcField<ProgsType::Function>;

    // The model, and the box around the entity in the world.
    Float modelindex;
    Vector absmin;
    Vector absmax;

    /// The clock of its own that what pushes has: a door, a lift.
    Float ltime;

    // How it moves and what it stops, numbers of `QcMoveType` and `QcSolid`.
    Float movetype;
    Float solid;

    // Where it is and where it goes.
    Vector origin;
    Vector oldorigin;
    Vector velocity;
    Vector angles;
    Vector avelocity;
    Vector punchangle;

    // What it is and looks like.
    String classname;
    String model;
    Float frame;
    Float skin;
    Float effects;

    // Its size around its origin.
    Vector mins;
    Vector maxs;
    Vector size;

    // The functions the engine calls for it, and when it thinks next.
    Function touch;
    Function use;
    Function think;
    Function blocked;
    Float nextthink;

    /// What it stands on.
    Entity groundentity;

    // What a player has and shows.
    Float health;
    Float frags;
    Float weapon;
    String weaponmodel;
    Float weaponframe;
    Float currentammo;
    Float ammo_shells;
    Float ammo_nails;
    Float ammo_rockets;
    Float ammo_cells;
    Float items;

    Float takedamage;

    /// The next entity of a list a builtin made, such as `findradius`.
    Entity chain;

    Float deadflag;

    /// Where the eyes are, from the origin.
    Vector view_ofs;

    // What a player presses.
    Float button0;
    Float button1;
    Float button2;
    Float impulse;

    // Where a player looks.
    Float fixangle;
    Vector v_angle;
    Float idealpitch;

    String netname;
    Entity enemy;
    Float flags;
    Float colormap;
    Float team;
    Float max_health;
    Float teleport_time;
    Float armortype;
    Float armorvalue;
    Float waterlevel;
    Float watertype;

    // What a monster turns and walks towards.
    Float ideal_yaw;
    Float yaw_speed;
    Entity aiment;
    Entity goalentity;

    // What the text of a level gives an entity.
    Float spawnflags;
    String target;
    String targetname;

    // The damage a player took in a frame, and from whom.
    Float dmg_take;
    Float dmg_save;
    Entity dmg_inflictor;

    Entity owner;
    Vector movedir;
    String message;
    Float sounds;
    String noise;
    String noise1;
    String noise2;
    String noise3;

    /// The names of those the program does not have, or has with another
    /// type.
    std::vector<std::string_view> missing;

    /// Looks every one up in a program.
    explicit QcFields(const Progs &progs);

    /// Whether those are found without which no level is handed to the game
    /// code or runs: `classname`, `spawnflags`, `movetype`, `ltime`,
    /// `origin`, `angles`, `velocity`, `avelocity`, `nextthink`, and
    /// `think`.
    [[nodiscard]] bool HasEssentials() const;
  };
} // quake

#endif //QUAKE_QC_FIELDS_HPP
