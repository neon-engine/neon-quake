#version 450
#extension GL_GOOGLE_include_directive : require

// Water, slime, lava, and the inside of a teleporter: the texture swims.
// The original draws a liquid with 64 of its units to one picture, however
// many pixels the picture has, and pushes it each way by a wave that runs
// along the other: an eighth of the picture far, and one wave long over two
// pictures. A liquid glows as it is where the level gave it no light. Where
// it gave it some, as the levels of today do, it is lit by it: the light
// lies still while the texture swims.

#include "fragment.glsl"

void main()
{
    vec2 size = vec2(textureSize(sampler2D(diffuse_texture, diffuse_sampler), 0));
    vec2 place = tex_coord * size / 64.0;
    vec2 swum = place + 0.125 * sin(place.yx * 3.14159265 + game_time());
    vec4 texel = read_pixels(diffuse_texture, diffuse_sampler, swum, dFdx(place), dFdy(place));

    vec3 lit = texel.rgb * vertex_color.rgb;
    if (has_lightmap(object.lightmap)) { lit *= baked_light(object.lightmap, lightmap_coord); }

    // as much of it is seen through as its colour says
    vec3 shown = in_fog(lit, world_position);
    frag_color = vec4(shown, object_alpha(object.material, object.color.a));
}
