#version 450
#extension GL_GOOGLE_include_directive : require

// A liquid, or a field of force, whose light changes: as liquid.frag draws
// one that is lit, with the light worked out from its lightmaps and what
// their styles are worth now, see styles.glsl.

#include "fragment.glsl"
#include "styles.glsl"

void main()
{
    vec2 size = vec2(textureSize(sampler2D(diffuse_texture, diffuse_sampler), 0));
    vec2 place = tex_coord * size / 64.0;
    vec2 swum = place + 0.125 * sin(place.yx * 3.14159265 + game_time());
    vec4 texel = read_pixels(diffuse_texture, diffuse_sampler, swum, dFdx(place), dFdy(place));

    vec3 lit = texel.rgb * light_of_styles(object.lightmap, lightmap_coord, vertex_color);

    // as much of it is seen through as its colour says
    frag_color = vec4(in_fog(lit, world_position), object_alpha(object.material, object.color.a));
}
