#pragma once

#include <vector>

#if defined(__INTELLISENSE__) || !defined(USE_CPP20_MODULES)
#    include <vulkan/vulkan_raii.hpp>
#else
import vulkan_hpp;
#endif

struct GLFWwindow;

class SwapchainBundle
{
  public:
    SwapchainBundle(vk::raii::PhysicalDevice const &physicalDevice,
                    vk::raii::Device const &device,
                    vk::raii::SurfaceKHR const &surface,
                    GLFWwindow *window);

    void create();
    void recreate();
    void cleanup();

    [[nodiscard]] const vk::raii::SwapchainKHR &getSwapChain() const;
    [[nodiscard]] const std::vector<vk::Image> &getImages() const;
    [[nodiscard]] const std::vector<vk::raii::ImageView> &getImageViews() const;
    [[nodiscard]] const vk::SurfaceFormatKHR &getSurfaceFormat() const;
    [[nodiscard]] const vk::Extent2D &getExtent() const;

    [[nodiscard]] const vk::raii::Image &getDepthImage() const;
    [[nodiscard]] const vk::raii::ImageView &getDepthImageView() const;
    [[nodiscard]] vk::Format getDepthFormat() const;

  private:
    vk::raii::PhysicalDevice const &physicalDevice;
    vk::raii::Device const &device;
    vk::raii::SurfaceKHR const &surface;
    GLFWwindow *window;

    vk::raii::SwapchainKHR swapChain = nullptr;
    std::vector<vk::Image> swapChainImages;
    vk::SurfaceFormatKHR swapChainSurfaceFormat;
    vk::Extent2D swapChainExtent;
    std::vector<vk::raii::ImageView> swapChainImageViews;

    vk::raii::Image depthImage = nullptr;
    vk::raii::DeviceMemory depthImageMemory = nullptr;
    vk::raii::ImageView depthImageView = nullptr;

    static uint32_t chooseSwapMinImageCount(vk::SurfaceCapabilitiesKHR const &surfaceCapabilities);
    static vk::SurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<vk::SurfaceFormatKHR> &availableFormats);
    static vk::PresentModeKHR chooseSwapPresentMode(std::vector<vk::PresentModeKHR> const &availablePresentModes);
    vk::Extent2D chooseSwapExtent(vk::SurfaceCapabilitiesKHR const &capabilities) const;

    vk::raii::ImageView createImageView(vk::Image const &image, vk::Format format, vk::ImageAspectFlags aspectFlags) const;
    std::pair<vk::raii::Image, vk::raii::DeviceMemory> createImage(uint32_t width,
                                                                    uint32_t height,
                                                                    vk::Format format,
                                                                    vk::ImageTiling tiling,
                                                                    vk::ImageUsageFlags usage,
                                                                    vk::MemoryPropertyFlags properties) const;
    uint32_t findMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties) const;
    vk::Format findSupportedFormat(const std::vector<vk::Format> &candidates,
                                   vk::ImageTiling tiling,
                                   vk::FormatFeatureFlags features) const;
};