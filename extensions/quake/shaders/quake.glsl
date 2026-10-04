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
