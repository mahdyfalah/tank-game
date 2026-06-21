#include "swapchain_bundle.h"

#include <GLFW/glfw3.h>

#include <algorithm>
#include <assert.h>
#include <limits>
#include <stdexcept>

SwapchainBundle::SwapchainBundle(vk::raii::PhysicalDevice const &physicalDevice,
                                 vk::raii::Device const &device,
                                 vk::raii::SurfaceKHR const &surface,
                                 GLFWwindow *window)
    : physicalDevice(physicalDevice),
      device(device),
      surface(surface),
      window(window)
{
}

void SwapchainBundle::create()
{
    vk::SurfaceCapabilitiesKHR surfaceCapabilities = physicalDevice.getSurfaceCapabilitiesKHR(*surface);
    swapChainExtent = chooseSwapExtent(surfaceCapabilities);
    uint32_t minImageCount = chooseSwapMinImageCount(surfaceCapabilities);

    std::vector<vk::SurfaceFormatKHR> availableFormats = physicalDevice.getSurfaceFormatsKHR(*surface);
    swapChainSurfaceFormat = chooseSwapSurfaceFormat(availableFormats);

    std::vector<vk::PresentModeKHR> availablePresentModes = physicalDevice.getSurfacePresentModesKHR(*surface);
    vk::PresentModeKHR presentMode = chooseSwapPresentMode(availablePresentModes);

    vk::SwapchainCreateInfoKHR swapChainCreateInfo{
        .surface = *surface,
        .minImageCount = minImageCount,
        .imageFormat = swapChainSurfaceFormat.format,
        .imageColorSpace = swapChainSurfaceFormat.colorSpace,
        .imageExtent = swapChainExtent,
        .imageArrayLayers = 1,
        .imageUsage = vk::ImageUsageFlagBits::eColorAttachment,
        .imageSharingMode = vk::SharingMode::eExclusive,
        .preTransform = surfaceCapabilities.currentTransform,
        .compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque,
        .presentMode = presentMode,
        .clipped = true};

    swapChain = vk::raii::SwapchainKHR(device, swapChainCreateInfo);
    swapChainImages = swapChain.getImages();

    swapChainImageViews.clear();
    swapChainImageViews.reserve(swapChainImages.size());
    for (auto &image : swapChainImages)
    {
        swapChainImageViews.emplace_back(createImageView(image, swapChainSurfaceFormat.format, vk::ImageAspectFlagBits::eColor));
    }

    vk::Format depthFormat = getDepthFormat();
    std::tie(depthImage, depthImageMemory) = createImage(swapChainExtent.width,
                                                         swapChainExtent.height,
                                                         depthFormat,
                                                         vk::ImageTiling::eOptimal,
                                                         vk::ImageUsageFlagBits::eDepthStencilAttachment,
                                                         vk::MemoryPropertyFlagBits::eDeviceLocal);
    depthImageView = createImageView(*depthImage, depthFormat, vk::ImageAspectFlagBits::eDepth);
}

void SwapchainBundle::recreate()
{
    cleanup();
    create();
}

void SwapchainBundle::cleanup()
{
    swapChainImageViews.clear();
    depthImageView = nullptr;
    depthImage = nullptr;
    depthImageMemory = nullptr;
    swapChain = nullptr;
    swapChainImages.clear();
}

const vk::raii::SwapchainKHR &SwapchainBundle::getSwapChain() const
{
    return swapChain;
}

const std::vector<vk::Image> &SwapchainBundle::getImages() const
{
    return swapChainImages;
}

const std::vector<vk::raii::ImageView> &SwapchainBundle::getImageViews() const
{
    return swapChainImageViews;
}

const vk::SurfaceFormatKHR &SwapchainBundle::getSurfaceFormat() const
{
    return swapChainSurfaceFormat;
}

const vk::Extent2D &SwapchainBundle::getExtent() const
{
    return swapChainExtent;
}

const vk::raii::Image &SwapchainBundle::getDepthImage() const
{
    return depthImage;
}

const vk::raii::ImageView &SwapchainBundle::getDepthImageView() const
{
    return depthImageView;
}

vk::Format SwapchainBundle::getDepthFormat() const
{
    return findSupportedFormat({vk::Format::eD32Sfloat, vk::Format::eD32SfloatS8Uint, vk::Format::eD24UnormS8Uint},
                               vk::ImageTiling::eOptimal,
                               vk::FormatFeatureFlagBits::eDepthStencilAttachment);
}

uint32_t SwapchainBundle::chooseSwapMinImageCount(vk::SurfaceCapabilitiesKHR const &surfaceCapabilities)
{
    auto minImageCount = std::max(3u, surfaceCapabilities.minImageCount);
    if ((0 < surfaceCapabilities.maxImageCount) && (surfaceCapabilities.maxImageCount < minImageCount))
    {
        minImageCount = surfaceCapabilities.maxImageCount;
    }
    return minImageCount;
}

vk::SurfaceFormatKHR SwapchainBundle::chooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR> &availableFormats)
{
    assert(!availableFormats.empty());
    const auto formatIt = std::ranges::find_if(
        availableFormats,
        [](const auto &format) { return format.format == vk::Format::eB8G8R8A8Srgb && format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear; });
    return formatIt != availableFormats.end() ? *formatIt : availableFormats[0];
}

vk::PresentModeKHR SwapchainBundle::chooseSwapPresentMode(std::vector<vk::PresentModeKHR> const &availablePresentModes)
{
    assert(std::ranges::any_of(availablePresentModes, [](auto presentMode) { return presentMode == vk::PresentModeKHR::eFifo; }));
    return std::ranges::any_of(availablePresentModes,
                               [](const vk::PresentModeKHR value) { return vk::PresentModeKHR::eMailbox == value; }) ?
               vk::PresentModeKHR::eMailbox :
               vk::PresentModeKHR::eFifo;
}

vk::Extent2D SwapchainBundle::chooseSwapExtent(vk::SurfaceCapabilitiesKHR const &capabilities) const
{
    if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
    {
        return capabilities.currentExtent;
    }

    int width, height;
    glfwGetFramebufferSize(window, &width, &height);

    return {
        std::clamp<uint32_t>(width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width),
        std::clamp<uint32_t>(height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height)};
}

vk::raii::ImageView SwapchainBundle::createImageView(vk::Image const &image, vk::Format format, vk::ImageAspectFlags aspectFlags) const
{
    vk::ImageViewCreateInfo viewInfo{
        .image = image,
        .viewType = vk::ImageViewType::e2D,
        .format = format,
        .subresourceRange = {.aspectMask = aspectFlags, .baseMipLevel = 0, .levelCount = 1, .baseArrayLayer = 0, .layerCount = 1}};
    return vk::raii::ImageView(device, viewInfo);
}

std::pair<vk::raii::Image, vk::raii::DeviceMemory> SwapchainBundle::createImage(uint32_t width,
                                                                                 uint32_t height,
                                                                                 vk::Format format,
                                                                                 vk::ImageTiling tiling,
                                                                                 vk::ImageUsageFlags usage,
                                                                                 vk::MemoryPropertyFlags properties) const
{
    vk::ImageCreateInfo imageInfo{
        .imageType = vk::ImageType::e2D,
        .format = format,
        .extent = {width, height, 1},
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = vk::SampleCountFlagBits::e1,
        .tiling = tiling,
        .usage = usage,
        .sharingMode = vk::SharingMode::eExclusive};

    vk::raii::Image image = vk::raii::Image(device, imageInfo);

    vk::MemoryRequirements memRequirements = image.getMemoryRequirements();
    vk::MemoryAllocateInfo allocInfo{
        .allocationSize = memRequirements.size,
        .memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties)};
    vk::raii::DeviceMemory imageMemory = vk::raii::DeviceMemory(device, allocInfo);
    image.bindMemory(imageMemory, 0);

    return {std::move(image), std::move(imageMemory)};
}

uint32_t SwapchainBundle::findMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties) const
{
    vk::PhysicalDeviceMemoryProperties memProperties = physicalDevice.getMemoryProperties();

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++)
    {
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties)
        {
            return i;
        }
    }

    throw std::runtime_error("failed to find suitable memory type!");
}

vk::Format SwapchainBundle::findSupportedFormat(const std::vector<vk::Format> &candidates,
                                                vk::ImageTiling tiling,
                                                vk::FormatFeatureFlags features) const
{
    for (const auto format : candidates)
    {
        vk::FormatProperties props = physicalDevice.getFormatProperties(format);
        if (((tiling == vk::ImageTiling::eLinear) && ((props.linearTilingFeatures & features) == features)) ||
            ((tiling == vk::ImageTiling::eOptimal) && ((props.optimalTilingFeatures & features) == features)))
        {
            return format;
        }
    }

    throw std::runtime_error("failed to find supported format!");
}
