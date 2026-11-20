/* !
@file               ModelData.hpp
@author             w.kyele@digipen.edu [100%]
@date               27/10/2026
@brief
    Declares the engine's internal model: the meshes a file produced, the
    material slots those meshes reference, and the node hierarchy that places
    them. Every model loader (glTF, OBJ, FBX) writes into these structures, so
    the renderer only ever sees this one shape.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

/*                                                                      guard
----------------------------------------------------------------------------- */
#pragma once

// Engine Assets Headers
#include "engine/assets/data/MaterialData.hpp"
#include "engine/assets/data/MeshData.hpp"

// Third-Party Headers
#include <glm/glm.hpp>

// Standard Library Headers
#include <cstdint>
#include <string>
#include <vector>

namespace Engine::Assets
{
    /*!***********************************************************************
    @brief          One node of a model's transform hierarchy. A node either
                    carries meshes, children, or both; both lists hold indices
                    into their ModelData array.
    *************************************************************************/
    struct ModelNode
    {
        std::string name;
        glm::mat4 localTransform = glm::mat4(1.0f);
        std::vector<std::uint32_t> meshIndices; // into ModelData::meshes
        std::vector<std::uint32_t> children;    // into ModelData::nodes
    };

    /*!***********************************************************************
    @brief          A skeleton that bends meshes: its bones (joints) are nodes of
                    the model. A vertex of a skinned mesh is moved by each of its
                    bones as jointWorld * inverseBind[joint], where jointWorld is
                    the bone node's transform from the model's root, weighted.
    *************************************************************************/
    struct Skin
    {
        std::string name;
        std::vector<std::uint32_t> joints;  // into ModelData::nodes
        std::vector<glm::mat4> inverseBind; // one per joint: the model's space to the bone's, at rest
    };

    /*!***********************************************************************
    @brief          One animated property of one node over time: keyframe times
                    (seconds, increasing) and the values at them - 3 floats each
                    for translation and scale, 4 (x, y, z, w) for rotation.
    *************************************************************************/
    struct AnimationChannel
    {
        enum class Path : std::uint8_t
        {
            Translation,
            Rotation,
            Scale,
        };
        enum class Interpolation : std::uint8_t
        {
            Linear, // rotations are slerped
            Step,   // holds each key until the next
        };

        std::uint32_t node = 0; // into ModelData::nodes
        Path path = Path::Translation;
        Interpolation interpolation = Interpolation::Linear;
        std::vector<float> times;
        std::vector<float> values;
    };

    /*!***********************************************************************
    @brief          A named animation ("Walk", "Jump"): the channels that move
                    the model's nodes, and how long it lasts.
    *************************************************************************/
    struct AnimationClip
    {
        std::string name;
        float duration = 0.0f; // seconds: the last key of any channel
        std::vector<AnimationChannel> channels;
    };

    /*!***********************************************************************
    @brief          The internal model blob: meshes, material slots and nodes.

                    Each MeshData splits its own index buffer into SubMesh runs
                    whose materialIndex points into materials. When nodes is
                    empty the file had no hierarchy (OBJ, plain FBX), and every
                    mesh is drawn straight under the model's root transform.
    *************************************************************************/
    struct ModelData
    {
        std::vector<MeshData> meshes;
        std::vector<MaterialData> materials;
        std::vector<ModelNode> nodes;
        std::vector<std::uint32_t> roots; // node indices with no parent; empty = derive from the children lists
        std::vector<Skin> skins;
        std::vector<AnimationClip> animations;
        // Level of detail: LOD 1, LOD 2, ... - each the meshes above again (same order, same nodes) with fewer
        // triangles, made by GenerateLods (MeshSimplifier.hpp) when the model is loaded; empty for a small model
        std::vector<std::vector<MeshData>> lods;
        std::string name;
    };
}
