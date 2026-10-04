#version 450
#extension GL_GOOGLE_include_directive : require

// Water, slime, lava, and the inside of a teleporter: the texture swims.
// Each way it is pushed by a wave that runs along the other, an eighth of
// the texture far, as the original does it. A liquid glows as it is and
// takes no light.

#include "fragment.glsl"

void main()
{
    vec2 swum = tex_coord + 0.125 * sin(tex_coord.yx * 8.0 + game_time());
    vec4 texel = read_pixels(diffuse_texture, diffuse_sampler, swum, dFdx(tex_coord), dFdy(tex_coord));

    frag_color = vec4(texel.rgb * vertex_color.rgb, object_alpha(object.material, texel.a));
}
