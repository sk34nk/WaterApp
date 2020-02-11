#include "createApp.hpp"

using namespace std;
using namespace app;

const bool enableValidationLayers = true;
#ifdef QT_NO_DEBUG*/
const bool enableValidationLayers = false;
#endif

void application::run()
{
    initWindow();
    initVulkan();
    createResources();
    mainLoop();
    cleanup();
}

void application::initWindow(void)
{
    glfwInit();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    windowApp = glfwCreateWindow(WIDTH, HEIGHT, "Water with Vulkan API", NULL, NULL);
}

VkPhysicalDevice application::findPhysicalDevice(VkInstance a_instance, bool a_printInfo, uint64_t a_preferredDeviceId)
{
    uint32_t deviceCount;
    vkEnumeratePhysicalDevices(a_instance, &deviceCount, NULL);
    if (deviceCount == 0) RUN_TIME_ERROR("findPhysicalDevice, no Vulkan devices found");

    vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(a_instance, &deviceCount, devices.data());

    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;

    if(a_printInfo != 0) std::cout << "findPhysicalDevice: { " << std::endl;

    VkPhysicalDeviceProperties props;
    VkPhysicalDeviceFeatures   features;

    for (size_t i = 0; i < devices.size(); i++)
    {
        vkGetPhysicalDeviceProperties(devices[i], &props);
        vkGetPhysicalDeviceFeatures(devices[i], &features);

        if(a_printInfo != 0) std::cout << "  device " << i << ", name = " << props.deviceName << std::endl;
        if(i == a_preferredDeviceId) physicalDevice = devices[i];
    }
    if(a_printInfo != 0) std::cout << "}" << std::endl;

    if(physicalDevice == VK_NULL_HANDLE) physicalDevice = devices[0];

    if(physicalDevice == VK_NULL_HANDLE) RUN_TIME_ERROR("vk_utils::FindPhysicalDevice, no Vulkan devices with compute capability found");

    return physicalDevice;
}

void application::initVulkan(void)
{
    const uint64_t deviceId = 0;

        vector<const char*> extensions;
        {
          uint32_t glfwExtensionCount = 0;
          const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
          extensions = vector<const char*>(glfwExtensions, glfwExtensions + glfwExtensionCount);
        }

        instance = createInstance(enableValidationLayers, enabledLayers, extensions);
        if (enableValidationLayers != 0) initDebugReportCallback(instance, &debugReportCallbackFn, &debugReportCallback);

        if (glfwCreateWindowSurface(instance, windowApp, NULL, &surface) != VK_SUCCESS)
            throw runtime_error("glfwCreateWindowSurface: failed to create window surface!");

        physicalDevice = findPhysicalDevice(instance, true, deviceId);
        uint32_t queueFID  = getQueueFamilyIndex(physicalDevice, VK_QUEUE_GRAPHICS_BIT);

        VkBool32 presentSupport = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, queueFID, surface, &presentSupport);
        if (presentSupport == VK_FALSE)
            throw std::runtime_error("vkGetPhysicalDeviceSurfaceSupportKHR: no present support for the target device and graphics queue");

        device = createLogicalDevice(queueFID, physicalDevice, enabledLayers, deviceExtensions);
        vkGetDeviceQueue(device, queueFID, 0, &graphicsQueue);
        vkGetDeviceQueue(device, queueFID, 0, &presentQueue);

        {
            VkCommandPoolCreateInfo poolInfo = {};
            poolInfo.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
            poolInfo.queueFamilyIndex = getQueueFamilyIndex(physicalDevice, VK_QUEUE_GRAPHICS_BIT);

            if (vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool) != VK_SUCCESS)
                throw std::runtime_error("[CreateCommandPoolAndBuffers]: failed to create command pool!");
        }

        createCwapChain(physicalDevice, device, surface, WIDTH, HEIGHT, &screen);

        createScreenImageViews(device, &screen);
}

void application::createResources(void)
  {
    createRenderPass(device, screen.swapChainImageFormat, &renderPass);

    createGraphicsPipeline(device, screen.swapChainExtent, renderPass, &pipelineLayout, &graphicsPipeline);

    createScreenFrameBuffers(device, renderPass, &screen);

    createVertexBuffer(device, physicalDevice, 6 * sizeof(float), &m_vbo, &m_vboMem);

    createAndWriteCommandBuffers(device, commandPool, screen.swapChainFramebuffers, screen.swapChainExtent, renderPass, graphicsPipeline, m_vbo,
                                 &commandBuffers);

    createSyncObjects(device, &m_sync);

    // put our vertices to GPU
    //
    float trianglePos[] =
    {
      -0.8f, -0.8f,
      0.8f, -0.8f,
      0.8f, 0.8f,
    };

    putTriangleVerticesToVBO_Now(device, commandPool, graphicsQueue, trianglePos, 6*2, m_vbo);
}

void application::mainLoop(void)
  {
    while (!glfwWindowShouldClose(windowApp))
    {
      glfwPollEvents();
      drawFrame();
    }

    vkDeviceWaitIdle(device);
  }

void application::cleanup(void)
{
    // free our vbo
    vkFreeMemory(device, m_vboMem, NULL);
    vkDestroyBuffer(device, m_vbo, NULL);

    if (enableValidationLayers)
    {
        // destroy callback.
        auto func = (PFN_vkDestroyDebugReportCallbackEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugReportCallbackEXT");
        if (func == NULL)
            throw std::runtime_error("Could not load vkDestroyDebugReportCallbackEXT");
        func(instance, debugReportCallback, NULL);
    }

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        vkDestroySemaphore(device, m_sync.renderFinishedSemaphores[i], NULL);
        vkDestroySemaphore(device, m_sync.imageAvailableSemaphores[i], NULL);
        vkDestroyFence    (device, m_sync.inFlightFences[i], NULL);
    }

    vkDestroyCommandPool(device, commandPool, NULL);

    for (auto framebuffer : screen.swapChainFramebuffers) vkDestroyFramebuffer(device, framebuffer, NULL);

    vkDestroyPipeline      (device, graphicsPipeline, NULL);
    vkDestroyPipelineLayout(device, pipelineLayout, NULL);
    vkDestroyRenderPass    (device, renderPass, NULL);

    for (auto imageView : screen.swapChainImageViews) vkDestroyImageView(device, imageView, NULL);

    vkDestroySwapchainKHR(device, screen.swapChain, NULL);
    vkDestroyDevice(device, NULL);

    vkDestroySurfaceKHR(instance, surface, NULL);
    vkDestroyInstance(instance, NULL);

    glfwDestroyWindow(windowApp);

    glfwTerminate();
}

VkInstance application::createInstance(bool                     a_enableValidationLayers,
                                       vector<const char *>&    a_enabledLayers,
                                       vector<const char *>     a_extentions)
{
    vector<const char *> enabledExtensions = a_extentions;
    /*
    By enabling validation layers, Vulkan will emit warnings if the API
    is used incorrectly. We shall enable the layer VK_LAYER_LUNARG_standard_validation,
    which is basically a collection of several useful validation layers.
    */
    if (a_enableValidationLayers)
    {
        /*
        We get all supported layers with vkEnumerateInstanceLayerProperties.
        */
        uint32_t layerCount;
        vkEnumerateInstanceLayerProperties(&layerCount, NULL);

        vector<VkLayerProperties> layerProperties(layerCount);
        vkEnumerateInstanceLayerProperties(&layerCount, layerProperties.data());
        /*
        And then we simply check if VK_LAYER_LUNARG_standard_validation is among the supported layers.
        */
        bool foundLayer = false;
        for (VkLayerProperties prop : layerProperties)
        {
            if (strcmp("VK_LAYER_LUNARG_standard_validation", prop.layerName) == 0)
            {
                foundLayer = true;
                break;
            }
        }

        if (!foundLayer) RUN_TIME_ERROR("Layer VK_LAYER_LUNARG_standard_validation not supported\n");

        a_enabledLayers.push_back(g_validationLayerData); // Alright, we can use this layer.

        /*
        We need to enable an extension named VK_EXT_DEBUG_REPORT_EXTENSION_NAME,
        in order to be able to print the warnings emitted by the validation layer.

        So again, we just check if the extension is among the supported extensions.
        */
        uint32_t extensionCount;

        vkEnumerateInstanceExtensionProperties(NULL, &extensionCount, NULL);
        vector<VkExtensionProperties> extensionProperties(extensionCount);
        vkEnumerateInstanceExtensionProperties(NULL, &extensionCount, extensionProperties.data());

        bool foundExtension = false;
        for (VkExtensionProperties prop : extensionProperties) {
            if (strcmp(VK_EXT_DEBUG_REPORT_EXTENSION_NAME, prop.extensionName) == 0) {
              foundExtension = true;
              break;
            }
        }

        if (!foundExtension)
            RUN_TIME_ERROR("Extension VK_EXT_DEBUG_REPORT_EXTENSION_NAME not supported\n");

        enabledExtensions.push_back(g_debugReportExtName);
    }

    /*
    Next, we actually create the instance.

    Contains application info. This is actually not that important.
    The only real important field is apiVersion.
    */
    VkApplicationInfo applicationInfo = {};
    applicationInfo.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    applicationInfo.pApplicationName   = "Water app";
    applicationInfo.applicationVersion = 0;
    applicationInfo.pEngineName        = "WaterEngine";
    applicationInfo.engineVersion      = 0;
    applicationInfo.apiVersion         = VK_API_VERSION_1_0;

    VkInstanceCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.flags = 0;
    createInfo.pApplicationInfo = &applicationInfo;

    // Give our desired layers and extensions to vulkan.
    createInfo.enabledLayerCount       = uint32_t(a_enabledLayers.size());
    createInfo.ppEnabledLayerNames     = a_enabledLayers.data();
    createInfo.enabledExtensionCount   = uint32_t(enabledExtensions.size());
    createInfo.ppEnabledExtensionNames = enabledExtensions.data();

    /*
    Actually create the instance.
    Having created the instance, we can actually start using vulkan.
    */
    VkInstance instance;
    VK_CHECK_RESULT(vkCreateInstance(&createInfo, NULL, &instance));

    return instance;
}

void application::initDebugReportCallback(VkInstance                    a_instance,
                                          DebugReportCallbackFuncType   a_callback,
                                          VkDebugReportCallbackEXT*     a_debugReportCallback)
{
  // Register a callback function for the extension VK_EXT_DEBUG_REPORT_EXTENSION_NAME, so that warnings emitted from the validation
  // layer are actually printed.

  VkDebugReportCallbackCreateInfoEXT createInfo = {};
  createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
  createInfo.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT | VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT;
  createInfo.pfnCallback = a_callback;

  // We have to explicitly load this function.
  //
  auto vkCreateDebugReportCallbackEXT = (PFN_vkCreateDebugReportCallbackEXT)vkGetInstanceProcAddr(a_instance, "vkCreateDebugReportCallbackEXT");
  if (vkCreateDebugReportCallbackEXT == nullptr)
    RUN_TIME_ERROR("Could not load vkCreateDebugReportCallbackEXT");

  // Create and register callback.
  VK_CHECK_RESULT(vkCreateDebugReportCallbackEXT(a_instance, &createInfo, NULL, a_debugReportCallback));
}



uint32_t application::getQueueFamilyIndex(VkPhysicalDevice a_physicalDevice, VkQueueFlagBits a_bits)
{
    uint32_t queueFamilyCount;

    vkGetPhysicalDeviceQueueFamilyProperties(a_physicalDevice, &queueFamilyCount, NULL);

    // Retrieve all queue families.
    vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(a_physicalDevice, &queueFamilyCount, queueFamilies.data());

    // Now find a family that supports compute.
    uint32_t i = 0;
    for (; i < queueFamilies.size(); ++i)
    {
        VkQueueFamilyProperties props = queueFamilies[i];

        if (props.queueCount > 0 && (props.queueFlags & a_bits))  // found a queue with compute. We're done!
        break;
    }

    if (i == queueFamilies.size())
        RUN_TIME_ERROR("getComputeQueueFamilyIndex: could not find a queue family that supports operations");

    return i;
}

VkDevice application::createLogicalDevice(uint32_t                       queueFamilyIndex,
                             VkPhysicalDevice               physicalDevice,
                             const vector<const char *>&    a_enabledLayers,
                             vector<const char *>           a_extentions)
{
    // When creating the device, we also specify what queues it has.
    //
    VkDeviceQueueCreateInfo queueCreateInfo = {};
    queueCreateInfo.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfo.queueFamilyIndex = queueFamilyIndex;
    queueCreateInfo.queueCount       = 1;    // create one queue in this family. We don't need more.
    float queuePriorities            = 1.0;  // we only have one queue, so this is not that imporant.
    queueCreateInfo.pQueuePriorities = &queuePriorities;

    // Now we create the logical device. The logical device allows us to interact with the physical device.
    //
    VkDeviceCreateInfo deviceCreateInfo = {};

    // Specify any desired device features here. We do not need any for this application, though.
    //
    VkPhysicalDeviceFeatures deviceFeatures = {};

    deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceCreateInfo.enabledLayerCount    = uint32_t(a_enabledLayers.size());  // need to specify validation layers here as well.
    deviceCreateInfo.ppEnabledLayerNames  = a_enabledLayers.data();
    deviceCreateInfo.pQueueCreateInfos    = &queueCreateInfo;        // when creating the logical device, we also specify what queues it has.
    deviceCreateInfo.queueCreateInfoCount = 1;
    deviceCreateInfo.pEnabledFeatures     = &deviceFeatures;
    deviceCreateInfo.enabledExtensionCount   = static_cast<uint32_t>(a_extentions.size());
    deviceCreateInfo.ppEnabledExtensionNames = a_extentions.data();

    VkDevice device;
    VK_CHECK_RESULT(vkCreateDevice(physicalDevice, &deviceCreateInfo, NULL, &device)); // create logical device.

    return device;
}

swapChainSupportDetails application::querySwapChainSupport(VkPhysicalDevice device, VkSurfaceKHR a_surface)
{
    swapChainSupportDetails details;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, a_surface, &details.capabilities);

    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, a_surface, &formatCount, nullptr);

    if (formatCount != 0) {
        details.formats.resize(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, a_surface, &formatCount, details.formats.data());
    }

    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, a_surface, &presentModeCount, nullptr);

    if (presentModeCount != 0) {
        details.presentModes.resize(presentModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, a_surface, &presentModeCount, details.presentModes.data());
    }
    return details;
}

void application::createCwapChain(VkPhysicalDevice          a_physDevice,
                                  VkDevice                  a_device,
                                  VkSurfaceKHR              a_surface,
                                  int                       a_width,
                                  int                       a_height,
                                  screenBufferResources*    a_buff)
{
    swapChainSupportDetails swapChainSupport = querySwapChainSupport(a_physDevice, a_surface);

    VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
    VkPresentModeKHR presentMode     = chooseSwapPresentMode(swapChainSupport.presentModes);
    VkExtent2D extent                = chooseSwapExtent(swapChainSupport.capabilities, a_width, a_height);

    uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
    if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount) {
        imageCount = swapChainSupport.capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR createInfo = {};
    createInfo.sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface          = a_surface;
    createInfo.minImageCount    = imageCount;
    createInfo.imageFormat      = surfaceFormat.format;
    createInfo.imageColorSpace  = surfaceFormat.colorSpace;
    createInfo.imageExtent      = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    createInfo.preTransform     = swapChainSupport.capabilities.currentTransform;
    createInfo.compositeAlpha   = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode      = presentMode;
    createInfo.clipped          = VK_TRUE;
    createInfo.oldSwapchain     = VK_NULL_HANDLE;

    if (vkCreateSwapchainKHR(a_device, &createInfo, nullptr, &a_buff->swapChain) != VK_SUCCESS)
        throw std::runtime_error("[vk_utils::CreateCwapChain]: failed to create swap chain!");

    vkGetSwapchainImagesKHR(a_device, a_buff->swapChain, &imageCount, nullptr);
    a_buff->swapChainImages.resize(imageCount);
    vkGetSwapchainImagesKHR(a_device, a_buff->swapChain, &imageCount, a_buff->swapChainImages.data());

    a_buff->swapChainImageFormat = surfaceFormat.format;
    a_buff->swapChainExtent      = extent;
}

VkSurfaceFormatKHR application::chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats)
{
    for (const auto& availableFormat : availableFormats) {
        if (availableFormat.format == VK_FORMAT_B8G8R8A8_UNORM && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
          return availableFormat;
        }
    }
    return availableFormats[0];
}

VkPresentModeKHR application::chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes)
{
    for (const auto& availablePresentMode : availablePresentModes) {
        if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) return availablePresentMode;
    }
    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D application::chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities, int a_width, int a_height)
{
    if (capabilities.currentExtent.width != UINT32_MAX)
    {
        return capabilities.currentExtent;
    }
    else
    {
        VkExtent2D actualExtent = { uint32_t(a_width), uint32_t(a_height) };

        actualExtent.width  = std::max(capabilities.minImageExtent.width, std::min(capabilities.maxImageExtent.width, actualExtent.width));
        actualExtent.height = std::max(capabilities.minImageExtent.height, std::min(capabilities.maxImageExtent.height, actualExtent.height));

        return actualExtent;
    }
}

void application::createScreenImageViews(VkDevice a_device, screenBufferResources* pScreen)
{
    pScreen->swapChainImageViews.resize(pScreen->swapChainImages.size());

    for (size_t i = 0; i < pScreen->swapChainImages.size(); i++)
    {
        VkImageViewCreateInfo createInfo = {};
        createInfo.sType        = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        createInfo.image        = pScreen->swapChainImages[i];
        createInfo.viewType     = VK_IMAGE_VIEW_TYPE_2D;
        createInfo.format       = pScreen->swapChainImageFormat;
        createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        createInfo.subresourceRange.baseMipLevel   = 0;
        createInfo.subresourceRange.levelCount     = 1;
        createInfo.subresourceRange.baseArrayLayer = 0;
        createInfo.subresourceRange.layerCount     = 1;

        if (vkCreateImageView(a_device, &createInfo, nullptr, &pScreen->swapChainImageViews[i]) != VK_SUCCESS)
            throw std::runtime_error("[vk_utils::CreateImageViews]: failed to create image views!");
    }
}


void application::createScreenFrameBuffers(VkDevice a_device, VkRenderPass a_renderPass, screenBufferResources* pScreen)
{
    pScreen->swapChainFramebuffers.resize(pScreen->swapChainImageViews.size());
    for (size_t i = 0; i < pScreen->swapChainImageViews.size(); i++)
    {
        VkImageView attachments[] = { pScreen->swapChainImageViews[i] };

        VkFramebufferCreateInfo framebufferInfo = {};
        framebufferInfo.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass      = a_renderPass;
        framebufferInfo.attachmentCount = 1;
        framebufferInfo.pAttachments    = attachments;
        framebufferInfo.width           = pScreen->swapChainExtent.width;
        framebufferInfo.height          = pScreen->swapChainExtent.height;
        framebufferInfo.layers          = 1;

        if (vkCreateFramebuffer(a_device, &framebufferInfo, NULL, &pScreen->swapChainFramebuffers[i]) != VK_SUCCESS)
            throw std::runtime_error("failed to create framebuffer!");
    }
}

void application::createRenderPass(VkDevice       a_device,
                                   VkFormat       a_swapChainImageFormat,
                                   VkRenderPass*  a_pRenderPass)
{
    VkAttachmentDescription colorAttachment = {};
    colorAttachment.format         = a_swapChainImageFormat;
    colorAttachment.samples        = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorAttachmentRef = {};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass  = {};
    subpass.pipelineBindPoint     = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount  = 1;
    subpass.pColorAttachments     = &colorAttachmentRef;

    VkSubpassDependency dependency = {};
    dependency.srcSubpass    = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass    = 0;
    dependency.srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo renderPassInfo = {};
    renderPassInfo.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments    = &colorAttachment;
    renderPassInfo.subpassCount    = 1;
    renderPassInfo.pSubpasses      = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies   = &dependency;

    if (vkCreateRenderPass(a_device, &renderPassInfo, nullptr, a_pRenderPass) != VK_SUCCESS)
        throw std::runtime_error("[CreateRenderPass]: failed to create render pass!");
}

void application::createGraphicsPipeline(VkDevice             a_device,
                                         VkExtent2D           a_screenExtent,
                                         VkRenderPass         a_renderPass,
                                         VkPipelineLayout*    a_pLayout,
                                         VkPipeline*          a_pPipiline)
{
    auto vertShaderCode = readFile("../WaterApp/shaders/vert.spv");
    auto fragShaderCode = readFile("../WaterApp/shaders/frag.spv");

    VkShaderModule vertShaderModule = createShaderModule(a_device, vertShaderCode);
    VkShaderModule fragShaderModule = createShaderModule(a_device, fragShaderCode);

    VkPipelineShaderStageCreateInfo vertShaderStageInfo = {};
    vertShaderStageInfo.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertShaderStageInfo.stage  = VK_SHADER_STAGE_VERTEX_BIT;
    vertShaderStageInfo.module = vertShaderModule;
    vertShaderStageInfo.pName  = "main";

    VkPipelineShaderStageCreateInfo fragShaderStageInfo = {};
    fragShaderStageInfo.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragShaderStageInfo.stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
    fragShaderStageInfo.module = fragShaderModule;
    fragShaderStageInfo.pName  = "main";

    VkPipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };

    VkVertexInputBindingDescription vInputBinding = { };
    vInputBinding.binding   = 0;
    vInputBinding.stride    = sizeof(float) * 2;
    vInputBinding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkVertexInputAttributeDescription vAttribute = {};
    vAttribute.binding  = 0;
    vAttribute.location = 0;
    vAttribute.format   = VK_FORMAT_R32G32_SFLOAT;
    vAttribute.offset   = 0;

    VkPipelineVertexInputStateCreateInfo vertexInputInfo = {};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount   = 1;
    vertexInputInfo.vertexAttributeDescriptionCount = 1;
    vertexInputInfo.pVertexBindingDescriptions      = &vInputBinding;
    vertexInputInfo.pVertexAttributeDescriptions    = &vAttribute;

    VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
    inputAssembly.sType                  = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology               = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    VkViewport viewport = {};
    viewport.x        = 0.0f;
    viewport.y        = 0.0f;
    viewport.width    = static_cast<float>(a_screenExtent.width);
    viewport.height   = static_cast<float>(a_screenExtent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor = {};
    scissor.offset = { 0, 0 };
    scissor.extent = a_screenExtent;

    VkPipelineViewportStateCreateInfo viewportState = {};
    viewportState.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports    = &viewport;
    viewportState.scissorCount  = 1;
    viewportState.pScissors     = &scissor;

    VkPipelineRasterizationStateCreateInfo rasterizer = {};
    rasterizer.sType                   = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable        = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode             = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth               = 1.0f;
    rasterizer.cullMode                = VK_CULL_MODE_NONE; // VK_CULL_MODE_BACK_BIT;
    rasterizer.frontFace               = VK_FRONT_FACE_CLOCKWISE;
    rasterizer.depthBiasEnable         = VK_FALSE;

    VkPipelineMultisampleStateCreateInfo multisampling = {};
    multisampling.sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable  = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable    = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo colorBlending = {};
    colorBlending.sType             = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable     = VK_FALSE;
    colorBlending.logicOp           = VK_LOGIC_OP_COPY;
    colorBlending.attachmentCount   = 1;
    colorBlending.pAttachments      = &colorBlendAttachment;
    colorBlending.blendConstants[0] = 0.0f;
    colorBlending.blendConstants[1] = 0.0f;
    colorBlending.blendConstants[2] = 0.0f;
    colorBlending.blendConstants[3] = 0.0f;

    VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
    pipelineLayoutInfo.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount         = 0;
    pipelineLayoutInfo.pushConstantRangeCount = 0;

    if (vkCreatePipelineLayout(a_device, &pipelineLayoutInfo, NULL, a_pLayout) != VK_SUCCESS)
        throw std::runtime_error("[CreateGraphicsPipeline]: failed to create pipeline layout!");

    VkGraphicsPipelineCreateInfo pipelineInfo = {};
    pipelineInfo.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount          = 2;
    pipelineInfo.pStages             = shaderStages;
    pipelineInfo.pVertexInputState   = &vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState      = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState   = &multisampling;
    pipelineInfo.pColorBlendState    = &colorBlending;
    pipelineInfo.layout              = (*a_pLayout);
    pipelineInfo.renderPass          = a_renderPass;
    pipelineInfo.subpass             = 0;
    pipelineInfo.basePipelineHandle  = VK_NULL_HANDLE;

    if (vkCreateGraphicsPipelines(a_device, VK_NULL_HANDLE, 1, &pipelineInfo, NULL, a_pPipiline) != VK_SUCCESS)
        throw std::runtime_error("[CreateGraphicsPipeline]: failed to create graphics pipeline!");

    vkDestroyShaderModule(a_device, fragShaderModule, NULL);
    vkDestroyShaderModule(a_device, vertShaderModule, NULL);
}

void application::createVertexBuffer(VkDevice         a_device,
                                     VkPhysicalDevice a_physDevice,
                                     const size_t     a_bufferSize,
                                     VkBuffer         *a_pBuffer,
                                     VkDeviceMemory   *a_pBufferMemory)
{
   VkBufferCreateInfo bufferCreateInfo = {};
   bufferCreateInfo.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
   bufferCreateInfo.pNext       = NULL;
   bufferCreateInfo.size        = a_bufferSize;
   bufferCreateInfo.usage       = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
   bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

   VK_CHECK_RESULT(vkCreateBuffer(a_device, &bufferCreateInfo, NULL, a_pBuffer)); // create bufferStaging.

   VkMemoryRequirements memoryRequirements;
   vkGetBufferMemoryRequirements(a_device, (*a_pBuffer), &memoryRequirements);

   VkMemoryAllocateInfo allocateInfo = {};
   allocateInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
   allocateInfo.pNext           = NULL;
   allocateInfo.allocationSize  = memoryRequirements.size; // specify required memory.
   allocateInfo.memoryTypeIndex = findMemoryType(memoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, a_physDevice); // #NOTE VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT

   VK_CHECK_RESULT(vkAllocateMemory(a_device, &allocateInfo, NULL, a_pBufferMemory));   // allocate memory on device.

   VK_CHECK_RESULT(vkBindBufferMemory(a_device, (*a_pBuffer), (*a_pBufferMemory), 0));  // Now associate that allocated memory with the bufferStaging. With that, the bufferStaging is backed by actual memory.
}

void application::createAndWriteCommandBuffers(VkDevice                       a_device,
                                               VkCommandPool                  a_cmdPool,
                                               std::vector<VkFramebuffer>     a_swapChainFramebuffers,
                                               VkExtent2D                     a_frameBufferExtent,
                                               VkRenderPass                   a_renderPass,
                                               VkPipeline                     a_graphicsPipeline,
                                               VkBuffer                       a_vPosBuffer,
                                               std::vector<VkCommandBuffer>*  a_cmdBuffers)
{
    std::vector<VkCommandBuffer>& commandBuffers = (*a_cmdBuffers);

    commandBuffers.resize(a_swapChainFramebuffers.size());

    VkCommandBufferAllocateInfo allocInfo = {};
    allocInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool        = a_cmdPool;
    allocInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = (uint32_t)commandBuffers.size();

    if (vkAllocateCommandBuffers(a_device, &allocInfo, commandBuffers.data()) != VK_SUCCESS)
        throw std::runtime_error("[CreateCommandPoolAndBuffers]: failed to allocate command buffers!");

    for (size_t i = 0; i < commandBuffers.size(); i++)
    {
        VkCommandBufferBeginInfo beginInfo = {};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

        if (vkBeginCommandBuffer(commandBuffers[i], &beginInfo) != VK_SUCCESS)
            throw std::runtime_error("[CreateCommandPoolAndBuffers]: failed to begin recording command buffer!");

        VkRenderPassBeginInfo renderPassInfo = {};
        renderPassInfo.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass        = a_renderPass;
        renderPassInfo.framebuffer       = a_swapChainFramebuffers[i];
        renderPassInfo.renderArea.offset = { 0, 0 };
        renderPassInfo.renderArea.extent = a_frameBufferExtent;

        VkClearValue clearColor = { 0.0f, 0.0f, 0.0f, 1.0f };
        renderPassInfo.clearValueCount = 1;
        renderPassInfo.pClearValues = &clearColor;

        vkCmdBeginRenderPass(commandBuffers[i], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

        vkCmdBindPipeline(commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, a_graphicsPipeline);

        // say we want to take vertices pos from a_vPosBuffer
        {
            VkBuffer vertexBuffers[] = { a_vPosBuffer };
            VkDeviceSize offsets[]   = { 0 };
            vkCmdBindVertexBuffers(commandBuffers[i], 0, 1, vertexBuffers, offsets);
        }

        vkCmdDraw(commandBuffers[i], 3, 1, 0, 0);

        vkCmdEndRenderPass(commandBuffers[i]);

        if (vkEndCommandBuffer(commandBuffers[i]) != VK_SUCCESS) {
            throw std::runtime_error("failed to record command buffer!");
        }
    }
}

void application::createSyncObjects(VkDevice a_device, syncObj* a_pSyncObjs)
{
    a_pSyncObjs->imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    a_pSyncObjs->renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    a_pSyncObjs->inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

    VkSemaphoreCreateInfo semaphoreInfo = {};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo = {};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
    {
        if (vkCreateSemaphore(a_device, &semaphoreInfo, nullptr, &a_pSyncObjs->imageAvailableSemaphores[i]) != VK_SUCCESS ||
            vkCreateSemaphore(a_device, &semaphoreInfo, nullptr, &a_pSyncObjs->renderFinishedSemaphores[i]) != VK_SUCCESS ||
            vkCreateFence    (a_device, &fenceInfo,     nullptr, &a_pSyncObjs->inFlightFences[i]) != VK_SUCCESS)
        {
            throw std::runtime_error("[CreateSyncObjects]: failed to create synchronization objects for a frame!");
        }
    }
}

void application::runCommandBuffer(VkCommandBuffer a_cmdBuff, VkQueue a_queue, VkDevice a_device)
{
    // Now we shall finally submit the recorded command bufferStaging to a queue.
    //
    VkSubmitInfo submitInfo = {};
    submitInfo.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1; // submit a single command bufferStaging
    submitInfo.pCommandBuffers    = &a_cmdBuff; // the command bufferStaging to submit.

    VkFence fence;
    VkFenceCreateInfo fenceCreateInfo = {};
    fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceCreateInfo.flags = 0;
    VK_CHECK_RESULT(vkCreateFence(a_device, &fenceCreateInfo, NULL, &fence));

    // We submit the command bufferStaging on the queue, at the same time giving a fence.
    VK_CHECK_RESULT(vkQueueSubmit(a_queue, 1, &submitInfo, fence));

    // The command will not have finished executing until the fence is signalled.
    // So we wait here. We will directly after this read our bufferStaging from the GPU,
    // and we will not be sure that the command has finished executing unless we wait for the fence.
    // Hence, we use a fence here.
    VK_CHECK_RESULT(vkWaitForFences(a_device, 1, &fence, VK_TRUE, 100000000000));

    vkDestroyFence(a_device, fence, NULL);
}

void application::putTriangleVerticesToVBO_Now(VkDevice       a_device,
                                               VkCommandPool  a_pool,
                                               VkQueue        a_queue,
                                               float*         a_triPos,
                                               int            a_floatsNum,
                                               VkBuffer       a_buffer)
{
    VkCommandBufferAllocateInfo allocInfo = {
        .sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool        = a_pool,
        .level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1
    };

    VkCommandBuffer cmdBuff;
    if (vkAllocateCommandBuffers(a_device, &allocInfo, &cmdBuff) != VK_SUCCESS)
        throw std::runtime_error("[putTriangleVerticesToVBO_Now]: failed to allocate command buffer!");

    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(cmdBuff, &beginInfo);
    vkCmdUpdateBuffer   (cmdBuff, a_buffer, 0, a_floatsNum * sizeof(float), a_triPos);
    vkEndCommandBuffer  (cmdBuff);

    runCommandBuffer(cmdBuff, a_queue, a_device);

    vkFreeCommandBuffers(a_device, a_pool, 1, &cmdBuff);
}

void application::drawFrame(void)
{
    vkWaitForFences(device, 1, &m_sync.inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);
    vkResetFences  (device, 1, &m_sync.inFlightFences[currentFrame]);

    uint32_t imageIndex;
    vkAcquireNextImageKHR(device, screen.swapChain, UINT64_MAX, m_sync.imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);

    VkSemaphore      waitSemaphores[] = { m_sync.imageAvailableSemaphores[currentFrame] };
    VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };

    VkSubmitInfo submitInfo = {};
    submitInfo.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores    = waitSemaphores;
    submitInfo.pWaitDstStageMask  = waitStages;

    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers    = &commandBuffers[imageIndex];

    VkSemaphore signalSemaphores[]  = { m_sync.renderFinishedSemaphores[currentFrame] };
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores    = signalSemaphores;

    if (vkQueueSubmit(graphicsQueue, 1, &submitInfo, m_sync.inFlightFences[currentFrame]) != VK_SUCCESS)
        throw std::runtime_error("[DrawFrame]: failed to submit draw command buffer!");

    VkPresentInfoKHR presentInfo = {};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores    = signalSemaphores;

    VkSwapchainKHR swapChains[] = { screen.swapChain };
    presentInfo.swapchainCount  = 1;
    presentInfo.pSwapchains     = swapChains;
    presentInfo.pImageIndices   = &imageIndex;

    vkQueuePresentKHR(presentQueue, &presentInfo);
    currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}

uint32_t application::findMemoryType(uint32_t memoryTypeBits, VkMemoryPropertyFlags properties, VkPhysicalDevice physicalDevice)
{
    VkPhysicalDeviceMemoryProperties memoryProperties;

    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memoryProperties);

    /*How does this search work?
    See the documentation of VkPhysicalDeviceMemoryProperties for a detailed description.*/
    for (size_t i = 0; i < memoryProperties.memoryTypeCount; ++i)
    {
        if ((memoryTypeBits & (1 << i)) && ((memoryProperties.memoryTypes[i].propertyFlags & properties) == properties))
            return i;
    }
    return -1;
}

VkShaderModule application::createShaderModule(VkDevice a_device, const std::vector<uint32_t>& code)
{
    VkShaderModuleCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size() * sizeof(uint32_t);
    createInfo.pCode = code.data();

    VkShaderModule shaderModule;
    if (vkCreateShaderModule(a_device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS)
        throw std::runtime_error("[CreateShaderModule]: failed to create shader module!");

    return shaderModule;
}

std::vector<uint32_t> application::readFile(const char* filename)
{
    FILE* fp = fopen(filename, "rb");
    if (fp == NULL)
    {
        std::string errorMsg = std::string("readFile, can't open file ") + std::string(filename);
        RUN_TIME_ERROR(errorMsg.c_str());
    }

    // get file size.
    fseek(fp, 0, SEEK_END);
    long filesize = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    long filesizepadded = long(ceil(filesize / 4.0)) * 4;

    std::vector<uint32_t> resData(filesizepadded/4);

    // read file contents.
    char *str = (char*)resData.data();
    fread(str, filesize, sizeof(char), fp);
    fclose(fp);

    // data padding.
    for (int i = filesize; i < filesizepadded; i++)
        str[i] = 0;

    return resData;
}

void application::runTimeError(const char* file, int line, const char* msg)
{
    std::stringstream strout;
    strout << "runtime_error at " << file << ", line " << line << ": " << msg << std::endl;
    throw std::runtime_error(strout.str().c_str());
}
