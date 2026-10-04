#ifndef QUAKE_LEVEL_COLLISION_HPP
#define QUAKE_LEVEL_COLLISION_HPP

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "formats/bsp-collision.hpp"
#include "formats/bsp-contents.hpp"
#include "formats/bsp-file.hpp"
#include "formats/qc-machine.hpp"
#include "level-box.hpp"
#include "level-trace-kind.hpp"
#include "level-trace-result.hpp"
#include "level-vector.hpp"
#include "qc-fields.hpp"

namespace quake
{
  /// What the entities of the game code collide with: the level, the parts
  /// of it that move, and each other.
  ///
  /// `BspCollision` moves a box through one model of a level. This moves
  /// one through everything, as the engine of the original does for
  /// `traceline`, for what falls and for what walks: first through the
  /// world, then through every entity that stops things. A door or a lift
  /// stops with the shape of its model, where the entity stands. Anything
  /// else stops with its box, `mins` to `maxs` around its `origin`.
  ///
  /// It reads the entities from the machine at the time it is asked, so
  /// nothing has to be told when one moves. The original keeps the
  /// entities in a tree of areas to find the near ones fast; here every
  /// entity is looked at, which gives the same answers.
  ///
  /// Everything is in the units and axes of the game, with Z pointing up.
  /// A model is never turned, see `BspCollision`.
  class LevelCollision final
  {
    QcMachine &_machine;
    QcFields _fields;

    /// What collides of each model of the level. Model 0 is the world.
    std::vector<BspCollision> _models;

    /// The model an entity stops things with the shape of: the world for
    /// entity 0, and model N for one whose `model` is `*N`. Nothing for
    /// any other.
    [[nodiscard]] const BspCollision *FindModel(std::int32_t entity) const;

    /// Moves a box against one entity alone.
    [[nodiscard]] LevelTraceResult ClipToEntity(
      std::int32_t entity,
      const LevelVector &start,
      const LevelVector &mins,
      const LevelVector &maxs,
      const LevelVector &end) const;

  public:
    /// What Trace() takes for the entity to pass when there is none.
    static constexpr std::int32_t no_entity = -1;

    /// By how much the box that hits a monster is larger to every side for
    /// a move of the kind `Missile`.
    static constexpr float missile_margin = 15.0f;

    /// By how much the box an entity is found by is larger than the entity
    /// to every side, and to the four sides on the ground for something to
    /// pick up.
    static constexpr float box_margin = 1.0f;
    static constexpr float item_margin = 15.0f;

    /// The machine has to outlive this. Until Build() gave it a level, the
    /// world is empty space.
    explicit LevelCollision(QcMachine &machine);

    /// Takes what collides of every model of a level, and keeps nothing of
    /// the file. Returns false, says why in `error`, and stays as it was
    /// when a model is refused, see `BspCollision::Build`.
    bool Build(const BspFile &file, std::string &error);

    /// How many models of the level there are, the world among them.
    [[nodiscard]] std::size_t GetModelCount() const;

    [[nodiscard]] QcMachine &GetMachine() const;

    [[nodiscard]] const QcFields &GetFields() const;

    /// Moves a box from `start` to `end` and says how far it came and what
    /// stopped it. `mins` and `maxs` are the corners of the box around the
    /// place that is moved, both zero for a point.
    ///
    /// The move goes through the world first, then through every entity
    /// that is not free and whose `solid` is neither `Not` nor `Trigger`,
    /// with the rules of the original:
    ///
    /// - `pass_entity` is left out, and so is what it owns and what owns
    ///   it, by the field `owner`: a missile does not hit who shot it.
    /// - When `pass_entity` has a size, an entity without one is left out.
    /// - Of the kind `NoMonsters` only what is a part of the level stops
    ///   the move.
    /// - Of the kind `Missile` an entity with the flag `Monster` is hit in
    ///   a larger box.
    ///
    /// `pass_entity` is `no_entity` for none. The world, entity 0, counts
    /// as one that is passed, as in the original, where the game code
    /// cannot name none: a move that passes the world leaves out every
    /// entity without an owner, since the world is what owns those.
    ///
    /// An entity whose `solid` is `Bsp` stops with the shape of the model
    /// its `model` names, `*N`, at its origin. Should it name no model of
    /// the level, it stops with its box.
    [[nodiscard]] LevelTraceResult Trace(
      const LevelVector &start,
      const LevelVector &mins,
      const LevelVector &maxs,
      const LevelVector &end,
      LevelTraceKind kind,
      std::int32_t pass_entity) const;

    /// What fills a place of the world. The entities are not asked, as in
    /// the original: the water of a part of the level that moves is none.
    [[nodiscard]] BspContents GetPointContents(const LevelVector &point) const;

    /// Whether an entity stands in what is solid, where it is now: in a
    /// wall, in a door, or in the box of another entity.
    [[nodiscard]] bool TestPosition(std::int32_t entity) const;

    /// The box an entity is found by: its `mins` and `maxs` around its
    /// `origin`, larger by `box_margin` to every side. Something to pick
    /// up, with the flag `Item`, is instead larger by `item_margin` to the
    /// four sides on the ground, so that it is touched from further away.
    [[nodiscard]] LevelBox GetBox(std::int32_t entity) const;

    /// Writes GetBox() into the fields `absmin` and `absmax` of an entity,
    /// which game code reads: a door finds the doors that touch it by
    /// them. A host calls it whenever it has placed or sized an entity, as
    /// the original does in `setorigin`, `setsize`, and `setmodel`.
    /// Nothing here reads the two fields.
    void Link(std::int32_t entity) const;
  };
} // quake

#endif //QUAKE_LEVEL_COLLISION_HPP
