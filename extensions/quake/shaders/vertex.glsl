// The vertex shader of everything the game draws: where a corner is on the
// screen, and what the fragment shaders are handed of it.

#define object objects[gl_InstanceIndex]
#include "scene-data.glsl"

layout (location = 0) in vec3 attr_pos_coords;
layout (location = 1) in vec3 attr_normal_coords;
layout (location = 2) in vec2 attr_tex_coords;
layout (location = 3) in vec4 attr_color;
layout (location = 4) in vec2 attr_lightmap_coords;

layout (location = 0) out vec2 tex_coord;
layout (location = 1) out vec4 vertex_color;
layout (location = 2) flat out uint object_index;
layout (location = 3) out vec2 lightmap_coord;
// where the corner is in the world, for what is drawn by where it is seen
// from: the sky
layout (location = 4) out vec3 world_position;

void main()
{
    object_index = gl_InstanceIndex;
    tex_coord = attr_tex_coords * object.texture_scale.xy;
    vertex_color = attr_color;
    lightmap_coord = attr_lightmap_coords;

    vec4 in_world = object.model * vec4(attr_pos_coords, 1.0);
    world_position = in_world.xyz;
    gl_Position = scene.projection * scene.view * in_world;
}
