#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : require

#include "common.h"

layout(location = 0) out vec4 normal;
layout(location = 1) out vec4 albedo;

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

layout(push_constant) uniform params_t
{
    mat4 mProjView;
    mat4 mModel;
} params;

void main()
{
  normal = vec4(surf.wNorm, 1.0f);
  vec4 r = params.mModel[0] +
           params.mModel[1] +
           params.mModel[2] +
           params.mModel[3];
  albedo = vec4(abs(cos(r)));
}

