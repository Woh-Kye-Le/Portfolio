/**
 * @file     scriptImple.cpp
 * @author   Jethro Sung
 * @email    sung.h
 * @date     2025-11-7
 * @brief    Implements SpinScript and ChasePlayerScript runtime behavior.
*/
#include "Core/gameApp.h"
#include "../include/chasePlayerScripts.h"
#include "../include/spinScripts.h"

/**
 * @brief Applies a constant rotation to entity each frame.
*/
void SpinScript::OnUpdate(IComponentContext& ctx, Entity e, float dt) {
    auto& app = static_cast<GameApp&>(ctx);
    if (auto* t = app.GetTransform(e)) {
        t->SetRotation(t->GetRotation() + speed * dt);
    }
}

/**
    * @brief Placeholder chase behavior:
    *        Currently rotates the entity to indicate activity.
    *
    * @todo Replace with real nav once EnemyAi integration lands.
*/
void ChasePlayerScript::OnUpdate(IComponentContext& ctx, Entity e, float dt) {
    if (auto* t = ctx.GetTransform(e)){
        t->SetRotation(t->GetRotation() + 0.3f * dt);
    }

    // Future AI control will go here:
    // auto* ai = app.GetEnemyController(e);
    // if (ai) { ai->SetTarget(GameApp::playerEntity); };
}