#version 320 es

precision mediump float;
uniform vec2 v_texcoord;
uniform sampler2D tex;
uniform vec2 fullSize;

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

const float PI = 3.14159265359;
const float PHI = 1.61803398875;
const int BLOOM_SAMPLES = 30;

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

vec3 hash33(in vec3 p3) {
  p3 = fract(p3 * vec3(.1031, .1030, .0973));
  p3 += dot(p3, p3.yxz + 19.19);
  return fract((p3.xxy + p3.yxx) * p3.zyx);
}

void main() {
  vec2 uv = v_texcoord;

  vec4 pixColor = texture2D(tex, uv);

  vec3 bloom = vec3(0);
  float totfac = 0.0;

  vec3 rnd = hash33(vec3(v_texcoord, time));
  float offset = rnd.x * 2.0 * PI;

  // bloom
  float bloom_radius = 80.0;
  for (int i = 0; i < BLOOM_SAMPLES; i++) {
    float theta = 2.0 * PI * PHI * float(i) + offset;
    float radius = sqrt(float(i)) / sqrt(float(BLOOM_SAMPLES));
    radius *= bloom_radius;
    vec2 offset = vec2(cos(theta), sin(theta)) * radius;
    vec2 delta = vec2(1.0 + exp(-abs(offset.y) * 0.1), 0.5);
    offset *= delta;
    vec4 here = textureGrad(tex, (fragCoord + offset) / iResolution.xy,
                            vec2(.02, 0), vec2(0, 0.02));
    float fact = smoothstep(bloom_radius, 0.0, radius);
    bloom += here.rgb * 0.05 * fact;
    totfac += fact;
  }

  bloom /= totfac;

  // chromatic aberration
  vec2 mo = uv * 2.0 - 1.0;
  mo *= 0.01;
  fragColor.r = textureLod(tex, uv - mo * 0.1, 0.0).r;
  fragColor.g = textureLod(tex, uv - mo * 0.6, 0.0).g;
  fragColor.b = textureLod(tex, uv - mo * 1.0, 0.0).b;

  // add bloom
  fragColor.rgb += bloom;

  gl_FragColor = pixColor;
}
