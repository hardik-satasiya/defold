#include <stdio.h>

#include <dlib/log.h>

#ifndef VK_USE_PLATFORM_VI_NN
#define VK_USE_PLATFORM_VI_NN
#endif

#include "../graphics.h"
#include "../graphics_native.h"
#include "../vulkan/graphics_vulkan_defines.h"
#include "../vulkan/graphics_vulkan_private.h"

#include <nn/nn_Assert.h>
#include <nn/nn_Log.h>

#include <nn/nn_Result.h>
#include <nn/fs.h>
#include <nn/oe.h>
#include <nn/os.h>
#include <nn/init.h>
#include <nn/vi.h>
#include <nn/hid.h>

#include <nv/nv_MemoryManagement.h>

#include <nn/mem.h>

namespace
{
    void* Allocate(size_t size, size_t alignment, void*)
    {
        return aligned_alloc(alignment, nn::util::align_up(size, alignment));
    }
    void Free(void* addr, void*)
    {
        free(addr);
    }
    void* Reallocate(void* addr, size_t newSize, void*)
    {
        return realloc(addr, newSize);
    }

}; // end anonymous namespace

// TODO: Put these in a native context struct
nn::vi::Display*    g_pDisplay = 0;
nn::vi::Layer*      g_pLayer = 0;
int                 g_NativeInitialized = 0;
nn::os::SystemEvent g_DisplayResolutionChangeEvent;

namespace dmGraphics
{
    static const int DM_GRAPHICS_BACKGROUND_WIDTH = 1920;
    static const int DM_GRAPHICS_BACKGROUND_HEIGHT = 1080;

    static void CreateLayer(int width, int height)
    {
        nn::vi::LayerCreationSettings layerCreationSettings(width, height);
        nn::Result result = nn::vi::CreateLayer(&g_pLayer, g_pDisplay, &layerCreationSettings);
        NN_ASSERT(result.IsSuccess());
    }

    static void DestroyLayer()
    {
        nn::vi::DestroyLayer(g_pLayer);
    }

    bool NativeInit(const ContextParams& params)
    {
        if (!g_NativeInitialized)
        {
            nv::SetGraphicsAllocator(Allocate, Free, Reallocate, NULL);
            nv::SetGraphicsDevtoolsAllocator(Allocate, Free, Reallocate, NULL);

            uint32_t wantedgfxmemsize = params.m_GraphicsMemorySize != 0 ? params.m_GraphicsMemorySize : 32 * 1024 * 1024;
            uint32_t gfxmemsize = wantedgfxmemsize;
            void* pgfxmemory = malloc(gfxmemsize);
            while (pgfxmemory == 0)
            {
                gfxmemsize /= 2;
                pgfxmemory = malloc(gfxmemsize);
            }

            if (wantedgfxmemsize != gfxmemsize)
            {
                dmLogWarning("Couldn't allocate %u bytes for graphics memory, best we got was %u bytes", wantedgfxmemsize, gfxmemsize);
            }

            nv::InitializeGraphics(pgfxmemory, gfxmemsize);

            dmLogInfo("GfxMemory is %p - %p (%d mb)\n\n", pgfxmemory, (uint8_t*)pgfxmemory + gfxmemsize, gfxmemsize/(1024*1024));

            nn::vi::Initialize();

            nn::Result result = nn::vi::OpenDefaultDisplay(&g_pDisplay);
            NN_ASSERT(result.IsSuccess());
            NN_UNUSED(result);

            // The advice given by the NSDK, is to create a 1080p layer, and instead use the nn::vi::SetLayerCrop
            // to set the region (as opposed to recreate the swapchain etc)
            CreateLayer(DM_GRAPHICS_BACKGROUND_WIDTH, DM_GRAPHICS_BACKGROUND_HEIGHT);

            nn::oe::GetDefaultDisplayResolutionChangeEvent( &g_DisplayResolutionChangeEvent );

            g_NativeInitialized = 1;
        }
        return true;
    }

    void NativeExit()
    {
        DestroyLayer();
        nn::vi::CloseDisplay(g_pDisplay);
        nn::vi::Finalize();
    }

    void NativeBeginFrame(HContext context)
    {
        if (g_DisplayResolutionChangeEvent.TryWait())
        {
            int width, height;
            nn::oe::GetDefaultDisplayResolution( &width, &height );
            VulkanResizeWindow(context, width, height);
        }
    }

    static const char*   g_extension_names[]       = {
        VK_KHR_SURFACE_EXTENSION_NAME,
        VK_EXT_DEBUG_REPORT_EXTENSION_NAME,
        VK_NN_VI_SURFACE_EXTENSION_NAME,
    };
    // Validation layers to enable
    static const char* g_validation_layers[3];
    static const char* DM_VULKAN_LAYER_SWAPCHAIN    = "VK_LAYER_NN_vi_swapchain";
    static const char* DM_VULKAN_LAYER_VALIDATION   = "VK_LAYER_LUNARG_standard_validation";
    static const char* DM_VULKAN_LAYER_RENDERDOC    = "VK_LAYER_RENDERDOC_Capture";

    // Validation layer extensions
    static const char*   g_validation_layer_ext[]     = { VK_EXT_DEBUG_UTILS_EXTENSION_NAME };

    const char** GetExtensionNames(uint16_t* num_extensions)
    {
        *num_extensions = sizeof(g_extension_names) / sizeof(g_extension_names[0]);
        return g_extension_names;
    }

    const char** GetValidationLayers(uint16_t* num_layers, bool use_validation, bool use_renderdoc)
    {
        uint16_t count = 0;
        g_validation_layers[count++] = DM_VULKAN_LAYER_SWAPCHAIN;

        if (use_validation)
        {
            g_validation_layers[count++] = DM_VULKAN_LAYER_VALIDATION;
        }
        if (use_renderdoc)
        {
            g_validation_layers[count++] = DM_VULKAN_LAYER_RENDERDOC;
        }

        *num_layers = count;
        return g_validation_layers;
    }

    const char** GetValidationLayersExt(uint16_t* num_layers)
    {
        *num_layers = sizeof(g_validation_layer_ext) / sizeof(g_validation_layer_ext[0]);
        return g_validation_layer_ext;
    }

    // Called from InitializeVulkan
    VkResult CreateWindowSurface(VkInstance vkInstance, VkSurfaceKHR* vkSurfaceOut, const bool enableHighDPI)
    {
        VkViSurfaceCreateInfoNN createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_VI_SURFACE_CREATE_INFO_NN;
        createInfo.pNext = NULL;
        createInfo.flags = 0;

        nn::vi::NativeWindowHandle nativeWindow;
        nn::vi::GetNativeWindow(&nativeWindow, g_pLayer);
        createInfo.window = nativeWindow;

        VkResult result = vkCreateViSurfaceNN(vkInstance, &createInfo, NULL, vkSurfaceOut);
        NN_ASSERT(result == VK_SUCCESS);

        return result;
    }

    uint32_t VulkanGetWindowRefreshRate(HContext context)
    {
        assert(context);
        if (context->m_WindowOpened)
            return 60;
        else
            return 0;
    }

    WindowResult VulkanOpenWindow(HContext context, WindowParams* params)
    {
        assert(context);
        assert(params);

        if (context->m_WindowOpened)
            return WINDOW_RESULT_ALREADY_OPENED;

        params->m_Width = DM_GRAPHICS_BACKGROUND_WIDTH;
        params->m_Height = DM_GRAPHICS_BACKGROUND_HEIGHT;

        if (!InitializeVulkan(context, params))
        {
            return WINDOW_RESULT_WINDOW_OPEN_ERROR;
        }

        int width, height;
        nn::oe::GetDefaultDisplayResolution(&width, &height);
        nn::vi::SetLayerCrop(g_pLayer, 0, 0, width, height);

        context->m_WindowOpened                  = 1;
        context->m_Width                         = width;
        context->m_Height                        = height;
        context->m_WindowWidth                   = width;
        context->m_WindowHeight                  = height;
        context->m_WindowResizeCallback          = params->m_ResizeCallback;
        context->m_WindowResizeCallbackUserData  = params->m_ResizeCallbackUserData;
        context->m_WindowCloseCallback           = params->m_CloseCallback;
        context->m_WindowCloseCallbackUserData   = params->m_CloseCallbackUserData;
        context->m_WindowFocusCallback           = params->m_FocusCallback;
        context->m_WindowFocusCallbackUserData   = params->m_FocusCallbackUserData;
        context->m_WindowIconifyCallback         = params->m_IconifyCallback;
        context->m_WindowIconifyCallbackUserData = params->m_IconifyCallbackUserData;

        return WINDOW_RESULT_OK;
    }


    void VulkanCloseWindow(HContext context)
    {
        if (context->m_WindowOpened)
        {
            VkDevice vk_device = context->m_LogicalDevice.m_Device;

            vkDeviceWaitIdle(vk_device);

            context->m_PipelineCache.Iterate(DestroyPipelineCacheCb, context);

            DestroyDeviceBuffer(vk_device, &context->m_MainTextureDepthStencil.m_DeviceBuffer.m_Handle);
            DestroyTexture(vk_device, &context->m_MainTextureDepthStencil.m_Handle);
            DestroyTexture(vk_device, &context->m_DefaultTexture->m_Handle);

            vkFreeCommandBuffers(vk_device, context->m_LogicalDevice.m_CommandPool, context->m_MainCommandBuffers.Size(), context->m_MainCommandBuffers.Begin());
            vkFreeCommandBuffers(vk_device, context->m_LogicalDevice.m_CommandPool, 1, &context->m_MainCommandBufferUploadHelper);

            DestroyMainFrameBuffers(context);

            vkDestroyRenderPass(vk_device, context->m_MainRenderPass, 0);

            for (uint8_t i=0; i < context->m_TextureSamplers.Size(); i++)
            {
                DestroyTextureSampler(vk_device, &context->m_TextureSamplers[i]);
            }

            for (uint8_t i=0; i < context->m_MainScratchBuffers.Size(); i++)
            {
                DestroyDeviceBuffer(vk_device, &context->m_MainScratchBuffers[i].m_DeviceBuffer.m_Handle);
            }

            for (uint8_t i=0; i < context->m_MainDescriptorAllocators.Size(); i++)
            {
                DestroyDescriptorAllocator(vk_device, &context->m_MainDescriptorAllocators[i].m_Handle);
            }

            for (uint8_t i=0; i < context->m_MainCommandBuffers.Size(); i++)
            {
                FlushResourcesToDestroy(vk_device, context->m_MainResourcesToDestroy[i]);
            }

            for (size_t i = 0; i < g_max_frames_in_flight; i++) {
                FrameResource& frame_resource = context->m_FrameResources[i];
                vkDestroySemaphore(vk_device, frame_resource.m_RenderFinished, 0);
                vkDestroySemaphore(vk_device, frame_resource.m_ImageAvailable, 0);
                vkDestroyFence(vk_device, frame_resource.m_SubmitFence, 0);
            }

            DestroySwapChain(vk_device, context->m_SwapChain);
            DestroyLogicalDevice(&context->m_LogicalDevice);
            DestroyPhysicalDevice(&context->m_PhysicalDevice);

            vkDestroySurfaceKHR(context->m_Instance, context->m_WindowSurface, 0);

            DestroyInstance(&context->m_Instance);

            context->m_WindowOpened = 0;

            if (context->m_DynamicOffsetBuffer)
            {
                free(context->m_DynamicOffsetBuffer);
            }

            delete context->m_SwapChain;
        }
    }

    void VulkanIconifyWindow(HContext context)
    {
        assert(context);
        if (context->m_WindowOpened)
        {
        }
    }


    uint32_t VulkanGetWindowState(HContext context, WindowState state)
    {
        assert(context);
        if (context->m_WindowOpened)
        {
            switch(state) {
            case WINDOW_STATE_OPENED:    return 1;
            case WINDOW_STATE_ICONIFIED: return 0; // not supported
            default:
                {
                    printf("UNKNOWN WINDOW STATE: %d\n", state);
                    return 1;
                }
            }
        }
        return 0;
    }

    uint32_t VulkanGetDisplayDpi(HContext context)
    {
        return 0;
    }

    uint32_t VulkanGetWidth(HContext context)
    {
        return context->m_Width;
    }

    uint32_t VulkanGetHeight(HContext context)
    {
        return context->m_Height;
    }

    uint32_t VulkanGetWindowWidth(HContext context)
    {
        assert(context);
        return context->m_WindowWidth;
    }

    uint32_t VulkanGetWindowHeight(HContext context)
    {
        assert(context);
        return context->m_WindowHeight;
    }

    void VulkanGetNativeWindowSize(uint32_t* width, uint32_t* height)
    {
        nn::oe::GetDefaultDisplayResolution((int*)width, (int*)height);
    }

    void VulkanSetWindowSize(HContext context, uint32_t width, uint32_t height)
    {
        assert(context);
        if (context->m_WindowOpened)
        {
            context->m_Width = width;
            context->m_Height = height;
            context->m_WindowWidth = width;
            context->m_WindowHeight = height;

            // According to the NSDK documentation, this is the most effective way to resize since it skips recreating the swapchain
            nn::vi::SetLayerCrop(g_pLayer, 0, 0, width, height);

            if (context->m_WindowResizeCallback)
            {
                context->m_WindowResizeCallback(context->m_WindowResizeCallbackUserData, width, height);
            }
        }
    }

    void VulkanResizeWindow(HContext context, uint32_t width, uint32_t height)
    {
        VulkanSetWindowSize(context, width, height);
    }

    void SwapBuffers()
    {
    }

    void VulkanSetSwapInterval(HContext context, uint32_t swap_interval)
    {
    }
}
