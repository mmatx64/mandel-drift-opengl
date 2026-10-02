#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include "fractal_gl.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
struct Scene { const char* name; double x,y,span; bool deep,sensitive; JuliaMorph morph{}; int ref_limit=2201; double angle=.37; };
double oracle(double zx,double zy,double cx,double cy,int n,int limit) {
  while(zx*zx+zy*zy<=256 && n<limit) {
    double next=zx*zx-zy*zy+cx; zy=2*zx*zy+cy; zx=next; ++n;
  }
  return n==limit ? -1 : n+1-std::log2(std::log2(std::hypot(zx,zy)));
}
std::vector<OrbitPoint> reference(const Scene& s) {
  double x=0,y=0; std::vector<OrbitPoint> ref;
  double ax=s.morph.amount>0?s.morph.x:s.x, ay=s.morph.amount>0?s.morph.y:s.y;
  for(int i=0;i<s.ref_limit;++i) {
    ref.push_back({float(x),float(y)}); if(x*x+y*y>256) break;
    if(i==0) { x=ax+s.morph.seed_shift_x; y=ay+s.morph.seed_shift_y; }
    else { double next=x*x-y*y+ax+s.morph.parameter_shift_x; y=2*x*y+ay+s.morph.parameter_shift_y; x=next; }
  }
  return ref;
}
void run(FractalGL& gpu,const Scene& s,int w,int h,bool benchmark) {
  const int bench_w=w,bench_h=h;
  if(benchmark) { w=97; h=61; }
  const int limit=std::clamp(260+int(58*std::log2(3.2/s.span)),260,s.deep?2200:850);
  const double angle=s.angle;
  auto ref=reference(s); gpu.upload_reference(ref);
  double dx=s.morph.amount>0?s.morph.x-s.x:0,dy=s.morph.amount>0?s.morph.y-s.y:0;
  auto launch=[&](bool checked=false){gpu.render(w,h,s.x,s.y,s.span,angle,limit,s.deep,dx,dy,int(ref.size()),s.morph,checked);};
  launch(); auto result=gpu.read(w,h);
  const unsigned int queued=gpu.fallback_count();
  launch(true); auto checked=gpu.read(w,h);
  if(result!=checked) throw std::runtime_error(std::string(s.name)+": queued/checked paths disagree");
  int classifications=0,outliers=0,escaped=0; double max_error=0;
  for(int i=0;i<w*h;++i) {
    double x=(i%w+.5-w*.5)*s.span/w, y=(i/w+.5-h*.5)*s.span/w;
    double ox=std::cos(angle)*x-std::sin(angle)*y,oy=std::sin(angle)*x+std::cos(angle)*y;
    double expected;
    auto m=s.morph;
    if(m.amount>0) {
      double rx=s.x-m.x+ox, ry=s.y-m.y+oy;
      expected=oracle(m.x+m.seed_shift_x+rx*m.seed_scale,m.y+m.seed_shift_y+ry*m.seed_scale,
          m.x+m.parameter_shift_x+rx*(1-m.amount),m.y+m.parameter_shift_y+ry*(1-m.amount),1,limit);
    } else expected=oracle(0,0,s.x+ox,s.y+oy,0,limit);
    if(!std::isfinite(result[i])) throw std::runtime_error("Non-finite escape value");
    escaped+=result[i]>=0;
    if((result[i]<0)!=(expected<0)) ++classifications;
    else if(expected>=0) {
      double error=std::abs(result[i]-expected); max_error=std::max(max_error,error);
      if(error>.035) ++outliers;
    }
  }
  std::printf("%s %dx%d: escaped=%d queued=%u oracle classification=%d outliers=%d max_error=%.6g%s\n",
      s.name,w,h,escaped,queued,classifications,outliers,max_error,s.sensitive?" (boundary diagnostic)":"");
  if(!s.sensitive && (classifications || outliers)) throw std::runtime_error("Stable CPU oracle mismatch");
  if(s.sensitive && (!escaped || *std::max_element(result.begin(),result.end())-*std::min_element(result.begin(),result.end())<1)) throw std::runtime_error("Detail scene became flat");
  if(benchmark && s.ref_limit==2201) {
    w=bench_w; h=bench_h;
    auto gen=reinterpret_cast<PFNGLGENQUERIESPROC>(SDL_GL_GetProcAddress("glGenQueries"));
    auto begin=reinterpret_cast<PFNGLBEGINQUERYPROC>(SDL_GL_GetProcAddress("glBeginQuery"));
    auto end=reinterpret_cast<PFNGLENDQUERYPROC>(SDL_GL_GetProcAddress("glEndQuery"));
    auto get=reinterpret_cast<PFNGLGETQUERYOBJECTUI64VPROC>(SDL_GL_GetProcAddress("glGetQueryObjectui64v"));
    auto del=reinterpret_cast<PFNGLDELETEQUERIESPROC>(SDL_GL_GetProcAddress("glDeleteQueries"));
    GLuint q=0; gen(1,&q); std::vector<double> ms;
    for(int i=0;i<8;++i) launch(); glFinish();
    for(int i=0;i<31;++i) { begin(GL_TIME_ELAPSED,q); launch(); end(GL_TIME_ELAPSED); GLuint64 ns=0; get(q,GL_QUERY_RESULT,&ns); ms.push_back(ns/1e6); }
    std::sort(ms.begin(),ms.end()); del(1,&q);
    std::printf("BENCH %s median=%.3fms p95=%.3fms (compute only, full resolution)\n",s.name,ms[15],ms[29]);
  }
  if(glGetError()!=GL_NO_ERROR) throw std::runtime_error("OpenGL error in numerical test");
}
}
int main(int argc,char** argv) {
  try {
    bool bench=argc>1 && std::string(argv[1])=="--benchmark";
    if(!SDL_Init(SDL_INIT_VIDEO)) throw std::runtime_error(SDL_GetError());
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,4); SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_Window* window=SDL_CreateWindow("OpenGL renderer validation",64,64,SDL_WINDOW_OPENGL|SDL_WINDOW_HIDDEN);
    if(!window) throw std::runtime_error(SDL_GetError());
    SDL_GLContext context=SDL_GL_CreateContext(window);
    if(!context) throw std::runtime_error(SDL_GetError());
    SDL_GL_MakeCurrent(window,context);
    std::printf("OpenGL %s | %s | %s\n",glGetString(GL_VERSION),glGetString(GL_VENDOR),glGetString(GL_RENDERER));
    {
      FractalGL gpu; int w=bench?1921:97,h=bench?1081:61; gpu.resize(w+16,h+8);
      const Scene scenes[]={
        {"wide",-.65,0,3.2,false,true,{},2201,0},
        {"exterior",1.5,.4,.2,false,false},
        {"interior",-.1,0,.1,false,false},
        {"deep-exterior",1.5,.4,.00003,true,false},
        {"deep-interior",0,0,.00003,true,false},
        {"deep-entry",-.743643887037151,.131825904205330,.004,true,true,{},2201,.2},
        {"deep-mid",-.743643887037151,.131825904205330,.0006,true,true,{},2201,.5},
        {"deep-hold",-.743643887037151,.131825904205330,.00003,true,true,{},2201,.8},
        {"deep-mirror",-.743643887037151,-.131825904205330,.00003,true,true,{},2201,-.8},
        {"short-reference",-.743643887037151,.131825904205330,.00003,true,true,{},8},
        {"empty-reference",1.5,.4,.00003,true,false,{},0},
        {"julia-stable",0,0,.00003,true,false,julia_morph(.1,.00003,0,0)},
        {"julia-deep",-.743643887037151,.131825904205330,.00003,true,true,julia_morph(.001,.00003,-.743643887037151,.131825904205330)},
        {"julia-wide",-.743643887037151,.131825904205330,.00003,true,true,julia_morph(1,.00003,-.743643887037151,.131825904205330)},
        {"julia-drift",-.743643887037151,.131825904205330,.00003,true,true,julia_morph(1,.00003,-.743643887037151,.131825904205330,kJuliaPlateauStart+3.5)}
      };
      for(const auto& s:scenes) run(gpu,s,w,h,bench);
      // Verify the reduction against CPU sampling of the actual GPU image,
      // independently of escape-time arithmetic and its boundary sensitivity.
      auto image=gpu.read(w,h);
      for(int folds:{3,5,7}) {
        auto detail=gpu.fold_detail(w,h,folds,.37f);
        float low=1e30f,high=-1e30f; int escaped=0;
        const float aspect=float(w)/h,sector=6.28318530718f/folds;
        for(int i=0;i<256;++i) {
          const float x=((i%16+.5f)/16-.5f)*aspect,y=(i/16+.5f)/16-.5f;
          float wrapped=std::fmod(std::atan2(y,x)-.37f+sector*.5f,sector);
          if(wrapped<0) wrapped+=sector;
          const float folded=std::abs(wrapped-sector*.5f)+.37f;
          const float radius=std::hypot(x,y),fit=.48f*std::min(aspect,1.f)/std::sqrt((aspect*aspect+1)*.25f);
          int px=std::clamp(int((.5f+radius*fit*std::cos(folded)/aspect)*w),0,w-1);
          int py=std::clamp(int((.5f+radius*fit*std::sin(folded))*h),0,h-1);
          float value=image[py*w+px];
          if(value>=0 && std::isfinite(value)) { ++escaped; low=std::min(low,value); high=std::max(high,value); }
        }
        if(detail[0]!=low || detail[1]!=high || detail[2]!=escaped || detail[3]!=256)
          throw std::runtime_error("Fold footprint/reduction differs from CPU sampling");
      }
      gpu.release_queue(); if(gpu.has_queue()) throw std::runtime_error("Queue release failed");
      gpu.resize(49,31); run(gpu,scenes[7],49,31,false);
    }
    SDL_GL_DestroyContext(context); SDL_DestroyWindow(window); SDL_Quit(); return 0;
  } catch(const std::exception& e) { std::fprintf(stderr,"OpenGL validation: %s\n",e.what()); SDL_Quit(); return 1; }
}
