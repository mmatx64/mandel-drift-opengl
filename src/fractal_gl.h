// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>
#include <SDL3/SDL_opengl_glext.h>
#include "julia_transition.h"
#include <array>
#include <vector>

struct OrbitPoint { float x, y; };
static_assert(sizeof(OrbitPoint) == 8, "std430 vec2 orbit layout must be 8 bytes");

// Context-owned OpenGL 4.3 renderer. No vendor SDK or CPU image upload.
class FractalGL {
 public:
  FractalGL();
  ~FractalGL();
  FractalGL(const FractalGL&) = delete;
  FractalGL& operator=(const FractalGL&) = delete;
  void resize(int width, int height);
  GLuint texture() const { return texture_; }
  void upload_reference(const std::vector<OrbitPoint>& orbit);
  void render(int width, int height, double x, double y, double span, double angle,
      int iterations, bool deep, double ref_dx, double ref_dy,
      int reference_length, JuliaMorph morph = {}, bool force_checked = false);
  std::array<float, 4> fold_detail(int width, int height, int folds, float angle);
  std::vector<float> read(int width, int height) const;
  void allocate_queue();
  void release_queue();
  bool has_queue() const { return queue_ != 0; }
  unsigned int fallback_count() const;
 private:
  struct Api;
  Api* gl_;
  GLuint programs_[8]{};
  GLuint fold_program_ = 0;
  GLuint texture_ = 0, orbit_ = 0, queue_ = 0, count_ = 0, detail_ = 0;
  int capacity_width_ = 0, capacity_height_ = 0;
  bool last_queued_ = false;
  void uniforms(GLuint program, int width, int height, double x, double y,
      double span, double angle, int iterations, double ref_dx, double ref_dy,
      int reference_length, JuliaMorph morph);
};
