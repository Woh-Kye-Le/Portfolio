#include "engine/audio/AudioSystem.hpp"
#include "engine/graphics/Camera/Camera3D.hpp"
#include "engine/graphics/Lighting/Lights.hpp"
#include "engine/scene/Scene.hpp"
#include "engine/scene/systems/AudioSystem.hpp"
#include "engine/scene/systems/RenderSystem.hpp"
#include "engine/scene/systems/SpinSystem.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <vector>

// The parts of RenderSystem, AudioSystem and SpinSystem that need no window or sound device: each one only acts on
// entities that have its required components, and reacts as soon as a component is added or removed.

namespace
{
    using namespace Engine::Scene;
    using Engine::Graphics::Camera3D;
    using Engine::Graphics::LightingContext;
    using Engine::Graphics::LightType;
    using Engine::Systems::RenderSystem;
    using Engine::Systems::SpinSystem;

    LightComponent MakeLight(float intensity)
    {
        LightComponent light;
        light.intensity = intensity;
        return light;
    }

    CameraComponent MakeCamera(bool primary)
    {
        CameraComponent camera;
        camera.primary = primary;
        return camera;
    }

    // Only entities with both Transform and Spin turn, the angle wraps at 360, and removing Spin stops it.
    TEST(EcsSpinSystem, TurnsOnlyEntitiesWithTransformAndSpin)
    {
        Scene scene;
        const auto spinner = scene.CreateEntity();
        const auto still = scene.CreateEntity();
        const auto noTransform = scene.CreateEntity();
        ASSERT_TRUE(scene.AddComponent(spinner, SpinComponent{.degreesPerSecond = 90.0f}));
        ASSERT_TRUE(scene.AddComponent<SpinComponent>(noTransform));
        ASSERT_TRUE(scene.RemoveComponent<TransformComponent>(noTransform));

        EXPECT_NO_THROW(SpinSystem::Update(scene, 1.0));
        EXPECT_FLOAT_EQ(scene.GetTransform(spinner)->rotation, 90.0f);
        EXPECT_FLOAT_EQ(scene.GetTransform(still)->rotation, 0.0f);

        SpinSystem::Update(scene, 3.5); // 90 + 315 wraps past 360
        EXPECT_FLOAT_EQ(scene.GetTransform(spinner)->rotation, 45.0f);

        ASSERT_TRUE(scene.RemoveComponent<SpinComponent>(spinner));
        SpinSystem::Update(scene, 1.0);
        EXPECT_FLOAT_EQ(scene.GetTransform(spinner)->rotation, 45.0f);
    }

    // A flickering light stays within its range and does change; a steady one, or any light while not playing, does not
    TEST(EcsRenderSystem, FlickeringLightsDipOnlyWhilePlaying)
    {
        float lowest = 1.0f;
        float highest = 0.0f;
        for (int i = 1; i < 1000; ++i)
        {
            const float brightness = RenderSystem::FlickerBrightness(i * 0.01, 7u, 0.6f);
            lowest = std::min(lowest, brightness);
            highest = std::max(highest, brightness);
        }
        EXPECT_GE(lowest, 0.4f);
        EXPECT_LE(highest, 1.0f);
        EXPECT_GT(highest - lowest, 0.1f) << "it can be seen to flicker";
        EXPECT_FLOAT_EQ(RenderSystem::FlickerBrightness(1.5, 7u, 0.0f), 1.0f);
        EXPECT_NE(RenderSystem::FlickerBrightness(1.5, 7u, 0.6f), RenderSystem::FlickerBrightness(1.5, 8u, 0.6f))
            << "lights do not flicker in step";

        Scene scene;
        const auto lamp = scene.CreateEntity();
        ASSERT_TRUE(scene.SetTransform(lamp, TransformComponent{}));
        LightComponent light;
        light.type = LightType::Point;
        light.intensity = 2.0f;
        light.flicker = 1.0f;
        ASSERT_TRUE(scene.AddComponent(lamp, light));
        LightingContext lighting;
        RenderSystem::CollectLights(scene, lighting);
        EXPECT_FLOAT_EQ(lighting.lights[0].intensity, 2.0f) << "steady in the editor";
        RenderSystem::CollectLights(scene, lighting, 1.5);
        EXPECT_FLOAT_EQ(lighting.lights[0].intensity,
                        2.0f * RenderSystem::FlickerBrightness(1.5, lamp.GetId(), 1.0f));
    }

    // A scene with no Light components keeps the default lighting; once there is one, the lights match the components
    // (position, colour, enabled) on every call.
    TEST(EcsRenderSystem, LightsFollowLightComponentsImmediately)
    {
        Scene scene;
        LightingContext lighting;
        lighting.AddDirectional({0.0f, -1.0f, 0.0f}, {1.0f, 1.0f, 1.0f});

        // No Light components: the renderer's default lighting is left alone
        RenderSystem::CollectLights(scene, lighting);
        ASSERT_EQ(lighting.lights.size(), 1u);
        EXPECT_EQ(lighting.lights[0].type, LightType::Directional);

        const auto lamp = scene.CreateEntity();
        TransformComponent transform;
        transform.x = 1.0f;
        transform.y = 2.0f;
        transform.z = 3.0f;
        ASSERT_TRUE(scene.SetTransform(lamp, transform));
        LightComponent light;
        light.type = LightType::Point;
        light.colorR = 0.5f;
        light.intensity = 4.0f;
        light.range = 12.0f;
        ASSERT_TRUE(scene.AddComponent(lamp, light));

        RenderSystem::CollectLights(scene, lighting);
        ASSERT_EQ(lighting.lights.size(), 1u);
        EXPECT_EQ(lighting.lights[0].type, LightType::Point);
        EXPECT_FLOAT_EQ(lighting.lights[0].position.x, 1.0f);
        EXPECT_FLOAT_EQ(lighting.lights[0].position.y, 2.0f);
        EXPECT_FLOAT_EQ(lighting.lights[0].position.z, 3.0f);
        EXPECT_FLOAT_EQ(lighting.lights[0].color.r, 0.5f);
        EXPECT_FLOAT_EQ(lighting.lights[0].intensity, 4.0f);
        EXPECT_FLOAT_EQ(lighting.lights[0].range, 12.0f);

        // Disabled: the component still replaces the defaults, but contributes no light
        scene.TryGetComponent<LightComponent>(lamp)->enabled = false;
        RenderSystem::CollectLights(scene, lighting);
        EXPECT_TRUE(lighting.lights.empty());

        // Removed: nothing overrides the lighting any more
        ASSERT_TRUE(scene.RemoveComponent<LightComponent>(lamp));
        lighting.AddDirectional({0.0f, -1.0f, 0.0f}, {1.0f, 1.0f, 1.0f});
        RenderSystem::CollectLights(scene, lighting);
        EXPECT_EQ(lighting.lights.size(), 1u);
    }

    // A light without a Transform is skipped, and lights come out in entity creation order no matter which order the
    // components were added.
    TEST(EcsRenderSystem, LightsRequireATransformAndComeInCreationOrder)
    {
        Scene scene;
        const auto first = scene.CreateEntity();
        const auto second = scene.CreateEntity();
        const auto noTransform = scene.CreateEntity();
        ASSERT_TRUE(scene.AddComponent(second, MakeLight(2.0f)));
        ASSERT_TRUE(scene.AddComponent(first, MakeLight(1.0f)));
        ASSERT_TRUE(scene.AddComponent(noTransform, MakeLight(3.0f)));
        ASSERT_TRUE(scene.RemoveComponent<TransformComponent>(noTransform));

        LightingContext lighting;
        RenderSystem::CollectLights(scene, lighting);
        ASSERT_EQ(lighting.lights.size(), 2u);
        EXPECT_FLOAT_EQ(lighting.lights[0].intensity, 1.0f);
        EXPECT_FLOAT_EQ(lighting.lights[1].intensity, 2.0f);
    }

    // The render camera is only touched when there is a primary camera with a Transform; then it takes that camera's
    // position, direction and lens.
    TEST(EcsRenderSystem, PrimaryCameraComponentDrivesTheRenderCamera)
    {
        Scene scene;
        Camera3D camera;
        camera.SetPerspective(60.0f, 16.0f / 9.0f, 0.1f, 100.0f);
        camera.SetPosition({9.0f, 9.0f, 9.0f});

        // No camera entity: the render camera is left to the caller
        EXPECT_FALSE(RenderSystem::ApplyPrimaryCamera(scene, camera));
        EXPECT_FLOAT_EQ(camera.GetPosition().x, 9.0f);

        const auto secondary = scene.CreateEntity();
        ASSERT_TRUE(scene.AddComponent(secondary, MakeCamera(false)));
        EXPECT_FALSE(RenderSystem::ApplyPrimaryCamera(scene, camera));

        const auto eye = scene.CreateEntity();
        TransformComponent transform;
        transform.x = 1.0f;
        transform.y = 2.0f;
        transform.z = 8.0f;
        ASSERT_TRUE(scene.SetTransform(eye, transform));
        ASSERT_TRUE(scene.AddComponent(eye, MakeCamera(true)));

        ASSERT_TRUE(RenderSystem::ApplyPrimaryCamera(scene, camera));
        EXPECT_FLOAT_EQ(camera.GetPosition().x, 1.0f);
        EXPECT_FLOAT_EQ(camera.GetPosition().y, 2.0f);
        EXPECT_FLOAT_EQ(camera.GetPosition().z, 8.0f);
        EXPECT_NEAR(camera.GetForward().z, -1.0f, 1e-5f); // no rotation looks down -Z

        // Turned 90 degrees around Y it looks down +X
        scene.TryGetComponent<TransformComponent>(eye)->rotation = 90.0f;
        ASSERT_TRUE(RenderSystem::ApplyPrimaryCamera(scene, camera));
        EXPECT_NEAR(camera.GetForward().x, 1.0f, 1e-5f);
        EXPECT_NEAR(camera.GetForward().z, 0.0f, 1e-5f);

        // Removing the component (or its Transform) hands the camera back at once
        ASSERT_TRUE(scene.RemoveComponent<TransformComponent>(eye));
        EXPECT_FALSE(RenderSystem::ApplyPrimaryCamera(scene, camera));
        ASSERT_TRUE(scene.AddComponent<TransformComponent>(eye));
        EXPECT_TRUE(RenderSystem::ApplyPrimaryCamera(scene, camera));
        ASSERT_TRUE(scene.RemoveComponent<CameraComponent>(eye));
        EXPECT_FALSE(RenderSystem::ApplyPrimaryCamera(scene, camera));
    }

    // With no sound device (audio never initialised) the ECS AudioSystem must leave every source untouched, so the
    // clip is still loaded once audio becomes available. Holds for both backends (FMOD and the stub).
    TEST(EcsAudioSystem, WaitsForAudioBeforeConsumingTheLoadAttempt)
    {
        Scene scene;
        const auto emitter = scene.CreateEntity();
        AudioSourceComponent withClip;
        withClip.clip = 1;
        ASSERT_TRUE(scene.AddComponent(emitter, withClip));
        const auto noTransform = scene.CreateEntity();
        ASSERT_TRUE(scene.AddComponent(noTransform, withClip));
        ASSERT_TRUE(scene.RemoveComponent<TransformComponent>(noTransform));

        Engine::Audio::AudioSystem audio; // not initialised
        ASSERT_FALSE(audio.IsAvailable());
        const std::vector<std::filesystem::path> clips{"missing.wav"};

        EXPECT_NO_THROW(Engine::Systems::AudioSystem::Update(scene, audio, clips, true));
        EXPECT_NO_THROW(Engine::Systems::AudioSystem::Update(scene, audio, clips, false));
        for (const auto entity : {emitter, noTransform})
        {
            const auto source = scene.GetComponent<AudioSourceComponent>(entity);
            ASSERT_TRUE(source.has_value());
            EXPECT_FALSE(source->loadAttempted);
            EXPECT_FALSE(source->started);
            EXPECT_FALSE(source->voice.IsValid());
        }
    }

    // StopAll clears started/voice on every AudioSource, even ones without a Transform.
    TEST(EcsAudioSystem, StopAllResetsPlaybackStateOfEverySource)
    {
        Scene scene;
        const auto first = scene.CreateEntity();
        const auto second = scene.CreateEntity();
        AudioSourceComponent playing;
        playing.started = true;
        ASSERT_TRUE(scene.AddComponent(first, playing));
        ASSERT_TRUE(scene.AddComponent(second, playing));
        ASSERT_TRUE(scene.RemoveComponent<TransformComponent>(second)); // StopAll does not need a Transform

        Engine::Audio::AudioSystem audio;
        Engine::Systems::AudioSystem::StopAll(scene, audio);
        for (const auto entity : {first, second})
        {
            EXPECT_FALSE(scene.GetComponent<AudioSourceComponent>(entity)->started);
            EXPECT_FALSE(scene.GetComponent<AudioSourceComponent>(entity)->voice.IsValid());
        }
    }
} // namespace
