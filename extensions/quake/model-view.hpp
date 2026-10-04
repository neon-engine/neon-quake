#ifndef QUAKE_MODEL_VIEW_HPP
#define QUAKE_MODEL_VIEW_HPP

#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include <neon/extension/neon-extension.hpp>

#include "formats/mdl-file.hpp"
#include "formats/mdl-mesh.hpp"
#include "game-data.hpp"

namespace quake
{
  /// Shows the models of the game, `progs/*.mdl`, on entities of the
  /// engine: a monster, a weapon, an item, the flame of a torch.
  ///
  /// A model is read from the data the first time it is asked for and kept,
  /// with its skins made known to the renderer as pictures. An entity shows
  /// one frame of one model with one skin, as the game code says with its
  /// fields `model`, `frame`, and `skin`. A frame places every vertex anew,
  /// so showing another frame hands the engine a mesh again; the game code
  /// does so ten times in a second at most.
  ///
  /// A frame that is a group plays by itself, see Update().
  ///
  /// A model is turned from the space of the game into that of the engine
  /// on the way, see QuakeSpace: what points forward in the game, along x,
  /// points along x in the engine too, so the yaw of the game is the yaw of
  /// the entity as it is. Whoever owns the entity places and turns it.
  class ModelView
  {
    /// A model as it was read, and what is made of it once for every frame.
    struct Model
    {
      MdlFile file;
      MdlMesh mesh;

      /// The triangles, wound the way the engine wants them.
      std::vector<std::uint32_t> indices;

      /// The path a material reads each skin by. Empty for a skin the
      /// renderer did not take.
      std::vector<std::string> skins;
    };

    /// What an entity shows.
    struct Shown
    {
      const Model *model = nullptr;
      std::size_t frame = 0;
      std::size_t pose = 0;
      std::size_t skin = 0;
      float light = 1.0f;
    };

    // The models by their names. One that cannot be read is kept as nothing,
    // so that it is read, and said, once.
    std::map<std::string, std::unique_ptr<Model>> _models;

    std::map<neon::extension::Entity, Shown> _shown;

    // the time Update() was last told, by which a group picks its pose
    double _time = 0.0;

    /// The model of a name, read now when it was not yet. Null when the
    /// data has none of the name or it cannot be read.
    const Model *Find(const neon::extension::World &world, const GameData &data, const std::string &name);

    /// Which pose of a frame is shown at the time that is: the one of a
    /// frame that is no group, or the one of a group whose turn it is.
    [[nodiscard]] std::size_t ChoosePose(const Model &model, std::size_t frame) const;

    /// Hands the engine the mesh of what an entity shows.
    static bool ShowPose(const neon::extension::World &world, neon::extension::Entity entity, const Shown &shown);

  public:
    /// Has an entity show a frame of a model by its name, such as
    /// `progs/player.mdl`, with one of its skins. The entity is given what
    /// draws it the first time. A frame or skin the model does not have
    /// shows the first one, as the original does. `light` is how bright the
    /// model is where it stands, 1 for as its skin is.
    ///
    /// Returns false, and shows nothing new, when there is no such model.
    bool Show(
      const neon::extension::World &world,
      const GameData &data,
      neon::extension::Entity entity,
      const std::string &name,
      std::int32_t frame,
      std::int32_t skin,
      float light = 1.0f);

    /// The effects the model an entity shows was made with, a bit for each,
    /// see `MdlHeader::flags`. Zero for an entity that shows none.
    [[nodiscard]] std::uint32_t GetFlags(neon::extension::Entity entity) const;

    /// No longer keeps track of an entity, which is gone or shows no model
    /// of this kind any more. Its mesh is whoever owns the entity's to
    /// remove.
    void Forget(neon::extension::Entity entity);

    /// Forgets every entity, when a level ends. The models are kept.
    void Clear();

    /// Lets the groups play: an entity that shows a frame that is a group
    /// gets the pose whose turn it is at `time`, in seconds.
    void Update(const neon::extension::World &world, double time);
  };
} // quake

#endif //QUAKE_MODEL_VIEW_HPP
