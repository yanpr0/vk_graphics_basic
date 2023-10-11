#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : require

// Author @patriciogv - 2015
// http://patriciogonzalezvivo.com

float random(vec2 st)
{
    return fract(sin(dot(st, vec2(12.9898, 78.233))) * 43758.5453123);
}

// Based on Morgan McGuire @morgan3d
// https://www.shadertoy.com/view/4dS3Wd
float noise(vec2 st)
{
    vec2 i = floor(st);
    vec2 f = fract(st);

    // Four corners in 2D of a tile
    float a = random(i);
    float b = random(i + vec2(1.0, 0.0));
    float c = random(i + vec2(0.0, 1.0));
    float d = random(i + vec2(1.0, 1.0));

    vec2 u = f * f * (3.0 - 2.0 * f);

    return mix(a, b, u.x) +
            (c - a) * u.y * (1.0 - u.x) +
            (d - b) * u.x * u.y;
}

#define OCTAVES 18
float fbm(vec2 st)
{
    // Initial values
    float value = 0.0;
    float amplitude = .7;
    float frequency = 0.;
    //
    // Loop of octaves
    for (int i = 0; i < OCTAVES; i++)
    {
        value += amplitude * noise(st);
        st *= 2.696;
        amplitude *= .5;
    }
    return value;
}


#include "unpack_attributes.h"
#include "common.h"


layout(location = 0) in vec4 vPosNorm;
layout(location = 1) in vec4 vTexCoordAndTang;

layout(push_constant) uniform params_t
{
    mat4 mProjView;
    float minHeight;
    float maxHeight;
} params;

layout(binding = 0, set = 0) uniform AppData
{
  UniformParams Params;
};

layout (location = 0 ) out VS_OUT
{
    vec3 wPos;
    vec3 wNorm;
    vec3 wTangent;
    vec2 texCoord;
} vOut;

out gl_PerVertex { vec4 gl_Position; };

float height(vec2 xy)
{
    return fbm(5.0 * xy) * (params.maxHeight - params.minHeight) + params.minHeight;
}

void main(void)
{
    const vec4 wNorm = vec4(DecodeNormal(floatBitsToInt(vPosNorm.w)),         0.0f);
    const vec4 wTang = vec4(DecodeNormal(floatBitsToInt(vTexCoordAndTang.z)), 0.0f);

    mat4 model = mat4(mat3(5.0f));

    vOut.wPos     = (model * vec4(vPosNorm.xyz, 1.0f)).xyz;
    vOut.wNorm    = normalize(mat3(transpose(inverse(model))) * wNorm.xyz);
    vOut.wTangent = normalize(mat3(transpose(inverse(model))) * wTang.xyz);
    vOut.texCoord = vTexCoordAndTang.xy;

    vec2 xy = vOut.texCoord;

    vOut.wPos.y = height(xy);

    float delta = Params.invRes;

    vec2 dx = vec2(delta, 0.0);
    vec2 dy = vec2(0.0 , delta);

    float hx_p = height(xy + dx);
    float hx_m = height(xy - dx);
    float hy_p = height(xy + dy);
    float hy_m = height(xy - dy);

    vec3 dh_x = vec3((hx_p - hx_m) / (2.0 * delta), 0.0, 0.0);
    vec3 dh_y = vec3(0.0, (hy_p - hy_m) / (2.0 * delta), 0.0);

    vec3 norm = cross(dh_x, dh_y);
    norm = normalize(norm);
    vOut.wNorm = norm;

    gl_Position   = params.mProjView * vec4(vOut.wPos, 1.0);
}

