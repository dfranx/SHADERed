#version 330 core

layout (triangles) in;
layout (line_strip, max_vertices = 12) out;

in VS_OUT {
    vec3 normal;
} gs_in[];

void GenerateLine(int i)
{
    gl_Position = gl_in[i].gl_Position;
    EmitVertex();
    gl_Position = gl_in[i].gl_Position + vec4(gs_in[i].normal, 0.0) * MAGNITUDE;
    EmitVertex();
    EndPrimitive();
}

void main()
{
    GenerateLine(0); // first vertex normal
    GenerateLine(1); // second vertex normal
    GenerateLine(2); // third vertex normal
}