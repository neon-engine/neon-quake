#version 450
#extension GL_GOOGLE_include_directive : require

// A wall, a model, a sprite: its texture read pixel by pixel, times the
// colour of its corners and the light that was worked out for it, with the
// light of the moment on top, and with its pixels that glow as they are.

#include "fragment.glsl"

void main()
{
    vec4 texel = read_pixels(diffuse_texture, diffuse_sampler, tex_coord, dFdx(tex_coord), dFdy(tex_coord));
    vec3 shown = texel.rgb * vertex_color.rgb;

    if (has_lightmap(object.lightmap))
    {
        vec3 light = baked_light(object.lightmap, lightmap_coord);

        // The light of a moment is added as the original adds it, to the
        // numbers a screen is given: so the light of the level is taken
        // back to those, and the sum to light again.
        vec3 more = light_of_the_moment(world_position);
        if (more != vec3(0.0))
        {
            vec3 kept = pow(light / object.lightmap.x, vec3(1.0 / 2.2));
            light = pow(min(kept + more, vec3(1.0)), vec3(2.2)) * object.lightmap.x;
        }
        shown *= light;
    }

    // what glows is shown as it is, whatever the light
    if (object.emissive.w > 0.5)
    {
        vec4 glow = read_pixels(glow_texture, glow_sampler, tex_coord, dFdx(tex_coord), dFdy(tex_coord));
        shown = shown * (1.0 - glow.a) + glow.rgb;
    }

    // the corners may make it see-through too
    frag_color = vec4(in_fog(shown, world_position), object_alpha(object.material, texel.a * vertex_color.a));
}
