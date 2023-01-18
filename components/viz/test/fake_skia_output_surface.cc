// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/test/fake_skia_output_surface.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "components/viz/common/frame_sinks/copy_output_request.h"
#include "components/viz/common/frame_sinks/copy_output_util.h"
#include "components/viz/common/resources/resource_format_utils.h"
#include "components/viz/service/display/output_surface_client.h"
#include "components/viz/service/display/output_surface_frame.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "gpu/command_buffer/common/mailbox_holder.h"
#include "gpu/command_buffer/common/shared_image_usage.h"
#include "third_party/khronos/GLES2/gl2ext.h"
#include "third_party/skia/include/core/SkPixelRef.h"
#include "third_party/skia/include/gpu/GrBackendSurface.h"
#include "third_party/skia/include/gpu/GrDirectContext.h"
#include "third_party/skia/include/gpu/gl/GrGLTypes.h"
#include "ui/gfx/gpu_fence_handle.h"
#include "ui/gfx/presentation_feedback.h"
#include "ui/gfx/swap_result.h"
#include "ui/gl/gl_utils.h"

namespace viz {

FakeSkiaOutputSurface::FakeSkiaOutputSurface(
    scoped_refptr<ContextProvider> context_provider)
    : SkiaOutputSurface(SkiaOutputSurface::Type::kOpenGL),
      context_provider_(std::move(context_provider)) {}

FakeSkiaOutputSurface::~FakeSkiaOutputSurface() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

void FakeSkiaOutputSurface::BindToClient(OutputSurfaceClient* client) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(client);
  DCHECK(!client_);
  client_ = client;
}

void FakeSkiaOutputSurface::EnsureBackbuffer() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

void FakeSkiaOutputSurface::DiscardBackbuffer() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
}

void FakeSkiaOutputSurface::Reshape(const ReshapeParams& params) {
  auto& sk_surface = sk_surfaces_[AggregatedRenderPassId{0}];
  SkColorType color_type = kRGBA_8888_SkColorType;
  SkImageInfo image_info = SkImageInfo::Make(
      params.size.width(), params.size.height(), color_type,
      kPremul_SkAlphaType, params.color_space.ToSkColorSpace());
  sk_surface =
      SkSurface::MakeRenderTarget(gr_context(), SkBudgeted::kNo, image_info);

  DCHECK(sk_surface);
}

void FakeSkiaOutputSurface::SwapBuffers(OutputSurfaceFrame frame) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (frame.delegated_ink_metadata)
    last_delegated_ink_metadata_ = std::move(frame.delegated_ink_metadata);
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(&FakeSkiaOutputSurface::SwapBuffersAck,
                                weak_ptr_factory_.GetWeakPtr()));
}

void FakeSkiaOutputSurface::ScheduleOutputSurfaceAsOverlay(
    OverlayProcessorInterface::OutputSurfaceOverlayPlane output_surface_plane) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  NOTIMPLEMENTED();
}

bool FakeSkiaOutputSurface::IsDisplayedAsOverlayPlane() const {
  return false;
}

void FakeSkiaOutputSurface::SetNeedsSwapSizeNotifications(
    bool needs_swap_size_notifications) {
  NOTIMPLEMENTED();
}

void FakeSkiaOutputSurface::SetUpdateVSyncParametersCallback(
    UpdateVSyncParametersCallback callback) {
  NOTIMPLEMENTED();
}

gfx::OverlayTransform FakeSkiaOutputSurface::GetDisplayTransform() {
  return gfx::OVERLAY_TRANSFORM_NONE;
}

SkCanvas* FakeSkiaOutputSurface::BeginPaintCurrentFrame() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  auto& sk_surface = sk_surfaces_[AggregatedRenderPassId{0}];
  DCHECK(sk_surface);
  DCHECK_EQ(current_render_pass_id_, AggregatedRenderPassId{0u});
  return sk_surface->getCanvas();
}

void FakeSkiaOutputSurface::MakePromiseSkImage(ImageContext* image_context) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  if (image_context->has_image())
    return;

  GrBackendTexture backend_texture;
  if (!GetGrBackendTexture(*image_context, &backend_texture)) {
    DLOG(ERROR) << "Failed to GetGrBackendTexture from mailbox.";
    return;
  }

  auto sk_color_type = ResourceFormatToClosestSkColorType(
      true /* gpu_compositing */, image_context->resource_format());
  image_context->SetImage(
      SkImage::MakeFromTexture(gr_context(), backend_texture,
                               kTopLeft_GrSurfaceOrigin, sk_color_type,
                               image_context->alpha_type(),
                               image_context->color_space()),
      backend_texture.getBackendFormat());
}

sk_sp<SkImage> FakeSkiaOutputSurface::MakePromiseSkImageFromYUV(
    const std::vector<ImageContext*>& contexts,
    sk_sp<SkColorSpace> image_color_space,
    SkYUVAInfo::PlaneConfig plane_config,
    SkYUVAInfo::Subsampling subsampling) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  NOTIMPLEMENTED();
  return nullptr;
}

gpu::SyncToken FakeSkiaOutputSurface::ReleaseImageContexts(
    std::vector<std::unique_ptr<ImageContext>> image_contexts) {
  return gpu::SyncToken();
}

std::unique_ptr<ExternalUseClient::ImageContext>
FakeSkiaOutputSurface::CreateImageContext(
    const gpu::MailboxHolder& holder,
    const gfx::Size& size,
    ResourceFormat format,
    bool concurrent_reads,
    const absl::optional<gpu::VulkanYCbCrInfo>& ycbcr_info,
    sk_sp<SkColorSpace> color_space,
    bool raw_draw_if_possible) {
  return std::make_unique<ExternalUseClient::ImageContext>(
      holder, size, format, ycbcr_info, std::move(color_space));
}

SkCanvas* FakeSkiaOutputSurface::BeginPaintRenderPass(
    const AggregatedRenderPassId& id,
    const gfx::Size& surface_size,
    ResourceFormat format,
    bool mipmap,
    sk_sp<SkColorSpace> color_space,
    bool is_overlay,
    const gpu::Mailbox& mailbox) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  // Make sure there is no unsubmitted PaintFrame or PaintRenderPass.
  DCHECK_EQ(current_render_pass_id_, AggregatedRenderPassId{0u});
  auto& sk_surface = sk_surfaces_[id];

  if (!sk_surface) {
    SkColorType color_type =
        ResourceFormatToClosestSkColorType(true /* gpu_compositing */, format);
    SkImageInfo image_info = SkImageInfo::Make(
        surface_size.width(), surface_size.height(), color_type,
        kPremul_SkAlphaType, std::move(color_space));
    sk_surface =
        SkSurface::MakeRenderTarget(gr_context(), SkBudgeted::kNo, image_info);
  }
  return sk_surface->getCanvas();
}

SkCanvas* FakeSkiaOutputSurface::RecordOverdrawForCurrentPaint() {
  NOTIMPLEMENTED();
  return nullptr;
}

void FakeSkiaOutputSurface::EndPaint(
    base::OnceClosure on_finished,
    base::OnceCallback<void(gfx::GpuFenceHandle)> return_release_fence_cb) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  sk_surfaces_[current_render_pass_id_]->flushAndSubmit();
  current_render_pass_id_ = AggregatedRenderPassId{0};

  if (on_finished)
    std::move(on_finished).Run();
  if (return_release_fence_cb)
    std::move(return_release_fence_cb).Run(gfx::GpuFenceHandle());
}

sk_sp<SkImage> FakeSkiaOutputSurface::MakePromiseSkImageFromRenderPass(
    const AggregatedRenderPassId& id,
    const gfx::Size& size,
    ResourceFormat format,
    bool mipmap,
    sk_sp<SkColorSpace> color_space,
    const gpu::Mailbox& mailbox) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  auto it = sk_surfaces_.find(id);
  DCHECK(it != sk_surfaces_.end());
  return it->second->makeImageSnapshot();
}

void FakeSkiaOutputSurface::RemoveRenderPassResource(
    std::vector<AggregatedRenderPassId> ids) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!ids.empty());

  for (const auto& id : ids) {
    auto it = sk_surfaces_.find(id);
    DCHECK(it != sk_surfaces_.end());
    sk_surfaces_.erase(it);
  }
}

void FakeSkiaOutputSurface::CopyOutput(
    AggregatedRenderPassId id,
    const copy_output::RenderPassGeometry& geometry,
    const gfx::ColorSpace& color_space,
    std::unique_ptr<CopyOutputRequest> request,
    const gpu::Mailbox& mailbox) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);

  DCHECK(sk_surfaces_.find(id) != sk_surfaces_.end());
  auto* surface = sk_surfaces_[id].get();
  if (request->result_format() != CopyOutputResult::Format::RGBA ||
      request->is_scaled() ||
      geometry.result_bounds != geometry.result_selection) {
    // TODO(crbug.com/644851): Complete the implementation for all request
    // types, scaling, etc.
    NOTIMPLEMENTED();
    return;
  }

  if (request->result_destination() ==
      CopyOutputResult::Destination::kNativeTextures) {
    // TODO(rivr): This implementation is incomplete and doesn't copy
    // anything into the mailbox, but currently the only tests that use this
    // don't actually check the returned texture data.
    auto* sii = GetSharedImageInterface();
    gpu::Mailbox local_mailbox = sii->CreateSharedImage(
        ResourceFormat::RGBA_8888, geometry.result_selection.size(),
        color_space, kTopLeft_GrSurfaceOrigin, kPremul_SkAlphaType,
        gpu::SHARED_IMAGE_USAGE_GLES2, gpu::kNullSurfaceHandle);

    CopyOutputResult::ReleaseCallbacks release_callbacks;
    release_callbacks.push_back(base::BindPostTask(
        base::SequencedTaskRunnerHandle::Get(),
        base::BindOnce(&FakeSkiaOutputSurface::DestroyCopyOutputTexture,
                       weak_ptr_factory_.GetWeakPtr(), local_mailbox)));

    request->SendResult(std::make_unique<CopyOutputTextureResult>(
        CopyOutputResult::Format::RGBA, geometry.result_bounds,
        CopyOutputResult::TextureResult(local_mailbox, GenerateSyncToken(),
                                        color_space),
        std::move(release_callbacks)));
    return;
  }

  GrDirectContext* direct = GrAsDirectContext(gr_context());
  auto copy_image = surface->makeImageSnapshot()->makeSubset(
      RectToSkIRect(geometry.sampling_bounds), direct);
  // Send copy request by copying into a bitmap.
  SkBitmap bitmap;
  copy_image->asLegacyBitmap(&bitmap);
  // TODO(crbug.com/795132): Plumb color space throughout SkiaRenderer up to
  // the SkSurface/SkImage here. Until then, play "musical chairs" with the
  // SkPixelRef to hack-in the RenderPass's |color_space|.
  sk_sp<SkPixelRef> pixels(SkSafeRef(bitmap.pixelRef()));
  SkIPoint origin = bitmap.pixelRefOrigin();
  bitmap.setInfo(bitmap.info().makeColorSpace(color_space.ToSkColorSpace()),
                 bitmap.rowBytes());
  bitmap.setPixelRef(std::move(pixels), origin.x(), origin.y());
  request->SendResult(std::make_unique<CopyOutputSkBitmapResult>(
      geometry.result_bounds, std::move(bitmap)));
}

gpu::SharedImageInterface* FakeSkiaOutputSurface::GetSharedImageInterface() {
  return context_provider_->SharedImageInterface();
}

void FakeSkiaOutputSurface::AddContextLostObserver(
    ContextLostObserver* observer) {}

void FakeSkiaOutputSurface::RemoveContextLostObserver(
    ContextLostObserver* observer) {}

gpu::SyncToken FakeSkiaOutputSurface::Flush() {
  return GenerateSyncToken();
}

bool FakeSkiaOutputSurface::EnsureMinNumberOfBuffers(int n) {
  return false;
}

void FakeSkiaOutputSurface::SetOutOfOrderCallbacks(
    bool out_of_order_callbacks) {
  TestContextSupport* support =
      static_cast<TestContextSupport*>(context_provider()->ContextSupport());
  support->set_out_of_order_callbacks(out_of_order_callbacks);
}

gpu::SyncToken FakeSkiaOutputSurface::GenerateSyncToken() {
  gpu::SyncToken sync_token(
      gpu::CommandBufferNamespace::VIZ_SKIA_OUTPUT_SURFACE,
      gpu::CommandBufferId(), ++next_sync_fence_release_);
  sync_token.SetVerifyFlush();
  return sync_token;
}

bool FakeSkiaOutputSurface::GetGrBackendTexture(
    const ImageContext& image_context,
    GrBackendTexture* backend_texture) {
  DCHECK(!image_context.mailbox_holder().mailbox.IsZero());

  auto* gl = context_provider()->ContextGL();
  gl->WaitSyncTokenCHROMIUM(
      image_context.mailbox_holder().sync_token.GetConstData());
  auto texture_id = gl->CreateAndTexStorage2DSharedImageCHROMIUM(
      image_context.mailbox_holder().mailbox.name);
  auto gl_format = TextureStorageFormat(
      image_context.resource_format(),
      context_provider()->ContextCapabilities().angle_rgbx_internal_format);
  GrGLTextureInfo gl_texture_info = {
      image_context.mailbox_holder().texture_target, texture_id, gl_format};
  *backend_texture = GrBackendTexture(image_context.size().width(),
                                      image_context.size().height(),
                                      GrMipMapped::kNo, gl_texture_info);
  return true;
}

void FakeSkiaOutputSurface::SwapBuffersAck() {
  base::TimeTicks now = base::TimeTicks::Now();
  client_->DidReceiveSwapBuffersAck({now, now},
                                    /*release_fence=*/gfx::GpuFenceHandle());
  client_->DidReceivePresentationFeedback({now, base::TimeDelta(), 0});
}

void FakeSkiaOutputSurface::DestroyCopyOutputTexture(
    const gpu::Mailbox& mailbox,
    const gpu::SyncToken& sync_token,
    bool is_lost) {
  GetSharedImageInterface()->DestroySharedImage(sync_token, mailbox);
}

void FakeSkiaOutputSurface::ScheduleGpuTaskForTesting(
    base::OnceClosure callback,
    std::vector<gpu::SyncToken> sync_tokens) {
  NOTIMPLEMENTED();
}

void FakeSkiaOutputSurface::InitDelegatedInkPointRendererReceiver(
    mojo::PendingReceiver<gfx::mojom::DelegatedInkPointRenderer>
        pending_receiver) {
  delegated_ink_renderer_receiver_arrived_ = true;
}

}  // namespace viz