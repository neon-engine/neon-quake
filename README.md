# neon-quake

The original Quake, run by [Neon Engine](../neon-engine): its `id1` data
loaded, and its game code interpreted, by a native extension. It is the
proof that the engine can be extended from outside, and that a game can be
made on it without touching the engine. The engine knows nothing of Quake.

**Where this stands.** The extension reads the archives of the game and shows
a level: its walls, floors, and ceilings with their textures, and its doors,
lifts, and buttons at rest where the level puts them, seen from where a
player starts, by a camera that flies. The level is not lit by its lightmaps
yet, nothing can be walked on, and nothing in it moves.

## What it is made of

```
assets/project.yml          the project: its name, and the scene it starts with
extensions/quake/
  extension.yml             the recipe of the extension
  quake.cpp                 its code: what it brings to the engine
  game-data.hpp/.cpp        the archives of the game, and its palette
  level-view.hpp/.cpp       a level shown in the world of the engine
  formats/                  the formats of the game's data, read from bytes
  assets/                   its scenes, and the data of the game under id1/
```

`formats/` knows nothing of the engine: an archive, a picture, a level, a
model, the game code, each read from bytes in memory into plain structs. It is
a library of its own, `quake-formats`, which the extension links, and which
is tested without the engine. `quake.cpp` and what joins it turn what the
formats read into entities, meshes, and textures of the engine. The game
counts in units of its own with z up, and winds its triangles the other way
than the engine; `formats/quake-space.hpp` is the one place that turns it
over, a unit being a thirty-second of a metre.

It is a project with an extension, see
[projects.md](../neon-engine/docs/projects.md#a-project-with-code-in-c) and
[extensions.md](../neon-engine/docs/extensions.md) of the engine.

## Building it

Neon Engine is checked out next to this repository, as `../neon-engine`, and
built once, so that there is a NeonRuntime. Nothing of the engine is compiled
here: only the files of the extension are.

```sh
cmake -S . -B build -G Ninja
cmake --build build
build/quake/NeonRuntime
```

| Option | Says |
|---|---|
| `-DNEON_ENGINE_DIRECTORY=<folder>` | Where the engine is, when it is not `../neon-engine` |
| `-DNEON_RUNTIME_DIRECTORY=<folder>` | Which NeonRuntime the game is put together with, when it is not one of the engine's builds. A folder with `NeonRuntime` and its `assets` |

The game is put together in `build/quake/`: the runtime, its assets with this
project's `project.yml` on top, and `extensions/quake/`.

## Running it

```sh
build/quake/NeonRuntime
```

The level that is shown is `maps/start.bsp`, where the game itself starts,
or the first level the data has when it has no such one. Another is chosen
without changing code, by a text file next to the data whose first line is
the name of a level:

```sh
echo maps/lq_e1m1.bsp > build/quake/extensions/quake/assets/level.txt
```

The extension reads it as `extensions://quake/assets/level.txt`. It is a
choice of one's own machine and is never committed: Git leaves
`extensions/quake/assets/level.txt` alone. When the data has no level of that
name the game says so and shows the level it starts with; without the file
it shows that level and says nothing.

A level is shown with its other models, each under an entity named after
what it is and its number, such as `func_door *3`. Triggers are volumes and
are left out, as are faces painted `trigger`, `clip`, or `skip`.

## The data of the game

The repository carries a game to run: the data of
[LibreQuake](https://github.com/lavenderdotpet/LibreQuake) 0.9, a free game
made for this engine's kind, under `extensions/quake/assets/id1/`, kept by
Git LFS. Its maps, models, textures, and sounds are under the BSD 3-clause
licence, and its game code, `progs.dat` inside `pak0.pak`, and `pop.lmp`
under the GPL 2; the notices are next to the data, in `id1/docs/`, and stay
with it.

```sh
git lfs install
git lfs pull
```

The extension reads it as `extensions://quake/assets/id1/pak0.pak`,
`pak1.pak`, and so on, each archive as the file it is, and takes it apart
itself; the engine knows nothing of archives. Whoever owns the original game
puts its `id1` in place of this one, on their own machine. The original's
data is not ours and is never committed.

Without any data the game starts and says that it is missing.

## Tests

A test sits next to the file it tests and is named after it, with `.test` in
front of the extension, as in the engine. The files of `formats/` are found
by the build, so a format is added by adding its files.

```sh
cmake --build build --target quake-tests
ctest --test-dir build
```

The tests make the bytes they read themselves. One that wants real data
looks for it in `QUAKE_TEST_DATA_DIRECTORY`, by default the data this
repository carries, and skips itself when the file it wants is not there as
a file of its own: the levels under `id1/maps` are, what is inside the
archives is not. A checkout of LibreQuake has the rest loose, under `lq1`.

## How it is written

The code is written for this repository, in the conventions of Neon Engine,
see its [style guide](../neon-engine/docs/style-guide.md). Other source ports
are read to understand the formats, and nothing is copied from them.

## Order of work

From the engine's issue #287: the pak files as a scheme of the file system
and the palette; a level drawn with its lightmaps; the collision of a level
and the player walking it; the models with their frames; the interpreter of
the game code with its entities as components; sounds and music.
