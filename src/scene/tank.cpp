#include "tank.h"

#include <algorithm>
#include <functional>
#include <limits>
#include <stdexcept>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include <tiny_gltf.h>

namespace
{
glm::mat4 toMat4(const std::vector<double> &matrix)
{
    glm::mat4 transform(1.0f);
    for (int col = 0; col < 4; ++col)
    {
        for (int row = 0; row < 4; ++row)
        {
            transform[col][row] = static_cast<float>(matrix[col * 4 + row]);
        }
    }
    return transform;
}

glm::mat4 nodeLocalTransform(const tinygltf::Node &node)
{
    if (node.matrix.size() == 16)
    {
        return toMat4(node.matrix);
    }

    glm::mat4 transform(1.0f);

    if (node.translation.size() == 3)
    {
        transform = glm::translate(transform, glm::vec3(
            static_cast<float>(node.translation[0]),
            static_cast<float>(node.translation[1]),
            static_cast<float>(node.translation[2])));
    }

    if (node.rotation.size() == 4)
    {
        const glm::quat rotation(
            static_cast<float>(node.rotation[3]),
            static_cast<float>(node.rotation[0]),
            static_cast<float>(node.rotation[1]),
            static_cast<float>(node.rotation[2]));
        transform *= glm::mat4_cast(rotation);
    }

    if (node.scale.size() == 3)
    {
        transform = glm::scale(transform, glm::vec3(
            static_cast<float>(node.scale[0]),
            static_cast<float>(node.scale[1]),
            static_cast<float>(node.scale[2])));
    }

    return transform;
}

uint32_t readIndex(const tinygltf::Accessor &indexAccessor, const unsigned char *indexData, size_t i)
{
    switch (indexAccessor.componentType)
    {
    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
        return static_cast<uint32_t>(reinterpret_cast<const uint8_t *>(indexData)[i]);
    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
        return static_cast<uint32_t>(reinterpret_cast<const uint16_t *>(indexData)[i]);
    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
        return reinterpret_cast<const uint32_t *>(indexData)[i];
    default:
        throw std::runtime_error("Unsupported glTF index component type.");
    }
}
}

Tank::Tank(const std::string &gltfPath, const TankSpawnConfig &spawnConfig)
{
    loadFromGltf(gltfPath, spawnConfig);
}

const TankMeshData &Tank::getMeshData() const
{
    return meshData;
}

void Tank::loadFromGltf(const std::string &gltfPath, const TankSpawnConfig &spawnConfig)
{
    const glm::mat4 gltfAxisToWorld = glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f));
    const glm::mat4 uprightCorrection = glm::rotate(glm::mat4(1.0f), glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f));
    const glm::mat4 gltfToWorld = uprightCorrection * gltfAxisToWorld;

    tinygltf::TinyGLTF loader;
    tinygltf::Model model;
    std::string error;
    std::string warning;

    const bool loaded = loader.LoadASCIIFromFile(&model, &error, &warning, gltfPath);
    if (!warning.empty())
    {
        // Intentionally ignored for now; warnings are usually non-fatal metadata issues.
    }
    if (!loaded)
    {
        throw std::runtime_error("Failed to load tank model: " + gltfPath + " " + error);
    }

    meshData = {};
    meshData.color = spawnConfig.color;

    const int sceneIndex = model.defaultScene >= 0 ? model.defaultScene : 0;
    if (sceneIndex < 0 || sceneIndex >= static_cast<int>(model.scenes.size()))
    {
        throw std::runtime_error("Tank glTF scene is missing.");
    }

    std::function<void(int, const glm::mat4 &)> processNode;
    processNode = [&](int nodeIndex, const glm::mat4 &parentTransform)
    {
        const tinygltf::Node &node = model.nodes[nodeIndex];
        const glm::mat4 worldTransform = parentTransform * nodeLocalTransform(node);

        if (node.mesh >= 0)
        {
            const tinygltf::Mesh &mesh = model.meshes[node.mesh];
            for (const tinygltf::Primitive &primitive : mesh.primitives)
            {
                if (primitive.mode != TINYGLTF_MODE_TRIANGLES)
                {
                    continue;
                }

                auto positionAttrIt = primitive.attributes.find("POSITION");
                if (positionAttrIt == primitive.attributes.end())
                {
                    continue;
                }

                const tinygltf::Accessor &positionAccessor = model.accessors[positionAttrIt->second];
                const tinygltf::BufferView &positionView = model.bufferViews[positionAccessor.bufferView];
                const tinygltf::Buffer &positionBuffer = model.buffers[positionView.buffer];

                const unsigned char *uvData = nullptr;
                size_t effectiveUvStride = 0;
                auto texCoordAttrIt = primitive.attributes.find("TEXCOORD_0");
                if (texCoordAttrIt != primitive.attributes.end())
                {
                    const tinygltf::Accessor &uvAccessor = model.accessors[texCoordAttrIt->second];
                    const tinygltf::BufferView &uvView = model.bufferViews[uvAccessor.bufferView];
                    const tinygltf::Buffer &uvBuffer = model.buffers[uvView.buffer];

                    if (uvAccessor.componentType != TINYGLTF_COMPONENT_TYPE_FLOAT || uvAccessor.type != TINYGLTF_TYPE_VEC2)
                    {
                        throw std::runtime_error("Tank glTF TEXCOORD_0 accessor must be float2.");
                    }

                    const size_t uvStride = uvAccessor.ByteStride(uvView);
                    effectiveUvStride = uvStride == 0 ? sizeof(float) * 2 : uvStride;
                    uvData = uvBuffer.data.data() + uvView.byteOffset + uvAccessor.byteOffset;
                }

                if (positionAccessor.componentType != TINYGLTF_COMPONENT_TYPE_FLOAT || positionAccessor.type != TINYGLTF_TYPE_VEC3)
                {
                    throw std::runtime_error("Tank glTF POSITION accessor must be float3.");
                }

                const size_t positionStride = positionAccessor.ByteStride(positionView);
                const size_t effectivePositionStride = positionStride == 0 ? sizeof(float) * 3 : positionStride;
                const unsigned char *positionData = positionBuffer.data.data() + positionView.byteOffset + positionAccessor.byteOffset;

                const uint32_t baseVertex = static_cast<uint32_t>(meshData.positions.size());
                meshData.positions.reserve(meshData.positions.size() + positionAccessor.count);
                meshData.texCoords.reserve(meshData.texCoords.size() + positionAccessor.count);

                for (size_t i = 0; i < positionAccessor.count; ++i)
                {
                    const float *raw = reinterpret_cast<const float *>(positionData + i * effectivePositionStride);
                    const glm::vec4 transformed = gltfToWorld * worldTransform * glm::vec4(raw[0], raw[1], raw[2], 1.0f);
                    meshData.positions.push_back(glm::vec3(transformed));

                    glm::vec2 uv(0.0f, 0.0f);
                    if (uvData)
                    {
                        const float *rawUv = reinterpret_cast<const float *>(uvData + i * effectiveUvStride);
                        uv = {rawUv[0], rawUv[1]};
                    }
                    meshData.texCoords.push_back(uv);
                }

                if (primitive.indices >= 0)
                {
                    const tinygltf::Accessor &indexAccessor = model.accessors[primitive.indices];
                    const tinygltf::BufferView &indexView = model.bufferViews[indexAccessor.bufferView];
                    const tinygltf::Buffer &indexBuffer = model.buffers[indexView.buffer];
                    const unsigned char *indexData = indexBuffer.data.data() + indexView.byteOffset + indexAccessor.byteOffset;

                    meshData.indices.reserve(meshData.indices.size() + indexAccessor.count);
                    for (size_t i = 0; i < indexAccessor.count; ++i)
                    {
                        meshData.indices.push_back(baseVertex + readIndex(indexAccessor, indexData, i));
                    }
                }
                else
                {
                    for (uint32_t i = 0; i < static_cast<uint32_t>(positionAccessor.count); ++i)
                    {
                        meshData.indices.push_back(baseVertex + i);
                    }
                }
            }
        }

        for (int childIndex : node.children)
        {
            processNode(childIndex, worldTransform);
        }
    };

    for (int nodeIndex : model.scenes[sceneIndex].nodes)
    {
        processNode(nodeIndex, glm::mat4(1.0f));
    }

    if (meshData.positions.empty() || meshData.indices.empty())
    {
        throw std::runtime_error("Tank mesh has no drawable triangles.");
    }

    glm::vec3 minPoint(std::numeric_limits<float>::max());
    glm::vec3 maxPoint(std::numeric_limits<float>::lowest());
    for (const glm::vec3 &position : meshData.positions)
    {
        minPoint = glm::min(minPoint, position);
        maxPoint = glm::max(maxPoint, position);
    }

    const glm::vec3 size = maxPoint - minPoint;
    const float longestSide = std::max({size.x, size.y, size.z});
    const float scale = (longestSide > 0.0f) ? (spawnConfig.desiredLength / longestSide) : 1.0f;
    const glm::vec3 center = (minPoint + maxPoint) * 0.5f;

    for (glm::vec3 &position : meshData.positions)
    {
        position = (position - center) * scale;
    }

    float minZ = std::numeric_limits<float>::max();
    for (const glm::vec3 &position : meshData.positions)
    {
        minZ = std::min(minZ, position.z);
    }

    const float zShift = spawnConfig.position.z + spawnConfig.groundOffset - minZ;

    for (glm::vec3 &position : meshData.positions)
    {
        position.x += spawnConfig.position.x;
        position.y += spawnConfig.position.y;
        position.z += zShift;
    }
}
