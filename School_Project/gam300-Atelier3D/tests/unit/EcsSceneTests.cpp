#include "engine/scene/Scene.hpp"
#include <gtest/gtest.h>
#include <stdexcept>
#include <unordered_set>

namespace
{
    using namespace Engine::Scene;

    template <class T> class EcsComponentTest : public ::testing::Test
    {
    };
    using RequiredComponents = ::testing::Types<TransformComponent, MeshRendererComponent, LightComponent,
                                                CameraComponent, AudioSourceComponent, PlayerControllerComponent,
                                                RigidBodyComponent>;
    TYPED_TEST_SUITE(EcsComponentTest, RequiredComponents);

    TYPED_TEST(EcsComponentTest, AddGetSetRemoveAndReadd)
    {
        Scene scene;
        const auto entity = scene.CreateEntity();
        const auto id = entity.GetId();
        // CreateEntity supplies Transform automatically. Start each type absent.
        if (scene.HasComponent<TypeParam>(entity))
        {
            ASSERT_TRUE(scene.RemoveComponent<TypeParam>(entity));
        }
        EXPECT_FALSE(scene.GetComponent<TypeParam>(entity).has_value());
        EXPECT_EQ(scene.TryGetComponent<TypeParam>(entity), nullptr);
        EXPECT_FALSE(scene.SetComponent<TypeParam>(entity, TypeParam{}));
        ASSERT_TRUE(scene.AddComponent<TypeParam>(entity));
        EXPECT_FALSE(scene.AddComponent<TypeParam>(entity));
        EXPECT_TRUE(scene.HasComponent<TypeParam>(entity));
        ASSERT_TRUE(scene.GetComponent<TypeParam>(entity).has_value());
        ASSERT_NE(scene.TryGetComponent<TypeParam>(entity), nullptr);
        const Scene& readOnly = scene;
        EXPECT_NE(readOnly.TryGetComponent<TypeParam>(entity), nullptr);
        EXPECT_TRUE(scene.SetComponent<TypeParam>(entity, TypeParam{}));
        EXPECT_EQ(scene.Count<TypeParam>(), 1u);
        ASSERT_TRUE(scene.RemoveComponent<TypeParam>(entity));
        EXPECT_FALSE(scene.RemoveComponent<TypeParam>(entity));
        EXPECT_EQ(scene.Count<TypeParam>(), 0u);
        ASSERT_TRUE(scene.AddComponent<TypeParam>(entity));
        EXPECT_EQ(entity.GetId(), id);
        EXPECT_TRUE(scene.IsAlive(entity));
        EXPECT_EQ(scene.EntityCount(), 1u);
    }

    TYPED_TEST(EcsComponentTest, RejectsDestroyedAndForeignHandles)
    {
        Scene scene;
        Scene other;
        const auto dead = scene.CreateEntity();
        const auto foreign = other.CreateEntity();
        scene.DestroyEntity(dead);
        for (auto entity : {Entity{}, dead, foreign})
        {
            EXPECT_FALSE(scene.HasComponent<TypeParam>(entity));
            EXPECT_FALSE(scene.GetComponent<TypeParam>(entity).has_value());
            EXPECT_EQ(scene.TryGetComponent<TypeParam>(entity), nullptr);
            EXPECT_FALSE(scene.AddComponent<TypeParam>(entity));
            EXPECT_FALSE(scene.SetComponent<TypeParam>(entity, TypeParam{}));
            EXPECT_FALSE(scene.RemoveComponent<TypeParam>(entity));
        }
    }

    // Entities get different non-zero ids, start with a Transform, can be found by id, and a destroyed id is never handed
    // out again.
    TEST(EcsScene, CreateDestroyAndNeverReuseAnId)
    {
        Scene scene;
        EXPECT_EQ(scene.EntityCount(), 0u);
        const auto first = scene.CreateEntity();
        const auto second = scene.CreateEntity();
        EXPECT_NE(first.GetId(), 0u);
        EXPECT_NE(first.GetId(), second.GetId());
        ASSERT_TRUE(scene.GetTransform(first).has_value());
        EXPECT_EQ(scene.FindEntity(first.GetId()).GetId(), first.GetId());
        scene.DestroyEntity(first);
        EXPECT_FALSE(scene.IsAlive(first));
        EXPECT_FALSE(static_cast<bool>(scene.FindEntity(first.GetId())));
        EXPECT_TRUE(scene.IsAlive(second));
        scene.DestroyEntity(first); // repeated destruction is safe
        scene.DestroyEntity(Entity{});
        EXPECT_EQ(scene.EntityCount(), 1u);
        const auto third = scene.CreateEntity();
        EXPECT_NE(third.GetId(), first.GetId());
        EXPECT_NE(third.GetId(), second.GetId());
        scene.DestroyEntity(second);
        scene.DestroyEntity(third);
        EXPECT_TRUE(scene.Entities().empty());
        EXPECT_EQ(scene.Count<TransformComponent>(), 0u);
    }

    // Two scenes can give out the same id - destroying an entity from the other scene must not touch ours.
    TEST(EcsScene, ForeignDestroyDoesNotDestroyLocalEntityWithSameId)
    {
        Scene scene;
        Scene other;
        const auto local = scene.CreateEntity();
        const auto foreign = other.CreateEntity();
        ASSERT_EQ(local.GetId(), foreign.GetId());
        scene.DestroyEntity(foreign);
        EXPECT_TRUE(scene.IsAlive(local));
        EXPECT_TRUE(other.IsAlive(foreign));
    }

    // Adding/removing components over and over (which moves the entity around inside EnTT) must keep its other component
    // values and not touch the neighbour.
    TEST(EcsScene, ArchetypeChangesPreserveValuesAndOtherEntities)
    {
        Scene scene;
        const auto entity = scene.CreateEntity();
        const auto neighbour = scene.CreateEntity();
        TransformComponent transform;
        transform.x = 12.0f;
        transform.rotation = 47.0f;
        ASSERT_TRUE(scene.SetTransform(entity, transform));
        LightComponent light;
        light.intensity = 7.0f;
        ASSERT_TRUE(scene.AddComponent(entity, light));
        for (int i = 0; i < 20; ++i)
        {
            ASSERT_TRUE(scene.AddComponent<MeshRendererComponent>(entity));
            ASSERT_TRUE(scene.RemoveComponent<MeshRendererComponent>(entity));
        }
        ASSERT_TRUE(scene.GetTransform(entity).has_value());
        EXPECT_EQ(*scene.GetTransform(entity), transform);
        ASSERT_TRUE(scene.GetComponent<LightComponent>(entity).has_value());
        EXPECT_FLOAT_EQ(scene.GetComponent<LightComponent>(entity)->intensity, 7.0f);
        EXPECT_EQ(*scene.GetTransform(neighbour), TransformComponent{});
        EXPECT_FALSE(scene.HasComponent<LightComponent>(neighbour));
    }

    // GetTransform gives a copy (editing it changes nothing until SetTransform), TryGetComponent gives the real stored
    // value.
    TEST(EcsScene, GetReturnsCopyAndPointerEditsStoredValue)
    {
        Scene scene;
        const auto entity = scene.CreateEntity();
        auto copy = scene.GetTransform(entity);
        ASSERT_TRUE(copy.has_value());
        copy->x = 9.0f;
        EXPECT_FLOAT_EQ(scene.GetTransform(entity)->x, 0.0f);
        ASSERT_TRUE(scene.SetTransform(entity, *copy));
        auto* stored = scene.TryGetComponent<TransformComponent>(entity);
        ASSERT_NE(stored, nullptr);
        stored->x = 15.0f;
        EXPECT_FLOAT_EQ(scene.GetTransform(entity)->x, 15.0f);
    }

    // Count<Transform, MeshRenderer> goes up and down right away as either component is added or removed.
    TEST(EcsScene, QueryMembershipChangesImmediately)
    {
        Scene scene;
        const auto entity = scene.CreateEntity();
        const auto count = [&] { return scene.Count<TransformComponent, MeshRendererComponent>(); };
        EXPECT_EQ(count(), 0u);
        ASSERT_TRUE(scene.AddComponent<MeshRendererComponent>(entity));
        EXPECT_EQ(count(), 1u);
        ASSERT_TRUE(scene.RemoveComponent<TransformComponent>(entity));
        EXPECT_EQ(count(), 0u);
        ASSERT_TRUE(scene.AddComponent<TransformComponent>(entity));
        EXPECT_EQ(count(), 1u);
        ASSERT_TRUE(scene.RemoveComponent<MeshRendererComponent>(entity));
        EXPECT_EQ(count(), 0u);
    }

    // An exception thrown inside ForEach reaches the caller and the scene is unlocked again afterwards. Creating an entity
    // inside ForEach is refused with logic_error.
    TEST(EcsScene, CallbackExceptionsPropagateAndReleaseIterationGuard)
    {
        Scene scene;
        scene.CreateEntity();
        EXPECT_THROW(scene.ForEach<TransformComponent>([](Entity, TransformComponent&)
                                                       { throw std::runtime_error("test callback"); }),
                     std::runtime_error);
        EXPECT_NO_THROW(scene.CreateEntity());
        EXPECT_EQ(scene.EntityCount(), 2u);
        EXPECT_THROW(scene.ForEach<TransformComponent>([&](Entity, TransformComponent&) { scene.CreateEntity(); }),
                     std::logic_error);
        EXPECT_NO_THROW(scene.CreateEntity());
    }

    // 1,000 entities: all ids unique, the query counts stay right after removing components from half of them and
    // destroying some.
    TEST(EcsScene, ThousandEntitiesRemainUniqueAcrossComponentChangesAndDestruction)
    {
        Scene scene;
        std::unordered_set<Entity::Id> ids;
        std::vector<Entity> entities;
        for (int i = 0; i < 1000; ++i)
        {
            const auto entity = scene.CreateEntity();
            ASSERT_TRUE(ids.insert(entity.GetId()).second);
            ASSERT_TRUE(scene.AddComponent<MeshRendererComponent>(entity));
            entities.push_back(entity);
        }
        EXPECT_EQ(scene.EntityCount(), 1000u);
        EXPECT_EQ((scene.Count<TransformComponent, MeshRendererComponent>()), 1000u);
        for (std::size_t i = 0; i < entities.size(); i += 2)
            ASSERT_TRUE(scene.RemoveComponent<MeshRendererComponent>(entities[i]));
        EXPECT_EQ((scene.Count<TransformComponent, MeshRendererComponent>()), 500u);
        for (auto entity : entities)
            scene.DestroyEntity(entity);
        EXPECT_EQ(scene.EntityCount(), 0u);
        EXPECT_EQ(scene.Count<TransformComponent>(), 0u);
    }
} // namespace
