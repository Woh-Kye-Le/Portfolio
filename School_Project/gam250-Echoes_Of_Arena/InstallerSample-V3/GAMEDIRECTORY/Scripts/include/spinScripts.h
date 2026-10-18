#pragma once
/**
 * @file     spinScripts.h
 * @author   Jethro Sung
 * @email    sung.h
 * @date     2025-11-7
 * @brief    Declares a simple rotational animation script.
 *
 * Causes entity to rotate continuously at a defined angular speed.
 * Useful for floating collectibles, debug markers, etc.
*/
#include "Scripting/scripts.h"

/**
    * @class SpinScript
    * @brief Rotates an entity about its pivot point every frame.
    *
    * @param speed Rotation speed in radians per second.
*/
class SpinScript : public IScript {
    float speed;
public:
    explicit SpinScript(float s) : speed(s) {}
    void OnUpdate(IComponentContext& ctx, Entity e, float dt) override;
};