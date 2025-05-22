// Auto-generated shader header
const char *glitch_frag = R"glsl(
#version 300 es
precision mediump float;

uniform float loudness;
out vec4 fragColor;

void main() {
    vec2 uv = gl_FragCoord.xy / vec2(1920.0, 1080.0);
    float r = sin(uv.x * 10.0 + loudness * 5.0 + 0.5);
    float g = sin(uv.y * 10.0 + loudness * 10.0);


    // fragColor = vec4(r, g, loudness, 1.0);
    fragColor = vec4(0.0, 0.0, 0.0, 1.0);
})glsl";
