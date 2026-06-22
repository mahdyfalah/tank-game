cmake_minimum_required (VERSION 3.29)

project (TankGame LANGUAGES CXX)

# ---------------------------------------------------------------------------
# Options
# ---------------------------------------------------------------------------
# Enable the experimental Vulkan C++20 module (`import vulkan_hpp;`). Off by
# default because it requires a very recent toolchain. The code falls back to
# the classic <vulkan/vulkan_raii.hpp> header when this is OFF.
option(ENABLE_CPP20_MODULE "Enable C++ 20 module support for Vulkan" OFF)

if(ENABLE_CPP20_MODULE)
  set(CMAKE_CXX_SCAN_FOR_MODULES ON)
endif()

# Look for our bundled Find*.cmake helpers first so the project is fully
# self-contained and can be moved out of the tutorial repo as-is.
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# ---------------------------------------------------------------------------
# Dependencies
#
# Everything below (except the Vulkan SDK itself) is provided by vcpkg. When
# you configure with the vcpkg toolchain file, the packages listed in
# vcpkg.json are installed automatically (manifest mode).
# ---------------------------------------------------------------------------
find_package (glfw3 REQUIRED)            # windowing + input
find_package (glm REQUIRED)              # math (vectors / matrices)
find_package (Vulkan 1.4.335 REQUIRED)   # Vulkan SDK 1.4.335 or newer

# Extra libraries that the simple base does not use yet, but that are wired up
# and ready for when the game grows (model loading, glTF, compressed textures).
find_package (tinyobjloader REQUIRED)    # .obj model loading
find_package (tinygltf REQUIRED)         # .gltf / .glb model loading
find_package (KTX REQUIRED)              # .ktx2 compressed textures
find_package (imgui REQUIRED)            # immediate-mode GUI (HUD / menus)

# ---------------------------------------------------------------------------
# Vulkan C++ module (only built when ENABLE_CPP20_MODULE is ON)
# ---------------------------------------------------------------------------
if(ENABLE_CPP20_MODULE)
  add_library(VulkanCppModule)
  add_library(Vulkan::cppm ALIAS VulkanCppModule)

  target_compile_definitions(VulkanCppModule
    PUBLIC VULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1 VULKAN_HPP_NO_STRUCT_CONSTRUCTORS=1)
  target_include_directories(VulkanCppModule PUBLIC "${Vulkan_INCLUDE_DIR}")
  target_link_libraries(VulkanCppModule PUBLIC Vulkan::Vulkan)
  set_target_properties(VulkanCppModule PROPERTIES CXX_STANDARD 20)

  if(MSVC)
    target_compile_options(VulkanCppModule PRIVATE
      /std:c++latest
      /permissive-
      /Zc:__cplusplus
      /EHsc
      /Zc:preprocessor
      /translateInclude
    )
  endif()

  target_sources(VulkanCppModule
    PUBLIC
    FILE_SET cxx_modules TYPE CXX_MODULES
    BASE_DIRS "${Vulkan_INCLUDE_DIR}"
    FILES "${Vulkan_INCLUDE_DIR}/vulkan/vulkan.cppm"
  )
  target_sources(VulkanCppModule
    PRIVATE "${Vulkan_INCLUDE_DIR}/vulkan/vulkan.cppm")
else()
  # Dummy interface library so the rest of the file is identical either way.
  add_library(VulkanCppModule INTERFACE)
  add_library(Vulkan::cppm ALIAS VulkanCppModule)
  target_link_libraries(VulkanCppModule INTERFACE Vulkan::Vulkan)
  target_compile_definitions(VulkanCppModule
    INTERFACE VULKAN_HPP_DISPATCH_LOADER_DYNAMIC=1 VULKAN_HPP_NO_STRUCT_CONSTRUCTORS=1)
endif()

# stb (header only) - used for loading the .jpg texture.
find_package(stb REQUIRED)
set(STB_INCLUDEDIR ${stb_INCLUDE_DIRS})

# slangc compiles the .slang shader to SPIR-V. It ships with the Vulkan SDK.
find_program(SLANGC_EXECUTABLE slangc HINTS $ENV{VULKAN_SDK}/bin REQUIRED)

# ---------------------------------------------------------------------------
# Application target
# ---------------------------------------------------------------------------
set(APP_OUTPUT_DIR ${CMAKE_BINARY_DIR}/${PROJECT_NAME})

add_executable (${PROJECT_NAME}
  src/main.cpp
  src/ground_tile_map.cpp
  src/camera.cpp
  src/game.cpp
  src/scene/tank.cpp
  src/scene/tank_controller.cpp
  src/scene/bullet.cpp
  src/scene/crate.cpp
  src/render/swapchain_bundle.cpp)
set_target_properties (${PROJECT_NAME} PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY ${APP_OUTPUT_DIR}
  CXX_STANDARD 20
  CXX_STANDARD_REQUIRED ON)

target_link_libraries (${PROJECT_NAME} PRIVATE Vulkan::cppm glfw glm::glm tinygltf::tinygltf imgui::imgui)
target_include_directories (${PROJECT_NAME} PRIVATE ${STB_INCLUDEDIR})

if(ENABLE_CPP20_MODULE)
  target_compile_definitions(${PROJECT_NAME} PRIVATE USE_CPP20_MODULES=1)
endif()

# Treat VK_ERROR_OUT_OF_DATE_KHR as a success code so swap-chain recreation is
# driven by our own resize handling instead of throwing.
target_compile_definitions(${PROJECT_NAME} PRIVATE VULKAN_HPP_HANDLE_ERROR_OUT_OF_DATE_AS_SUCCESS)

if(WIN32 AND ${CMAKE_GENERATOR} MATCHES "Visual Studio.*")
  set_target_properties(${PROJECT_NAME} PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY ${APP_OUTPUT_DIR})
endif()

# ---------------------------------------------------------------------------
# Shaders: compile shaders/shader.slang -> <output>/shaders/slang.spv
# ---------------------------------------------------------------------------
set(SHADER_TARGET   ${PROJECT_NAME}_shaders)
set(SHADERS_DIR     ${APP_OUTPUT_DIR}/shaders)
set(SHADER_SOURCES  "${CMAKE_CURRENT_SOURCE_DIR}/shaders/shader.slang")
set(SHADERS_DIR_STAMP ${SHADERS_DIR}/.dir.stamp)
set(ENTRY_POINTS    -entry vertMain -entry fragMain)

add_custom_command(
  OUTPUT ${SHADERS_DIR_STAMP}
  COMMAND ${CMAKE_COMMAND} -E make_directory ${SHADERS_DIR}
  COMMAND ${CMAKE_COMMAND} -E touch ${SHADERS_DIR_STAMP}
  COMMENT "Preparing shader output directory"
  VERBATIM)
add_custom_command(
  OUTPUT  ${SHADERS_DIR}/slang.spv
  COMMAND ${SLANGC_EXECUTABLE} ${SHADER_SOURCES} -target spirv -profile spirv_1_4
          -emit-spirv-directly -fvk-use-entrypoint-name ${ENTRY_POINTS}
          -o ${SHADERS_DIR}/slang.spv
  WORKING_DIRECTORY ${SHADERS_DIR}
  DEPENDS ${SHADERS_DIR_STAMP} ${SHADER_SOURCES}
  COMMENT "Compiling Slang shaders"
  VERBATIM)
add_custom_target(${SHADER_TARGET} DEPENDS ${SHADERS_DIR}/slang.spv)
add_dependencies(${PROJECT_NAME} ${SHADER_TARGET})

# ---------------------------------------------------------------------------
# Assets: copy the texture next to the executable's working directory.
# ---------------------------------------------------------------------------
file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/assets/textures/grass.jpg"
     DESTINATION ${APP_OUTPUT_DIR}/textures)

file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/assets/models/cartoon_tank"
  DESTINATION ${APP_OUTPUT_DIR}/models)

file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/assets/models/9mm_bullet"
  DESTINATION ${APP_OUTPUT_DIR}/models)

file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/assets/models/wooden_crate"
  DESTINATION ${APP_OUTPUT_DIR}/models)
