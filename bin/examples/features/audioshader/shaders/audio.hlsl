// Created by inigo quilez - iq/2014
// License Creative Commons Attribution-NonCommercial-ShareAlike 3.0 Unported License.
// https://www.shadertoy.com/view/XdfXWS

float noise( float x )
{    
	return frac(sin(1371.1*x)*43758.5453);
}

float doString( float soundTime )
{    
	float noteTime = frac( soundTime/4.0 )*4.0;
	float noteID   = floor( soundTime/4.0 );
	
	float w = 220.0;    
	float y = 0.0;
	
	float t = noteTime;  
	y += 0.5*frac( t*w*1.00 )                      *exp( -0.0010*w*t );
	y += 0.3*frac( t*w*(1.50+0.1*fmod(noteID,2.0)) )*exp( -0.0011*w*t );
	y += 0.2*frac( t*w*2.01 )                      *exp( -0.0012*w*t );

	y = -1.0 + 2.0*y;
	
	return 0.35*y;
}

float doChis( float soundTime )
{
	float inter = 0.5;
	float noteTime = frac( soundTime/inter )*inter;
	float noteID   = floor( soundTime/inter );
	return 0.5*noise( noteTime )*exp(-20.0*noteTime );
}

float doTom( float soundTime )
{
	float inter = 0.5;
    soundTime += inter*0.5;   
	
	float noteTime = frac( soundTime/inter )*inter;
	float f = 100.0 - 100.0*clamp(noteTime*2.0,0.0,1.0);
	return clamp( 70.0*sin(6.2831*f*noteTime)*exp(-15.0*noteTime ), 0.0, 1.0 );
}

// stereo sound output. x=left, y=right
float2 mainSound( float time )
{
	float2 y = float2( 0.0 );
	y += float2(0.75,0.25) * doString( time );    
	y += float2(0.50,0.50) * doChis( time );    
	y += float2(0.50,0.50) * doTom( time );
	
	return float2( y );
}