#version 430
layout(local_size_x = 16, local_size_y = 16) in;

uniform float roll;
writeonly uniform image2D img_output;

/* credits http://wili.cc/blog/opengl-cs.html */
void main() {
	ivec2 pixel_coords = ivec2(gl_GlobalInvocationID.xy);
	
	float localCoef = length(vec2(ivec2(gl_LocalInvocationID.xy)-8)/8.0);
	float globalCoef = sin(float(gl_WorkGroupID.x+gl_WorkGroupID.y)*1 + roll)*0.5f;
	
	vec4 px = vec4(1.0-globalCoef*localCoef, 0, 0, 1.0);
	imageStore(img_output, pixel_coords, px);
}





