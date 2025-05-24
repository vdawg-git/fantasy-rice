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

  float timeMod = mod(time * 100.0, 32.0) / 110.0;
  float loudnessEaseIn = loudness * loudness * loudness * loudness;
  float loudnessClipped = loudness < 0.5 ? 0.0 : loudnessEaseIn;
  float rnd1 = fract(sin(timeMod) * 500.848);
  float rnd2 = fract(sin(timeMod) * 1500.848);
  float rnd3 = fract(cos(timeMod) * 822.848);
  float rnd4 = fract(sin(timeMod) * 9222.848);

  float intensity = loudnessClipped;
  float gnm = sat(intensity);
  float rnd0 = rand(mytrunc(vec2(timeMod, timeMod), 6.0));
  float r0 = sat((1.0 - gnm) * 0.7 + rnd0);
  float r1 = 0.5 - 0.5 * gnm + r0;
  r1 = 1.0 -
       max(0.0,
           ((r1 < 1.0) ? r1 : 0.9999999)); // note: weird ass bug on old drivers
  // float rnd2 = rand(vec2(mytrunc(uv.y, 40.0 * r1), timeMod)); // vert
  float r2 = sat(rnd2);
  // float rnd3 = rand(vec2(mytrunc(uv.y, 10.0 * r0), timeMod));
  float r3 = (1.0 - sat(rnd3 + 0.8)) - 0.1;
  // float rnd1 = rand(vec2(mytrunc(uv.x, 10.0 * r0), timeMod)); // horz

  vec4 pixColor = texture2D(tex, uv);

  vec3 col = pixColor.rgb;

  // invert
  if (loudness > 0.8 && rnd2 > 0.8) {
    col = vec3(1.0 - col.r, 1.0 - col.g, 1.0 - col.b);
  }

  float factor1 = 0.07 * loudnessClipped * rnd1;
  float factor2 = 0.1 * loudnessClipped * rnd2;
  // chromatic aberation
  if (loudnessClipped > 0.1) {
    float noise = fract(sin(time) * 43758.5453123 * uv.y) * 0.0006;
    col.r = texture2D(tex, vec2(noise + uv.x + factor1 * rnd2 - factor1 * rnd3,
                                uv.y + factor2 * rnd2 - factor1 * rnd4))
                .x;
    col.g =
        texture2D(tex, vec2(noise + uv.x + 0.1 * loudnessClipped * rnd1 * rnd2,
                            uv.y + 0.001 * loudnessClipped))
            .y;
    col.b =
        texture2D(tex, vec2(noise + uv.x - 0.0008 * loudnessClipped, uv.y)).z;
  }

  // intensity

  float pxrnd = rand(uv + timeMod);
  float ofs = 0.05 * r2 * intensity * (rnd0 > 0.5 ? 1.0 : -1.0);
  ofs += 0.5 * pxrnd * ofs;
  uv.y += 0.1 * rnd1 * intensity * loudnessEaseIn;

  const int NUM_SAMPLES = 8;
  const float RCP_NUM_SAMPLES_F = 1.0 / float(NUM_SAMPLES);

  vec3 sum = vec3(0.0);
  vec3 wsum = vec3(0.0);
  for (int i = 0; i < NUM_SAMPLES; ++i) {
    float t = float(i) * RCP_NUM_SAMPLES_F;
    uv.x = sat(uv.x + ofs * t);
    vec3 samplecol = texture2D(tex, uv, -10.0).rgb;
    vec3 s = spectrum_offset(t);
    samplecol.rgb = samplecol.rgb * s;
    sum += samplecol;
    wsum += s;
  }
  sum.rgb /= wsum;

  col = sum;

  // lq colors + grade
  // col.r = mix(col.r, mix(col.g, col.b, 0.9), 0.05);
  // col.g = mix(col.g, mix(col.r, col.b, 0.3), 0.05);
  // col.b = mix(col.b, mix(col.g, col.r, 0.8), 0.05);

  // col.rb *= vec2(1.04, 0.8); // crt phosphor  tinting

  // gl_FragColor = gl_FragColor;
  gl_FragColor = vec4(col, 1.0);
}
