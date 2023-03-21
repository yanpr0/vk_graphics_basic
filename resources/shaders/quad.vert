#version 450
#extension GL_ARB_separate_shader_objects : enable

layout (location = 0 ) out VS_OUT
{
  vec2 texCoord;
} vOut;

void main(void)
{
  if(gl_VertexIndex == 0)
  {
    gl_Position   = vec4(-1.0f, -1.0f, 0.0f, 1.0f);
    vOut.texCoord = vec2(0.0f, 0.0f);
  }
  else if(gl_VertexIndex == 1)
  {
    gl_Position   = vec4(3.0f, -1.0f, 0.0f, 1.0f);
    vOut.texCoord = vec2(2.0f, 0.0f);
  }
  else
  {
    gl_Position   = vec4(-1.0f, 3.0f, 0.0f, 1.0f);
    vOut.texCoord = vec2(0.0f, 2.0f);
  }
}
