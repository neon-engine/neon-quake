#version 450
#extension GL_GOOGLE_include_directive : require

// A wall, a model, a sprite, a picture of the status bar: its texture read
// pixel by pixel, times the colour of its corners and the light that was
// worked out for it.

#include "fragment.glsl"

void main()
{
    vec4 texel = read_pixels(diffuse_texture, diffuse_sampler, tex_coord, dFdx(tex_coord), dFdy(tex_coord));
    vec3 shown = texel.rgb * vertex_color.rgb;

    if (has_lightmap(object.lightmap)) { shown *= baked_light(object.lightmap, lightmap_coord); }

    frag_color = vec4(shown, object_alpha(object.material, texel.a));
}
