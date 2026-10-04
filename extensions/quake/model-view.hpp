#ifndef QUAKE_MODEL_VIEW_HPP
#define QUAKE_MODEL_VIEW_HPP

#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <utility>
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
  /// The game code changes the frame of a monster ten times in a second,
  /// which is how often the original drew another pose. Here a pose blends
  /// into the next over that tenth of a second, corner by corner, so that
  /// what moves does so in every frame that is drawn, however many there
  /// are in a second. `blend` of Show() says no for what must not: a pose
  /// that is not next to the one before, as when a model is put elsewhere.
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

      /// The corners of the poses that were shown, in the space of the
      /// engine, by the frame and the pose in it: made once, since a
      /// monster comes back to the same poses over and over.
      std::map<std::pair<std::size_t, std::size_t>, std::vector<neon::extension::Vertex>> poses;
    };

    /// What an entity shows.
    struct Shown
    {
      Model *model = nullptr;
      std::size_t frame = 0;
      std::size_t pose = 0;
      std::size_t skin = 0;
      float light = 1.0f;

      /// The corners the entity showed when it was last told another pose,
      /// and when that was: for a tenth of a second it shows what lies
      /// between those and the pose it has now.
      std::vector<neon::extension::Vertex> from;
      double changed_at = 0.0;
      bool is_blending = false;
    };

    // The models by their names. One that cannot be read is kept as nothing,
    // so that it is read, and said, once.
    // (a model is changed after it was found: the poses it has shown are
    // kept with it)
    std::map<std::string, std::unique_ptr<Model>> _models;

    std::map<neon::extension::Entity, Shown> _shown;

    // the time Update() was last told, by which a group picks its pose
    double _time = 0.0;

    /// How long a pose takes to become the next, in seconds: as long as the
    /// game code shows one.
    static constexpr double blend_time = 0.1;

    /// The model of a name, read now when it was not yet. Null when the
    /// data has none of the name or it cannot be read.
    Model *Find(const neon::extension::World &world, const GameData &data, const std::string &name);

    /// Which pose of a frame is shown at the time that is: the one of a
    /// frame that is no group, or the one of a group whose turn it is.
    [[nodiscard]] std::size_t ChoosePose(const Model &model, std::size_t frame) const;

    /// The corners of a pose of a model, made the first time it is asked
    /// for. Null when the model has no such pose.
    static const std::vector<neon::extension::Vertex> *FindCorners(Model &model, std::size_t frame, std::size_t pose);

    /// The corners an entity shows at the time that is: those of its pose,
    /// or what lies between the pose before and it while it blends. Empty
    /// when the model has no such pose.
    [[nodiscard]] std::vector<neon::extension::Vertex> MakeCorners(Shown &shown) const;

    /// Hands the engine the mesh of what an entity shows at the time that
    /// is.
    bool ShowPose(const neon::extension::World &world, neon::extension::Entity entity, Shown &shown) const;

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
      float light = 1.0f,
      bool blend = true);

    /// The effects the model an entity shows was made with, a bit for each,
    /// see `MdlHeader::flags`. Zero for an entity that shows none.
    [[nodiscard]] std::uint32_t GetFlags(neon::extension::Entity entity) const;

    /// No longer keeps track of an entity, which is gone or shows no model
    /// of this kind any more. Its mesh is whoever owns the entity's to
    /// remove.
    void Forget(neon::extension::Entity entity);

    /// Forgets every entity, when a level ends. The models are kept.
    void Clear();

    /// Lets the groups play and the poses blend, once in every frame that
    /// is drawn: an entity that shows a frame that is a group gets the pose
    /// whose turn it is at `time`, in seconds, and one that was told
    /// another pose a moment ago shows what lies between the two.
    void Update(const neon::extension::World &world, double time);
  };
} // quake

#endif //QUAKE_MODEL_VIEW_HPP
