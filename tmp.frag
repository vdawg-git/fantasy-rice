precision mediump float;
varying vec2 v_texcoord;
uniform sampler2D tex;

// https://www.shadertoy.com/view/4lSGRw
// https://www.shadertoy.com/view/sdK3z1

// uniform mediump float time;
uniform mediump float loudness;

// const float loudness = 0.6;
const float time = 1.0;
// const float display_framerate = 30.0;
const float display_framerate = 60.0;
// const vec2 display_resolution = vec2(1128.0, 752.0);
const vec2 display_resolution = vec2(2256.0, 1504.0);

vec2 curve(vec2 uv) {
  uv = (uv - 0.5) * 2.0;
  uv *= 1.1;
  uv.x *= 1.0 + pow((abs(uv.y) / 8.0), 2.0);
  uv.y *= 1.0 + pow((abs(uv.x) / 8.0), 2.0);
  uv = (uv / 2.0) + 0.5;
  uv = uv * 0.92 + 0.04;
  return uv;
}

float sat(float t) { return clamp(t, 0.0, 1.0); }

vec2 sat(vec2 t) { return clamp(t, 0.0, 1.0); }

// remaps inteval [a;b] to [0;1]
float remap(float t, float a, float b) { return sat((t - a) / (b - a)); }

// note: /\ t=[0;0.5;1], y=[0;1;0]
float linterp(float t) { return sat(1.0 - abs(2.0 * t - 1.0)); }

vec3 spectrum_offset(float t) {
  vec3 ret;
  float lo = step(t, 0.5);
  float hi = 1.0 - lo;
  float w = linterp(remap(t, 1.0 / 6.0, 5.0 / 6.0));
  float neg_w = 1.0 - w;
  ret = vec3(lo, 1.0, hi) * vec3(neg_w, w, neg_w);
  return pow(ret, vec3(1.0 / 2.2));
}

// note: [0;1]
float rand(vec2 n) {
  return fract(sin(dot(n.xy, vec2(12.9898, 78.233))) * 43758.5453);
}

// note: [-1;1]
float srand(vec2 n) { return rand(n) * 2.0 - 1.0; }

float mytrunc(float x, float num_levels) {
  return floor(x * num_levels) / num_levels;
}

vec2 mytrunc(vec2 x, float num_levels) {
  return floor(x * num_levels) / num_levels;
}

float rand(vec2 uv, float t) {
  return fract(sin(dot(uv, vec2(1225.6548, 321.8942))) * 4251.4865 + t);
}

void main() {
  vec2 uv = v_texcoord;

  vec4 pixColor = texture2D(tex, uv);
  pixColor.b = loudness;

  gl_FragColor = pixColor;
}
