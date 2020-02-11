#ifndef WATERAPP_HPP
#define WATERAPP_HPP
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.hpp>
#include <sstream>
#include <cmath>
#include <iostream>

#define VK_VERSION_1_0 1
#define RUN_TIME_ERROR(e) (runTimeError(__FILE__,__LINE__,(e)))
#define VK_CHECK_RESULT(f)                                                              \
{																						\
    VkResult res = (f);                                                                 \
    if (res != VK_SUCCESS)                                                              \
    {																					\
        printf("Fatal : VkResult is %d in %s at line %d\n", res,  __FILE__, __LINE__);  \
        assert(res == VK_SUCCESS);                                                      \
    }																					\
}

using namespace std;

namespace app
{

const int WIDTH  = 800;
const int HEIGHT = 600;

const int MAX_FRAMES_IN_FLIGHT = 2;

static const char* g_validationLayerData = "VK_LAYER_LUNARG_standard_validation";
static const char* g_debugReportExtName  = VK_EXT_DEBUG_REPORT_EXTENSION_NAME;

class swapChainSupportDetails
{
public:
    VkSurfaceCapabilitiesKHR          capabilities;
    std::vector<VkSurfaceFormatKHR>   formats;
    std::vector<VkPresentModeKHR>     presentModes;
};

class application
{
public:
    void run();

private:
    GLFWwindow*                     windowApp;
    VkInstance                      instance;
    vector<const char*>             enabledLayers;
    VkDebugUtilsMessengerEXT        debugMessenger;
    VkSurfaceKHR                    surface;
    VkPhysicalDevice                physicalDevice = VK_NULL_HANDLE;
    VkDevice                        device;
    VkQueue                         graphicsQueue;
    VkQueue                         presentQueue;
    VkCommandPool                   commandPool;
    VkRenderPass                    renderPass;
    VkPipelineLayout                pipelineLayout;
    VkPipeline                      graphicsPipeline;
    VkBuffer                        m_vbo;     //
    VkDeviceMemory                  m_vboMem;  // we will store our vertices data here
    std::vector<VkCommandBuffer>    commandBuffers;
    size_t                          currentFrame = 0;

    struct screenBufferResources
    {
        VkSwapchainKHR             swapChain;
        std::vector<VkImage>       swapChainImages;
        VkFormat                   swapChainImageFormat;
        VkExtent2D                 swapChainExtent;
        std::vector<VkImageView>   swapChainImageViews;
        std::vector<VkFramebuffer> swapChainFramebuffers;
    };

    screenBufferResources screen;

    swapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device, VkSurfaceKHR a_surface);

    const std::vector<const char*>  deviceExtensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

    struct syncObj
    {
      std::vector<VkSemaphore> imageAvailableSemaphores;
      std::vector<VkSemaphore> renderFinishedSemaphores;
      std::vector<VkFence>     inFlightFences;
    } m_sync;

    static VKAPI_ATTR VkBool32 VKAPI_CALL debugReportCallbackFn(VkDebugReportFlagsEXT       flags,
                                                                VkDebugReportObjectTypeEXT  objectType,
                                                                uint64_t                    object,
                                                                size_t                      location,
                                                                int32_t                     messageCode,
                                                                const char*                 pLayerPrefix,
                                                                const char*                 pMessage,
                                                                void*                       pUserData)
    {
        printf("[Debug Report]: %s: %s\n", pLayerPrefix, pMessage);
        return VK_FALSE;
    } VkDebugReportCallbackEXT debugReportCallback;

    typedef VkBool32 (VKAPI_PTR *DebugReportCallbackFuncType)(VkDebugReportFlagsEXT      flags,
                                                              VkDebugReportObjectTypeEXT objectType,
                                                              uint64_t                   object,
                                                              size_t                     location,
                                                              int32_t                    messageCode,
                                                              const char*                pLayerPrefix,
                                                              const char*                pMessage,
                                                              void*                      pUserData);
    void initWindow();
    void initVulkan();
    void createResources();
    void mainLoop();
    void cleanup();
    VkInstance createInstance(bool                  a_enableValidationLayers,
                              vector<const char *>& a_enabledLayers,
                              vector<const char *>  a_extentions);
    void initDebugReportCallback(VkInstance                     a_instance,
                                 DebugReportCallbackFuncType    a_callback,
                                 VkDebugReportCallbackEXT*      a_debugReportCallback);
    void runTimeError(const char* file, int line, const char* msg);
    VkPhysicalDevice findPhysicalDevice(VkInstance a_instance, bool a_printInfo, int a_preferredDeviceId);
    uint32_t getQueueFamilyIndex(VkPhysicalDevice a_physicalDevice, VkQueueFlagBits a_bits);
    VkDevice createLogicalDevice(uint32_t                       queueFamilyIndex,
                                 VkPhysicalDevice               physicalDevice,
                                 const vector<const char *>&    a_enabledLayers,
                                 vector<const char *>           a_extentions);
    void createCwapChain(VkPhysicalDevice          a_physDevice,
                         VkDevice                  a_device,
                         VkSurfaceKHR              a_surface,
                         int                       a_width,
                         int                       a_height,
                         screenBufferResources*    a_buff);
    VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
    VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities, int a_width, int a_height);
    void createScreenImageViews(VkDevice a_device, screenBufferResources* pScreen);
    void createRenderPass(VkDevice a_device, VkFormat a_swapChainImageFormat, VkRenderPass* a_pRenderPass);
    void createGraphicsPipeline(VkDevice             a_device,
                                VkExtent2D           a_screenExtent,
                                VkRenderPass         a_renderPass,
                                VkPipelineLayout*    a_pLayout,
                                VkPipeline*          a_pPipiline);
    void createScreenFrameBuffers(VkDevice a_device, VkRenderPass a_renderPass, screenBufferResources* pScreen);
    void createVertexBuffer(VkDevice         a_device,
                            VkPhysicalDevice a_physDevice,
                            const size_t     a_bufferSize,
                            VkBuffer         *a_pBuffer,
                            VkDeviceMemory   *a_pBufferMemory);
    void createAndWriteCommandBuffers(VkDevice                       a_device,
                                             VkCommandPool                  a_cmdPool,
                                             std::vector<VkFramebuffer>     a_swapChainFramebuffers,
                                             VkExtent2D                     a_frameBufferExtent,
                                             VkRenderPass                   a_renderPass,
                                             VkPipeline                     a_graphicsPipeline,
                                             VkBuffer                       a_vPosBuffer,
                                             std::vector<VkCommandBuffer>*  a_cmdBuffers);
    void createSyncObjects(VkDevice a_device, syncObj* a_pSyncObjs);
    void putTriangleVerticesToVBO_Now(VkDevice       a_device,
                                             VkCommandPool  a_pool,
                                             VkQueue        a_queue,
                                             float*         a_triPos,
                                             int            a_floatsNum,
                                             VkBuffer       a_buffer);
    void runCommandBuffer(VkCommandBuffer a_cmdBuff, VkQueue a_queue, VkDevice a_device);
    void drawFrame(void);
    uint32_t findMemoryType(uint32_t memoryTypeBits, VkMemoryPropertyFlags properties, VkPhysicalDevice physicalDevice);
    VkPhysicalDevice findPhysicalDevice(VkInstance a_instance, bool a_printInfo, uint64_t a_preferredDeviceId);
    VkShaderModule createShaderModule(VkDevice a_device, const std::vector<uint32_t>& code);
    std::vector<uint32_t> readFile(const char* filename);
};
}
#endif // WATERAPP_HPP
