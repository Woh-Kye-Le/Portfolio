/* !
@file    	    Components.hpp
@author  	    sung.h@digipen.edu [93%]
         	    c.kojat@digipen.edu [7%]
@date    		01/10/2026
@brief
    Lists every ECS component type the engine supports and the compile-time
    check that a type is one.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

/*                                                                      guard
----------------------------------------------------------------------------- */
#pragma once

#include "engine/scene/components/AnimatorComponent.hpp"
#include "engine/scene/components/AudioSourceComponent.hpp"
#include "engine/scene/components/CameraComponent.hpp"
#include "engine/scene/components/EntityIdComponent.hpp"
#include "engine/scene/components/HierarchyComponent.hpp"
#include "engine/scene/components/LightComponent.hpp"
#include "engine/scene/components/MeshRendererComponent.hpp"
#include "engine/scene/components/PhysicsBodyComponent.hpp"
#include "engine/scene/components/PlayerControllerComponent.hpp"
#include "engine/scene/components/RigidBodyComponent.hpp"
#include "engine/scene/components/ScreenElementComponent.hpp"
#include "engine/scene/components/SkyComponent.hpp"
#include "engine/scene/components/SpinComponent.hpp"
#include "engine/scene/components/TransformComponent.hpp"

#include <tuple>
#include <type_traits>

namespace Engine::Scene
{
    // Every component type supported by the engine scene API
    using EngineComponents =
        std::tuple<EntityIdComponent, TransformComponent, MeshRendererComponent, LightComponent, CameraComponent,
                   AudioSourceComponent, PlayerControllerComponent, SpinComponent, HierarchyComponent,
                   RigidBodyComponent, PhysicsBodyComponent, ScreenElementComponent, AnimatorComponent, SkyComponent>;

    // Compile-time check used by Scene's templates, so using a type that isn't a component is a build error
    // instead of a confusing EnTT error. Add new component types to the tuple above.
    template <class T, class Tuple> struct TupleContains;

    template <class T, class... Ts>
    struct TupleContains<T, std::tuple<Ts...>> : std::bool_constant<(std::is_same_v<T, Ts> || ...)>
    {
    };

    template <class T>
    inline constexpr bool IsEngineComponent = TupleContains<std::remove_cv_t<T>, EngineComponents>::value;
}
