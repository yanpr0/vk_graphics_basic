#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : require

#include "common.h"

layout(location = 0) out vec4 out_fragColor;

layout (location = 0 ) in VS_OUT
{
  vec2 texCoord;
} vOut;

layout (binding = 0) uniform sampler2D hdr;

layout(push_constant) uniform params_t
{
  int gammaCorrection;
  int toneMappingMethod;
} params;

vec3 reinhard(vec3 hdrColor)
{
  return hdrColor / (hdrColor + vec3(1.0));
}

vec3 exposure(vec3 hdrColor)
{
  float e = 5.0;
  return vec3(1.0) - exp(-hdrColor * e);
}

void main()
{
  const vec3 hdrColor = textureLod(hdr, vOut.texCoord, 0).xyz;

  vec3 color = vec3(0.0);

  switch (params.toneMappingMethod)
  {
  case 0:
    color = clamp(hdrColor, vec3(0.0), vec3(1.0));
    break;
  case 1:
    color = reinhard(hdrColor);
    break;
  case 2:
    color = exposure(hdrColor);
    break;
  }

  if (params.gammaCorrection != 0)
  {
      const float gamma = 2.2;
      color = pow(color, vec3(1.0 /  gamma));
  }

  out_fragColor = vec4(color, 1.0);
}

