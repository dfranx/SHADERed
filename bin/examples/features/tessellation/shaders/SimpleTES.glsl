#version 400

layout(triangles, equal_spacing, ccw) in;

uniform mat4 matVP;

in vec3 posES[];

vec3 interpolate(vec3 v0, vec3 v1, vec3 v2)
{
	return v0 * vec3(gl_TessCoord.x) + v1 * vec3(gl_TessCoord.y) + v2 * vec3(gl_TessCoord.z);
}

void main()
{
	vec3 worldPos = interpolate(posES[0], posES[1], posES[2]);
	gl_Position = matVP * vec4(worldPos, 1.0);
}
