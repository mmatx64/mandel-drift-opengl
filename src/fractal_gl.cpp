#include "fractal_gl.h"
#include "fractal_shaders.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <string>

namespace {
template<class T> T load(const char* name) {
  auto p=SDL_GL_GetProcAddress(name);
  if(!p) throw std::runtime_error(std::string("OpenGL 4.3 function unavailable: ")+name);
  return reinterpret_cast<T>(p);
}
}
struct FractalGL::Api {
#define GL_ENTRY(type, name) type name=load<type>("gl" #name)
  GL_ENTRY(PFNGLCREATESHADERPROC, CreateShader);
  GL_ENTRY(PFNGLSHADERSOURCEPROC, ShaderSource);
  GL_ENTRY(PFNGLCOMPILESHADERPROC, CompileShader);
  GL_ENTRY(PFNGLGETSHADERIVPROC, GetShaderiv);
  GL_ENTRY(PFNGLGETSHADERINFOLOGPROC, GetShaderInfoLog);
  GL_ENTRY(PFNGLDELETESHADERPROC, DeleteShader);
  GL_ENTRY(PFNGLCREATEPROGRAMPROC, CreateProgram);
  GL_ENTRY(PFNGLATTACHSHADERPROC, AttachShader);
  GL_ENTRY(PFNGLLINKPROGRAMPROC, LinkProgram);
  GL_ENTRY(PFNGLGETPROGRAMIVPROC, GetProgramiv);
  GL_ENTRY(PFNGLGETPROGRAMINFOLOGPROC, GetProgramInfoLog);
  GL_ENTRY(PFNGLDELETEPROGRAMPROC, DeleteProgram);
  GL_ENTRY(PFNGLUSEPROGRAMPROC, UseProgram);
  GL_ENTRY(PFNGLGENBUFFERSPROC, GenBuffers);
  GL_ENTRY(PFNGLBINDBUFFERPROC, BindBuffer);
  GL_ENTRY(PFNGLBINDBUFFERBASEPROC, BindBufferBase);
  GL_ENTRY(PFNGLBUFFERDATAPROC, BufferData);
  GL_ENTRY(PFNGLBUFFERSUBDATAPROC, BufferSubData);
  GL_ENTRY(PFNGLGETBUFFERSUBDATAPROC, GetBufferSubData);
  GL_ENTRY(PFNGLDELETEBUFFERSPROC, DeleteBuffers);
  GL_ENTRY(PFNGLBINDIMAGETEXTUREPROC, BindImageTexture);
  GL_ENTRY(PFNGLDISPATCHCOMPUTEPROC, DispatchCompute);
  GL_ENTRY(PFNGLMEMORYBARRIERPROC, MemoryBarrier);
  GL_ENTRY(PFNGLGETUNIFORMLOCATIONPROC, GetUniformLocation);
  GL_ENTRY(PFNGLUNIFORM1IPROC, Uniform1i);
  GL_ENTRY(PFNGLUNIFORM2IPROC, Uniform2i);
  GL_ENTRY(PFNGLUNIFORM1FPROC, Uniform1f);
  GL_ENTRY(PFNGLUNIFORM1DPROC, Uniform1d);
  GL_ENTRY(PFNGLUNIFORM2DPROC, Uniform2d);
#undef GL_ENTRY
  GLuint compile(const std::string& source) {
    GLuint shader=CreateShader(GL_COMPUTE_SHADER);
    const char* text=source.c_str(); ShaderSource(shader,1,&text,nullptr); CompileShader(shader);
    GLint ok=0; GetShaderiv(shader,GL_COMPILE_STATUS,&ok);
    if(!ok) {
      char log[8192]{}; GetShaderInfoLog(shader,sizeof(log),nullptr,log); DeleteShader(shader);
      throw std::runtime_error(std::string("Fractal compute shader: ")+log);
    }
    GLuint program=CreateProgram(); AttachShader(program,shader); LinkProgram(program); DeleteShader(shader);
    GetProgramiv(program,GL_LINK_STATUS,&ok);
    if(!ok) {
      char log[8192]{}; GetProgramInfoLog(program,sizeof(log),nullptr,log); DeleteProgram(program);
      throw std::runtime_error(std::string("Fractal compute program: ")+log);
    }
    return program;
  }
};

FractalGL::FractalGL() : gl_(new Api) {
  // Specialized shader variants keep wide views free of double restart work.
  try {
    const int modes[]={0,1,2,3,2,3,4,4};
    for(int i=0;i<8;++i)
      programs_[i]=gl_->compile("#version 430 core\n#define MODE "+std::to_string(modes[i])+
          "\n#define MORPH "+std::to_string(i==1 || i==4 || i==5 || i==7)+"\n"+kFractalCompute);
    fold_program_=gl_->compile(kFoldCompute);
    gl_->GenBuffers(1,&orbit_); gl_->BindBuffer(GL_SHADER_STORAGE_BUFFER,orbit_);
    gl_->BufferData(GL_SHADER_STORAGE_BUFFER,2201*sizeof(OrbitPoint),nullptr,GL_DYNAMIC_DRAW);
    gl_->GenBuffers(1,&detail_); gl_->BindBuffer(GL_SHADER_STORAGE_BUFFER,detail_);
    gl_->BufferData(GL_SHADER_STORAGE_BUFFER,4*sizeof(float),nullptr,GL_DYNAMIC_READ);
  } catch(...) {
    for(auto p:programs_) if(p) gl_->DeleteProgram(p);
    if(fold_program_) gl_->DeleteProgram(fold_program_);
    if(orbit_) gl_->DeleteBuffers(1,&orbit_);
    if(detail_) gl_->DeleteBuffers(1,&detail_);
    delete gl_; throw;
  }
}
FractalGL::~FractalGL() {
  release_queue();
  if(texture_) glDeleteTextures(1,&texture_);
  gl_->DeleteBuffers(1,&orbit_); gl_->DeleteBuffers(1,&detail_);
  for(auto p:programs_) gl_->DeleteProgram(p);
  gl_->DeleteProgram(fold_program_); delete gl_;
}
void FractalGL::resize(int width,int height) {
  if(width==capacity_width_ && height==capacity_height_) return;
  if(width<=0 || height<=0) throw std::runtime_error("Invalid fractal dimensions");
  release_queue();
  if(texture_) glDeleteTextures(1,&texture_);
  capacity_width_=width; capacity_height_=height;
  glGenTextures(1,&texture_); glBindTexture(GL_TEXTURE_2D,texture_);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
  glTexImage2D(GL_TEXTURE_2D,0,GL_R32F,width,height,0,GL_RED,GL_FLOAT,nullptr);
}
void FractalGL::upload_reference(const std::vector<OrbitPoint>& orbit) {
  if(orbit.size()>2201) throw std::runtime_error("Reference exceeds orbit capacity");
  gl_->BindBuffer(GL_SHADER_STORAGE_BUFFER,orbit_);
  gl_->BufferSubData(GL_SHADER_STORAGE_BUFFER,0,orbit.size()*sizeof(OrbitPoint),orbit.data());
}
void FractalGL::allocate_queue() {
  if(queue_) return;
  gl_->GenBuffers(1,&queue_); gl_->BindBuffer(GL_SHADER_STORAGE_BUFFER,queue_);
  gl_->BufferData(GL_SHADER_STORAGE_BUFFER,static_cast<GLsizeiptr>(capacity_width_)*capacity_height_*sizeof(unsigned int),nullptr,GL_DYNAMIC_DRAW);
  gl_->GenBuffers(1,&count_); gl_->BindBuffer(GL_SHADER_STORAGE_BUFFER,count_);
  gl_->BufferData(GL_SHADER_STORAGE_BUFFER,sizeof(unsigned int),nullptr,GL_DYNAMIC_DRAW);
}
void FractalGL::release_queue() {
  if(queue_) gl_->DeleteBuffers(1,&queue_);
  if(count_) gl_->DeleteBuffers(1,&count_);
  queue_=count_=0;
}
void FractalGL::uniforms(GLuint p,int w,int h,double x,double y,double s,double a,
    int iterations,double dx,double dy,int length,JuliaMorph m) {
  gl_->UseProgram(p);
  auto loc=[&](const char* name){return gl_->GetUniformLocation(p,name);};
  gl_->Uniform2i(loc("size"),w,h); gl_->Uniform1i(loc("limit"),iterations);
  gl_->Uniform1i(loc("reference_length"),length);
  gl_->Uniform2d(loc("center"),x,y); gl_->Uniform2d(loc("rotation"),std::cos(a),std::sin(a));
  gl_->Uniform2d(loc("reference_offset"),dx,dy); gl_->Uniform1d(loc("span"),s);
  gl_->Uniform2d(loc("anchor"),m.x,m.y); gl_->Uniform1d(loc("amount"),m.amount);
  gl_->Uniform1d(loc("seed_scale"),m.seed_scale);
  gl_->Uniform2d(loc("seed_shift"),m.seed_shift_x,m.seed_shift_y);
  gl_->Uniform2d(loc("parameter_shift"),m.parameter_shift_x,m.parameter_shift_y);
}
void FractalGL::render(int w,int h,double x,double y,double s,double a,int iterations,
    bool deep,double dx,double dy,int length,JuliaMorph m,bool force_checked) {
  if(w<=0 || h<=0 || w>capacity_width_ || h>capacity_height_ || iterations<1 || iterations>2200 || length<0 || length>2201)
    throw std::runtime_error("Invalid fractal dispatch");
  bool morph=m.amount>0 || m.seed_scale!=1 || m.seed_shift_x!=0 || m.seed_shift_y!=0 || m.parameter_shift_x!=0 || m.parameter_shift_y!=0;
  const bool wide=morph && s*m.seed_scale>=kJuliaFloatSpan;
  const bool queued=(deep || morph) && !wide && length>iterations && !force_checked;
  last_queued_ = queued;
  int program=wide?1:!deep && !morph?0:(morph?4:2)+(queued?1:0);
  if(queued) {
    allocate_queue();
    // Previous shader writes must finish before the API resets this counter.
    gl_->MemoryBarrier(GL_BUFFER_UPDATE_BARRIER_BIT);
    unsigned int zero=0; gl_->BindBuffer(GL_SHADER_STORAGE_BUFFER,count_);
    gl_->BufferSubData(GL_SHADER_STORAGE_BUFFER,0,sizeof(zero),&zero);
  }
  gl_->BindBufferBase(GL_SHADER_STORAGE_BUFFER,0,orbit_);
  gl_->BindBufferBase(GL_SHADER_STORAGE_BUFFER,1,queue_);
  gl_->BindBufferBase(GL_SHADER_STORAGE_BUFFER,2,count_);
  gl_->BindImageTexture(0,texture_,0,GL_FALSE,0,GL_WRITE_ONLY,GL_R32F);
  // Prior texture fetches/readbacks must precede new image stores.
  gl_->MemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
  uniforms(programs_[program],w,h,x,y,s,a,iterations,dx,dy,length,m);
  gl_->DispatchCompute((w+7)/8,(h+15)/16,1);
  if(queued) {
    gl_->MemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT|GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
    uniforms(programs_[morph?7:6],w,h,x,y,s,a,iterations,dx,dy,length,m);
    gl_->DispatchCompute(128,1,1);
  }
  gl_->MemoryBarrier(GL_TEXTURE_FETCH_BARRIER_BIT|GL_SHADER_IMAGE_ACCESS_BARRIER_BIT|GL_TEXTURE_UPDATE_BARRIER_BIT);
}
std::array<float,4> FractalGL::fold_detail(int w,int h,int folds,float angle) {
  gl_->UseProgram(fold_program_);
  gl_->Uniform2i(gl_->GetUniformLocation(fold_program_,"size"),w,h);
  gl_->Uniform1i(gl_->GetUniformLocation(fold_program_,"folds"),folds);
  gl_->Uniform1f(gl_->GetUniformLocation(fold_program_,"angle"),angle);
  gl_->BindImageTexture(0,texture_,0,GL_FALSE,0,GL_READ_ONLY,GL_R32F);
  gl_->BindBufferBase(GL_SHADER_STORAGE_BUFFER,3,detail_); gl_->DispatchCompute(1,1,1);
  gl_->MemoryBarrier(GL_BUFFER_UPDATE_BARRIER_BIT);
  std::array<float,4> result{}; gl_->BindBuffer(GL_SHADER_STORAGE_BUFFER,detail_);
  gl_->GetBufferSubData(GL_SHADER_STORAGE_BUFFER,0,sizeof(result),result.data()); return result;
}
std::vector<float> FractalGL::read(int w,int h) const {
  gl_->MemoryBarrier(GL_TEXTURE_UPDATE_BARRIER_BIT);
  std::vector<float> capacity(static_cast<size_t>(capacity_width_)*capacity_height_), active(static_cast<size_t>(w)*h);
  glBindTexture(GL_TEXTURE_2D,texture_); glGetTexImage(GL_TEXTURE_2D,0,GL_RED,GL_FLOAT,capacity.data());
  for(int y=0;y<h;++y) std::copy_n(capacity.data()+y*capacity_width_,w,active.data()+y*w);
  return active;
}
unsigned int FractalGL::fallback_count() const {
  unsigned int value=0;
  if(count_ && last_queued_) { gl_->MemoryBarrier(GL_BUFFER_UPDATE_BARRIER_BIT); gl_->BindBuffer(GL_SHADER_STORAGE_BUFFER,count_); gl_->GetBufferSubData(GL_SHADER_STORAGE_BUFFER,0,sizeof(value),&value); }
  return value;
}
