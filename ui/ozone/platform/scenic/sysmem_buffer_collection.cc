// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/scenic/sysmem_buffer_collection.h"

#include <fuchsia/sysmem/cpp/fidl.h>

#include <tuple>

#include "base/bits.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "build/build_config.h"
#include "gpu/vulkan/vulkan_device_queue.h"
#include "gpu/vulkan/vulkan_function_pointers.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/ozone/platform/scenic/scenic_surface_factory.h"
#include "ui/ozone/platform/scenic/sysmem_native_pixmap.h"

namespace ui {

namespace {

size_t RoundUp(size_t value, size_t alignment) {
  return ((value + alignment - 1) / alignment) * alignment;
}

VkFormat VkFormatForBufferFormat(gfx::BufferFormat buffer_format) {
  switch (buffer_format) {
    case gfx::BufferFormat::YVU_420:
      return VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM;

    case gfx::BufferFormat::YUV_420_BIPLANAR:
      return VK_FORMAT_G8_B8R8_2PLANE_420_UNORM;

    case gfx::BufferFormat::R_8:
      return VK_FORMAT_R8_UNORM;

    case gfx::BufferFormat::RG_88:
      return VK_FORMAT_R8G8_UNORM;

    case gfx::BufferFormat::BGRA_8888:
    case gfx::BufferFormat::BGRX_8888:
      return VK_FORMAT_B8G8R8A8_UNORM;

    case gfx::BufferFormat::RGBA_8888:
    case gfx::BufferFormat::RGBX_8888:
      return VK_FORMAT_R8G8B8A8_UNORM;

    default:
      NOTREACHED();
      return VK_FORMAT_UNDEFINED;
  }
}

size_t GetBytesPerPixel(gfx::BufferFormat buffer_format) {
  switch (buffer_format) {
    case gfx::BufferFormat::YVU_420:
    case gfx::BufferFormat::YUV_420_BIPLANAR:
    case gfx::BufferFormat::R_8:
      return 1U;

    case gfx::BufferFormat::RG_88:
      return 2U;

    case gfx::BufferFormat::BGRA_8888:
    case gfx::BufferFormat::BGRX_8888:
    case gfx::BufferFormat::RGBA_8888:
    case gfx::BufferFormat::RGBX_8888:
      return 4U;

    default:
      NOTREACHED();
      return 1;
  }
}

bool IsYuvVkFormat(VkFormat format) {
  switch (format) {
    case VK_FORMAT_G8_B8_R8_3PLANE_420_UNORM:
    case VK_FORMAT_G8_B8R8_2PLANE_420_UNORM:
      return true;
    case VK_FORMAT_R8_UNORM:
    case VK_FORMAT_R8G8_UNORM:
    case VK_FORMAT_B8G8R8A8_UNORM:
    case VK_FORMAT_R8G8B8A8_UNORM:
      return false;
    default:
      NOTREACHED();
      return false;
  }
}

VkFormatFeatureFlags GetFormatFeatureFlagsFromUsage(VkImageUsageFlags usage) {
  VkFormatFeatureFlags result = {};
  if (usage & VK_IMAGE_USAGE_TRANSFER_SRC_BIT) {
    result |= VK_FORMAT_FEATURE_TRANSFER_SRC_BIT;
  }
  if (usage & VK_IMAGE_USAGE_TRANSFER_DST_BIT) {
    result |= VK_FORMAT_FEATURE_TRANSFER_DST_BIT;
  }
  if (usage & VK_IMAGE_USAGE_SAMPLED_BIT) {
    result |= VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT;
  }
  if (usage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) {
    result |= VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT;
  }
  if (usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) {
    result |= VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT;
  }
  if (usage & VK_IMAGE_USAGE_STORAGE_BIT) {
    result |= VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT;
  }
  return result;
}

VkImageFormatConstraintsInfoFUCHSIA GetDefaultImageFormatConstraintsInfo(
    const VkImageCreateInfo& create_info) {
  DCHECK(create_info.format != VK_FORMAT_UNDEFINED);
  DCHECK(create_info.usage != 0);

  static const VkSysmemColorSpaceFUCHSIA kSrgbColorSpace = {
      .sType = VK_STRUCTURE_TYPE_SYSMEM_COLOR_SPACE_FUCHSIA,
      .pNext = nullptr,
      .colorSpace =
          static_cast<uint32_t>(fuchsia::sysmem::ColorSpaceType::SRGB),
  };

  static const VkSysmemColorSpaceFUCHSIA kYuvDefaultColorSpace = {
      .sType = VK_STRUCTURE_TYPE_SYSMEM_COLOR_SPACE_FUCHSIA,
      .pNext = nullptr,
      .colorSpace =
          static_cast<uint32_t>(fuchsia::sysmem::ColorSpaceType::REC709),
  };

  VkImageFormatConstraintsInfoFUCHSIA format_info = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_FORMAT_CONSTRAINTS_INFO_FUCHSIA,
      .pNext = nullptr,
      .imageCreateInfo = create_info,
      .requiredFormatFeatures =
          GetFormatFeatureFlagsFromUsage(create_info.usage),
      .sysmemPixelFormat = 0u,
      .colorSpaceCount = 1u,
      .pColorSpaces = IsYuvVkFormat(create_info.format) ? &kYuvDefaultColorSpace
                                                        : &kSrgbColorSpace,
  };
  return format_info;
}

struct ImageConstraintsInfo {
  VkImageConstraintsInfoFUCHSIA image_constraints;
  VkImageFormatConstraintsInfoFUCHSIA format_constraints;

  ImageConstraintsInfo(
      const VkImageConstraintsInfoFUCHSIA& image_constraints_in,
      const VkImageFormatConstraintsInfoFUCHSIA& format_constraints_in)
      : image_constraints(image_constraints_in),
        format_constraints(format_constraints_in) {
    image_constraints.pFormatConstraints = &format_constraints;
    image_constraints.formatConstraintsCount = 1u;
  }

  ImageConstraintsInfo(ImageConstraintsInfo&& from) = delete;
  ImageConstraintsInfo(const ImageConstraintsInfo&) = delete;
  ImageConstraintsInfo& operator=(const ImageConstraintsInfo&) = delete;
};

std::unique_ptr<ImageConstraintsInfo> InitializeImageConstraintsInfo(
    const VkImageCreateInfo& vk_image_info,
    bool allow_protected_memory) {
  VkImageFormatConstraintsInfoFUCHSIA format_constraints =
      GetDefaultImageFormatConstraintsInfo(vk_image_info);
  VkImageConstraintsInfoFUCHSIA image_constraints = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_CONSTRAINTS_INFO_FUCHSIA,
      .pNext = nullptr,
      .bufferCollectionConstraints =
          VkBufferCollectionConstraintsInfoFUCHSIA{
              .sType =
                  VK_STRUCTURE_TYPE_BUFFER_COLLECTION_CONSTRAINTS_INFO_FUCHSIA,
              .pNext = nullptr,
              .minBufferCount = 1u,
              // Using the default value (0) for the fields below means that
              // there is no other constraints except for the minimum buffer
              // count.
              .maxBufferCount = 0u,
              .minBufferCountForCamping = 0u,
              .minBufferCountForDedicatedSlack = 0u,
              .minBufferCountForSharedSlack = 0u,
          },
      // TODO(crbug.com/1289315): Instead of always allowing protected
      // memory, Chrome should query if the Vulkan physical device
      // supports protected memory and only set the flag if it is
      // supported.
      .flags = allow_protected_memory
                   ? VK_IMAGE_CONSTRAINTS_INFO_PROTECTED_OPTIONAL_FUCHSIA
                   : 0u,
  };

  return std::make_unique<ImageConstraintsInfo>(image_constraints,
                                                format_constraints);
}

}  // namespace

// static
bool SysmemBufferCollection::IsNativePixmapConfigSupported(
    gfx::BufferFormat format,
    gfx::BufferUsage usage) {
  switch (format) {
    case gfx::BufferFormat::YUV_420_BIPLANAR:
    case gfx::BufferFormat::R_8:
    case gfx::BufferFormat::RG_88:
    case gfx::BufferFormat::RGBA_8888:
    case gfx::BufferFormat::RGBX_8888:
    case gfx::BufferFormat::BGRA_8888:
    case gfx::BufferFormat::BGRX_8888:
      break;

    default:
      return false;
  }
  switch (usage) {
    case gfx::BufferUsage::SCANOUT:
    case gfx::BufferUsage::GPU_READ:
      break;

    case gfx::BufferUsage::SCANOUT_CPU_READ_WRITE:
    case gfx::BufferUsage::GPU_READ_CPU_READ_WRITE:
      break;

    default:
      return false;
  }
  return true;
}

SysmemBufferCollection::SysmemBufferCollection()
    : SysmemBufferCollection(gfx::SysmemBufferCollectionId::Create()) {}

SysmemBufferCollection::SysmemBufferCollection(gfx::SysmemBufferCollectionId id)
    : id_(id) {}

bool SysmemBufferCollection::Initialize(
    fuchsia::sysmem::Allocator_Sync* allocator,
    ScenicSurfaceFactory* scenic_surface_factory,
    zx::channel token_handle,
    gfx::Size size,
    gfx::BufferFormat format,
    gfx::BufferUsage usage,
    VkDevice vk_device,
    size_t min_buffer_count,
    bool register_with_image_pipe) {
  DCHECK(IsNativePixmapConfigSupported(format, usage));
  DCHECK(!collection_);
  DCHECK(!vk_buffer_collection_);

  // Currently all supported |usage| values require GPU access, which requires
  // a valid VkDevice.
  if (vk_device == VK_NULL_HANDLE)
    return false;

  if (size.IsEmpty()) {
    // Buffer collection that doesn't have explicit size is expected to be
    // shared with other participants, who will determine the actual image size.
    DCHECK(token_handle);

    // Set nominal size of 1x1, which will be used only for
    // vkSetBufferCollectionConstraintsFUCHSIA(). The actual size of the
    // allocated buffers is determined by constraints set by other sysmem
    // clients for the same collection. Size of the Vulkan image is determined
    // by the values passed to CreateVkImage().
    min_size_ = gfx::Size(1, 1);
  } else {
    min_size_ = size;
  }

  format_ = format;
  usage_ = usage;
  vk_device_ = vk_device;
  is_protected_ = false;

  if (register_with_image_pipe) {
    overlay_view_task_runner_ = base::ThreadTaskRunnerHandle::Get();
    scenic_overlay_view_ = std::make_unique<ScenicOverlayView>(
        scenic_surface_factory->CreateScenicSession());
  }

  fuchsia::sysmem::BufferCollectionTokenSyncPtr collection_token;
  if (token_handle) {
    collection_token.Bind(std::move(token_handle));
  } else {
    zx_status_t status =
        allocator->AllocateSharedCollection(collection_token.NewRequest());
    if (status != ZX_OK) {
      ZX_DLOG(ERROR, status)
          << "fuchsia.sysmem.Allocator.AllocateSharedCollection()";
      return false;
    }
  }

  return InitializeInternal(allocator, std::move(collection_token),
                            min_buffer_count);
}

scoped_refptr<gfx::NativePixmap> SysmemBufferCollection::CreateNativePixmap(
    size_t buffer_index,
    gfx::Size size) {
  CHECK_LT(buffer_index, num_buffers());

  gfx::NativePixmapHandle handle;
  handle.buffer_collection_id = id();
  handle.buffer_index = buffer_index;
  handle.ram_coherency =
      buffers_info_.settings.buffer_settings.coherency_domain ==
      fuchsia::sysmem::CoherencyDomain::RAM;

  zx::vmo main_plane_vmo;
  if (is_mappable()) {
    DCHECK(buffers_info_.buffers[buffer_index].vmo.is_valid());
    zx_status_t status = buffers_info_.buffers[buffer_index].vmo.duplicate(
        ZX_RIGHT_SAME_RIGHTS, &main_plane_vmo);
    if (status != ZX_OK) {
      ZX_DLOG(ERROR, status) << "zx_handle_duplicate";
      return nullptr;
    }
  }

  const fuchsia::sysmem::ImageFormatConstraints& format =
      buffers_info_.settings.image_format_constraints;

  // The logic should match LogicalBufferCollection::Allocate().
  size_t stride =
      RoundUp(std::max(static_cast<size_t>(format.min_bytes_per_row),
                       size.width() * GetBytesPerPixel(format_)),
              format.bytes_per_row_divisor);
  size_t plane_offset = buffers_info_.buffers[buffer_index].vmo_usable_start;
  size_t plane_size = stride * size.height();
  handle.planes.emplace_back(stride, plane_offset, plane_size,
                             std::move(main_plane_vmo));

  // For YUV images add a second plane.
  if (format_ == gfx::BufferFormat::YUV_420_BIPLANAR) {
    size_t uv_plane_offset = plane_offset + plane_size;
    size_t uv_plane_size = plane_size / 2;
    handle.planes.emplace_back(stride, uv_plane_offset, uv_plane_size,
                               zx::vmo());
    DCHECK_LE(uv_plane_offset + uv_plane_size, buffer_size_);
  }

  return new SysmemNativePixmap(this, std::move(handle), size);
}

bool SysmemBufferCollection::CreateVkImage(
    size_t buffer_index,
    VkDevice vk_device,
    gfx::Size size,
    VkImage* vk_image,
    VkImageCreateInfo* vk_image_info,
    VkDeviceMemory* vk_device_memory,
    VkDeviceSize* mem_allocation_size,
    absl::optional<gpu::VulkanYCbCrInfo>* ycbcr_info) {
  DCHECK_CALLED_ON_VALID_THREAD(vulkan_thread_checker_);

  if (vk_device_ != vk_device) {
    DLOG(FATAL) << "Tried to import NativePixmap that was created for a "
                   "different VkDevice.";
    return false;
  }

  VkBufferCollectionPropertiesFUCHSIA properties = {
      VK_STRUCTURE_TYPE_BUFFER_COLLECTION_PROPERTIES_FUCHSIA};
  if (vkGetBufferCollectionPropertiesFUCHSIA(vk_device_, vk_buffer_collection_,
                                             &properties) != VK_SUCCESS) {
    DLOG(ERROR) << "vkGetBufferCollectionPropertiesFUCHSIA failed";
    return false;
  }

  InitializeImageCreateInfo(vk_image_info, size);

  VkBufferCollectionImageCreateInfoFUCHSIA image_format_fuchsia = {
      VK_STRUCTURE_TYPE_BUFFER_COLLECTION_IMAGE_CREATE_INFO_FUCHSIA,
  };
  image_format_fuchsia.collection = vk_buffer_collection_;
  image_format_fuchsia.index = buffer_index;
  vk_image_info->pNext = &image_format_fuchsia;

  if (vkCreateImage(vk_device_, vk_image_info, nullptr, vk_image) !=
      VK_SUCCESS) {
    DLOG(ERROR) << "Failed to create VkImage.";
    return false;
  }

  vk_image_info->pNext = nullptr;

  VkMemoryRequirements requirements;
  vkGetImageMemoryRequirements(vk_device, *vk_image, &requirements);

  uint32_t viable_memory_types =
      properties.memoryTypeBits & requirements.memoryTypeBits;
  uint32_t memory_type = base::bits::CountTrailingZeroBits(viable_memory_types);

  VkMemoryDedicatedAllocateInfoKHR dedicated_allocate = {
      VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO_KHR};
  dedicated_allocate.image = *vk_image;
  VkImportMemoryBufferCollectionFUCHSIA buffer_collection_info = {
      VK_STRUCTURE_TYPE_IMPORT_MEMORY_BUFFER_COLLECTION_FUCHSIA,
      &dedicated_allocate};
  buffer_collection_info.collection = vk_buffer_collection_;
  buffer_collection_info.index = buffer_index;

  VkMemoryAllocateInfo alloc_info = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
                                     &buffer_collection_info};
  alloc_info.allocationSize = requirements.size;
  alloc_info.memoryTypeIndex = memory_type;

  if (vkAllocateMemory(vk_device_, &alloc_info, nullptr, vk_device_memory) !=
      VK_SUCCESS) {
    DLOG(ERROR) << "Failed to create VkMemory from sysmem buffer.";
    vkDestroyImage(vk_device_, *vk_image, nullptr);
    *vk_image = VK_NULL_HANDLE;
    return false;
  }

  if (vkBindImageMemory(vk_device_, *vk_image, *vk_device_memory, 0u) !=
      VK_SUCCESS) {
    DLOG(ERROR) << "Failed to bind sysmem buffer to a VkImage.";
    vkDestroyImage(vk_device_, *vk_image, nullptr);
    *vk_image = VK_NULL_HANDLE;
    vkFreeMemory(vk_device_, *vk_device_memory, nullptr);
    *vk_device_memory = VK_NULL_HANDLE;
    return false;
  }

  *mem_allocation_size = requirements.size;

  auto color_space =
      buffers_info_.settings.image_format_constraints.color_space[0].type;
  switch (color_space) {
    case fuchsia::sysmem::ColorSpaceType::SRGB:
      *ycbcr_info = absl::nullopt;
      break;

    case fuchsia::sysmem::ColorSpaceType::REC709: {
      // Currently sysmem doesn't specify location of chroma samples relative to
      // luma (see fxbug.dev/13677). Assume they are cosited with luma. YCbCr
      // info here must match the values passed for the same buffer in
      // FuchsiaVideoDecoder. |format_features| are resolved later in the GPU
      // process before the ycbcr info is passed to Skia.
      *ycbcr_info = gpu::VulkanYCbCrInfo(
          vk_image_info->format, /*external_format=*/0,
          VK_SAMPLER_YCBCR_MODEL_CONVERSION_YCBCR_709,
          VK_SAMPLER_YCBCR_RANGE_ITU_NARROW, VK_CHROMA_LOCATION_COSITED_EVEN,
          VK_CHROMA_LOCATION_COSITED_EVEN, /*format_features=*/0);
      break;
    }

    default:
      DLOG(ERROR) << "Sysmem allocated buffer with unsupported color space: "
                  << static_cast<int>(color_space);
      return false;
  }

  return true;
}

void SysmemBufferCollection::AddOnDeletedCallback(
    base::OnceClosure on_deleted) {
  on_deleted_.push_back(std::move(on_deleted));
}

SysmemBufferCollection::~SysmemBufferCollection() {
  if (vk_buffer_collection_ != VK_NULL_HANDLE) {
    vkDestroyBufferCollectionFUCHSIA(vk_device_, vk_buffer_collection_,
                                     nullptr);
  }

  if (collection_)
    collection_->Close();

  for (auto& callback : on_deleted_)
    std::move(callback).Run();

  if (scenic_overlay_view_ &&
      !overlay_view_task_runner_->BelongsToCurrentThread()) {
    overlay_view_task_runner_->DeleteSoon(FROM_HERE,
                                          std::move(scenic_overlay_view_));
  }
}

bool SysmemBufferCollection::InitializeInternal(
    fuchsia::sysmem::Allocator_Sync* allocator,
    fuchsia::sysmem::BufferCollectionTokenSyncPtr collection_token,
    size_t min_buffer_count) {
  fidl::InterfaceHandle<fuchsia::sysmem::BufferCollectionToken>
      collection_token_for_vulkan;
  collection_token->Duplicate(ZX_RIGHT_SAME_RIGHTS,
                              collection_token_for_vulkan.NewRequest());

  // Duplicate one more token for Scenic if this collection can be used as an
  // overlay.
  fidl::InterfaceHandle<fuchsia::sysmem::BufferCollectionToken>
      collection_token_for_scenic;
  if (scenic_overlay_view_) {
    collection_token->Duplicate(ZX_RIGHT_SAME_RIGHTS,
                                collection_token_for_scenic.NewRequest());
  }

  zx_status_t status = collection_token->Sync();
  if (status != ZX_OK) {
    ZX_DLOG(ERROR, status) << "fuchsia.sysmem.BufferCollectionToken.Sync()";
    return false;
  }

  if (scenic_overlay_view_) {
    scenic_overlay_view_->Initialize(std::move(collection_token_for_scenic));
  }

  status = allocator->BindSharedCollection(std::move(collection_token),
                                           collection_.NewRequest());
  if (status != ZX_OK) {
    ZX_DLOG(ERROR, status) << "fuchsia.sysmem.Allocator.BindSharedCollection()";
    return false;
  }

  fuchsia::sysmem::BufferCollectionConstraints constraints;
  if (is_mappable()) {
    constraints.usage.cpu =
        fuchsia::sysmem::cpuUsageRead | fuchsia::sysmem::cpuUsageWrite;

    constraints.has_buffer_memory_constraints = true;
    constraints.buffer_memory_constraints.ram_domain_supported = true;
    constraints.buffer_memory_constraints.cpu_domain_supported = true;
  } else {
    constraints.usage.none = fuchsia::sysmem::noneUsage;
  }

  constraints.min_buffer_count = min_buffer_count;
  constraints.image_format_constraints_count = 0;

  status = collection_->SetConstraints(/*has_constraints=*/true,
                                       std::move(constraints));
  if (status != ZX_OK) {
    ZX_DLOG(ERROR, status)
        << "fuchsia.sysmem.BufferCollection.SetConstraints()";
    return false;
  }

  zx::channel token_channel = collection_token_for_vulkan.TakeChannel();
  VkBufferCollectionCreateInfoFUCHSIA buffer_collection_create_info = {
      VK_STRUCTURE_TYPE_BUFFER_COLLECTION_CREATE_INFO_FUCHSIA};
  buffer_collection_create_info.collectionToken = token_channel.get();
  if (vkCreateBufferCollectionFUCHSIA(vk_device_,
                                      &buffer_collection_create_info, nullptr,
                                      &vk_buffer_collection_) != VK_SUCCESS) {
    vk_buffer_collection_ = VK_NULL_HANDLE;
    DLOG(ERROR) << "vkCreateBufferCollectionFUCHSIA() failed";
    return false;
  }

  // vkCreateBufferCollectionFUCHSIA() takes ownership of the token on success.
  std::ignore = token_channel.release();

  VkImageCreateInfo image_create_info;
  InitializeImageCreateInfo(&image_create_info, min_size_);

  // TODO(crbug.com/1289315): Instead of always allowing protected memory,
  // Chrome should query if the Vulkan physical device supports protected
  // memory and only set the flag if it is supported.
  auto image_constraints_info = InitializeImageConstraintsInfo(
      image_create_info, /* allow_protected_memory */ true);

  if (vkSetBufferCollectionImageConstraintsFUCHSIA(
          vk_device_, vk_buffer_collection_,
          &image_constraints_info->image_constraints) != VK_SUCCESS) {
    DLOG(ERROR) << "vkSetBufferCollectionConstraintsFUCHSIA() failed";
    return false;
  }

  zx_status_t wait_status;
  status = collection_->WaitForBuffersAllocated(&wait_status, &buffers_info_);
  if (status != ZX_OK) {
    ZX_DLOG(ERROR, status) << "fuchsia.sysmem.BufferCollection failed";
    return false;
  }

  if (wait_status != ZX_OK) {
    ZX_DLOG(ERROR, status) << "fuchsia.sysmem.BufferCollection::"
                              "WaitForBuffersAllocated() failed.";
    return false;
  }

  DCHECK_GE(buffers_info_.buffer_count, min_buffer_count);
  DCHECK(buffers_info_.settings.has_image_format_constraints);

  buffer_size_ = buffers_info_.settings.buffer_settings.size_bytes;
  is_protected_ = buffers_info_.settings.buffer_settings.is_secure;

  // CreateVkImage() should always be called on the same thread, but it may be
  // different from the thread that called Initialize().
  DETACH_FROM_THREAD(vulkan_thread_checker_);

  return true;
}

void SysmemBufferCollection::InitializeImageCreateInfo(
    VkImageCreateInfo* vk_image_info,
    gfx::Size size) {
  *vk_image_info = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
  vk_image_info->flags = is_protected_ ? VK_IMAGE_CREATE_PROTECTED_BIT : 0u;
  vk_image_info->imageType = VK_IMAGE_TYPE_2D;
  vk_image_info->format = VkFormatForBufferFormat(format_);
  vk_image_info->extent = VkExtent3D{static_cast<uint32_t>(size.width()),
                                     static_cast<uint32_t>(size.height()), 1};
  vk_image_info->mipLevels = 1;
  vk_image_info->arrayLayers = 1;
  vk_image_info->samples = VK_SAMPLE_COUNT_1_BIT;
  vk_image_info->tiling =
      is_mappable() ? VK_IMAGE_TILING_LINEAR : VK_IMAGE_TILING_OPTIMAL;

  vk_image_info->usage = VK_IMAGE_USAGE_SAMPLED_BIT |
                         VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                         VK_IMAGE_USAGE_TRANSFER_DST_BIT;
  if (usage_ == gfx::BufferUsage::SCANOUT) {
    vk_image_info->usage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
  }

  vk_image_info->sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  vk_image_info->initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
}

}  // namespace ui