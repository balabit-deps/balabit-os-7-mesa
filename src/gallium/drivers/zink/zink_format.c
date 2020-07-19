#include "zink_screen.h"

static const VkFormat formats[PIPE_FORMAT_COUNT] = {
#define MAP_FORMAT_NORM(FMT) \
   [PIPE_FORMAT_ ## FMT ## _UNORM] = VK_FORMAT_ ## FMT ## _UNORM, \
   [PIPE_FORMAT_ ## FMT ## _SNORM] = VK_FORMAT_ ## FMT ## _SNORM,

#define MAP_FORMAT_SCALED(FMT) \
   [PIPE_FORMAT_ ## FMT ## _USCALED] = VK_FORMAT_ ## FMT ## _USCALED, \
   [PIPE_FORMAT_ ## FMT ## _SSCALED] = VK_FORMAT_ ## FMT ## _SSCALED,

#define MAP_FORMAT_INT(FMT) \
   [PIPE_FORMAT_ ## FMT ## _UINT] = VK_FORMAT_ ## FMT ## _UINT, \
   [PIPE_FORMAT_ ## FMT ## _SINT] = VK_FORMAT_ ## FMT ## _SINT,

#define MAP_FORMAT_SRGB(FMT) \
   [PIPE_FORMAT_ ## FMT ## _SRGB] = VK_FORMAT_ ## FMT ## _SRGB,

#define MAP_FORMAT_FLOAT(FMT) \
   [PIPE_FORMAT_ ## FMT ## _FLOAT] = VK_FORMAT_ ## FMT ## _SFLOAT,

   // one component

   // 8-bits
   MAP_FORMAT_NORM(R8)
   MAP_FORMAT_SCALED(R8)
   MAP_FORMAT_INT(R8)
   // 16-bits
   MAP_FORMAT_NORM(R16)
   MAP_FORMAT_SCALED(R16)
   MAP_FORMAT_INT(R16)
   MAP_FORMAT_FLOAT(R16)
   // 32-bits
   MAP_FORMAT_INT(R32)
   MAP_FORMAT_FLOAT(R32)

   // two components

   // 8-bits
   MAP_FORMAT_NORM(R8G8)
   MAP_FORMAT_SCALED(R8G8)
   MAP_FORMAT_INT(R8G8)
   // 16-bits
   MAP_FORMAT_NORM(R16G16)
   MAP_FORMAT_SCALED(R16G16)
   MAP_FORMAT_INT(R16G16)
   MAP_FORMAT_FLOAT(R16G16)
   // 32-bits
   MAP_FORMAT_INT(R32G32)
   MAP_FORMAT_FLOAT(R32G32)

   // three components

   // 8-bits
   MAP_FORMAT_NORM(R8G8B8)
   MAP_FORMAT_SCALED(R8G8B8)
   MAP_FORMAT_INT(R8G8B8)
   MAP_FORMAT_SRGB(R8G8B8)
   // 16-bits
   MAP_FORMAT_NORM(R16G16B16)
   MAP_FORMAT_SCALED(R16G16B16)
   MAP_FORMAT_INT(R16G16B16)
   MAP_FORMAT_FLOAT(R16G16B16)
   // 32-bits
   MAP_FORMAT_INT(R32G32B32)
   MAP_FORMAT_FLOAT(R32G32B32)

   // four components

   // 8-bits
   MAP_FORMAT_NORM(R8G8B8A8)
   MAP_FORMAT_SCALED(R8G8B8A8)
   MAP_FORMAT_INT(R8G8B8A8)
   MAP_FORMAT_SRGB(R8G8B8A8)
   [PIPE_FORMAT_B8G8R8A8_UNORM] = VK_FORMAT_B8G8R8A8_UNORM,
   [PIPE_FORMAT_B8G8R8X8_UNORM] = VK_FORMAT_B8G8R8A8_UNORM,
   MAP_FORMAT_SRGB(B8G8R8A8)
   [PIPE_FORMAT_A8B8G8R8_SRGB] = VK_FORMAT_A8B8G8R8_SRGB_PACK32,
   // 16-bits
   MAP_FORMAT_NORM(R16G16B16A16)
   MAP_FORMAT_SCALED(R16G16B16A16)
   MAP_FORMAT_INT(R16G16B16A16)
   MAP_FORMAT_FLOAT(R16G16B16A16)
   // 32-bits
   MAP_FORMAT_INT(R32G32B32A32)
   MAP_FORMAT_FLOAT(R32G32B32A32)

   // other color formats
   [PIPE_FORMAT_B5G6R5_UNORM] = VK_FORMAT_R5G6B5_UNORM_PACK16,
   [PIPE_FORMAT_B5G5R5A1_UNORM] = VK_FORMAT_B5G5R5A1_UNORM_PACK16,
   [PIPE_FORMAT_R11G11B10_FLOAT] = VK_FORMAT_B10G11R11_UFLOAT_PACK32,
   [PIPE_FORMAT_R9G9B9E5_FLOAT] = VK_FORMAT_E5B9G9R9_UFLOAT_PACK32,
   [PIPE_FORMAT_R10G10B10A2_UNORM] = VK_FORMAT_A2B10G10R10_UNORM_PACK32,
   [PIPE_FORMAT_B10G10R10A2_UNORM] = VK_FORMAT_A2R10G10B10_UNORM_PACK32,
   [PIPE_FORMAT_R10G10B10A2_UINT] = VK_FORMAT_A2B10G10R10_UINT_PACK32,
   [PIPE_FORMAT_B10G10R10A2_UINT] = VK_FORMAT_A2R10G10B10_UINT_PACK32,

   // depth/stencil formats
   [PIPE_FORMAT_Z32_FLOAT] = VK_FORMAT_D32_SFLOAT,
   [PIPE_FORMAT_Z32_FLOAT_S8X24_UINT] = VK_FORMAT_D32_SFLOAT_S8_UINT,
   [PIPE_FORMAT_Z16_UNORM] = VK_FORMAT_D16_UNORM,
   [PIPE_FORMAT_Z24X8_UNORM] = VK_FORMAT_X8_D24_UNORM_PACK32,
   [PIPE_FORMAT_Z24_UNORM_S8_UINT] = VK_FORMAT_D24_UNORM_S8_UINT,

   // compressed formats
   [PIPE_FORMAT_DXT1_RGB] = VK_FORMAT_BC1_RGB_UNORM_BLOCK,
   [PIPE_FORMAT_DXT1_RGBA] = VK_FORMAT_BC1_RGBA_UNORM_BLOCK,
   [PIPE_FORMAT_DXT3_RGBA] = VK_FORMAT_BC2_UNORM_BLOCK,
   [PIPE_FORMAT_DXT5_RGBA] = VK_FORMAT_BC3_UNORM_BLOCK,
   [PIPE_FORMAT_DXT1_SRGB] = VK_FORMAT_BC1_RGB_SRGB_BLOCK,
   [PIPE_FORMAT_DXT1_SRGBA] = VK_FORMAT_BC1_RGBA_SRGB_BLOCK,
   [PIPE_FORMAT_DXT3_SRGBA] = VK_FORMAT_BC2_SRGB_BLOCK,
   [PIPE_FORMAT_DXT5_SRGBA] = VK_FORMAT_BC3_SRGB_BLOCK,

   [PIPE_FORMAT_RGTC1_UNORM] = VK_FORMAT_BC4_UNORM_BLOCK,
   [PIPE_FORMAT_RGTC1_SNORM] = VK_FORMAT_BC4_SNORM_BLOCK,
   [PIPE_FORMAT_RGTC2_UNORM] = VK_FORMAT_BC5_UNORM_BLOCK,
   [PIPE_FORMAT_RGTC2_SNORM] = VK_FORMAT_BC5_SNORM_BLOCK,
   [PIPE_FORMAT_BPTC_RGBA_UNORM] = VK_FORMAT_BC7_UNORM_BLOCK,
   [PIPE_FORMAT_BPTC_SRGBA] = VK_FORMAT_BC7_SRGB_BLOCK,
   [PIPE_FORMAT_BPTC_RGB_FLOAT] = VK_FORMAT_BC6H_SFLOAT_BLOCK,
   [PIPE_FORMAT_BPTC_RGB_UFLOAT] = VK_FORMAT_BC6H_UFLOAT_BLOCK,
};

bool
zink_is_depth_format_supported(struct zink_screen *screen, VkFormat format)
{
   VkFormatProperties props;
   vkGetPhysicalDeviceFormatProperties(screen->pdev, format, &props);
   return (props.linearTilingFeatures | props.optimalTilingFeatures) &
          VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT;
}

VkFormat
zink_get_format(struct zink_screen *screen, enum pipe_format format)
{
   VkFormat ret = formats[format];

   if (ret == VK_FORMAT_X8_D24_UNORM_PACK32 &&
       !screen->have_X8_D24_UNORM_PACK32) {
      assert(zink_is_depth_format_supported(screen, VK_FORMAT_D32_SFLOAT));
      return VK_FORMAT_D32_SFLOAT;
   }

   if (ret == VK_FORMAT_D24_UNORM_S8_UINT &&
       !screen->have_D24_UNORM_S8_UINT) {
      assert(zink_is_depth_format_supported(screen,
                                            VK_FORMAT_D32_SFLOAT_S8_UINT));
      return VK_FORMAT_D32_SFLOAT_S8_UINT;
   }

   return ret;
}
