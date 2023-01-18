// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_SKIA_GL_IMAGE_REPRESENTATION_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_SKIA_GL_IMAGE_REPRESENTATION_H_

#include "base/memory/raw_ptr.h"
#include "gpu/command_buffer/service/shared_image/shared_image_representation.h"
#include "ui/gl/gl_context.h"

namespace gpu {
// This is a wrapper class for SkiaImageRepresentation to be used in GL
// mode. For most of the SharedImageBackings, GLTextureImageRepresentation
// and SkiaImageRepresentation implementations do the same work which
// results in duplicate code. Hence instead of implementing
// SkiaImageRepresentation, this wrapper can be directly used or
// implemented by the backings.
class GPU_GLES2_EXPORT SkiaGLImageRepresentation
    : public SkiaImageRepresentation {
 public:
  static std::unique_ptr<SkiaGLImageRepresentation> Create(
      std::unique_ptr<GLTextureImageRepresentationBase> gl_representation,
      scoped_refptr<SharedContextState> context_state,
      SharedImageManager* manager,
      SharedImageBacking* backing,
      MemoryTypeTracker* tracker);

  ~SkiaGLImageRepresentation() override;

  sk_sp<SkSurface> BeginWriteAccess(
      int final_msaa_count,
      const SkSurfaceProps& surface_props,
      std::vector<GrBackendSemaphore>* begin_semaphores,
      std::vector<GrBackendSemaphore>* end_semaphores) override;
  sk_sp<SkPromiseImageTexture> BeginWriteAccess(
      std::vector<GrBackendSemaphore>* begin_semaphores,
      std::vector<GrBackendSemaphore>* end_semaphores,
      std::unique_ptr<GrBackendSurfaceMutableState>* end_state) override;
  void EndWriteAccess(sk_sp<SkSurface> surface) override;
  sk_sp<SkPromiseImageTexture> BeginReadAccess(
      std::vector<GrBackendSemaphore>* begin_semaphores,
      std::vector<GrBackendSemaphore>* end_semaphores) override;
  void EndReadAccess() override;

  bool SupportsMultipleConcurrentReadAccess() override;

 private:
  SkiaGLImageRepresentation(
      std::unique_ptr<GLTextureImageRepresentationBase> gl_representation,
      sk_sp<SkPromiseImageTexture> promise_texture,
      scoped_refptr<SharedContextState> context_state,
      SharedImageManager* manager,
      SharedImageBacking* backing,
      MemoryTypeTracker* tracker);

  void CheckContext();

  std::unique_ptr<GLTextureImageRepresentationBase> gl_representation_;
  sk_sp<SkPromiseImageTexture> promise_texture_;
  scoped_refptr<SharedContextState> context_state_;
  sk_sp<SkSurface> surface_;
  RepresentationAccessMode mode_ = RepresentationAccessMode::kNone;
#if DCHECK_IS_ON()
  raw_ptr<gl::GLContext> context_;
#endif
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_SKIA_GL_IMAGE_REPRESENTATION_H_