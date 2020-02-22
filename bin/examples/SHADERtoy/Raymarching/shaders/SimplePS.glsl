#version 330

out vec4 outColor;

uniform vec2 iResolution;
uniform vec2 iMouse;
uniform float iTime;

#define ROTATE false   
#define flag true

//---------------------------------------------------------
vec3 rotateX(vec3 p, float a)
{
  float sa = sin(a);
  float ca = cos(a);
  return vec3(p.x, ca * p.y - sa * p.z, sa * p.y + ca * p.z);
}
vec3 rotateY(vec3 p, float a)
{
  float sa = sin(a);
  float ca = cos(a);
  return vec3(ca * p.x + sa * p.z, p.y, -sa * p.x + ca * p.z);
}
vec3 rotateZ(vec3 p, float a)
{
  float sa = sin(a);
  float ca = cos(a);
  return vec3(ca * p.x - sa * p.y, sa * p.x + ca * p.y, p.z);
}

float length2( vec2 p )  // sqrt(x^2+y^2) 
{
  return sqrt( p.x*p.x + p.y*p.y );
}

float length6( vec2 p )  // (x^6+y^6)^(1/6)
{
  p = p*p*p; 
  p = p*p;
  return pow( p.x + p.y, 1.0/6.0 );
}

float length8( vec2 p )  // (x^8+y^8)^(1/8)
{
  p = p*p; 
  p = p*p; 
  p = p*p;
  return pow( p.x + p.y, 1.0/8.0 );
}

//---------------------------------------------------------
//  primitives
//---------------------------------------------------------
float sdPlane( vec3 p )
{
  return p.y;
}

float sdSphere( vec3 p, float radius )
{
  return length(p) - radius;
}

float sdWaveSphere(vec3 p, float radius, int waves, float waveSize) 
{	// deformation of radius
	float d = waveSize*(radius-length(p.y));
	radius += d*cos(atan(p.x,p.z)*float(waves));
	return length(p) - radius;
}

float sdBox( vec3 p, vec3 b )
{
  vec3 d = abs(p) - b;
  return min(max(d.x, max(d.y, d.z)), 0.0) + length(max(d, 0.0));
}

float udRoundBox( vec3 p, vec3 b, float r )
{
  return length(max(abs(p)-b, 0.0))-r;
}

// t.x = torus radius,  t.y = ring radius
float sdTorus( vec3 p, vec2 t )
{
  return length(vec2(length(p.xz)-t.x, p.y)) - t.y;
}

float sdTorus82( vec3 p, vec2 t )
{
  vec2 q = vec2(length2(p.xz)-t.x, p.y);
  return length8(q) - t.y;
}

float sdTorus88( vec3 p, vec2 t )
{
  vec2 q = vec2(length8(p.xz)-t.x, p.y);
  return length8(q)-t.y;
}

float sdBlob (vec3 pos, float r)
{
  vec3 v1 = pos * 6.0;
  return 0.05*(r + 0.5* (length(dot(v1, v1)) -0.51*(cos(4.*v1.x) +cos(4.*v1.y) +cos(4.*v1.z))));
}

// Capsule:  a,b = end points, r = cylinder radius
float sdCapsule( vec3 p, vec3 a, vec3 b, float r )
{
  vec3 pa = p-a, ba = b-a;
  float h = clamp( dot(pa, ba)/dot(ba, ba), 0.0, 1.0 );
  return length( pa - ba*h ) - r;
}

// Triangle prism: 
float sdTriPrism( vec3 p, float radius, float height )
{
  vec3 q = abs(p);
  #ifdef flag
    return max(q.z-height, max(q.x*0.866025 +p.y*0.5, -p.y) - radius*0.5);
  #else
    float d1 = q.z-height;
    float d2 = max(q.x*0.866025+p.y*0.5, -p.y) - radius*0.5;
    return length(max(vec2(d1, d2), 0.0)) + min(max(d1, d2), 0.0);
  #endif
}

// hexagonal prism:r 
float sdHexPrism( vec3 p, float radius, float height)
{
  vec3 q = abs(p);
  #ifdef flag
    return max(q.z-height, max((q.x*0.866025 +q.y*0.5), q.y) - radius);
  #else
    float d1 = q.z-heighty;
    float d2 = max((q.x*0.866025 +q.y*0.5), q.y)-radius;
    return length(max(vec2(d1, d2), 0.0)) + min(max(d1, d2), 0.);
  #endif
}

float sdCylinder( vec3 p, vec2 h )
{
  vec2 d = abs(vec2(length(p.xz), p.y)) - h;
  return min(max(d.x, d.y), 0.0) + length(max(d, 0.0));
}

float sdCylinder( vec3 p, vec3 h )
{
  return length(p.xz - h.xy) - h.z;
}
// h.xy = base rectangle size,  h.z = height
float sdCylinder6( vec3 p, vec3 h )
{
  return max( length6(p.xz) - h.x, abs(p.y) - h.z );
}

float sdCone( in vec3 p, in vec3 c )
{
  vec2 q = vec2( length(p.xz), p.y );
  #if 0
    return max( max( dot(q, c.xy), p.y), -p.y -c.z );
  #else
    float d1 = -p.y - c.z;
    float d2 = max( dot(q, c.xy), p.y);
    return length(max(vec2(d1, d2), 0.0)) + min(max(d1, d2), 0.);
  #endif
}

//----------------------------------------------------------------------
// distance operations
//----------------------------------------------------------------------

// Substraction: d1 -d2
float opS( float d1, float d2 )
{
  return max(-d2, d1);
}

// Union: d1 +d2
vec2 opU( vec2 d1, vec2 d2 )
{
  return (d1.x < d2.x) ? d1 : d2;
}

//----------------------------------------------------------------------
// domain operations
//----------------------------------------------------------------------

// Repetition: 
vec3 opRep( vec3 p, vec3 c )
{
  return mod(p, c)-0.5*c;
}

// Twist: 
vec3 opTwist( vec3 p, float angle )
{
  float  c = cos(10.0*p.y + angle);
  float  s = sin(10.0*p.y + angle);
  mat2   m = mat2(c, -s, s, c);
  return vec3(m*p.xz, p.y);
}

//----------------------------------------------------------------------
// sphere cutted out from a rounded box
//----------------------------------------------------------------------
float sdBoxMinusSphere( vec3 pos, float radius )
{
  return opS( udRoundBox( pos, vec3(0.15), 0.05)
            , sdSphere(   pos, radius - 0.012 + 0.02*sin(iTime)));
}
//----------------------------------------------------------------------
// rack-wheel with holes
//----------------------------------------------------------------------
float sdRackWheel( vec3 pos)
{
  return opS(sdTorus82(  pos-vec3(-2.0, 0.2, 0.0), vec2(0.20, 0.1)), 
             sdCylinder( opRep( vec3(atan(pos.x+2.0, pos.z)/6.2831 + 0.1*iTime, 
                                     pos.y, 
                                     0.02+0.5*length(pos-vec3(-2.0, 0.2, 0.0))), 
                                vec3(0.05, 1.0, 0.05)
							   )
                          , vec2(0.02, 0.6)
			           )
		    );
}
//----------------------------------------------------------------------
float sdBallyBall( vec3 pos)
{
  return 0.7 * sdSphere(pos, 0.2 ) 
         + 0.03*sin(50.0*pos.x)*sin(50.0*pos.y+8.0*iTime)*sin(50.0*pos.z);
}
//----------------------------------------------------------------------
float sdTwistedTorus( vec3 pos, float angle)
{
  return 0.5*sdTorus( opTwist(pos,angle), vec2(0.20, 0.05));
}
//----------------------------------------------------------------------
vec2 map( vec3 pos )
{
  vec3 r1, r2;
  float sphy = 0.35 + 0.1 * sin(iTime);
  vec3 sp = pos - vec3( 1.0, sphy, 0.0);
  vec2 res = vec2( sdPlane(     pos), 1.0 ); 
  res = opU( res, vec2( sdSphere(    sp, 0.25 ), 46.9 ) );
  res = opU( res, vec2( sdBlob(      pos-vec3( 0.0, 0.25, 0.0), -0.5 - 0.45*sin(iTime) ), 246.9 ) );

  res = opU( res, vec2( sdBox(       pos-vec3( 1.0, 0.25, 0.0), vec3(0.20) ), 3.0 ) );
  res = opU( res, vec2( udRoundBox(  pos-vec3( 1.0, 0.25, 1.0), vec3(0.12), 0.1 ), 41.0 ) );
                                     r1 = rotateX (pos - vec3( 0.0, 0.25, 1.0), sin(iTime)*0.3);
                                     r1 = rotateY (r1, iTime*2.0);
  res = opU( res, vec2( sdTorus(     r1, vec2(0.20, 0.05) ), 25.0 ) );
                                     r1 = vec3(-1.0, 0.2, 0.0) + rotateY (vec3(-0.15, 0.0, -0.15), iTime);
                                     r2 = vec3(-1.0, 0.2, 0.0) + rotateY (vec3(-0.15, 0.0, +0.15), iTime);
  res = opU( res, vec2( sdCapsule(   pos, r1, r2, 0.1  ), 31.9 ) );
  res = opU( res, vec2( sdTriPrism(  pos-vec3(-1.0, 0.25, -1.0), 0.1*sin(iTime) + 0.25, 0.05 ), 43.5 ) );
                                     float h1 = 0.2 + 0.06 * sin(iTime);
  res = opU( res, vec2( sdCylinder(  pos-vec3( 1.0, 0.30, -1.0), vec2(0.1, h1) ), 8.0 ) );
  res = opU( res, vec2( sdCone(      pos-vec3( 0.0, 0.50, -1.0), vec3(0.8, 0.5+0.1*sin(iTime), 0.3) ), 55.0 ) );
  res = opU( res, vec2( sdTorus82(   pos-vec3( 0.0, 0.25, 2.0), vec2(0.20, 0.05) ), 50.0 ) );
                                     r1 = rotateY (pos - vec3(-1.0, 0.25, 2.0), iTime*0.25);
  res = opU( res, vec2( sdTorus88(   r1, vec2(0.20, 0.05) ), 43.0 ) );
                                     r1 = rotateY (pos - vec3(1.0, 0.30, 2.0), iTime*0.25);
  res = opU( res, vec2( sdCylinder6( r1, vec3(0.1, 0.05, 0.25) ), 12.0 ) );
                                     r1 = rotateX (pos - vec3(-1.0, 0.25, 1.0), iTime*0.5);
  res = opU( res, vec2( sdHexPrism(  r1, 0.25, 0.05 ), 17.0 ) );
  res = opU( res, vec2( sdBoxMinusSphere(pos - vec3(-2.0, 0.2, 1.0), 0.25), 13.0 ) );
  res = opU( res, vec2( sdRackWheel   ( pos ), 51.0 ) );
  res = opU( res, vec2( sdTwistedTorus( pos - vec3(-2.0, 0.25, 2.0), 6.0*sin(iTime) ), 46.7 ) );
  res = opU( res, vec2( sdWaveSphere  ( pos - vec3(-2.0, 0.25, -1.0), 0.2 , 10, sin(iTime)*0.2), 880 ) );
// res = opU( res, vec2( sdBallyBall   ( pos - vec3(-2.0, 0.25, -1.0)), 65.0 ) );
  return res;
}

//----------------------------------------------------------------------
vec2 castRay( vec3 ro, vec3 rd )
{
  float tmin = 1.0;
  float tmax = 20.0;

  #if 0
    float tp1 = (0.0-ro.y) / rd.y; 
    if ( tp1>0.0 ) 
	  tmax = min( tmax, tp1 );
    float tp2 = (1.6-ro.y)/rd.y; 
    if ( tp2>0.0 ) 
    { 
      if ( ro.y>1.6 ) tmin = max( tmin, tp2 );
      else            tmax = min( tmax, tp2 );
    }
  #endif

  float precis = 0.002;
  float t = tmin;
  float m = -1.0;
  for ( int i=0; i<50; i++ )
  {
    vec2 res = map( ro+rd*t );
    if ( res.x<precis || t>tmax ) break;
    t += res.x;
    m = res.y;
  }

  if ( t>tmax ) m=-1.0;
  return vec2( t, m );
}

//----------------------------------------------------------------------
float softshadow( vec3 ro, vec3 rd, float mint, float tmax )
{
  float res = 1.0;
  float t = mint;
  for ( int i=0; i<16; i++ )
  {
    float h = map( ro + rd*t ).x;
    res = min( res, 8.0*h/t );
    t += clamp( h, 0.02, 0.10 );
    if ( h<0.001 || t>tmax ) break;
  }
  return clamp( res, 0.0, 1.0 );
}

//----------------------------------------------------------------------
vec3 calcNormal( vec3 pos )
{
  vec3 eps = vec3( 0.001, 0.0, 0.0 );
  vec3 nor = vec3(
  map(pos+eps.xyy).x - map(pos-eps.xyy).x, 
  map(pos+eps.yxy).x - map(pos-eps.yxy).x, 
  map(pos+eps.yyx).x - map(pos-eps.yyx).x );
  return normalize(nor);
}

//----------------------------------------------------------------------
float calcAO( vec3 pos, vec3 nor )
{
  float occ = 0.0;
  float sca = 1.0;
  for ( int i=0; i<5; i++ )
  {
    float hr = 0.01 + 0.12*float(i)/4.0;
    vec3 aopos =  nor * hr + pos;
    float dd = map( aopos ).x;
    occ += -(dd-hr)*sca;
    sca *= 0.95;
  }
  return clamp( 1.0 - 3.0*occ, 0.0, 1.0 );
}
//---------------------------------------------------------
vec3 render( vec3 ro, vec3 rd )
{ 
  vec3 col = vec3(0.8, 0.9, 1.0);
  vec2 res = castRay(ro, rd);
  float t = res.x;
  float m = res.y;
  if ( m>-0.5 )
  {
    vec3 pos = ro + t*rd;
    vec3 nor = calcNormal( pos );
    vec3 ref = reflect( rd, nor );

    // material        
    col = 0.45 + 0.3*sin( vec3(0.05, 0.08, 0.10)*(m-1.0) );

    if ( m<1.5 )
    {
      float f = mod( floor(5.0*pos.z) + floor(5.0*pos.x), 2.0);
      col = 0.4 + 0.1*f*vec3(1.0);
    }

    // lighting        
    float occ = calcAO( pos, nor );
    vec3  lig = normalize( vec3(-0.6, 0.7, -0.5) );
    float amb = clamp( 0.5+0.5*nor.y, 0.0, 1.0 );
    float dif = clamp( dot( nor, lig ), 0.0, 1.0 );
    float bac = clamp( dot( nor, normalize(vec3(-lig.x, 0.0, -lig.z))), 0.0, 1.0 )*clamp( 1.0-pos.y, 0.0, 1.0);
    float dom = smoothstep( -0.1, 0.1, ref.y );
    float fre = pow( clamp(1.0+dot(nor, rd), 0.0, 1.0), 2.0 );
    float spe = pow(clamp( dot( ref, lig ), 0.0, 1.0 ), 16.0);

    dif *= softshadow( pos, lig, 0.02, 2.5 );
    dom *= softshadow( pos, ref, 0.02, 2.5 );

    vec3 brdf = vec3(0.0);
    brdf += 1.20*dif*vec3(1.00, 0.90, 0.60);
    brdf += 1.20*spe*vec3(1.00, 0.90, 0.60)*dif;
    brdf += 0.30*amb*vec3(0.50, 0.70, 1.00)*occ;
    brdf += 0.40*dom*vec3(0.50, 0.70, 1.00)*occ;
    brdf += 0.30*bac*vec3(0.25, 0.25, 0.25)*occ;
    brdf += 0.40*fre*vec3(1.00, 1.00, 1.00)*occ;
    brdf += 0.02;
    col = col*brdf;
    col = mix( col, vec3(0.8, 0.9, 1.0), 1.0-exp( -0.005*t*t ) );
  }
  return vec3( clamp(col, 0.0, 1.0) );
}
//---------------------------------------------------------
void main( )
{
  vec2 p = 2.0*(gl_FragCoord.xy / iResolution.xy) - 1.0;
  p.x *= iResolution.x / iResolution.y;
  vec2 mo = iMouse.xy;

  // camera  
  float angle = ROTATE ? 0.1*iTime : 0.0;
  float rx = -0.5 + 3.2*cos(angle + 6.0*mo.x);
  float rz =  0.5 + 3.2*sin(angle + 6.0*mo.x);
  vec3 ro = vec3( rx, 1.0 + 2.0*mo.y, rz );
  vec3 ta = vec3( -0.5, -0.4, 0.5 );

  // camera tx
  vec3 cw = normalize( ta-ro );
  vec3 cp = vec3( 0.0, 1.0, 0.0 );
  vec3 cu = normalize( cross(cw, cp) );
  vec3 cv = normalize( cross(cu, cw) );
  vec3 rd = normalize( p.x*cu + p.y*cv + 2.5*cw );

  // pixel color
  vec3 col = render( ro, rd );
  col = pow( col, vec3(0.4545) );
  outColor=vec4( col, 1.0 );
}