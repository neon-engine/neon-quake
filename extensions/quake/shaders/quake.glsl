// What the shaders of the game share.
//
// The clock. The engine hands a shader no time and nothing of a game's own,
// so the game keeps one point light that gives no light, and writes into
// the three numbers of how it fades: the time of the game in seconds, how
// thick the fog of the level is, and nothing yet. Where the light stands is
// the colour of the fog. See GameCode::ShowView.
float game_time()
{
    return scene.point_lights[0].attenuation.x;
}

// The fog of a level over what is drawn at a place of the world. It is as
// the ports of today have it: what is seen fades into the colour of the fog
// with the square of how far it is, counted in the units of the game, 32 to
// a metre. A level without fog has none of it.
vec3 in_fog(vec3 shown, vec3 at)
{
    float density = scene.point_lights[0].attenuation.y;
    if (density <= 0.0) { return shown; }

    float far = length(at - scene.view_position.xyz) * 32.0 * density / 64.0;
    return mix(scene.point_lights[0].position.xyz, shown, clamp(exp(-far * far), 0.0, 1.0));
}

// The sky is not at any distance. It takes half the colour of the fog, as
// the ports have it.
vec3 sky_in_fog(vec3 shown)
{
    float density = scene.point_lights[0].attenuation.y;
    return density <= 0.0 ? shown : mix(shown, scene.point_lights[0].position.xyz, 0.5);
}

// A texture read pixel by pixel, as the game shows its pictures: the pixel
// the place lies in, and not what lies between it and its neighbours. Far
// away, where a pixel of the screen covers many of the picture, the smaller
// copies of the picture are read, so that it does not glitter.
vec4 read_pixels(texture2D picture, sampler through, vec2 at, vec2 across, vec2 down)
{
    vec2 size = vec2(textureSize(sampler2D(picture, through), 0));
    vec2 middle = (floor(at * size) + 0.5) / size;
    return textureGrad(sampler2D(picture, through), middle, across, down);
}
