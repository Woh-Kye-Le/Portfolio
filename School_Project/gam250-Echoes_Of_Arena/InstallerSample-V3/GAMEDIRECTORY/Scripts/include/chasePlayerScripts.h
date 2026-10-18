#pragma once
/*
 * @file     chasePlayerScripts.h
 * @author   Jethro Sung
 * @email    sung.h
 * @date     2025-11-7
 * @brief    Declares a placeholder script for following the Player.
*/
#include "Scripting/scripts.h"

/**
 * @class ChasePlayerScript
 * @brief Proof-of-concept chasing behavior for enemy testing.
 *
 * @note Currently only applies a temporary rotation animation.
 * @todo Integrate with EnemyAi once pathfinding is complete.
*/
class ChasePlayerScript : public IScript {
public:
    void OnUpdate(IComponentContext& ctx, Entity e, float dt) override;
};