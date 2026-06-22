#include <algorithm>
#include <array>
#include <assert.h>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <vector>

#if defined(__INTELLISENSE__) || !defined(USE_CPP20_MODULES)
#	include <vulkan/vulkan_raii.hpp>
#else
import vulkan_hpp;
#endif

#define GLFW_INCLUDE_VULKAN        // REQUIRED only for GLFW CreateWindowSurface.
#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "camera.h"
#include "game.h"
#include "ground_tile_map.h"
#include "render/swapchain_bundle.h"
#include "scene/tank.h"
#include "scene/tank_controller.h"
#include "scene/bullet.h"
#include "scene/crate.h"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

constexpr uint32_t WIDTH                = 800;
constexpr uint32_t HEIGHT               = 600;
constexpr int      MAX_FRAMES_IN_FLIGHT = 2;

const std::vector<char const *> validationLayers = {
    "VK_LAYER_KHRONOS_validation"};

#ifdef NDEBUG
constexpr bool enableValidationLayers = false;
#else
constexpr bool enableValidationLayers = true;
#endif

struct Vertex
{
	glm::vec3 pos;
	glm::vec3 color;
	glm::vec2 texCoord;

	static vk::VertexInputBindingDescription getBindingDescription()
	{
		return {.binding = 0, .stride = sizeof(Vertex), .inputRate = vk::VertexInputRate::eVertex};
	}

	static std::array<vk::VertexInputAttributeDescription, 3> getAttributeDescriptions()
	{
		return {{{.location = 0, .binding = 0, .format = vk::Format::eR32G32B32Sfloat, .offset = offsetof(Vertex, pos)},
		         {.location = 1, .binding = 0, .format = vk::Format::eR32G32B32Sfloat, .offset = offsetof(Vertex, color)},
		         {.location = 2, .binding = 0, .format = vk::Format::eR32G32Sfloat, .offset = offsetof(Vertex, texCoord)}}};
	}
};

struct UniformBufferObject
{
	glm::mat4 model;
	glm::mat4 view;
	glm::mat4 proj;
};

static void appendMeshToScene(std::vector<Vertex> &sceneVertices,
	                          std::vector<uint32_t> &sceneIndices,
	                          const std::vector<glm::vec3> &positions,
	                          const std::vector<glm::vec2> &texCoords,
	                          const std::vector<uint32_t> &indices,
	                          const glm::vec3 &color)
{
	const uint32_t baseVertex = static_cast<uint32_t>(sceneVertices.size());
	sceneVertices.reserve(sceneVertices.size() + positions.size());

	for (size_t i = 0; i < positions.size(); ++i)
	{
		const glm::vec2 uv = i < texCoords.size() ? texCoords[i] : glm::vec2(0.0f, 0.0f);
		sceneVertices.push_back({positions[i], color, uv});
	}

	sceneIndices.reserve(sceneIndices.size() + indices.size());
	for (uint32_t index : indices)
	{
		sceneIndices.push_back(baseVertex + index);
	}
}

constexpr uint32_t MAP_TILES_PER_SIDE = 50;
constexpr float    MAP_TILE_SIZE      = 1.0f;
constexpr float    MAP_UV_PER_TILE    = 1.0f;
constexpr float    MAP_HALF_EXTENT    = (static_cast<float>(MAP_TILES_PER_SIDE) * MAP_TILE_SIZE) * 0.5f;

constexpr glm::vec3 TANK_SPAWN_POSITION = {0.0f, 0.0f, 0.0f};
constexpr float     TANK_DESIRED_LENGTH = 3.2f;
constexpr float     TANK_GROUND_OFFSET  = 0.0f;
constexpr glm::vec3 TANK_COLOR          = {1.0f, 1.0f, 1.0f};
constexpr const char *GROUND_TEXTURE_PATH = "textures/grass.jpg";
constexpr const char *TANK_TEXTURE_PATH   = "models/cartoon_tank/textures/Main.001_baseColor.png";
constexpr const char *BULLET_TEXTURE_PATH = "models/9mm_bullet/textures/bullet_baseColor.jpeg";

// Bullets / shooting.
constexpr int   MAX_BULLETS           = 16;
constexpr float BULLET_DESIRED_LENGTH = 0.6f;
constexpr float BULLET_SPEED          = 32.0f;
constexpr float BULLET_SPAWN_HEIGHT   = 0.6f;
constexpr float BULLET_MUZZLE_OFFSET  = 1.9f;
constexpr float BULLET_MAX_RANGE      = MAP_HALF_EXTENT * 2.5f;

// Crates / targets.
constexpr const char *CRATE_TEXTURE_PATH = "models/wooden_crate/textures/Scene_-_Root_baseColor.png";
constexpr int   MAX_CRATES            = 5;
constexpr float CRATE_DESIRED_SIZE    = 1.4f;
constexpr float CRATE_SPAWN_INTERVAL  = 5.0f;
constexpr float CRATE_SPAWN_MARGIN    = 4.0f;
constexpr float CRATE_HIT_RADIUS      = 1.1f;

constexpr glm::vec3 CAMERA_POSITION = {18.0f, -18.0f, 18.0f};
constexpr glm::vec3 CAMERA_TARGET   = {0.0f, 0.0f, 0.0f};
constexpr glm::vec3 CAMERA_UP       = {0.0f, 0.0f, 1.0f};

// Offset kept between the camera and the tank it follows (isometric viewpoint).
constexpr glm::vec3 CAMERA_FOLLOW_OFFSET = CAMERA_POSITION - CAMERA_TARGET;
// Higher values snap to the tank faster; lower values give a softer trail.
constexpr float     CAMERA_FOLLOW_RATE   = 5.0f;

Camera mainCamera(CAMERA_POSITION, CAMERA_TARGET, CAMERA_UP, 25.0f, 0.1f, 100.0f);

class HelloTriangleApplication
{
  public:
	void run()
	{
		initWindow();
		initVulkan();
		mainLoop();
		cleanup();
	}

  private:
	GLFWwindow                      *window = nullptr;
	vk::raii::Context                context;
	vk::raii::Instance               instance       = nullptr;
	vk::raii::DebugUtilsMessengerEXT debugMessenger = nullptr;
	vk::raii::SurfaceKHR             surface        = nullptr;
	vk::raii::PhysicalDevice         physicalDevice = nullptr;
	vk::raii::Device                 device         = nullptr;
	uint32_t                         queueIndex     = ~0;
	vk::raii::Queue                  queue          = nullptr;
	std::unique_ptr<SwapchainBundle> swapchainBundle;

	vk::raii::DescriptorSetLayout descriptorSetLayout = nullptr;
	vk::raii::PipelineLayout      pipelineLayout      = nullptr;
	vk::raii::Pipeline            graphicsPipeline    = nullptr;

	vk::raii::Image        groundTextureImage       = nullptr;
	vk::raii::DeviceMemory groundTextureImageMemory = nullptr;
	vk::raii::ImageView    groundTextureImageView   = nullptr;
	vk::raii::Image        tankTextureImage         = nullptr;
	vk::raii::DeviceMemory tankTextureImageMemory   = nullptr;
	vk::raii::ImageView    tankTextureImageView     = nullptr;
	vk::raii::Image        bulletTextureImage       = nullptr;
	vk::raii::DeviceMemory bulletTextureImageMemory = nullptr;
	vk::raii::ImageView    bulletTextureImageView   = nullptr;
	vk::raii::Image        crateTextureImage        = nullptr;
	vk::raii::DeviceMemory crateTextureImageMemory  = nullptr;
	vk::raii::ImageView    crateTextureImageView    = nullptr;
	vk::raii::Sampler      textureSampler     = nullptr;

	vk::raii::Buffer       vertexBuffer       = nullptr;
	vk::raii::DeviceMemory vertexBufferMemory = nullptr;
	vk::raii::Buffer       indexBuffer        = nullptr;
	vk::raii::DeviceMemory indexBufferMemory  = nullptr;

	std::vector<Vertex>   sceneVertices;
	std::vector<uint32_t> sceneIndices;
	uint32_t              groundIndexCount = 0;
	uint32_t              tankFirstIndex   = 0;
	uint32_t              tankIndexCount   = 0;
	uint32_t              bulletFirstIndex = 0;
	uint32_t              bulletIndexCount = 0;
	uint32_t              crateFirstIndex  = 0;
	uint32_t              crateIndexCount  = 0;

	std::vector<vk::raii::Buffer>       groundUniformBuffers;
	std::vector<vk::raii::DeviceMemory> groundUniformBuffersMemory;
	std::vector<void *>                 groundUniformBuffersMapped;
	std::vector<vk::raii::Buffer>       tankUniformBuffers;
	std::vector<vk::raii::DeviceMemory> tankUniformBuffersMemory;
	std::vector<void *>                 tankUniformBuffersMapped;
	std::vector<vk::raii::Buffer>       bulletUniformBuffers;
	std::vector<vk::raii::DeviceMemory> bulletUniformBuffersMemory;
	std::vector<void *>                 bulletUniformBuffersMapped;

	vk::raii::DescriptorPool             descriptorPool = nullptr;
	std::vector<vk::raii::DescriptorSet> groundDescriptorSets;
	std::vector<vk::raii::DescriptorSet> tankDescriptorSets;
	std::vector<vk::raii::DescriptorSet> bulletDescriptorSets;
	std::vector<vk::raii::Buffer>       crateUniformBuffers;
	std::vector<vk::raii::DeviceMemory> crateUniformBuffersMemory;
	std::vector<void *>                 crateUniformBuffersMapped;
	std::vector<vk::raii::DescriptorSet> crateDescriptorSets;

	vk::raii::CommandPool                commandPool = nullptr;
	std::vector<vk::raii::CommandBuffer> commandBuffers;

	std::vector<vk::raii::Semaphore> presentCompleteSemaphores;
	std::vector<vk::raii::Semaphore> renderFinishedSemaphores;
	std::vector<vk::raii::Fence>     inFlightFences;
	uint32_t                         frameIndex = 0;
	TankController                   tankController{TANK_SPAWN_POSITION, MAP_HALF_EXTENT, TANK_SPAWN_POSITION.z};
	BulletSystem                     bulletSystem{static_cast<std::size_t>(MAX_BULLETS), MAP_HALF_EXTENT, BULLET_MAX_RANGE};
	CrateSystem                      crateSystem{static_cast<std::size_t>(MAX_CRATES), CRATE_SPAWN_INTERVAL, MAP_HALF_EXTENT, CRATE_SPAWN_MARGIN};
	Game                             game{45.0f};
	vk::raii::DescriptorPool         imguiDescriptorPool = nullptr;
	bool                             spaceWasPressed = false;
	bool                             enterWasPressed = false;
	std::chrono::high_resolution_clock::time_point lastFrameTime = std::chrono::high_resolution_clock::now();

	bool framebufferResized = false;

	std::vector<const char *> requiredDeviceExtension = {
	    vk::KHRSwapchainExtensionName,
	    vk::KHRSpirv14ExtensionName,
	    vk::KHRSynchronization2ExtensionName};

	void initWindow()
	{
		glfwInit();

		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

		window = glfwCreateWindow(WIDTH, HEIGHT, "Tank Game", nullptr, nullptr);
		glfwSetWindowUserPointer(window, this);
		glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);
	}

	static void framebufferResizeCallback(GLFWwindow *window, int width, int height)
	{
		auto app                = reinterpret_cast<HelloTriangleApplication *>(glfwGetWindowUserPointer(window));
		app->framebufferResized = true;
	}

	void initVulkan()
	{
		createInstance();
		setupDebugMessenger();
		createSurface();
		pickPhysicalDevice();
		createLogicalDevice();
		swapchainBundle = std::make_unique<SwapchainBundle>(physicalDevice, device, surface, window);
		swapchainBundle->create();
		buildSceneGeometry();
		createDescriptorSetLayout();
		createGraphicsPipeline();
		createCommandPool();
		createTextureImages();
		createTextureImageViews();
		createTextureSampler();
		createVertexBuffer();
		createIndexBuffer();
		createUniformBuffers();
		createDescriptorPool();
		createDescriptorSets();
		createCommandBuffers();
		createSyncObjects();
		initImGui();
	}

	void initImGui()
	{
		// A descriptor pool dedicated to ImGui. It allocates one combined image
		// sampler per texture (font atlas plus any extra textures), so keep a few
		// spare slots to stay compatible across ImGui backend versions.
		std::array<vk::DescriptorPoolSize, 1> poolSizes{{{.type = vk::DescriptorType::eCombinedImageSampler, .descriptorCount = 16}}};
		vk::DescriptorPoolCreateInfo          poolInfo{
		             .flags         = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
		             .maxSets       = 16,
		             .poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
		             .pPoolSizes    = poolSizes.data()};
		imguiDescriptorPool = vk::raii::DescriptorPool(device, poolInfo);

		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGui::StyleColorsDark();
		ImGui::GetStyle().ScaleAllSizes(1.2f);

		ImGui_ImplGlfw_InitForVulkan(window, true);

		VkFormat                      colorFormat = static_cast<VkFormat>(swapchainBundle->getSurfaceFormat().format);
		VkPipelineRenderingCreateInfo pipelineRenderingInfo{};
		pipelineRenderingInfo.sType                   = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
		pipelineRenderingInfo.colorAttachmentCount    = 1;
		pipelineRenderingInfo.pColorAttachmentFormats = &colorFormat;
		pipelineRenderingInfo.depthAttachmentFormat   = static_cast<VkFormat>(swapchainBundle->getDepthFormat());

		ImGui_ImplVulkan_InitInfo initInfo{};
		initInfo.ApiVersion                                       = VK_API_VERSION_1_3;
		initInfo.Instance                                        = static_cast<VkInstance>(*instance);
		initInfo.PhysicalDevice                                  = static_cast<VkPhysicalDevice>(*physicalDevice);
		initInfo.Device                                          = static_cast<VkDevice>(*device);
		initInfo.QueueFamily                                     = queueIndex;
		initInfo.Queue                                           = static_cast<VkQueue>(*queue);
		initInfo.DescriptorPool                                  = static_cast<VkDescriptorPool>(*imguiDescriptorPool);
		initInfo.MinImageCount                                   = MAX_FRAMES_IN_FLIGHT;
		initInfo.ImageCount                                      = static_cast<uint32_t>(swapchainBundle->getImages().size());
		initInfo.UseDynamicRendering                             = true;
		initInfo.PipelineInfoMain.MSAASamples                    = VK_SAMPLE_COUNT_1_BIT;
		initInfo.PipelineInfoMain.PipelineRenderingCreateInfo    = pipelineRenderingInfo;
		ImGui_ImplVulkan_Init(&initInfo);
	}

	// Returns the tank, bullets and crates to their initial state for a new round.
	void resetWorld()
	{
		tankController.reset();
		bulletSystem.clear();
		crateSystem.reset();
		mainCamera.follow(tankController.getPosition(), CAMERA_FOLLOW_OFFSET, 1.0f);
	}

	void buildSceneGeometry()
	{
		sceneVertices.clear();
		sceneIndices.clear();

		GroundTileMap         groundTileMap(MAP_TILES_PER_SIDE, MAP_TILE_SIZE, MAP_UV_PER_TILE);
		const GroundMeshData &groundMesh = groundTileMap.getMeshData();
		appendMeshToScene(sceneVertices,
		                  sceneIndices,
		                  groundMesh.positions,
		                  groundMesh.texCoords,
		                  groundMesh.indices,
		                  {1.0f, 1.0f, 1.0f});
		groundIndexCount = static_cast<uint32_t>(groundMesh.indices.size());

		const Tank tank(resolveResourcePath("models/cartoon_tank/scene.gltf"),
		                {.position = {0.0f, 0.0f, 0.0f},
		                 .desiredLength = TANK_DESIRED_LENGTH,
		                 .groundOffset = TANK_GROUND_OFFSET,
		                 .color = TANK_COLOR});
		const TankMeshData &tankMesh = tank.getMeshData();
		appendMeshToScene(sceneVertices,
		                  sceneIndices,
		                  tankMesh.positions,
		                  tankMesh.texCoords,
		                  tankMesh.indices,
		                  tankMesh.color);
		tankFirstIndex = groundIndexCount;
		tankIndexCount = static_cast<uint32_t>(tankMesh.indices.size());

		const BulletMeshData bulletMesh = loadBulletMesh(resolveResourcePath("models/9mm_bullet/scene.gltf"),
		                                                 {.desiredLength = BULLET_DESIRED_LENGTH});
		bulletFirstIndex = static_cast<uint32_t>(sceneIndices.size());
		appendMeshToScene(sceneVertices,
		                  sceneIndices,
		                  bulletMesh.positions,
		                  bulletMesh.texCoords,
		                  bulletMesh.indices,
		                  bulletMesh.color);
		bulletIndexCount = static_cast<uint32_t>(bulletMesh.indices.size());

		const CrateMeshData crateMesh = loadCrateMesh(resolveResourcePath("models/wooden_crate/scene.gltf"),
		                                              {.desiredSize = CRATE_DESIRED_SIZE});
		crateFirstIndex = static_cast<uint32_t>(sceneIndices.size());
		appendMeshToScene(sceneVertices,
		                  sceneIndices,
		                  crateMesh.positions,
		                  crateMesh.texCoords,
		                  crateMesh.indices,
		                  crateMesh.color);
		crateIndexCount = static_cast<uint32_t>(crateMesh.indices.size());

		if (sceneVertices.empty() || sceneIndices.empty() || groundIndexCount == 0 || tankIndexCount == 0 || bulletIndexCount == 0 || crateIndexCount == 0)
		{
			throw std::runtime_error("Scene geometry is empty.");
		}
	}

	void mainLoop()
	{
		lastFrameTime = std::chrono::high_resolution_clock::now();
		while (!glfwWindowShouldClose(window))
		{
			glfwPollEvents();
			auto  currentFrameTime = std::chrono::high_resolution_clock::now();
			float deltaTimeSeconds = std::chrono::duration<float>(currentFrameTime - lastFrameTime).count();
			lastFrameTime          = currentFrameTime;

			// --- ImGui frame + HUD / menus ------------------------------------
			ImGui_ImplVulkan_NewFrame();
			ImGui_ImplGlfw_NewFrame();
			ImGui::NewFrame();
			const vk::Extent2D &extent = swapchainBundle->getExtent();
			game.buildUi(static_cast<float>(extent.width), static_cast<float>(extent.height));

			// Enter also starts / restarts the round (rising edge).
			const bool enterIsPressed = glfwGetKey(window, GLFW_KEY_ENTER) == GLFW_PRESS;
			if (enterIsPressed && !enterWasPressed && !game.isPlaying())
			{
				game.start();
			}
			enterWasPressed = enterIsPressed;

			if (game.consumeJustStarted())
			{
				resetWorld();
			}

			// --- World simulation (only while a round is running) -------------
			if (game.isPlaying())
			{
				tankController.update(window, deltaTimeSeconds);

				// Fire a bullet on the rising edge of Space (one shot per press).
				const bool spaceIsPressed = glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS;
				if (spaceIsPressed && !spaceWasPressed)
				{
					const glm::vec3 forward = tankController.getForward();
					glm::vec3       muzzle  = tankController.getPosition() + forward * BULLET_MUZZLE_OFFSET;
					muzzle.z                = BULLET_SPAWN_HEIGHT;
					bulletSystem.fire(muzzle, forward, BULLET_SPEED);
				}
				spaceWasPressed = spaceIsPressed;

				bulletSystem.update(deltaTimeSeconds);
				crateSystem.update(deltaTimeSeconds);

				// Resolve bullet/crate hits: shooting a crate removes both and scores.
				{
					const std::vector<Bullet> &bullets = bulletSystem.getBullets();
					const std::vector<Crate>  &crates  = crateSystem.getCrates();
					std::vector<std::size_t>   crateHits;
					std::vector<std::size_t>   bulletHits;
					for (std::size_t bi = 0; bi < bullets.size(); ++bi)
					{
						for (std::size_t ci = 0; ci < crates.size(); ++ci)
						{
							const glm::vec3 &bp = bullets[bi].getPosition();
							const glm::vec3 &cp = crates[ci].getPosition();
							const float      dx = bp.x - cp.x;
							const float      dy = bp.y - cp.y;
							if (dx * dx + dy * dy <= CRATE_HIT_RADIUS * CRATE_HIT_RADIUS)
							{
								crateHits.push_back(ci);
								bulletHits.push_back(bi);
							}
						}
					}
					if (!crateHits.empty())
					{
						crateSystem.removeAt(crateHits);
						bulletSystem.removeAt(bulletHits);
						game.registerCrateHit(static_cast<int>(crateHits.size()));
					}
				}

				// Frame-rate independent smoothing toward the tank.
				const float cameraInterpolation = 1.0f - std::exp(-CAMERA_FOLLOW_RATE * deltaTimeSeconds);
				mainCamera.follow(tankController.getPosition(), CAMERA_FOLLOW_OFFSET, cameraInterpolation);
			}
			else
			{
				spaceWasPressed = false;
			}

			// Advance the countdown last so the final frame still renders the world.
			game.update(deltaTimeSeconds);

			ImGui::Render();
			drawFrame();
		}

		device.waitIdle();
	}

	void cleanup()
	{
		ImGui_ImplVulkan_Shutdown();
		ImGui_ImplGlfw_Shutdown();
		ImGui::DestroyContext();

		glfwDestroyWindow(window);

		glfwTerminate();
	}

	void recreateSwapChain()
	{
		int width = 0, height = 0;
		while (width == 0 || height == 0)
		{
			glfwGetFramebufferSize(window, &width, &height);
			glfwWaitEvents();
		}

		device.waitIdle();
		swapchainBundle->recreate();
	}

	void createInstance()
	{
		constexpr vk::ApplicationInfo appInfo{.pApplicationName   = "Hello Triangle",
		                                      .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
		                                      .pEngineName        = "No Engine",
		                                      .engineVersion      = VK_MAKE_VERSION(1, 0, 0),
		                                      .apiVersion         = vk::ApiVersion14};

		// Get the required layers
		std::vector<char const *> requiredLayers;
		if (enableValidationLayers)
		{
			requiredLayers.assign(validationLayers.begin(), validationLayers.end());
		}

		// Check if the required layers are supported by the Vulkan implementation.
		auto layerProperties    = context.enumerateInstanceLayerProperties();
		auto unsupportedLayerIt = std::ranges::find_if(requiredLayers,
		                                               [&layerProperties](auto const &requiredLayer) {
			                                               return std::ranges::none_of(layerProperties,
			                                                                           [requiredLayer](auto const &layerProperty) { return strcmp(layerProperty.layerName, requiredLayer) == 0; });
		                                               });
		if (unsupportedLayerIt != requiredLayers.end())
		{
			throw std::runtime_error("Required layer not supported: " + std::string(*unsupportedLayerIt));
		}

		// Get the required extensions.
		auto requiredExtensions = getRequiredInstanceExtensions();

		// Check if the required extensions are supported by the Vulkan implementation.
		auto extensionProperties = context.enumerateInstanceExtensionProperties();
		auto unsupportedPropertyIt =
		    std::ranges::find_if(requiredExtensions,
		                         [&extensionProperties](auto const &requiredExtension) {
			                         return std::ranges::none_of(extensionProperties,
			                                                     [requiredExtension](auto const &extensionProperty) { return strcmp(extensionProperty.extensionName, requiredExtension) == 0; });
		                         });
		if (unsupportedPropertyIt != requiredExtensions.end())
		{
			throw std::runtime_error("Required extension not supported: " + std::string(*unsupportedPropertyIt));
		}

		vk::InstanceCreateInfo createInfo{.pApplicationInfo        = &appInfo,
		                                  .enabledLayerCount       = static_cast<uint32_t>(requiredLayers.size()),
		                                  .ppEnabledLayerNames     = requiredLayers.data(),
		                                  .enabledExtensionCount   = static_cast<uint32_t>(requiredExtensions.size()),
		                                  .ppEnabledExtensionNames = requiredExtensions.data()};
		instance = vk::raii::Instance(context, createInfo);
	}

	void setupDebugMessenger()
	{
		if (!enableValidationLayers)
			return;

		vk::DebugUtilsMessageSeverityFlagsEXT severityFlags(vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning |
		                                                    vk::DebugUtilsMessageSeverityFlagBitsEXT::eError);
		vk::DebugUtilsMessageTypeFlagsEXT     messageTypeFlags(
		    vk::DebugUtilsMessageTypeFlagBitsEXT::eGeneral | vk::DebugUtilsMessageTypeFlagBitsEXT::ePerformance | vk::DebugUtilsMessageTypeFlagBitsEXT::eValidation);
		vk::DebugUtilsMessengerCreateInfoEXT debugUtilsMessengerCreateInfoEXT{.messageSeverity = severityFlags,
		                                                                      .messageType     = messageTypeFlags,
		                                                                      .pfnUserCallback = &debugCallback};
		debugMessenger = instance.createDebugUtilsMessengerEXT(debugUtilsMessengerCreateInfoEXT);
	}

	void createSurface()
	{
		VkSurfaceKHR _surface;
		if (glfwCreateWindowSurface(*instance, window, nullptr, &_surface) != 0)
		{
			throw std::runtime_error("failed to create window surface!");
		}
		surface = vk::raii::SurfaceKHR(instance, _surface);
	}

	bool isDeviceSuitable(vk::raii::PhysicalDevice const &physicalDevice)
	{
		// Check if the physicalDevice supports the Vulkan 1.3 API version
		bool supportsVulkan1_3 = physicalDevice.getProperties().apiVersion >= VK_API_VERSION_1_3;

		// Check if any of the queue families support graphics operations
		auto queueFamilies    = physicalDevice.getQueueFamilyProperties();
		bool supportsGraphics = std::ranges::any_of(queueFamilies, [](auto const &qfp) { return !!(qfp.queueFlags & vk::QueueFlagBits::eGraphics); });

		// Check if all required physicalDevice extensions are available
		auto availableDeviceExtensions = physicalDevice.enumerateDeviceExtensionProperties();
		bool supportsAllRequiredExtensions =
		    std::ranges::all_of(requiredDeviceExtension,
		                        [&availableDeviceExtensions](auto const &requiredDeviceExtension) {
			                        return std::ranges::any_of(availableDeviceExtensions,
			                                                   [requiredDeviceExtension](auto const &availableDeviceExtension) { return strcmp(availableDeviceExtension.extensionName, requiredDeviceExtension) == 0; });
		                        });

		// Check if the physicalDevice supports the required features
		auto features                 = physicalDevice.template getFeatures2<vk::PhysicalDeviceFeatures2,
		                                                                     vk::PhysicalDeviceVulkan13Features,
		                                                                     vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>();
		bool supportsRequiredFeatures = features.template get<vk::PhysicalDeviceFeatures2>().features.samplerAnisotropy &&
		                                features.template get<vk::PhysicalDeviceVulkan13Features>().dynamicRendering &&
		                                features.template get<vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT>().extendedDynamicState;

		// Return true if the physicalDevice meets all the criteria
		return supportsVulkan1_3 && supportsGraphics && supportsAllRequiredExtensions && supportsRequiredFeatures;
	}

	void pickPhysicalDevice()
	{
		std::vector<vk::raii::PhysicalDevice> physicalDevices = instance.enumeratePhysicalDevices();
		auto const                            devIter         = std::ranges::find_if(physicalDevices, [&](auto const &physicalDevice) { return isDeviceSuitable(physicalDevice); });
		if (devIter == physicalDevices.end())
		{
			throw std::runtime_error("failed to find a suitable GPU!");
		}
		physicalDevice = *devIter;
	}

	void createLogicalDevice()
	{
		std::vector<vk::QueueFamilyProperties> queueFamilyProperties = physicalDevice.getQueueFamilyProperties();

		// get the first index into queueFamilyProperties which supports both graphics and present
		for (uint32_t qfpIndex = 0; qfpIndex < queueFamilyProperties.size(); qfpIndex++)
		{
			if ((queueFamilyProperties[qfpIndex].queueFlags & vk::QueueFlagBits::eGraphics) &&
			    physicalDevice.getSurfaceSupportKHR(qfpIndex, *surface))
			{
				// found a queue family that supports both graphics and present
				queueIndex = qfpIndex;
				break;
			}
		}
		if (queueIndex == ~0)
		{
			throw std::runtime_error("Could not find a queue for graphics and present -> terminating");
		}

		// query for Vulkan 1.3 features
		vk::StructureChain<vk::PhysicalDeviceFeatures2, vk::PhysicalDeviceVulkan13Features, vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT> featureChain = {
		    {.features = {.samplerAnisotropy = true}},                   // vk::PhysicalDeviceFeatures2
		    {.synchronization2 = true, .dynamicRendering = true},        // vk::PhysicalDeviceVulkan13Features
		    {.extendedDynamicState = true}                               // vk::PhysicalDeviceExtendedDynamicStateFeaturesEXT
		};

		// create a Device
		float                     queuePriority = 0.5f;
		vk::DeviceQueueCreateInfo deviceQueueCreateInfo{.queueFamilyIndex = queueIndex, .queueCount = 1, .pQueuePriorities = &queuePriority};
		vk::DeviceCreateInfo      deviceCreateInfo{.pNext                   = &featureChain.get<vk::PhysicalDeviceFeatures2>(),
		                                           .queueCreateInfoCount    = 1,
		                                           .pQueueCreateInfos       = &deviceQueueCreateInfo,
		                                           .enabledExtensionCount   = static_cast<uint32_t>(requiredDeviceExtension.size()),
		                                           .ppEnabledExtensionNames = requiredDeviceExtension.data()};

		device = vk::raii::Device(physicalDevice, deviceCreateInfo);
		queue  = vk::raii::Queue(device, queueIndex, 0);
	}

	void createDescriptorSetLayout()
	{
		std::array<vk::DescriptorSetLayoutBinding, 2> bindings{
		    {{.binding = 0, .descriptorType = vk::DescriptorType::eUniformBuffer, .descriptorCount = 1, .stageFlags = vk::ShaderStageFlagBits::eVertex},
		     {.binding = 1, .descriptorType = vk::DescriptorType::eCombinedImageSampler, .descriptorCount = 1, .stageFlags = vk::ShaderStageFlagBits::eFragment}}};

		vk::DescriptorSetLayoutCreateInfo layoutInfo{.bindingCount = static_cast<uint32_t>(bindings.size()), .pBindings = bindings.data()};
		descriptorSetLayout = vk::raii::DescriptorSetLayout(device, layoutInfo);
	}

	void createGraphicsPipeline()
	{
		vk::raii::ShaderModule shaderModule = createShaderModule(readFile(resolveResourcePath("shaders/slang.spv")));

		vk::PipelineShaderStageCreateInfo vertShaderStageInfo{.stage = vk::ShaderStageFlagBits::eVertex, .module = shaderModule, .pName = "vertMain"};
		vk::PipelineShaderStageCreateInfo fragShaderStageInfo{.stage = vk::ShaderStageFlagBits::eFragment, .module = shaderModule, .pName = "fragMain"};
		vk::PipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};

		auto                                     bindingDescription    = Vertex::getBindingDescription();
		auto                                     attributeDescriptions = Vertex::getAttributeDescriptions();
		vk::PipelineVertexInputStateCreateInfo   vertexInputInfo{.vertexBindingDescriptionCount   = 1,
		                                                         .pVertexBindingDescriptions      = &bindingDescription,
		                                                         .vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size()),
		                                                         .pVertexAttributeDescriptions    = attributeDescriptions.data()};
		vk::PipelineInputAssemblyStateCreateInfo inputAssembly{.topology = vk::PrimitiveTopology::eTriangleList};
		vk::PipelineViewportStateCreateInfo      viewportState{.viewportCount = 1, .scissorCount = 1};

		vk::PipelineRasterizationStateCreateInfo rasterizer{.depthClampEnable        = vk::False,
		                                                    .rasterizerDiscardEnable = vk::False,
		                                                    .polygonMode             = vk::PolygonMode::eFill,
		                                                    .cullMode                = vk::CullModeFlagBits::eNone,
		                                                    .frontFace               = vk::FrontFace::eCounterClockwise,
		                                                    .depthBiasEnable         = vk::False,
		                                                    .lineWidth               = 1.0f};

		vk::PipelineMultisampleStateCreateInfo multisampling{.rasterizationSamples = vk::SampleCountFlagBits::e1, .sampleShadingEnable = vk::False};

		vk::PipelineDepthStencilStateCreateInfo depthStencil{
		    .depthTestEnable       = vk::True,
		    .depthWriteEnable      = vk::True,
		    .depthCompareOp        = vk::CompareOp::eLess,
		    .depthBoundsTestEnable = vk::False,
		    .stencilTestEnable     = vk::False};
		vk::PipelineColorBlendAttachmentState colorBlendAttachment{
		    .blendEnable    = vk::False,
		    .colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA};

		vk::PipelineColorBlendStateCreateInfo colorBlending{
		    .logicOpEnable = vk::False, .logicOp = vk::LogicOp::eCopy, .attachmentCount = 1, .pAttachments = &colorBlendAttachment};

		std::vector<vk::DynamicState>      dynamicStates = {vk::DynamicState::eViewport, vk::DynamicState::eScissor};
		vk::PipelineDynamicStateCreateInfo dynamicState{.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()), .pDynamicStates = dynamicStates.data()};

		vk::PipelineLayoutCreateInfo pipelineLayoutInfo{.setLayoutCount = 1, .pSetLayouts = &*descriptorSetLayout, .pushConstantRangeCount = 0};
		pipelineLayout = vk::raii::PipelineLayout(device, pipelineLayoutInfo);

		vk::Format depthFormat = swapchainBundle->getDepthFormat();

		const vk::SurfaceFormatKHR &surfaceFormat = swapchainBundle->getSurfaceFormat();
		vk::StructureChain<vk::GraphicsPipelineCreateInfo, vk::PipelineRenderingCreateInfo> pipelineCreateInfoChain = {
		    {.stageCount          = 2,
		     .pStages             = shaderStages,
		     .pVertexInputState   = &vertexInputInfo,
		     .pInputAssemblyState = &inputAssembly,
		     .pViewportState      = &viewportState,
		     .pRasterizationState = &rasterizer,
		     .pMultisampleState   = &multisampling,
		     .pDepthStencilState  = &depthStencil,
		     .pColorBlendState    = &colorBlending,
		     .pDynamicState       = &dynamicState,
		     .layout              = pipelineLayout,
		     .renderPass          = nullptr},
		    {.colorAttachmentCount = 1, .pColorAttachmentFormats = &surfaceFormat.format, .depthAttachmentFormat = depthFormat}};

		graphicsPipeline = vk::raii::Pipeline(device, nullptr, pipelineCreateInfoChain.get<vk::GraphicsPipelineCreateInfo>());
	}

	void createCommandPool()
	{
		vk::CommandPoolCreateInfo poolInfo{.flags            = vk::CommandPoolCreateFlagBits::eResetCommandBuffer,
		                                   .queueFamilyIndex = queueIndex};
		commandPool = vk::raii::CommandPool(device, poolInfo);
	}

	void createTextureImageFromPath(const std::string &texturePath, vk::raii::Image &outImage, vk::raii::DeviceMemory &outImageMemory)
	{
		int            texWidth, texHeight, texChannels;
		stbi_uc       *pixels    = stbi_load(resolveResourcePath(texturePath).c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
		vk::DeviceSize imageSize = texWidth * texHeight * 4;

		if (!pixels)
		{
			throw std::runtime_error("failed to load texture image!");
		}

		auto [stagingBuffer, stagingBufferMemory] =
		    createBuffer(imageSize, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

		void *data = stagingBufferMemory.mapMemory(0, imageSize);
		memcpy(data, pixels, imageSize);
		stagingBufferMemory.unmapMemory();

		stbi_image_free(pixels);

		std::tie(outImage, outImageMemory) = createImage(texWidth,
		                                                 texHeight,
		                                                 vk::Format::eR8G8B8A8Srgb,
		                                                 vk::ImageTiling::eOptimal,
		                                                 vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled,
		                                                 vk::MemoryPropertyFlagBits::eDeviceLocal);

		vk::raii::CommandBuffer commandBuffer = beginSingleTimeCommands();
		transitionImageLayout(commandBuffer, outImage, vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal);
		copyBufferToImage(commandBuffer, stagingBuffer, outImage, static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight));
		transitionImageLayout(commandBuffer, outImage, vk::ImageLayout::eTransferDstOptimal, vk::ImageLayout::eShaderReadOnlyOptimal);
		endSingleTimeCommands(std::move(commandBuffer));
	}

	void createTextureImages()
	{
		createTextureImageFromPath(GROUND_TEXTURE_PATH, groundTextureImage, groundTextureImageMemory);
		createTextureImageFromPath(TANK_TEXTURE_PATH, tankTextureImage, tankTextureImageMemory);
		createTextureImageFromPath(BULLET_TEXTURE_PATH, bulletTextureImage, bulletTextureImageMemory);
		createTextureImageFromPath(CRATE_TEXTURE_PATH, crateTextureImage, crateTextureImageMemory);
	}

	void createTextureImageViews()
	{
		groundTextureImageView = createImageView(*groundTextureImage, vk::Format::eR8G8B8A8Srgb, vk::ImageAspectFlagBits::eColor);
		tankTextureImageView   = createImageView(*tankTextureImage, vk::Format::eR8G8B8A8Srgb, vk::ImageAspectFlagBits::eColor);
		bulletTextureImageView = createImageView(*bulletTextureImage, vk::Format::eR8G8B8A8Srgb, vk::ImageAspectFlagBits::eColor);
		crateTextureImageView  = createImageView(*crateTextureImage, vk::Format::eR8G8B8A8Srgb, vk::ImageAspectFlagBits::eColor);
	}

	void createTextureSampler()
	{
		vk::PhysicalDeviceProperties properties = physicalDevice.getProperties();
		vk::SamplerCreateInfo        samplerInfo{.magFilter        = vk::Filter::eLinear,
		                                         .minFilter        = vk::Filter::eLinear,
		                                         .mipmapMode       = vk::SamplerMipmapMode::eLinear,
		                                         .addressModeU     = vk::SamplerAddressMode::eRepeat,
		                                         .addressModeV     = vk::SamplerAddressMode::eRepeat,
		                                         .addressModeW     = vk::SamplerAddressMode::eRepeat,
		                                         .mipLodBias       = 0.0f,
		                                         .anisotropyEnable = vk::True,
		                                         .maxAnisotropy    = properties.limits.maxSamplerAnisotropy,
		                                         .compareEnable    = vk::False,
		                                         .compareOp        = vk::CompareOp::eAlways};
		textureSampler = vk::raii::Sampler(device, samplerInfo);
	}

	vk::raii::ImageView createImageView(vk::Image const &image, vk::Format format, vk::ImageAspectFlags aspectFlags)
	{
		vk::ImageViewCreateInfo viewInfo{
		    .image            = image,
		    .viewType         = vk::ImageViewType::e2D,
		    .format           = format,
		    .subresourceRange = {.aspectMask = aspectFlags, .baseMipLevel = 0, .levelCount = 1, .baseArrayLayer = 0, .layerCount = 1}};
		return vk::raii::ImageView(device, viewInfo);
	}

	std::pair<vk::raii::Image, vk::raii::DeviceMemory> createImage(uint32_t width, uint32_t height, vk::Format format, vk::ImageTiling tiling, vk::ImageUsageFlags usage, vk::MemoryPropertyFlags properties)
	{
		vk::ImageCreateInfo imageInfo{.imageType   = vk::ImageType::e2D,
		                              .format      = format,
		                              .extent      = {width, height, 1},
		                              .mipLevels   = 1,
		                              .arrayLayers = 1,
		                              .samples     = vk::SampleCountFlagBits::e1,
		                              .tiling      = tiling,
		                              .usage       = usage,
		                              .sharingMode = vk::SharingMode::eExclusive};

		vk::raii::Image image = vk::raii::Image(device, imageInfo);

		vk::MemoryRequirements memRequirements = image.getMemoryRequirements();
		vk::MemoryAllocateInfo allocInfo{.allocationSize  = memRequirements.size,
		                                 .memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties)};
		vk::raii::DeviceMemory imageMemory = vk::raii::DeviceMemory(device, allocInfo);
		image.bindMemory(imageMemory, 0);

		return {std::move(image), std::move(imageMemory)};
	}

	void transitionImageLayout(vk::raii::CommandBuffer &commandBuffer, const vk::raii::Image &image, vk::ImageLayout oldLayout, vk::ImageLayout newLayout)
	{
		vk::ImageMemoryBarrier barrier{.oldLayout           = oldLayout,
		                               .newLayout           = newLayout,
		                               .srcQueueFamilyIndex = vk::QueueFamilyIgnored,
		                               .dstQueueFamilyIndex = vk::QueueFamilyIgnored,
		                               .image               = image,
		                               .subresourceRange    = {.aspectMask = vk::ImageAspectFlagBits::eColor, .levelCount = 1, .layerCount = 1}};

		vk::PipelineStageFlags sourceStage;
		vk::PipelineStageFlags destinationStage;

		if (oldLayout == vk::ImageLayout::eUndefined && newLayout == vk::ImageLayout::eTransferDstOptimal)
		{
			barrier.srcAccessMask = {};
			barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;

			sourceStage      = vk::PipelineStageFlagBits::eTopOfPipe;
			destinationStage = vk::PipelineStageFlagBits::eTransfer;
		}
		else if (oldLayout == vk::ImageLayout::eTransferDstOptimal && newLayout == vk::ImageLayout::eShaderReadOnlyOptimal)
		{
			barrier.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
			barrier.dstAccessMask = vk::AccessFlagBits::eShaderRead;

			sourceStage      = vk::PipelineStageFlagBits::eTransfer;
			destinationStage = vk::PipelineStageFlagBits::eFragmentShader;
		}
		else
		{
			throw std::invalid_argument("unsupported layout transition!");
		}
		commandBuffer.pipelineBarrier(sourceStage, destinationStage, {}, {}, {}, barrier);
	}

	void copyBufferToImage(vk::raii::CommandBuffer &commandBuffer, const vk::raii::Buffer &buffer, vk::raii::Image &image, uint32_t width, uint32_t height)
	{
		vk::BufferImageCopy region{.bufferOffset      = 0,
		                           .bufferRowLength   = 0,
		                           .bufferImageHeight = 0,
		                           .imageSubresource  = {.aspectMask = vk::ImageAspectFlagBits::eColor, .mipLevel = 0, .baseArrayLayer = 0, .layerCount = 1},
		                           .imageOffset       = {0, 0, 0},
		                           .imageExtent       = {width, height, 1}};
		commandBuffer.copyBufferToImage(buffer, image, vk::ImageLayout::eTransferDstOptimal, region);
	}

	void createVertexBuffer()
	{
		vk::DeviceSize bufferSize = sizeof(sceneVertices[0]) * sceneVertices.size();

		auto [stagingBuffer, stagingBufferMemory] =
		    createBuffer(bufferSize, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

		void *dataStaging = stagingBufferMemory.mapMemory(0, bufferSize);
		memcpy(dataStaging, sceneVertices.data(), bufferSize);
		stagingBufferMemory.unmapMemory();

		std::tie(vertexBuffer, vertexBufferMemory) =
		    createBuffer(bufferSize, vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst, vk::MemoryPropertyFlagBits::eDeviceLocal);

		copyBuffer(stagingBuffer, vertexBuffer, bufferSize);
	}

	std::pair<vk::raii::Buffer, vk::raii::DeviceMemory> createBuffer(vk::DeviceSize size, vk::BufferUsageFlags usage, vk::MemoryPropertyFlags properties)
	{
		vk::BufferCreateInfo   bufferInfo{.size = size, .usage = usage, .sharingMode = vk::SharingMode::eExclusive};
		vk::raii::Buffer       buffer          = vk::raii::Buffer(device, bufferInfo);
		vk::MemoryRequirements memRequirements = buffer.getMemoryRequirements();
		vk::MemoryAllocateInfo allocInfo{.allocationSize = memRequirements.size, .memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties)};
		vk::raii::DeviceMemory bufferMemory = vk::raii::DeviceMemory(device, allocInfo);
		buffer.bindMemory(*bufferMemory, 0);
		return {std::move(buffer), std::move(bufferMemory)};
	}

	void createIndexBuffer()
	{
		vk::DeviceSize bufferSize = sizeof(sceneIndices[0]) * sceneIndices.size();

		auto [stagingBuffer, stagingBufferMemory] =
		    createBuffer(bufferSize, vk::BufferUsageFlagBits::eTransferSrc, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);

		void *data = stagingBufferMemory.mapMemory(0, bufferSize);
		memcpy(data, sceneIndices.data(), (size_t) bufferSize);
		stagingBufferMemory.unmapMemory();

		std::tie(indexBuffer, indexBufferMemory) =
		    createBuffer(bufferSize, vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst, vk::MemoryPropertyFlagBits::eDeviceLocal);

		copyBuffer(stagingBuffer, indexBuffer, bufferSize);
	}

	void createUniformBuffers()
	{
		groundUniformBuffers.clear();
		groundUniformBuffersMemory.clear();
		groundUniformBuffersMapped.clear();
		tankUniformBuffers.clear();
		tankUniformBuffersMemory.clear();
		tankUniformBuffersMapped.clear();
		bulletUniformBuffers.clear();
		bulletUniformBuffersMemory.clear();
		bulletUniformBuffersMapped.clear();
		crateUniformBuffers.clear();
		crateUniformBuffersMemory.clear();
		crateUniformBuffersMapped.clear();

		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
		{
			vk::DeviceSize bufferSize = sizeof(UniformBufferObject);
			auto [groundBuffer, groundBufferMem]  = createBuffer(
			    bufferSize, vk::BufferUsageFlagBits::eUniformBuffer, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
			groundUniformBuffers.emplace_back(std::move(groundBuffer));
			groundUniformBuffersMemory.emplace_back(std::move(groundBufferMem));
			groundUniformBuffersMapped.emplace_back(groundUniformBuffersMemory.back().mapMemory(0, bufferSize));

			auto [tankBuffer, tankBufferMem]  = createBuffer(
			    bufferSize, vk::BufferUsageFlagBits::eUniformBuffer, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
			tankUniformBuffers.emplace_back(std::move(tankBuffer));
			tankUniformBuffersMemory.emplace_back(std::move(tankBufferMem));
			tankUniformBuffersMapped.emplace_back(tankUniformBuffersMemory.back().mapMemory(0, bufferSize));

			for (size_t b = 0; b < static_cast<size_t>(MAX_BULLETS); ++b)
			{
				auto [bulletBuffer, bulletBufferMem] = createBuffer(
				    bufferSize, vk::BufferUsageFlagBits::eUniformBuffer, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
				bulletUniformBuffers.emplace_back(std::move(bulletBuffer));
				bulletUniformBuffersMemory.emplace_back(std::move(bulletBufferMem));
				bulletUniformBuffersMapped.emplace_back(bulletUniformBuffersMemory.back().mapMemory(0, bufferSize));
			}

			for (size_t c = 0; c < static_cast<size_t>(MAX_CRATES); ++c)
			{
				auto [crateBuffer, crateBufferMem] = createBuffer(
				    bufferSize, vk::BufferUsageFlagBits::eUniformBuffer, vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent);
				crateUniformBuffers.emplace_back(std::move(crateBuffer));
				crateUniformBuffersMemory.emplace_back(std::move(crateBufferMem));
				crateUniformBuffersMapped.emplace_back(crateUniformBuffersMemory.back().mapMemory(0, bufferSize));
			}
		}
	}

	void createDescriptorPool()
	{
		constexpr uint32_t descriptorSetCount = MAX_FRAMES_IN_FLIGHT * (2 + MAX_BULLETS + MAX_CRATES);
		std::array<vk::DescriptorPoolSize, 2> poolSize{{{.type = vk::DescriptorType::eUniformBuffer, .descriptorCount = descriptorSetCount},
		                                                {.type = vk::DescriptorType::eCombinedImageSampler, .descriptorCount = descriptorSetCount}}};
		vk::DescriptorPoolCreateInfo          poolInfo{.flags         = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
		                                               .maxSets       = descriptorSetCount,
		                                               .poolSizeCount = static_cast<uint32_t>(poolSize.size()),
		                                               .pPoolSizes    = poolSize.data()};
		descriptorPool = vk::raii::DescriptorPool(device, poolInfo);
	}

	void createDescriptorSets()
	{
		std::vector<vk::DescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT * (2 + MAX_BULLETS + MAX_CRATES), descriptorSetLayout);
		vk::DescriptorSetAllocateInfo        allocInfo{
		    .descriptorPool     = descriptorPool,
		    .descriptorSetCount = static_cast<uint32_t>(layouts.size()),
		    .pSetLayouts        = layouts.data()};

		groundDescriptorSets.clear();
		tankDescriptorSets.clear();
		bulletDescriptorSets.clear();
		crateDescriptorSets.clear();
		auto allDescriptorSets = device.allocateDescriptorSets(allocInfo);
		groundDescriptorSets.reserve(MAX_FRAMES_IN_FLIGHT);
		tankDescriptorSets.reserve(MAX_FRAMES_IN_FLIGHT);
		bulletDescriptorSets.reserve(MAX_FRAMES_IN_FLIGHT * MAX_BULLETS);
		crateDescriptorSets.reserve(MAX_FRAMES_IN_FLIGHT * MAX_CRATES);
		size_t nextSet = 0;
		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
		{
			groundDescriptorSets.push_back(std::move(allDescriptorSets[nextSet++]));
		}
		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
		{
			tankDescriptorSets.push_back(std::move(allDescriptorSets[nextSet++]));
		}
		for (size_t i = 0; i < static_cast<size_t>(MAX_FRAMES_IN_FLIGHT) * MAX_BULLETS; ++i)
		{
			bulletDescriptorSets.push_back(std::move(allDescriptorSets[nextSet++]));
		}
		for (size_t i = 0; i < static_cast<size_t>(MAX_FRAMES_IN_FLIGHT) * MAX_CRATES; ++i)
		{
			crateDescriptorSets.push_back(std::move(allDescriptorSets[nextSet++]));
		}

		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
		{
			vk::DescriptorBufferInfo groundBufferInfo{.buffer = groundUniformBuffers[i], .offset = 0, .range = sizeof(UniformBufferObject)};
			vk::DescriptorBufferInfo tankBufferInfo{.buffer = tankUniformBuffers[i], .offset = 0, .range = sizeof(UniformBufferObject)};
			vk::DescriptorImageInfo  groundImageInfo{.sampler = textureSampler, .imageView = groundTextureImageView, .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal};
			vk::DescriptorImageInfo  tankImageInfo{.sampler = textureSampler, .imageView = tankTextureImageView, .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal};

			std::array<vk::WriteDescriptorSet, 4> descriptorWrites{{
			    {.dstSet          = groundDescriptorSets[i],
			     .dstBinding      = 0,
			     .dstArrayElement = 0,
			     .descriptorCount = 1,
			     .descriptorType  = vk::DescriptorType::eUniformBuffer,
			     .pBufferInfo     = &groundBufferInfo},
			    {.dstSet          = groundDescriptorSets[i],
			     .dstBinding      = 1,
			     .dstArrayElement = 0,
			     .descriptorCount = 1,
			     .descriptorType  = vk::DescriptorType::eCombinedImageSampler,
			     .pImageInfo      = &groundImageInfo},
			    {.dstSet          = tankDescriptorSets[i],
			     .dstBinding      = 0,
			     .dstArrayElement = 0,
			     .descriptorCount = 1,
			     .descriptorType  = vk::DescriptorType::eUniformBuffer,
			     .pBufferInfo     = &tankBufferInfo},
			    {.dstSet          = tankDescriptorSets[i],
			     .dstBinding      = 1,
			     .dstArrayElement = 0,
			     .descriptorCount = 1,
			     .descriptorType  = vk::DescriptorType::eCombinedImageSampler,
			     .pImageInfo      = &tankImageInfo}}};
			device.updateDescriptorSets(descriptorWrites, {});
		}

		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
		{
			vk::DescriptorImageInfo bulletImageInfo{.sampler = textureSampler, .imageView = bulletTextureImageView, .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal};
			for (size_t b = 0; b < static_cast<size_t>(MAX_BULLETS); ++b)
			{
				const size_t            slot = i * MAX_BULLETS + b;
				vk::DescriptorBufferInfo bulletBufferInfo{.buffer = bulletUniformBuffers[slot], .offset = 0, .range = sizeof(UniformBufferObject)};
				std::array<vk::WriteDescriptorSet, 2> bulletWrites{{
				    {.dstSet          = bulletDescriptorSets[slot],
				     .dstBinding      = 0,
				     .dstArrayElement = 0,
				     .descriptorCount = 1,
				     .descriptorType  = vk::DescriptorType::eUniformBuffer,
				     .pBufferInfo     = &bulletBufferInfo},
				    {.dstSet          = bulletDescriptorSets[slot],
				     .dstBinding      = 1,
				     .dstArrayElement = 0,
				     .descriptorCount = 1,
				     .descriptorType  = vk::DescriptorType::eCombinedImageSampler,
				     .pImageInfo      = &bulletImageInfo}}};
				device.updateDescriptorSets(bulletWrites, {});
			}
		}

		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
		{
			vk::DescriptorImageInfo crateImageInfo{.sampler = textureSampler, .imageView = crateTextureImageView, .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal};
			for (size_t c = 0; c < static_cast<size_t>(MAX_CRATES); ++c)
			{
				const size_t             slot = i * MAX_CRATES + c;
				vk::DescriptorBufferInfo crateBufferInfo{.buffer = crateUniformBuffers[slot], .offset = 0, .range = sizeof(UniformBufferObject)};
				std::array<vk::WriteDescriptorSet, 2> crateWrites{{
				    {.dstSet          = crateDescriptorSets[slot],
				     .dstBinding      = 0,
				     .dstArrayElement = 0,
				     .descriptorCount = 1,
				     .descriptorType  = vk::DescriptorType::eUniformBuffer,
				     .pBufferInfo     = &crateBufferInfo},
				    {.dstSet          = crateDescriptorSets[slot],
				     .dstBinding      = 1,
				     .dstArrayElement = 0,
				     .descriptorCount = 1,
				     .descriptorType  = vk::DescriptorType::eCombinedImageSampler,
				     .pImageInfo      = &crateImageInfo}}};
				device.updateDescriptorSets(crateWrites, {});
			}
		}
	}

	vk::raii::CommandBuffer beginSingleTimeCommands()
	{
		vk::CommandBufferAllocateInfo allocInfo{.commandPool = commandPool, .level = vk::CommandBufferLevel::ePrimary, .commandBufferCount = 1};
		vk::raii::CommandBuffer       commandBuffer = std::move(vk::raii::CommandBuffers(device, allocInfo).front());

		vk::CommandBufferBeginInfo beginInfo{.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit};
		commandBuffer.begin(beginInfo);

		return std::move(commandBuffer);
	}

	void endSingleTimeCommands(vk::raii::CommandBuffer &&commandBuffer)
	{
		commandBuffer.end();

		vk::SubmitInfo submitInfo{.commandBufferCount = 1, .pCommandBuffers = &*commandBuffer};
		queue.submit(submitInfo, nullptr);
		queue.waitIdle();
	}

	void copyBuffer(vk::raii::Buffer &srcBuffer, vk::raii::Buffer &dstBuffer, vk::DeviceSize size)
	{
		vk::raii::CommandBuffer commandCopyBuffer = beginSingleTimeCommands();
		commandCopyBuffer.copyBuffer(*srcBuffer, *dstBuffer, vk::BufferCopy{.size = size});
		endSingleTimeCommands(std::move(commandCopyBuffer));
	}

	uint32_t findMemoryType(uint32_t typeFilter, vk::MemoryPropertyFlags properties)
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

	void createCommandBuffers()
	{
		commandBuffers.clear();
		vk::CommandBufferAllocateInfo allocInfo{.commandPool = commandPool, .level = vk::CommandBufferLevel::ePrimary, .commandBufferCount = MAX_FRAMES_IN_FLIGHT};
		commandBuffers = vk::raii::CommandBuffers(device, allocInfo);
	}

	void recordCommandBuffer(uint32_t imageIndex)
	{
		auto &commandBuffer = commandBuffers[frameIndex];
		commandBuffer.begin({});

		// Before starting rendering, transition the swapchain image to vk::ImageLayout::eColorAttachmentOptimal
		transition_image_layout(
		    swapchainBundle->getImages()[imageIndex],
		    vk::ImageLayout::eUndefined,
		    vk::ImageLayout::eColorAttachmentOptimal,
		    {},                                                        // srcAccessMask (no need to wait for previous operations)
		    vk::AccessFlagBits2::eColorAttachmentWrite,                // dstAccessMask
		    vk::PipelineStageFlagBits2::eColorAttachmentOutput,        // srcStage
		    vk::PipelineStageFlagBits2::eColorAttachmentOutput,        // dstStage
		    vk::ImageAspectFlagBits::eColor);

		// Transition depth image to depth attachment optimal layout
		transition_image_layout(
		    *swapchainBundle->getDepthImage(),
		    vk::ImageLayout::eUndefined,
		    vk::ImageLayout::eDepthAttachmentOptimal,
		    vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
		    vk::AccessFlagBits2::eDepthStencilAttachmentWrite,
		    vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests,
		    vk::PipelineStageFlagBits2::eEarlyFragmentTests | vk::PipelineStageFlagBits2::eLateFragmentTests,
		    vk::ImageAspectFlagBits::eDepth);

		vk::ClearValue clearColor = vk::ClearColorValue(0.0f, 0.0f, 0.0f, 1.0f);
		vk::ClearValue clearDepth = vk::ClearDepthStencilValue(1.0f, 0);

		vk::RenderingAttachmentInfo colorAttachmentInfo = {
		    .imageView   = swapchainBundle->getImageViews()[imageIndex],
		    .imageLayout = vk::ImageLayout::eColorAttachmentOptimal,
		    .loadOp      = vk::AttachmentLoadOp::eClear,
		    .storeOp     = vk::AttachmentStoreOp::eStore,
		    .clearValue  = clearColor};

		vk::RenderingAttachmentInfo depthAttachmentInfo = {
		    .imageView   = swapchainBundle->getDepthImageView(),
		    .imageLayout = vk::ImageLayout::eDepthAttachmentOptimal,
		    .loadOp      = vk::AttachmentLoadOp::eClear,
		    .storeOp     = vk::AttachmentStoreOp::eDontCare,
		    .clearValue  = clearDepth};

		const vk::Extent2D &swapchainExtent = swapchainBundle->getExtent();

		vk::RenderingInfo renderingInfo = {
		    .renderArea           = {.offset = {0, 0}, .extent = swapchainExtent},
		    .layerCount           = 1,
		    .colorAttachmentCount = 1,
		    .pColorAttachments    = &colorAttachmentInfo,
		    .pDepthAttachment     = &depthAttachmentInfo};

		commandBuffer.beginRendering(renderingInfo);
		commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, *graphicsPipeline);
		commandBuffer.setViewport(0, vk::Viewport(0.0f, 0.0f, static_cast<float>(swapchainExtent.width), static_cast<float>(swapchainExtent.height), 0.0f, 1.0f));
		commandBuffer.setScissor(0, vk::Rect2D(vk::Offset2D(0, 0), swapchainExtent));
		commandBuffer.bindVertexBuffers(0, *vertexBuffer, {0});
		commandBuffer.bindIndexBuffer(*indexBuffer, 0, vk::IndexType::eUint32);

		commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipelineLayout, 0, *groundDescriptorSets[frameIndex], nullptr);
		commandBuffer.drawIndexed(groundIndexCount, 1, 0, 0, 0);

		commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipelineLayout, 0, *tankDescriptorSets[frameIndex], nullptr);
		commandBuffer.drawIndexed(tankIndexCount, 1, tankFirstIndex, 0, 0);

		const size_t activeBullets = std::min(bulletSystem.getBullets().size(), static_cast<size_t>(MAX_BULLETS));
		for (size_t b = 0; b < activeBullets; ++b)
		{
			commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipelineLayout, 0, *bulletDescriptorSets[frameIndex * MAX_BULLETS + b], nullptr);
			commandBuffer.drawIndexed(bulletIndexCount, 1, bulletFirstIndex, 0, 0);
		}

		const size_t activeCrates = std::min(crateSystem.getCrates().size(), static_cast<size_t>(MAX_CRATES));
		for (size_t c = 0; c < activeCrates; ++c)
		{
			commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, pipelineLayout, 0, *crateDescriptorSets[frameIndex * MAX_CRATES + c], nullptr);
			commandBuffer.drawIndexed(crateIndexCount, 1, crateFirstIndex, 0, 0);
		}

		// Draw the ImGui HUD / menus on top of the scene.
		if (ImDrawData *drawData = ImGui::GetDrawData())
		{
			ImGui_ImplVulkan_RenderDrawData(drawData, static_cast<VkCommandBuffer>(*commandBuffer));
		}
		commandBuffer.endRendering();

		// After rendering, transition the swapchain image to vk::ImageLayout::ePresentSrcKHR
		transition_image_layout(
		    swapchainBundle->getImages()[imageIndex],
		    vk::ImageLayout::eColorAttachmentOptimal,
		    vk::ImageLayout::ePresentSrcKHR,
		    vk::AccessFlagBits2::eColorAttachmentWrite,                // srcAccessMask
		    {},                                                        // dstAccessMask
		    vk::PipelineStageFlagBits2::eColorAttachmentOutput,        // srcStage
		    vk::PipelineStageFlagBits2::eBottomOfPipe,                 // dstStage
		    vk::ImageAspectFlagBits::eColor);
		commandBuffer.end();
	}

	void transition_image_layout(
	    vk::Image               image,
	    vk::ImageLayout         old_layout,
	    vk::ImageLayout         new_layout,
	    vk::AccessFlags2        src_access_mask,
	    vk::AccessFlags2        dst_access_mask,
	    vk::PipelineStageFlags2 src_stage_mask,
	    vk::PipelineStageFlags2 dst_stage_mask,
	    vk::ImageAspectFlags    image_aspect_flags)
	{
		vk::ImageMemoryBarrier2 barrier = {
		    .srcStageMask        = src_stage_mask,
		    .srcAccessMask       = src_access_mask,
		    .dstStageMask        = dst_stage_mask,
		    .dstAccessMask       = dst_access_mask,
		    .oldLayout           = old_layout,
		    .newLayout           = new_layout,
		    .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		    .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
		    .image               = image,
		    .subresourceRange    = {
		        .aspectMask     = image_aspect_flags,
		        .baseMipLevel   = 0,
		        .levelCount     = 1,
		        .baseArrayLayer = 0,
		        .layerCount     = 1}};
		vk::DependencyInfo dependency_info = {
		    .dependencyFlags         = {},
		    .imageMemoryBarrierCount = 1,
		    .pImageMemoryBarriers    = &barrier};
		commandBuffers[frameIndex].pipelineBarrier2(dependency_info);
	}

	void createSyncObjects()
	{
		assert(presentCompleteSemaphores.empty() && renderFinishedSemaphores.empty() && inFlightFences.empty());

		for (size_t i = 0; i < swapchainBundle->getImages().size(); i++)
		{
			renderFinishedSemaphores.emplace_back(device, vk::SemaphoreCreateInfo());
		}

		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
		{
			presentCompleteSemaphores.emplace_back(device, vk::SemaphoreCreateInfo());
			inFlightFences.emplace_back(device, vk::FenceCreateInfo{.flags = vk::FenceCreateFlagBits::eSignaled});
		}
	}

	void updateUniformBuffer(uint32_t currentImage)
	{
		UniformBufferObject groundUbo{};
		UniformBufferObject tankUbo{};

		groundUbo.model = glm::mat4(1.0f);
		tankUbo.model   = tankController.getModelMatrix();

		groundUbo.view = mainCamera.getViewMatrix();
		tankUbo.view   = groundUbo.view;
		const vk::Extent2D &swapchainExtent = swapchainBundle->getExtent();
		groundUbo.proj = mainCamera.getProjectionMatrix(static_cast<float>(swapchainExtent.width) / static_cast<float>(swapchainExtent.height));
		tankUbo.proj   = groundUbo.proj;
		groundUbo.proj[1][1] *= -1;
		tankUbo.proj[1][1] *= -1;

		memcpy(groundUniformBuffersMapped[currentImage], &groundUbo, sizeof(groundUbo));
		memcpy(tankUniformBuffersMapped[currentImage], &tankUbo, sizeof(tankUbo));

		const std::vector<Bullet> &bullets = bulletSystem.getBullets();
		for (size_t b = 0; b < bullets.size() && b < static_cast<size_t>(MAX_BULLETS); ++b)
		{
			UniformBufferObject bulletUbo{};
			bulletUbo.model = bullets[b].getModelMatrix();
			bulletUbo.view  = groundUbo.view;
			bulletUbo.proj  = groundUbo.proj;
			memcpy(bulletUniformBuffersMapped[currentImage * MAX_BULLETS + b], &bulletUbo, sizeof(bulletUbo));
		}

		const std::vector<Crate> &crates = crateSystem.getCrates();
		for (size_t c = 0; c < crates.size() && c < static_cast<size_t>(MAX_CRATES); ++c)
		{
			UniformBufferObject crateUbo{};
			crateUbo.model = crates[c].getModelMatrix();
			crateUbo.view  = groundUbo.view;
			crateUbo.proj  = groundUbo.proj;
			memcpy(crateUniformBuffersMapped[currentImage * MAX_CRATES + c], &crateUbo, sizeof(crateUbo));
		}
	}

	void drawFrame()
	{
		// Note: inFlightFences, presentCompleteSemaphores, and commandBuffers are indexed by frameIndex,
		//       while renderFinishedSemaphores is indexed by imageIndex
		auto fenceResult = device.waitForFences(*inFlightFences[frameIndex], vk::True, UINT64_MAX);
		if (fenceResult != vk::Result::eSuccess)
		{
			throw std::runtime_error("failed to wait for fence!");
		}

		auto [result, imageIndex] = swapchainBundle->getSwapChain().acquireNextImage(UINT64_MAX, *presentCompleteSemaphores[frameIndex], nullptr);

		// Due to VULKAN_HPP_HANDLE_ERROR_OUT_OF_DATE_AS_SUCCESS being defined, eErrorOutOfDateKHR can be checked as a result
		// here and does not need to be caught by an exception.
		if (result == vk::Result::eErrorOutOfDateKHR)
		{
			recreateSwapChain();
			return;
		}
		// On other success codes than eSuccess and eSuboptimalKHR we just throw an exception.
		// On any error code, aquireNextImage already threw an exception.
		if (result != vk::Result::eSuccess && result != vk::Result::eSuboptimalKHR)
		{
			assert(result == vk::Result::eTimeout || result == vk::Result::eNotReady);
			throw std::runtime_error("failed to acquire swap chain image!");
		}
		updateUniformBuffer(frameIndex);

		// Only reset the fence if we are submitting work
		device.resetFences(*inFlightFences[frameIndex]);

		commandBuffers[frameIndex].reset();
		recordCommandBuffer(imageIndex);

		vk::PipelineStageFlags waitDestinationStageMask(vk::PipelineStageFlagBits::eColorAttachmentOutput);
		const vk::SubmitInfo   submitInfo{.waitSemaphoreCount   = 1,
		                                  .pWaitSemaphores      = &*presentCompleteSemaphores[frameIndex],
		                                  .pWaitDstStageMask    = &waitDestinationStageMask,
		                                  .commandBufferCount   = 1,
		                                  .pCommandBuffers      = &*commandBuffers[frameIndex],
		                                  .signalSemaphoreCount = 1,
		                                  .pSignalSemaphores    = &*renderFinishedSemaphores[imageIndex]};
		queue.submit(submitInfo, *inFlightFences[frameIndex]);

		const vk::PresentInfoKHR presentInfoKHR{.waitSemaphoreCount = 1,
		                                        .pWaitSemaphores    = &*renderFinishedSemaphores[imageIndex],
		                                        .swapchainCount     = 1,
		                                        .pSwapchains        = &*swapchainBundle->getSwapChain(),
		                                        .pImageIndices      = &imageIndex};
		result = queue.presentKHR(presentInfoKHR);
		// Due to VULKAN_HPP_HANDLE_ERROR_OUT_OF_DATE_AS_SUCCESS being defined, eErrorOutOfDateKHR can be checked as a result
		// here and does not need to be caught by an exception.
		if ((result == vk::Result::eSuboptimalKHR) || (result == vk::Result::eErrorOutOfDateKHR) || framebufferResized)
		{
			framebufferResized = false;
			recreateSwapChain();
		}
		else
		{
			// There are no other success codes than eSuccess; on any error code, presentKHR already threw an exception.
			assert(result == vk::Result::eSuccess);
		}
		frameIndex = (frameIndex + 1) % MAX_FRAMES_IN_FLIGHT;
	}

	[[nodiscard]] vk::raii::ShaderModule createShaderModule(const std::vector<char> &code) const
	{
		vk::ShaderModuleCreateInfo createInfo{.codeSize = code.size(), .pCode = reinterpret_cast<const uint32_t *>(code.data())};
		vk::raii::ShaderModule     shaderModule{device, createInfo};

		return shaderModule;
	}

	std::vector<const char *> getRequiredInstanceExtensions()
	{
		uint32_t glfwExtensionCount = 0;
		auto     glfwExtensions     = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

		std::vector extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);
		if (enableValidationLayers)
		{
			extensions.push_back(vk::EXTDebugUtilsExtensionName);
		}

		return extensions;
	}

	static VKAPI_ATTR vk::Bool32 VKAPI_CALL debugCallback(vk::DebugUtilsMessageSeverityFlagBitsEXT severity, vk::DebugUtilsMessageTypeFlagsEXT type, const vk::DebugUtilsMessengerCallbackDataEXT *pCallbackData, void *)
	{
		if (severity == vk::DebugUtilsMessageSeverityFlagBitsEXT::eError || severity == vk::DebugUtilsMessageSeverityFlagBitsEXT::eWarning)
		{
			std::cerr << "validation layer: type " << to_string(type) << " msg: " << pCallbackData->pMessage << std::endl;
		}

		return vk::False;
	}

	static std::vector<char> readFile(const std::string &filename)
	{
		std::ifstream file(filename, std::ios::ate | std::ios::binary);
		if (!file.is_open())
		{
			throw std::runtime_error("failed to open file: " + filename);
		}
		std::vector<char> buffer(file.tellg());
		file.seekg(0, std::ios::beg);
		file.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
		file.close();
		return buffer;
	}

	static std::string resolveResourcePath(const std::string &relativePath)
	{
		const std::filesystem::path directPath = std::filesystem::path(relativePath);
		if (std::filesystem::exists(directPath))
		{
			return directPath.string();
		}

		const std::filesystem::path parentPath = std::filesystem::path("..") / relativePath;
		if (std::filesystem::exists(parentPath))
		{
			return parentPath.string();
		}

		throw std::runtime_error("failed to open file: " + relativePath);
	}
};

int main()
{
	try
	{
		HelloTriangleApplication app;
		app.run();
	}
	catch (const std::exception &e)
	{
		std::cerr << e.what() << std::endl;
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
