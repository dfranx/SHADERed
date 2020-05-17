struct InputData
{
	float4 Position;
	float4 Velocity;
};

cbuffer Uniforms : register(b0)
{
	float delta;
};

RWStructuredBuffer<InputData> data : register(u0);

[numthreads(1, 1, 1)]
void main( uint3 id : SV_DispatchThreadID )
{
	float4 pos = data[id.x].Position;
	float4 vel = data[id.x].Velocity;
	
	data[id.x].Position += vel*delta;
	
	if (abs(pos.x) >= 10.0f) {
		data[id.x].Velocity.x = -vel.x;
		data[id.x].Position.x = 9.99f * sign(pos.x);
	}
	if (abs(pos.y) >= 10.0f) {
		data[id.x].Velocity.y = -vel.y;
		data[id.x].Position.y = 9.99f * sign(pos.y);
	}
	if (abs(pos.z) >= 10.0f) {
		data[id.x].Velocity.z = -vel.z;
		data[id.x].Position.z = 9.99f * sign(pos.z);
	}
}