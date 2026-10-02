#pragma once

// MODE: 0 wide Mandelbrot, 1 wide Julia, 2 checked perturbation,
// 3 long-reference perturbation, 4 queued double restart.
// MORPH selects Julia's seed/parameter recurrence at compile time.
inline constexpr const char* kFractalCompute = R"GLSL(
layout(local_size_x=8, local_size_y=16) in;
layout(r32f, binding=0) uniform writeonly image2D escape_image;
layout(std430, binding=0) readonly buffer Orbit { vec2 reference[]; };
layout(std430, binding=1) buffer Queue { uint pixels[]; };
layout(std430, binding=2) buffer Counter { uint count; };
uniform ivec2 size;
uniform int limit, reference_length;
uniform dvec2 center, rotation, reference_offset, anchor, seed_shift, parameter_shift;
uniform double span, amount, seed_scale;

bool invalid(float x) { return isnan(x) || isinf(x); }
float smooth_value(int n, float r2) { return float(n)+2.0-log2(log2(r2)); }
dvec2 offset(ivec2 p) {
  dvec2 v=(dvec2(p)+dvec2(.5)-dvec2(size)*.5)*(span/double(size.x));
  return dvec2(rotation.x*v.x-rotation.y*v.y,rotation.y*v.x+rotation.x*v.y);
}
float restart(ivec2 p) {
  dvec2 v=offset(p);
#if MORPH
  dvec2 r=(center-anchor)+v;
  dvec2 z=anchor+seed_shift+r*seed_scale;
  dvec2 c=anchor+parameter_shift+r*(1.0-amount);
  int n=1;
#else
  dvec2 z=dvec2(0), c=center+v;
  int n=0;
#endif
  dvec2 z2=z*z;
  while(z2.x+z2.y<=256.0 && n<limit) {
    z=dvec2(z2.x-z2.y+c.x,2.0*z.x*z.y+c.y);
    z2=z*z; ++n;
  }
  // GLSL has no double log2. Escaping r2 is safe to narrow here.
  return n==limit ? -1.0 : smooth_value(n,float(z2.x+z2.y));
}
void main() {
#if MODE == 4
  for(uint i=gl_GlobalInvocationID.x+gl_GlobalInvocationID.y*1024u;
      i<count; i+=16384u) {
    uint index=pixels[i];
    ivec2 p=ivec2(index%uint(size.x),index/uint(size.x));
    imageStore(escape_image,p,vec4(restart(p)));
  }
#else
  ivec2 p=ivec2(gl_GlobalInvocationID.xy);
  if(any(greaterThanEqual(p,size))) return;
  float value=-1.0;
#if MODE <= 1
  vec2 v=vec2(p)+.5-vec2(size)*.5;
  vec2 rot=vec2(rotation);
  vec2 r=vec2(rot.x*v.x-rot.y*v.y,rot.y*v.x+rot.x*v.y);
#if MODE == 1
  dvec2 delta=center-anchor;
  vec2 z=vec2(anchor+seed_shift+delta*seed_scale)+r*float(span*seed_scale/double(size.x));
  vec2 c=vec2(anchor+parameter_shift+delta*(1.0-amount))+r*float(span*(1.0-amount)/double(size.x));
  int n=1;
#else
  vec2 c=vec2(center)+r*(float(span)/float(size.x));
  float cy2=c.y*c.y, q=(c.x-.25)*(c.x-.25)+cy2;
  if(q*(q+c.x-.25)<=.25*cy2 || (c.x+1.0)*(c.x+1.0)+cy2<=.0625) {
    imageStore(escape_image,p,vec4(-1)); return;
  }
  vec2 z=vec2(0); int n=0;
#endif
  vec2 z2=z*z;
  while(z2.x+z2.y<=256.0 && n<limit) {
    z=vec2(z2.x-z2.y+c.x,2.0*z.x*z.y+c.y);
    z2=z*z; ++n;
  }
  if(n<limit) value=smooth_value(n,z2.x+z2.y);
#else
  dvec2 relative=offset(p)-reference_offset;
#if MORPH
  vec2 dc=vec2(relative*(1.0-amount)), d=vec2(relative*seed_scale);
  int n=1;
#else
  vec2 dc=vec2(relative), d=vec2(0); int n=0;
#endif
  bool fallback=false; float r2=0;
  for(; n<limit; ++n) {
#if MODE == 2
    if(n>=reference_length) { fallback=true; break; }
#endif
    vec2 z=reference[n], actual=z+d;
    r2=dot(actual,actual);
    if(r2>256.0) { fallback=invalid(r2); break; }
    float ref2=dot(z,z);
    if(invalid(r2) || (ref2>1e-8 && r2<ref2*1.000001e-6)) { fallback=true; break; }
#if MODE == 2
    if(n+1>=reference_length) { fallback=true; break; }
#endif
    d=vec2(2.0*(z.x*d.x-z.y*d.y)+d.x*d.x-d.y*d.y+dc.x,
           2.0*(z.x*d.y+z.y*d.x+d.x*d.y)+dc.y);
  }
  if(fallback) {
#if MODE == 3
    pixels[atomicAdd(count,1u)]=uint(p.y*size.x+p.x); return;
#else
    value=restart(p);
#endif
  } else if(n<limit) value=smooth_value(n,r2);
#endif
  imageStore(escape_image,p,vec4(value));
#endif
}
)GLSL";

inline constexpr const char* kFoldCompute = R"GLSL(#version 430 core
layout(local_size_x=256) in;
layout(r32f,binding=0) uniform readonly image2D escape_image;
layout(std430,binding=3) buffer Detail { vec4 detail; };
uniform ivec2 size;
uniform int folds;
uniform float angle;
shared float low[256], high[256];
shared uint escaped[256];
void main() {
  uint i=gl_LocalInvocationID.x;
  float aspect=float(size.x)/float(size.y);
  vec2 v=(vec2(i%16u,i/16u)+.5)/16.0-.5;
  v.x*=aspect;
  float radius=length(v), sector=6.28318530718/float(folds);
  float folded=abs(mod(atan(v.y,v.x)-angle+sector*.5,sector)-sector*.5)+angle;
  float fit=.48*min(aspect,1.0)/sqrt((aspect*aspect+1.0)*.25);
  ivec2 p=clamp(ivec2((.5+radius*fit*vec2(cos(folded)/aspect,sin(folded)))*vec2(size)),ivec2(0),size-1);
  float value=imageLoad(escape_image,p).r;
  bool valid=value>=0 && !isnan(value) && !isinf(value);
  low[i]=valid?value:1e30; high[i]=valid?value:-1e30; escaped[i]=valid?1u:0u;
  barrier();
  for(uint step=128u;step>0u;step/=2u) {
    if(i<step) { low[i]=min(low[i],low[i+step]); high[i]=max(high[i],high[i+step]); escaped[i]+=escaped[i+step]; }
    barrier();
  }
  if(i==0u) detail=vec4(low[0],high[0],float(escaped[0]),256.0);
}
)GLSL";
