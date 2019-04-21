// constant buffer for projection and view matrix
cbuffer cbStatic : register(b0)
{
	float base;
	float falloff;
	float radius;
}; 

struct PINPUT
{
	float4 Pos : SV_POSITION;
	float2 UV : TEXCOORD;
};

Texture2D tex : register(t0);
Texture2D noiseTex : register(t1);
SamplerState smp : register(s0);

//static const float base = 0.4;				// brightness
//static const float falloff = (0.00001 + 0.000001) / 6;		// allowed difference in depths
//static const float radius = 0.01;			// hemisphere radius

// samples for random vectors
static const int sampleCount = 64;
static const float3 samples[64] = {
		float3(-0.539896, -0.292773, 0.026356),         float3(0.065046, -0.170591, 0.127994),  float3(0.179425, -0.375964, 0.334838),  float3(0.239261, -0.220701, 0.286647),
		float3(0.320076, 0.855644, 0.090857),   float3(0.245883, -0.077057, 0.312905),  float3(0.130452, 0.087652, 0.034867),
		float3(0.093748, 0.133852, 0.337499),   float3(0.626633, 0.480975, 0.473688),   float3(0.298282, -0.090300, 0.103377),
		float3(0.552611, -0.520351, 0.647003),  float3(0.079112, 0.024968, 0.172365),   float3(-0.582619, 0.544593, 0.244123),
		float3(-0.322462, -0.313900, 0.463531),         float3(0.135052, -0.310919, 0.553837),  float3(0.342856, 0.106938, 0.446264),
		float3(-0.104535, -0.157742, 0.008550),         float3(-0.005378, 0.086567, 0.077075),  float3(-0.081828, -0.039418, 0.057307),
		float3(-0.030392, -0.261691, 0.267999),         float3(0.202511, 0.577822, 0.748556),   float3(-0.283245, 0.652555, 0.136841),
		float3(-0.483390, 0.715190, 0.122549),  float3(-0.652714, 0.225648, 0.286146),  float3(0.704482, -0.105992, 0.096554),
		float3(-0.485229, -0.485618, 0.431509),         float3(-0.065098, -0.036231, 0.022172),         float3(-0.113699, 0.208805, 0.081170),
		float3(-0.084052, -0.001158, 0.689189),         float3(0.110677, -0.094703, 0.127262),  float3(0.110718, -0.423335, 0.438998),
		float3(0.908666, -0.364416, 0.155000),  float3(-0.581715, -0.463490, 0.614530),         float3(-0.223703, -0.190523, 0.003661),
		float3(0.060276, 0.270624, 0.016028),   float3(0.552193, -0.488710, 0.020909),  float3(0.268781, 0.472188, 0.080382),
		float3(-0.159595, -0.010449, 0.207906),         float3(0.471661, 0.309936, 0.623213),   float3(-0.034014, 0.031046, 0.024288),
		float3(0.005885, 0.312389, 0.438813),   float3(0.057237, -0.035735, 0.077092),  float3(0.034021, -0.181817, 0.141748),
		float3(0.197519, 0.293338, 0.210668),   float3(-0.095937, -0.199035, 0.016074),         float3(-0.779008, 0.013149, 0.165792),
		float3(-0.605346, -0.150658, 0.135774),         float3(0.289472, -0.017426, 0.167104),  float3(0.165138, 0.107611, 0.067614),
		float3(-0.180146, 0.357549, 0.157629),  float3(0.405622, 0.097586, 0.466096),   float3(-0.066620, 0.435386, 0.248445),
		float3(-0.532673, 0.057612, 0.152923),  float3(-0.230665, -0.145810, 0.163038),         float3(-0.061525, 0.120263, 0.236975),
		float3(-0.511383, -0.562347, 0.481795),         float3(-0.504473, 0.700149, 0.248427),  float3(-0.046033, 0.063672, 0.099918),
		float3(0.172733, -0.193304, 0.137286),  float3(0.603972, 0.678038, 0.036920),   float3(0.116689, -0.516991, 0.175624),
		float3(0.001302, 0.000207, 0.004064),   float3(-0.011270, 0.122578, 0.102418),  float3(-0.252300, 0.290324, 0.260783),
};

float3 normal_from_depth(float depth, float2 texcoords) {
  
  const float2 offset1 = float2(0.0,0.001);
  const float2 offset2 = float2(0.001,0.0);
  
  float depth1 = tex.Sample(smp, texcoords + offset1).w;
  float depth2 = tex.Sample(smp, texcoords + offset2).w;
  
  float3 p1 = float3(offset1, depth1 - depth);
  float3 p2 = float3(offset2, depth2 - depth);
  
  float3 normal = cross(p1, p2);
  normal.z = -normal.z;
  
  return normalize(normal);
}

float4 main(PINPUT pin) : SV_Target
{	
	float4 bufferSample = tex.Sample(smp, pin.UV);

	float depth = bufferSample.w;
	
	clip((depth != 0) - 1);

	float3 random = normalize(noiseTex.Sample(smp, pin.UV).xyz);
	//random.z = 0;

	float3 position = float3(pin.UV, depth);
	
	float3 normal = bufferSample.xyz * 2 - 1;
	
	float r = radius;
	float occ = 0.0;

	for (int i = 0; i < sampleCount; i++) {
		float3 ray = r * reflect(samples[i], random);
		float3 hemi_ray = position + sign(dot(ray, normal)) * ray;
		float occ_depth = tex.Sample(smp, saturate(hemi_ray.xy)).w;
		float difference = saturate(depth - occ_depth);
		occ += ((occ_depth-depth >= falloff) ? 1 : 0);
	}
	
	occ = 1-occ / sampleCount;
	
	return saturate(occ +base) * float4(1,0, 0, 1);// *tex.Sample(smp, In.UV);
}
