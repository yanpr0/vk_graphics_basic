#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : require

#include "common.h"

layout(location = 0) out vec4 out_fragColor;

layout(binding = 0, set = 0) uniform AppData
{
  UniformParams Params;
};

layout(push_constant) uniform params_t
{
    mat4 mProjView;
};

layout (binding = 1) uniform sampler2D shadowMap;
layout (binding = 2) uniform sampler2D normals;
layout (binding = 3) uniform sampler2D albedo;
layout (binding = 4) uniform sampler2D depth;

layout (location = 0 ) in VS_OUT
{
    vec2 texCoord;
} surf;

vec3 depth2pos()
{
  float x = surf.texCoord.x * 2.0f - 1.0f;
  float y = surf.texCoord.y * 2.0f - 1.0f;

  vec4 posNDC = vec4(x, y, textureLod(depth, surf.texCoord, 0).x, 1.0f);
  vec4 pos = inverse(mProjView) * posNDC;
  return pos.xyz / pos.w;
}

void main()
{
  vec3 wPos = depth2pos();

  const vec4 posLightClipSpace = Params.lightMatrix*vec4(wPos, 1.0f); //
  const vec3 posLightSpaceNDC  = posLightClipSpace.xyz/posLightClipSpace.w;    // for orto matrix, we don't need perspective division, you can remove it if you want; this is general case;
  const vec2 shadowTexCoord    = posLightSpaceNDC.xy*0.5f + vec2(0.5f, 0.5f);  // just shift coords from [-1,1] to [0,1]

  const bool  outOfView = (shadowTexCoord.x < 0.0001f || shadowTexCoord.x > 0.9999f || shadowTexCoord.y < 0.0091f || shadowTexCoord.y > 0.9999f);
  const float shadow    = ((posLightSpaceNDC.z < textureLod(shadowMap, shadowTexCoord, 0).x + 0.001f) || outOfView) ? 1.0f : 0.0f;

  const vec4 dark_violet = vec4(0.59f, 0.0f, 0.82f, 1.0f);
  const vec4 chartreuse  = vec4(0.5f, 1.0f, 0.0f, 1.0f);

  vec4 lightColor1 = mix(dark_violet, chartreuse, abs(sin(Params.time)));
  vec4 lightColor2 = vec4(1.0f, 1.0f, 1.0f, 1.0f);

  vec3 lightDir   = normalize(Params.lightPos - wPos);
  vec4 lightColor = max(dot(textureLod(normals, surf.texCoord, 0).xyz, lightDir), 0.0f) * lightColor1;
  out_fragColor   = (lightColor*shadow + vec4(0.1f)) * textureLod(albedo, surf.texCoord, 0);
}
