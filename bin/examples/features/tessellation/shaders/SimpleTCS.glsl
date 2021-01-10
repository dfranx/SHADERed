#version 400

layout (vertices = 3) out;

uniform vec3 cameraPos;

in vec3 posCS[];
out vec3 posES[];

float calcTess(float a, float b)
{
	float perc = ((a + b) / 2.0) / 50.0;
	return 1.0f + (1-perc) * 15.0f;
}

void main()
{
	posES[gl_InvocationID] = posCS[gl_InvocationID];
	
	float dist0 = distance(cameraPos, posCS[0]);
	float dist1 = distance(cameraPos, posCS[1]);
	float dist2 = distance(cameraPos, posCS[2]);
	
	gl_TessLevelOuter[0] = calcTess(dist1, dist2);
	gl_TessLevelOuter[1] = calcTess(dist2, dist0);
	gl_TessLevelOuter[2] = calcTess(dist0, dist1);
	gl_TessLevelInner[0] = gl_TessLevelOuter[0];
}
