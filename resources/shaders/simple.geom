#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : require

#include "unpack_attributes.h"
#include "common.h"

layout (triangles) in;
layout (triangle_strip, max_vertices = 9) out;

layout(push_constant) uniform params_t
{
    mat4 mProjView;
    mat4 mModel;
} params;

layout(binding = 0, set = 0) uniform AppData
{
    UniformParams Params;
};

layout (location = 0 ) in VS_OUT
{
    vec3 wPos;
    vec3 wNorm;
    vec3 wTangent;
    vec2 texCoord;
} vOut[];

layout (location = 0 ) out GS_OUT
{
    vec3 wPos;
    vec3 wNorm;
    vec3 wTangent;
    vec2 texCoord;
} gOut;

void main(void)
{
    float coef = min(length(cross(vOut[1].wPos - vOut[0].wPos, vOut[2].wPos - vOut[0].wPos)) * 100.0, 0.10); // so we don't move out very far from small triangles

    vec3 mid      = (vOut[0].wPos + vOut[1].wPos + vOut[2].wPos) / 3.0;
    vec3 norm     = normalize(vOut[0].wNorm + vOut[1].wNorm + vOut[2].wNorm);
    vec3 tangent  = normalize(vOut[0].wTangent + vOut[1].wTangent + vOut[2].wTangent);
    vec2 texCoord = (vOut[0].texCoord + vOut[1].texCoord + vOut[2].texCoord) / 3.0;
    vec3 pos      = mid + norm * sin(Params.time) * coef;

    for (int i = 0; i < 3; ++i)
    {
        for (int j = 0; j < 2; ++j)
        {
            uint k = (i + j) % 3; // for correct vertex traverse order

            gOut.wPos     = vOut[k].wPos;
            gOut.wNorm    = vOut[k].wNorm;
            gOut.wTangent = vOut[k].wTangent;
            gOut.texCoord = vOut[k].texCoord;
            gl_Position   = params.mProjView * vec4(vOut[k].wPos, 1.0);
            EmitVertex();
        }

        gOut.wPos     = pos;
        gOut.wNorm    = norm;
        gOut.wTangent = tangent;
        gOut.texCoord = texCoord;
        gl_Position   = params.mProjView * vec4(pos, 1.0);
        EmitVertex();

        EndPrimitive();
    }
}
