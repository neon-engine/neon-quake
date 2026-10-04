// What every fragment shader of the game starts with.

#define object objects[object_index]
#include "scene-data.glsl"

layout (location = 0) in vec2 tex_coord;
layout (location = 1) in vec4 vertex_color;
layout (location = 2) flat in uint object_index;
layout (location = 3) in vec2 lightmap_coord;
layout (location = 4) in vec3 world_position;

layout (location = 0) out vec4 frag_color;

layout (set = 0, binding = 2) uniform texture2D diffuse_texture;
layout (set = 0, binding = 5) uniform sampler diffuse_sampler;

// the pixels of the texture that glow, where a material has such a picture
layout (set = 0, binding = 4) uniform texture2D glow_texture;
layout (set = 0, binding = 7) uniform sampler glow_sampler;

#include "lightmap.glsl"
#include "quake.glsl"
