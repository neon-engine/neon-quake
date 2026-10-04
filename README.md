# neon-quake

The original Quake, run by [Neon Engine](../neon-engine): its `id1` data
loaded, and its game code interpreted, by a native extension. It is the
proof that the engine can be extended from outside, and that a game can be
made on it without touching the engine. The engine knows nothing of Quake.

**Where this stands.** A project and an extension that starts and looks for
the data, and the beginning of the formats: the reader of bytes and the
palette. Nothing of the data is shown yet.

## What it is made of

```
assets/project.yml          the project: its name, and the scene it starts with
extensions/quake/
  extension.yml             the recipe of the extension
  quake.cpp                 its code: what it brings to the engine
  formats/                  the formats of the game's data, read from bytes
  assets/                   its scenes, and whatever else it brings
```

`formats/` knows nothing of the engine: an archive, a picture, a level, a
model, the game code, each read from bytes in memory into plain structs. It is
a library of its own, `quake-formats`, which the extension links, and which
is tested without the engine. `quake.cpp` and what joins it turn what the
formats read into entities, meshes, and textures of the engine.

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

## The data of the game

The data is not ours and is never committed. The extension reads the
player's own copy from `id1` under its assets next to the runtime, as
`extensions://quake/assets/id1/pak0.pak`:

```
build/quake/extensions/quake/assets/id1/pak0.pak
```

Without it the game starts and says that it is missing. An archive is read as
the file it is and taken apart by the extension; the engine knows nothing of
archives.

## Tests

A test sits next to the file it tests and is named after it, with `.test` in
front of the extension, as in the engine. The files of `formats/` are found
by the build, so a format is added by adding its files.

```sh
cmake --build build --target quake-tests
ctest --test-dir build
```

The tests make the bytes they read themselves. One that wants real data
looks for it in `QUAKE_TEST_DATA_DIRECTORY`, by default `../LibreQuake/lq1`,
a checkout of [LibreQuake](https://github.com/lavenderdotpet/LibreQuake),
whose data is free, and skips itself when it is not there. Nothing of it is
committed here.

## How it is written

The code is written for this repository, in the conventions of Neon Engine,
see its [style guide](../neon-engine/docs/style-guide.md). Other source ports
are read to understand the formats, and nothing is copied from them.

## Order of work

From the engine's issue #287: the pak files as a scheme of the file system
and the palette; a level drawn with its lightmaps; the collision of a level
and the player walking it; the models with their frames; the interpreter of
the game code with its entities as components; sounds and music.
