#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : require

#include "common.h"

layout(location = 0) out vec4 out_fragColor;

layout(push_constant) uniform params_t
{
    mat4 mProjView;
    mat4 mModel;
} params;

layout (location = 0 ) in VS_OUT
{
  vec3 wPos;
  vec3 wNorm;
  vec3 wTangent;
  vec2 texCoord;
} surf;

layout(binding = 0, set = 0) uniform AppData
{
  UniformParams Params;
};

layout (binding = 1) uniform sampler2D shadowMap;

vec3 getAlbedo()
{
  return normalize(abs(vec3(1) - params.mModel[3].xyz + params.mModel[2].xyz));
  //if (float(i) > Params.sssScale)return vec3(1,0,0);
  //else return vec3(0,1,0);
  /*switch (i % 7)
  {
  case 0: return vec3(1.0, 0.0, 0.0);
  case 1: return vec3(0.0, 1.0, 0.0);
  case 2: return vec3(0.0, 0.0, 1.0);
  case 3: return vec3(1.0, 1.0, 0.0);
  case 4: return vec3(1.0, 0.0, 1.0);
  case 5: return vec3(0.0, 1.0, 1.0);
  case 6: return vec3(1.0, 1.0, 1.0);
  default: return vec3(1.0, 1.0, 1.0);
  }*/
  switch (1 % 7)
  {
    case 0:
    return vec3(0.57f, 0.4f, 0.65f);
  case 1:
    return vec3(0.29f, 0.44f, 0.65f);
  case 2:
    return vec3(0.3f, 0.5f, 0.32f);
  case 3:
    return vec3(0.73f, 0.8f, 0.19f);
  case 4:
    return vec3(0.31f, 0.25f, 0.85f);
  case 5:
    return vec3(1.0f, 0.0f, 0.0f);
    }
}

// https://www.iryoku.com/translucency/downloads/Real-Time-Realistic-Skin-Translucency.pdf
// This function can be precomputed for efficiency
vec3 T(float s) {
  return vec3(0.233, 0.455, 0.649) * exp(-s*s/0.0064) +
         vec3(0.1,   0.336, 0.344) * exp(-s*s/0.0484) +
         vec3(0.118, 0.198, 0.0)   * exp(-s*s/0.187) +
         vec3(0.113, 0.007, 0.007) * exp(-s*s/0.567) +
         vec3(0.358, 0.004, 0.0)   * exp(-s*s/1.99) +
         vec3(0.078, 0.0,   0.0)   * exp(-s*s/7.41);
}

vec4 getTransmittance(vec3 lightColor)
{
  vec4 shrinkedPos = vec4(surf.wPos - 0.005 * surf.wNorm, 1.0); // avoid edge artefacts

  const vec4 posLightClipSpace = Params.lightMatrix * shrinkedPos;
  const vec3 posLightSpaceNDC  = posLightClipSpace.xyz / posLightClipSpace.w;
  const vec2 shadowTexCoord    = posLightSpaceNDC.xy * 0.5f + vec2(0.5f, 0.5f);
  const float shadowMapDepth   = textureLod(shadowMap, shadowTexCoord, 0).x;

  float subsurfTravel = distance(shrinkedPos, inverse(Params.lightMatrix) * vec4(posLightSpaceNDC.xy, shadowMapDepth, 1.0));
  float s = subsurfTravel / Params.sssScale;
  const vec3 lightDir = normalize(Params.lightPos - shrinkedPos.xyz);
  float E = max(0.3 + dot(-surf.wNorm, lightDir), 0.0);
  vec3 transmittance = T(s) * lightColor * getAlbedo() * E;

  return vec4(transmittance, 1.0);
}

void main()
{
  const vec4 posLightClipSpace = Params.lightMatrix*vec4(surf.wPos, 1.0f); //
  const vec3 posLightSpaceNDC  = posLightClipSpace.xyz/posLightClipSpace.w;    // for orto matrix, we don't need perspective division, you can remove it if you want; this is general case;
  const vec2 shadowTexCoord    = posLightSpaceNDC.xy*0.5f + vec2(0.5f, 0.5f);  // just shift coords from [-1,1] to [0,1]

  const bool  outOfView = (shadowTexCoord.x < 0.0001f || shadowTexCoord.x > 0.9999f || shadowTexCoord.y < 0.0091f || shadowTexCoord.y > 0.9999f);
  const float shadowMapDepth = textureLod(shadowMap, shadowTexCoord, 0).x;
  const float shadow = ((posLightSpaceNDC.z < shadowMapDepth + 0.001f) || outOfView) ? 1.0f : 0.0f;

  const vec4 dark_violet = vec4(0.59f, 0.0f, 0.82f, 1.0f);
  const vec4 chartreuse  = vec4(0.5f, 1.0f, 0.0f, 1.0f);

  vec4 lightColor1 = mix(dark_violet, chartreuse, abs(sin(Params.time)));
  vec4 lightColor2 = vec4(1.0f, 1.0f, 1.0f, 1.0f);

  vec4 lightColor = lightColor2;
  vec3 lightDir   = normalize(Params.lightPos - surf.wPos);
  out_fragColor   = (max(dot(surf.wNorm, lightDir), 0.0f) * lightColor * shadow + vec4(0.1f)) * vec4(getAlbedo(), 1.0f);

  if (Params.sss != 0)
  {
    out_fragColor += getTransmittance(lightColor.xyz);
  }
}

