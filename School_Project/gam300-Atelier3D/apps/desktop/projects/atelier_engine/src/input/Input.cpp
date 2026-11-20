/* !
@file    	    Input.cpp
@author  	    beh.s@digipen.edu [100%]
@date    		27/09/2026
@brief
    Input system following the GLFW input model. Provides keyboard and mouse
    input handling.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

#include "engine/input/Input.hpp"

// In our main init code
//Engine::Input::InputSystem input;
//Engine::Input::RegisterCallbacks(window, input);
//
//while (!glfwWindowShouldClose(window))
//{
//    glfwPollEvents(); // triggers the callbacks above
//    // ... game/editor logic using input.GetKey(), input.IsKeyPressed(), etc. ...
//    input.Update(); // advance frame state
//}

namespace Engine::Input
{
    // Give the callbacks a way to reach your InputSystem instance.
    // Simplest approach: a global/singleton pointer set at init time.
    static InputSystem* g_input = nullptr;

    void RegisterCallbacks(GLFWwindow* window, InputSystem& input)
    {
        // supports only one active InputSystem at a time; if you need more, you'll need to manage a mapping of windows
        // to InputSystem instances
        g_input = &input;

        // physical keyboard keys
        glfwSetKeyCallback(window,
                           [](GLFWwindow*, int key, int /*scancode*/, int action, int /*mods*/)
                           {
                               if (!g_input)
                                   return;
                               if (action == GLFW_PRESS || action == GLFW_REPEAT)
                                   g_input->SetKey(key, KeyState::Down);
                               else if (action == GLFW_RELEASE)
                                   g_input->SetKey(key, KeyState::Up);
                           });

        // mouse buttons only
        glfwSetMouseButtonCallback(window,
                                   [](GLFWwindow*, int button, int action, int /*mods*/)
                                   {
                                       if (!g_input)
                                           return;
                                       g_input->SetMouseButton(button,
                                                               action == GLFW_PRESS ? KeyState::Down : KeyState::Up);
                                   });

        // mouse posiition
        glfwSetCursorPosCallback(window,
                                 [](GLFWwindow*, double x, double y)
                                 {
                                     if (!g_input)
                                         return;
                                     g_input->SetMousePosition(static_cast<float>(x), static_cast<float>(y));
                                 });

        // scroll wheel/trackpad scroll
        glfwSetScrollCallback(window,
                              [](GLFWwindow*, double /*xoffset*/, double yoffset)
                              {
                                  if (!g_input)
                                      return;
                                  g_input->SetScrollDelta(static_cast<float>(yoffset));
                              });

        // actual typed characters
        glfwSetCharCallback(window,
                            [](GLFWwindow*, unsigned int codepoint)
                            {
                                if (!g_input)
                                    return;
                                g_input->AddTextInput(codepoint);
                            });
    }
}
