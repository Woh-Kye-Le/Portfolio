/* !
@file    	    PlayerControllerSystem.hpp
@author  	    sung.h@digipen.edu [67%]
         	    c.kojat@digipen.edu [33%]
@date    		01/10/2026
@brief
    Declares PlayerControllerSystem: player movement and mouse look, and keeping
    the Main camera behind the player.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

/*                                                                      guard
----------------------------------------------------------------------------- */
#pragma once

namespace Engine::Scene
{
    class Scene;
}

namespace Engine::Input
{
    class GameInput;
}

namespace Engine::Physics
{
    class PhysicsWorld;
}

namespace Engine::Graphics
{
    class Camera3D;
}

namespace Engine::Systems
{
    class PlayerControllerSystem
    {
    public:
        static constexpr float kMinPitch = -80.0f;
        static constexpr float kMaxPitch = 40.0f;
        static constexpr float kMinDistance = 1.5f;
        static constexpr float kMaxDistance = 30.0f;

        // Every entity with a Transform and a PlayerController: the move actions move it relative to the way it
        // faces (Transform rotation, around Y), and while Look is held the mouse turns it and tilts its camera. The
        // scroll wheel moves its camera closer or further away.
        static void Update(Scene::Scene& scene, const Input::GameInput& input, double deltaSeconds);

        // Puts the scene's Main camera (the primary CameraComponent, by its Transform) behind the first player with
        // followCamera, looking where the player faces. With the physics world, the camera does not go through what
        // is between it and the player (a wall, a tree): it comes in to just in front of it. False when there is no
        // such player or no Main camera.
        static bool MoveMainCameraToPlayer(Scene::Scene& scene, const Physics::PhysicsWorld* world = nullptr);

        // The same view, set on camera directly: for scenes with a following player but no Main camera
        static bool FollowWithCamera(Scene::Scene& scene, Graphics::Camera3D& camera,
                                     const Physics::PhysicsWorld* world = nullptr);

        // How far in front of what is in the way the camera stays, so its near plane does not cut into it
        static constexpr float kCameraClearance = 0.3f;
        // The closest the camera comes to the player's head when something is right behind it
        static constexpr float kClosestCamera = 0.4f;
    };
}
