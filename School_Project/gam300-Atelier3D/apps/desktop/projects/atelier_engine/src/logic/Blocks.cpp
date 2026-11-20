/* !
@file    	    Blocks.cpp
@author  	    c.kojat@digipen.edu [100%]
@date    		27/10/2026
@brief
    Implements the block table (every kind of block, its label, shape and
    defaults), block ids, variables, the placement rules and the blocks as
    sentences.

All content © 2026 DigiPen Institute of Technology Singapore. All rights reserved.
*//*__________________________________________________________________________*/

#include "engine/logic/Blocks.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <format>
#include <iterator>
#include <stdexcept>

namespace Engine::Logic
{
    namespace
    {
        // Each entry is where its Op puts it (InfoOf indexes the table by Op)
        template <std::size_t N> constexpr bool InOpOrder(std::array<BlockInfo, N> blocks)
        {
            for (std::size_t i = 0; i < N; ++i)
            {
                if (static_cast<std::size_t>(blocks[i].op) != i)
                {
                    return false;
                }
            }
            return true;
        }

        using enum Op;
        using C = Category;
        using S = BlockShape;

        // Every kind of block, in the order of Op. Labels use %n (number or value), %k (key), %t (text), %v
        // (variable) and %c (condition).
        constexpr std::array kBlocks{
            // Events
            BlockInfo{WhenPlayStarts,
                      "when_play_starts",
                      C::Events,
                      S::Hat,
                      "when Play starts",
                      false,
                      false,
                      {},
                      {},
                      "Runs the blocks under it once, as soon as Play is pressed"},
            BlockInfo{WhenKeyPressed,
                      "when_key_pressed",
                      C::Events,
                      S::Hat,
                      "when %k key pressed",
                      false,
                      false,
                      {},
                      "Space",
                      "Runs the blocks under it each time the key is pressed"},
            BlockInfo{WhenIReceive,
                      "when_i_receive",
                      C::Events,
                      S::Hat,
                      "when I receive %t",
                      false,
                      false,
                      {},
                      "go",
                      "Runs the blocks under it each time any object broadcasts this message"},
            BlockInfo{Broadcast,
                      "broadcast",
                      C::Events,
                      S::Stack,
                      "broadcast %t",
                      false,
                      false,
                      {},
                      "go",
                      "Sends a message to every object: their \"when I receive\" scripts for it start"},

            // Motion
            BlockInfo{MoveForward,
                      "move_forward",
                      C::Motion,
                      S::Stack,
                      "move forward %n",
                      false,
                      false,
                      {1.0f},
                      {},
                      "Moves the object the way it faces (a negative number moves it back)",
                      Property::Position},
            BlockInfo{MoveRight,
                      "move_right",
                      C::Motion,
                      S::Stack,
                      "move right %n",
                      false,
                      false,
                      {1.0f},
                      {},
                      "Moves the object sideways, to its right (a negative number moves it left)",
                      Property::Position},
            BlockInfo{TurnBy,
                      "turn_by",
                      C::Motion,
                      S::Stack,
                      "turn %n degrees",
                      false,
                      false,
                      {15.0f},
                      {},
                      "Turns the object around its up axis (a negative number turns the other way)",
                      Property::Rotation},
            BlockInfo{GoTo,
                      "go_to",
                      C::Motion,
                      S::Stack,
                      "go to x %n y %n z %n",
                      false,
                      false,
                      {},
                      {},
                      "Puts the object at this position",
                      Property::Position},
            BlockInfo{MoveBy,
                      "move_by",
                      C::Motion,
                      S::Stack,
                      "change position by x %n y %n z %n",
                      false,
                      false,
                      {1.0f, 0.0f, 0.0f},
                      {},
                      "Adds to the object's position",
                      Property::Position},
            BlockInfo{GlideTo,
                      "glide_to",
                      C::Motion,
                      S::Stack,
                      "glide %n secs to x %n y %n z %n",
                      false,
                      false,
                      {1.0f, 0.0f, 0.0f, 0.0f},
                      {},
                      "Moves the object smoothly to the position over this many seconds",
                      Property::Position},
            BlockInfo{GoToObject,
                      "go_to_object",
                      C::Motion,
                      S::Stack,
                      "go to %t",
                      false,
                      false,
                      {},
                      "Player",
                      "Puts the object where the object with this name is",
                      Property::Position},
            BlockInfo{PointTowards,
                      "point_towards",
                      C::Motion,
                      S::Stack,
                      "point towards %t",
                      false,
                      false,
                      {},
                      "Player",
                      "Turns the object to face the object with this name",
                      Property::Rotation},

            // Looks
            BlockInfo{Show,
                      "show",
                      C::Looks,
                      S::Stack,
                      "show",
                      false,
                      false,
                      {},
                      {},
                      "Makes the object visible",
                      Property::Visible},
            BlockInfo{Hide,
                      "hide",
                      C::Looks,
                      S::Stack,
                      "hide",
                      false,
                      false,
                      {},
                      {},
                      "Makes the object invisible (it is still there)",
                      Property::Visible},
            BlockInfo{SetColor,
                      "set_color",
                      C::Looks,
                      S::Stack,
                      "set color to r %n g %n b %n",
                      false,
                      false,
                      {1.0f, 0.2f, 0.2f},
                      {},
                      "Tints the object: red, green and blue from 0 to 1",
                      Property::Colour},
            BlockInfo{SetSize,
                      "set_size",
                      C::Looks,
                      S::Stack,
                      "set size to %n",
                      false,
                      false,
                      {1.0f},
                      {},
                      "Makes the object this big (1 is its normal size)",
                      Property::Size},
            BlockInfo{ChangeSizeBy,
                      "change_size_by",
                      C::Looks,
                      S::Stack,
                      "change size by %n",
                      false,
                      false,
                      {0.1f},
                      {},
                      "Makes the object bigger (or smaller, with a negative number)",
                      Property::Size},
            BlockInfo{Say,
                      "say",
                      C::Looks,
                      S::Stack,
                      "say %t",
                      false,
                      false,
                      {},
                      "Hello!",
                      "Writes a message in the Console",
                      Property::Says},

            // Loops
            BlockInfo{Repeat,
                      "repeat",
                      C::Loops,
                      S::Stack,
                      "repeat %n",
                      true,
                      false,
                      {10.0f},
                      {},
                      "Runs the blocks inside this many times, one time per frame"},
            BlockInfo{RepeatUntil,
                      "repeat_until",
                      C::Loops,
                      S::Stack,
                      "repeat until %c",
                      true,
                      false,
                      {},
                      {},
                      "Runs the blocks inside again and again, one time per frame, until the condition is true"},
            BlockInfo{Forever,
                      "forever",
                      C::Loops,
                      S::Cap,
                      "forever",
                      true,
                      false,
                      {},
                      {},
                      "Runs the blocks inside again and again, one time per frame, until Stop"},
            BlockInfo{Wait,
                      "wait",
                      C::Loops,
                      S::Stack,
                      "wait %n seconds",
                      false,
                      false,
                      {1.0f},
                      {},
                      "Waits before going on to the next block"},
            BlockInfo{WaitUntil,
                      "wait_until",
                      C::Loops,
                      S::Stack,
                      "wait until %c",
                      false,
                      false,
                      {},
                      {},
                      "Waits until the condition is true"},
            BlockInfo{Stop,
                      "stop",
                      C::Loops,
                      S::Cap,
                      "stop this script",
                      false,
                      false,
                      {},
                      {},
                      "Ends this script (the object's other scripts go on)"},

            // Conditions
            BlockInfo{If,
                      "if",
                      C::Conditions,
                      S::Stack,
                      "if %c then",
                      true,
                      false,
                      {},
                      {},
                      "Runs the blocks inside only when the condition is true"},
            BlockInfo{IfElse,
                      "if_else",
                      C::Conditions,
                      S::Stack,
                      "if %c then",
                      true,
                      true,
                      {},
                      {},
                      "Runs the first blocks when the condition is true, the ones under else when it is not"},
            BlockInfo{Equal,
                      "equal",
                      C::Conditions,
                      S::Condition,
                      "%n = %n",
                      false,
                      false,
                      {0.0f, 0.0f},
                      {},
                      "True when the two numbers are the same"},
            BlockInfo{Less,
                      "less",
                      C::Conditions,
                      S::Condition,
                      "%n < %n",
                      false,
                      false,
                      {0.0f, 50.0f},
                      {},
                      "True when the first number is smaller"},
            BlockInfo{Greater,
                      "greater",
                      C::Conditions,
                      S::Condition,
                      "%n > %n",
                      false,
                      false,
                      {0.0f, 50.0f},
                      {},
                      "True when the first number is bigger"},
            BlockInfo{And,
                      "and",
                      C::Conditions,
                      S::Condition,
                      "%c and %c",
                      false,
                      false,
                      {},
                      {},
                      "True when both conditions are true"},
            BlockInfo{Or,
                      "or",
                      C::Conditions,
                      S::Condition,
                      "%c or %c",
                      false,
                      false,
                      {},
                      {},
                      "True when either condition is true"},
            BlockInfo{Not,
                      "not",
                      C::Conditions,
                      S::Condition,
                      "not %c",
                      false,
                      false,
                      {},
                      {},
                      "True when the condition is false"},

            // Math
            BlockInfo{Add, "add", C::Math, S::Value, "%n + %n", false, false, {}, {}, "The two numbers added"},
            BlockInfo{Subtract,
                      "subtract",
                      C::Math,
                      S::Value,
                      "%n - %n",
                      false,
                      false,
                      {},
                      {},
                      "The first number take away the second"},
            BlockInfo{
                Multiply, "multiply", C::Math, S::Value, "%n * %n", false, false, {}, {}, "The two numbers multiplied"},
            BlockInfo{Divide,
                      "divide",
                      C::Math,
                      S::Value,
                      "%n / %n",
                      false,
                      false,
                      {},
                      {},
                      "The first number divided by the second"},
            BlockInfo{Mod,
                      "mod",
                      C::Math,
                      S::Value,
                      "%n mod %n",
                      false,
                      false,
                      {},
                      {},
                      "What is left over when the first number is divided by the second"},
            BlockInfo{Random,
                      "random",
                      C::Math,
                      S::Value,
                      "pick random %n to %n",
                      false,
                      false,
                      {1.0f, 10.0f},
                      {},
                      "A random number between the two (a whole number when both are whole)"},
            BlockInfo{Round,
                      "round",
                      C::Math,
                      S::Value,
                      "round %n",
                      false,
                      false,
                      {},
                      {},
                      "The number rounded to the nearest whole number"},
            BlockInfo{
                Abs, "abs", C::Math, S::Value, "abs of %n", false, false, {}, {}, "The number without its minus sign"},

            // Variables
            BlockInfo{SetVariable,
                      "set_variable",
                      C::Variables,
                      S::Stack,
                      "set %v to %n",
                      false,
                      false,
                      {},
                      {},
                      "Gives the variable this value"},
            BlockInfo{ChangeVariable,
                      "change_variable",
                      C::Variables,
                      S::Stack,
                      "change %v by %n",
                      false,
                      false,
                      {1.0f},
                      {},
                      "Adds to the variable (a negative number takes away)"},
            BlockInfo{VariableValue,
                      "variable",
                      C::Variables,
                      S::Value,
                      "%v",
                      false,
                      false,
                      {},
                      {},
                      "The variable's value: put it in any number"},

            // Sensing
            BlockInfo{KeyDown,
                      "key_down",
                      C::Sensing,
                      S::Condition,
                      "key %k pressed?",
                      false,
                      false,
                      {},
                      "Space",
                      "True while the key is held down"},
            BlockInfo{Timer,
                      "timer",
                      C::Sensing,
                      S::Value,
                      "timer",
                      false,
                      false,
                      {},
                      {},
                      "Seconds since Play started (or since the timer was reset)"},
            BlockInfo{ResetTimer,
                      "reset_timer",
                      C::Sensing,
                      S::Stack,
                      "reset timer",
                      false,
                      false,
                      {},
                      {},
                      "Starts the timer again from 0"},
            BlockInfo{PositionX,
                      "x_position",
                      C::Sensing,
                      S::Value,
                      "x position",
                      false,
                      false,
                      {},
                      {},
                      "Where the object is, from left to right"},
            BlockInfo{PositionY,
                      "y_position",
                      C::Sensing,
                      S::Value,
                      "y position",
                      false,
                      false,
                      {},
                      {},
                      "How high the object is"},
            BlockInfo{PositionZ,
                      "z_position",
                      C::Sensing,
                      S::Value,
                      "z position",
                      false,
                      false,
                      {},
                      {},
                      "Where the object is, from front to back"},
            BlockInfo{Direction,
                      "direction",
                      C::Sensing,
                      S::Value,
                      "direction",
                      false,
                      false,
                      {},
                      {},
                      "Which way the object faces, in degrees"},
            BlockInfo{DistanceTo,
                      "distance_to",
                      C::Sensing,
                      S::Value,
                      "distance to %t",
                      false,
                      false,
                      {},
                      "Player",
                      "How far away the object with this name is"},

            // Library

            // Physics
            BlockInfo{Jump,
                      "jump",
                      C::Physics,
                      S::Stack,
                      "jump %n high",
                      false,
                      false,
                      {1.5f},
                      {},
                      "Jumps this high (in units) when it stands on something. Needs a Dynamic Rigid Body"},
            BlockInfo{PushForward,
                      "push_forward",
                      C::Physics,
                      S::Stack,
                      "push forward %n",
                      false,
                      false,
                      {5.0f},
                      {},
                      "Pushes the object the way it faces, this fast (units per second), once: a kick. A negative "
                      "number pushes it back. Needs a Dynamic Rigid Body"},
            BlockInfo{PushBy,
                      "push_by",
                      C::Physics,
                      S::Stack,
                      "push by x %n y %n z %n",
                      false,
                      false,
                      {0.0f, 5.0f, 0.0f},
                      {},
                      "Speeds the object up along each axis by this much (units per second), once. Needs a Dynamic "
                      "Rigid Body"},
            BlockInfo{OnGround,
                      "on_ground",
                      C::Physics,
                      S::Condition,
                      "on the ground?",
                      false,
                      false,
                      {},
                      {},
                      "True while the object stands on something. Needs a Rigid Body"},
            BlockInfo{Touching,
                      "touching",
                      C::Physics,
                      S::Condition,
                      "touching %t?",
                      false,
                      false,
                      {},
                      "Player",
                      "True while the object's Rigid Body touches the named object's"},
            // More Motion, Sensing and Events, then Screen
            BlockInfo{PointInDirection,
                      "point_in_direction",
                      C::Motion,
                      S::Stack,
                      "point in direction %n",
                      false,
                      false,
                      {90.0f},
                      {},
                      "Turns the object to face this direction, in degrees (0 faces -Z, 90 faces -X)",
                      Property::Rotation},
            BlockInfo{DirectionOf,
                      "direction_of",
                      C::Sensing,
                      S::Value,
                      "direction of %t",
                      false,
                      false,
                      {},
                      "Player",
                      "The direction the named object faces, in degrees"},
            BlockInfo{GoToScene,
                      "go_to_scene",
                      C::Events,
                      S::Stack,
                      "go to scene %t",
                      false,
                      false,
                      {},
                      "main",
                      "Stops this scene and starts another of the game's scenes (by its name in the Scenes window)"},
            BlockInfo{ShowScreenText,
                      "show_text_on_screen",
                      C::Screen,
                      S::Stack,
                      "show text %t on screen",
                      false,
                      false,
                      {},
                      "Hello!",
                      "Shows text on the game's screen (one thing per object: text, a picture or a bar)"},
            BlockInfo{
                ShowScreenPicture,
                "show_picture_on_screen",
                C::Screen,
                S::Stack,
                "show picture %t on screen",
                false,
                false,
                {},
                "crate",
                "Shows one of the game's pictures on the screen, by its name (or its path, assets/art/sprites/...)"},
            BlockInfo{WhenScreenClicked,
                      "when_clicked_on_screen",
                      C::Screen,
                      S::Hat,
                      "when clicked on screen",
                      false,
                      false,
                      {},
                      {},
                      "Runs the blocks under it each time the object's text, picture or bar is clicked (with the mouse "
                      "free)"},
            BlockInfo{LockMouse,
                      "lock_mouse",
                      C::Screen,
                      S::Stack,
                      "lock the mouse",
                      false,
                      false,
                      {},
                      {},
                      "Hides the pointer and lets the mouse turn the player, as a click in the game does"},
            BlockInfo{FreeMouse,
                      "free_mouse",
                      C::Screen,
                      S::Stack,
                      "free the mouse",
                      false,
                      false,
                      {},
                      {},
                      "Gives the pointer back, as Esc does: for clicking things on the screen (a menu, a button)"},
            BlockInfo{PlayAnimation,
                      "play_animation",
                      C::Looks,
                      S::Stack,
                      "play animation %t",
                      false,
                      false,
                      {},
                      "Walk",
                      "Plays an animation of the object's model over and over, by its name (Walk, Run, Idle)"},
            BlockInfo{PlayAnimationOnce,
                      "play_animation_once",
                      C::Looks,
                      S::Stack,
                      "play animation %t once",
                      false,
                      false,
                      {},
                      "Jump",
                      "Plays an animation of the object's model from its start to its end, then holds it there"},
            BlockInfo{StopAnimation,
                      "stop_animation",
                      C::Looks,
                      S::Stack,
                      "stop animation",
                      false,
                      false,
                      {},
                      {},
                      "Freezes the object's animation where it is"},
            BlockInfo{SetAnimationSpeed,
                      "set_animation_speed",
                      C::Looks,
                      S::Stack,
                      "set animation speed to %n",
                      false,
                      false,
                      {1.0f},
                      {},
                      "How fast the object's animation plays: 1 as made, 2 twice as fast, 0.5 half"},
            BlockInfo{SetTilt,
                      "set_tilt",
                      C::Motion,
                      S::Stack,
                      "set tilt to %n",
                      false,
                      false,
                      {0.0f},
                      {},
                      "Tips the object's front up (above 0) or down (below 0), in degrees; fly forward follows it",
                      Property::Rotation},
            BlockInfo{FlyForward,
                      "fly_forward",
                      C::Motion,
                      S::Stack,
                      "fly forward %n",
                      false,
                      false,
                      {1.0f},
                      {},
                      "Moves the object the way it faces and up or down along its tilt, like a thrown or shot thing",
                      Property::Position},
            BlockInfo{TiltOf,
                      "tilt_of",
                      C::Sensing,
                      S::Value,
                      "tilt of %t",
                      false,
                      false,
                      {},
                      "Player",
                      "How far the named object is tipped up, in degrees; for a player, how far it looks up or down"},
            BlockInfo{PlaySound,
                      "play_sound",
                      C::Sound,
                      S::Stack,
                      "play sound %t",
                      false,
                      false,
                      {},
                      "beep",
                      "Plays one of the game's sounds once, by its name (or its path, assets/audio/...), from where "
                      "the object is: nearer the camera is louder"},
            BlockInfo{
                PlayFootstep,
                "play_footstep",
                C::Sound,
                S::Stack,
                "play footstep",
                false,
                false,
                {},
                {},
                "Plays one step on what the object stands on: the step_<surface> sounds of the Surface set on its "
                "Rigid Body (grass, stone...), a different one each time. Nothing in the air"},
            // More Events, started through the event bus
            BlockInfo{WhenITouch,
                      "when_i_touch",
                      C::Events,
                      S::Hat,
                      "when I touch %t",
                      false,
                      false,
                      {},
                      "anything",
                      "Runs the blocks under it each time the object starts touching this object (or anything). Both "
                      "need a Rigid Body"},
            BlockInfo{WhenTimerEnds,
                      "when_timer_ends",
                      C::Events,
                      S::Hat,
                      "when timer %t ends",
                      false,
                      false,
                      {},
                      "alarm",
                      "Runs the blocks under it each time this timer runs out"},
            BlockInfo{StartTimer,
                      "start_timer",
                      C::Events,
                      S::Stack,
                      "start timer %t for %n seconds",
                      false,
                      false,
                      {3.0f},
                      "alarm",
                      "Counts down from the seconds given: when it runs out, \"when timer\" scripts for it start. "
                      "Starting it again starts it over"},
            BlockInfo{BroadcastWithValue,
                      "broadcast_with_value",
                      C::Events,
                      S::Stack,
                      "broadcast %t with %n",
                      false,
                      false,
                      {1.0f},
                      "score",
                      "Sends a message with a number: the \"when I receive\" scripts it starts read the number as "
                      "\"message value\""},
            BlockInfo{MessageValue,
                      "message_value",
                      C::Events,
                      S::Value,
                      "message value",
                      false,
                      false,
                      {},
                      {},
                      "The number sent with the message that started this script (0 if none)"},
            BlockInfo{SetViewAngle,
                      "set_view_angle",
                      C::Looks,
                      S::Stack,
                      "set view angle to %n degrees",
                      false,
                      false,
                      {70.0f},
                      {},
                      "How wide the Main camera sees, 10 to 120 degrees: smaller zooms in (aiming down the sights)"},
            BlockInfo{SetCameraHeight,
                      "set_camera_height",
                      C::Motion,
                      S::Stack,
                      "set camera height to %n",
                      false,
                      false,
                      {0.7f},
                      {},
                      "How high above the player its camera is: lower it to crouch. Needs a Player Controller"},
            BlockInfo{SetSensitivity,
                      "set_mouse_sensitivity",
                      C::Game,
                      S::Stack,
                      "set mouse sensitivity to %n",
                      false,
                      false,
                      {0.2f},
                      {},
                      "How far the view turns for each pixel the mouse moves, in degrees (0.2 is the usual); kept "
                      "for the rest of the game"},
            BlockInfo{SetVolume,
                      "set_volume",
                      C::Game,
                      S::Stack,
                      "set %t volume to %n",
                      false,
                      false,
                      {100.0f},
                      "master",
                      "The loudness of master, music or sfx, 0 to 100; kept for the rest of the game"},
            BlockInfo{SetFullscreen,
                      "set_fullscreen",
                      C::Game,
                      S::Stack,
                      "set fullscreen %t",
                      false,
                      false,
                      {},
                      "on",
                      "Fills the screen with the game (on) or puts it back in a window (off). Only the exported game "
                      "changes its window: in the editor it is remembered, not done"},
            BlockInfo{SetWindowSize,
                      "set_window_size",
                      C::Game,
                      S::Stack,
                      "set window size to %n by %n",
                      false,
                      false,
                      {1280.0f, 720.0f},
                      {},
                      "The size of the game's window, in pixels (its resolution). Only the exported game changes its "
                      "window"},
            BlockInfo{QuitGame,
                      "quit_game",
                      C::Game,
                      S::Stack,
                      "quit the game",
                      false,
                      false,
                      {},
                      {},
                      "Closes the game (in the editor: stops playing it)"},
            BlockInfo{SettingOf,
                      "setting",
                      C::Game,
                      S::Value,
                      "setting %t",
                      false,
                      false,
                      {},
                      "sensitivity",
                      "The value of a setting: sensitivity, master volume, music volume, sfx volume, fullscreen "
                      "(1 or 0), width, height, or one of your own from \"set setting\""},
            BlockInfo{SetSetting,
                      "set_setting",
                      C::Game,
                      S::Stack,
                      "set setting %t to %n",
                      false,
                      false,
                      {0.0f},
                      "difficulty",
                      "Remembers a number of your own under this name in every scene of the game: read it with "
                      "\"setting\""},
            BlockInfo{RayAhead,
                      "ray_ahead",
                      C::Sensing,
                      S::Stack,
                      "cast ray ahead up to %n from height %n",
                      false,
                      false,
                      {100.0f, 1.5f},
                      {},
                      "Looks along a straight line the way this object faces (a first-person player: where it looks). "
                      "Then ask \"ray hit\" and \"ray distance\" what it found"},
            BlockInfo{RayAt,
                      "ray_at",
                      C::Sensing,
                      S::Stack,
                      "cast ray at %t up to %n from height %n spread %n",
                      false,
                      false,
                      {100.0f, 1.5f, 0.0f},
                      "Player",
                      "Looks along a straight line towards an object, up to the spread's degrees off. Good for line of "
                      "sight (\"ray hit Player?\") and for aiming"},
            BlockInfo{RayHit,
                      "ray_hit",
                      C::Sensing,
                      S::Condition,
                      "ray hit %t?",
                      false,
                      false,
                      {},
                      "anything",
                      "True when the last ray this object cast stopped at that object (\"anything\": at all)"},
            BlockInfo{RayDistance,
                      "ray_distance",
                      C::Sensing,
                      S::Value,
                      "ray distance",
                      false,
                      false,
                      {},
                      {},
                      "How far the last ray this object cast went before it hit something (0 if it hit nothing)"},
            BlockInfo{TellRayHit,
                      "tell_ray_hit",
                      C::Events,
                      S::Stack,
                      "tell what the ray hit %t",
                      false,
                      false,
                      {},
                      "hit",
                      "Sends a message to only the object the last ray hit: its \"when I receive\" scripts start"},
            BlockInfo{ValueOf,
                      "value_of",
                      C::Variables,
                      S::Value,
                      "value of %t",
                      false,
                      false,
                      {},
                      "Player.score",
                      "A variable of any object, as Object.variable. \"hit.health\" is the health of what the last "
                      "ray hit"},
            BlockInfo{SetValueOf,
                      "set_value_of",
                      C::Variables,
                      S::Stack,
                      "set value of %t to %n",
                      false,
                      false,
                      {0.0f},
                      "Player.score",
                      "Sets a variable of any object, as Object.variable (\"hit.health\": of what the last ray hit)"},
            BlockInfo{ChangeValueOf,
                      "change_value_of",
                      C::Variables,
                      S::Stack,
                      "change value of %t by %n",
                      false,
                      false,
                      {1.0f},
                      "Player.score",
                      "Adds to a variable of any object, as Object.variable (\"hit.health\": of what the last ray "
                      "hit; a negative number takes away)"},
            BlockInfo{ValueExists,
                      "value_exists",
                      C::Variables,
                      S::Condition,
                      "%t exists?",
                      false,
                      false,
                      {},
                      "hit.health",
                      "True when that object has that variable: \"hit.health\" asks whether what the ray hit can be "
                      "hurt"},
            BlockInfo{WhenKeyReleased,
                      "when_key_released",
                      C::Events,
                      S::Hat,
                      "when %k key released",
                      false,
                      false,
                      {},
                      "Space",
                      "Runs once when the chosen key or mouse button is released"},

            BlockInfo{WhenKeyHeld,
                      "when_key_held",
                      C::Events,
                      S::Hat,
                      "while %k key held",
                      false,
                      false,
                      {},
                      "Space",
                      "Starts while held, once per simulation step after the previous run finishes"},

            BlockInfo{WhenCollision,
                      "when_collision",
                      C::Events,
                      S::Hat,
                      "on collision",
                      false,
                      false,
                      {},
                      {},
                      "Runs when this object's physics body begins touching another body"},

            BlockInfo{WhenTimer,
                      "when_timer",
                      C::Events,
                      S::Hat,
                      "every %n seconds",
                      false,
                      false,
                      {1.0f},
                      {},
                      "Repeats at this interval in simulation time; pauses with the game; runs never overlap"},

            BlockInfo{MoveBack,
                      "move_back",
                      C::Motion,
                      S::Stack,
                      "move back %n",
                      false,
                      false,
                      {1.0f},
                      {},
                      "Moves opposite the direction the object faces, multiplied by its speed",
                      Property::Position},
            BlockInfo{ApplyForce,
                      "apply_force",
                      C::Physics,
                      S::Stack,
                      "apply force x %n y %n z %n",
                      false,
                      false,
                      {0.0f, 10.0f, 0.0f},
                      {},
                      "Applies a world-space force for one simulation step to a Dynamic Rigid Body. Repeat for "
                      "sustained force"},

            BlockInfo{SetSpeed,
                      "set_speed",
                      C::Motion,
                      S::Stack,
                      "set speed multiplier to %n",
                      false,
                      false,
                      {1.0f},
                      {},
                      "Multiplies this object's relative movement: 1 normal, 2 double, 0 stopped. Resets on Play"},
            BlockInfo{Call,
                      "library_block",
                      C::Library,
                      S::Stack,
                      "%t",
                      false,
                      false,
                      {},
                      {},
                      "Runs the blocks saved under this name in My Blocks, as if they were here"},
        };

        static_assert(kBlocks.size() == static_cast<std::size_t>(Call) + 1, "one table entry per Op");
        static_assert(InOpOrder(kBlocks), "the table is in the order of Op: InfoOf looks a block up by its Op");

        // How often a field marker appears in a label
        std::size_t CountOf(std::string_view label, std::string_view marker)
        {
            std::size_t count = 0;
            for (std::size_t at = label.find(marker); at != std::string_view::npos; at = label.find(marker, at + 1))
            {
                ++count;
            }
            return count;
        }

        // Every list of blocks a block holds
        template <class B> auto Children(B& block)
        {
            return std::array{&block.inputs, &block.slots, &block.body, &block.elseBody};
        }

        void Collect(const std::vector<Block>& blocks, std::uint32_t& highest, std::size_t& count)
        {
            for (const Block& block : blocks)
            {
                highest = std::max(highest, block.id);
                count += block.id != 0 ? 1 : 0; // an empty slot or input is not a block
                for (const std::vector<Block>* inner : Children(block))
                {
                    Collect(*inner, highest, count);
                }
            }
        }

        const Block* Find(const std::vector<Block>& blocks, std::uint32_t id)
        {
            for (const Block& block : blocks)
            {
                if (block.id == id)
                {
                    return &block;
                }
                for (const std::vector<Block>* inner : Children(block))
                {
                    if (const Block* found = Find(*inner, id))
                    {
                        return found;
                    }
                }
            }
            return nullptr;
        }

        // Calls visit(block) for every block of blocks and inside them
        template <class List, class Visitor> void Visit(List& blocks, const Visitor& visit)
        {
            for (auto& block : blocks)
            {
                if (block.id != 0)
                {
                    visit(block);
                }
                for (auto* inner : Children(block))
                {
                    Visit(*inner, visit);
                }
            }
        }

        // The blocks of logic that match, with where they are
        template <class Match> std::vector<BlockAt> BlocksWhere(const LogicDesc& logic, const Match& match)
        {
            std::vector<BlockAt> found;
            for (std::size_t script = 0; script < logic.scripts.size(); ++script)
            {
                const std::vector<Block>& blocks = logic.scripts[script].blocks;
                const bool runs = !blocks.empty() && InfoOf(blocks.front().op).shape == BlockShape::Hat;
                Visit(blocks,
                      [&](const Block& block)
                      {
                          if (match(block))
                          {
                              found.push_back({&block, script, runs});
                          }
                      });
            }
            return found;
        }
    }

    std::span<const BlockInfo> AllBlocks()
    {
        return kBlocks;
    }

    const BlockInfo& InfoOf(Op op)
    {
        return kBlocks.at(static_cast<std::size_t>(op));
    }

    std::optional<Op> OpNamed(std::string_view name)
    {
        const auto found =
            std::find_if(kBlocks.begin(), kBlocks.end(), [name](const BlockInfo& info) { return info.name == name; });
        return found != kBlocks.end() ? std::optional(found->op) : std::nullopt;
    }

    std::string_view CategoryName(Category category)
    {
        switch (category)
        {
        case Category::Events: return "Events";
        case Category::Motion: return "Motion";
        case Category::Looks: return "Looks";
        case Category::Loops: return "Loops";
        case Category::Conditions: return "Conditions";
        case Category::Math: return "Math";
        case Category::Variables: return "Variables";
        case Category::Sensing: return "Sensing";
        case Category::Physics: return "Physics";
        case Category::Screen: return "Screen";
        case Category::Sound: return "Sound";
        case Category::Game: return "Game";
        case Category::Library: return "My Blocks";
        }
        return "?";
    }

    std::string_view CategoryHelp(Category category)
    {
        switch (category)
        {
        case Category::Events: return "When scripts start, and messages between objects";
        case Category::Motion: return "Move and turn the object";
        case Category::Looks: return "Show, hide, colour and size";
        case Category::Loops: return "Do something again and again, or wait";
        case Category::Conditions: return "Ifs: do something only when a condition is true";
        case Category::Math: return "Numbers made from other numbers: drop them into any number";
        case Category::Variables: return "Numbers the object remembers and changes";
        case Category::Sensing: return "Keys, the timer, and where things are";
        case Category::Physics:
            return "Jump, push and touch, for objects with a Rigid Body. With a Dynamic one, Motion blocks walk it: "
                   "walls stop it and it pushes what it walks into";
        case Category::Screen:
            return "What the game shows on its screen: each object can show text, a picture or a bar; and the "
                   "player's health bar, the crosshair and the mouse";
        case Category::Sound: return "Play the game's sounds: a swing, a hit, a footstep";
        case Category::Game:
            return "The player's settings and the game's window: sensitivity, volume, fullscreen, quit";
        case Category::Library:
            return "Your own blocks: a long stack saved as one block (right-click a block on the canvas > Save as "
                   "My Block)";
        }
        return "";
    }

    // Scratch's hues, so learners coming from it find the same groups, but spread apart so no two look alike (Events,
    // Loops and Variables were three near-identical oranges) and light enough for dark labels (5:1 or more)
    std::uint32_t CategoryColor(Category category)
    {
        switch (category)
        {
        case Category::Events: return 0xFFC21A;
        case Category::Motion: return 0x4C97FF;
        case Category::Looks: return 0x9B72FF;
        case Category::Loops: return 0xFF8A1F;
        case Category::Conditions: return 0xE466D6;
        case Category::Math: return 0x48C46B;
        case Category::Variables: return 0xFF5F57;
        case Category::Sensing: return 0x38C6E8;
        case Category::Physics: return 0x2BD9C0; // Theme::kPhysics
        case Category::Screen: return 0x9AD14B;
        case Category::Sound: return 0xD9A86C; // brass, apart from Events' gold and Loops' orange
        case Category::Game: return 0xAAB4C8;  // steel
        case Category::Library: return 0xE8578F;
        }
        return 0x888888;
    }

    std::size_t NumberCount(Op op)
    {
        return CountOf(InfoOf(op).label, "%n");
    }

    std::size_t SlotCount(Op op)
    {
        return CountOf(InfoOf(op).label, "%c");
    }

    bool HasKeyField(Op op)
    {
        return CountOf(InfoOf(op).label, "%k") > 0;
    }

    bool HasTextField(Op op)
    {
        return CountOf(InfoOf(op).label, "%t") > 0;
    }

    bool HasVariableField(Op op)
    {
        return CountOf(InfoOf(op).label, "%v") > 0;
    }

    Block MakeBlock(Op op, std::uint32_t id)
    {
        const BlockInfo& info = InfoOf(op);
        Block block{.id = id, .op = op, .numbers = info.numbers, .text = std::string(info.text)};
        block.inputs.resize(NumberCount(op)); // empty: id 0
        block.slots.resize(SlotCount(op));
        return block;
    }

    std::uint32_t NextBlockId(const LogicDesc& logic)
    {
        std::uint32_t highest = 0;
        std::size_t count = 0;
        for (const Script& script : logic.scripts)
        {
            Collect(script.blocks, highest, count);
        }
        return highest + 1;
    }

    void Renumber(std::vector<Block>& blocks, std::uint32_t& nextId)
    {
        for (Block& block : blocks)
        {
            if (block.id != 0)
            {
                block.id = nextId++;
            }
            for (std::vector<Block>* inner : Children(block))
            {
                Renumber(*inner, nextId);
            }
        }
    }

    const Block* FindBlock(const LogicDesc& logic, std::uint32_t id)
    {
        if (id == 0)
        {
            return nullptr;
        }
        for (const Script& script : logic.scripts)
        {
            if (const Block* found = Find(script.blocks, id))
            {
                return found;
            }
        }
        return nullptr;
    }

    const Block* FindBlock(const std::vector<Block>& blocks, std::uint32_t id)
    {
        return id == 0 ? nullptr : Find(blocks, id);
    }

    bool ContainsAny(const std::vector<Block>& blocks, const std::vector<std::uint32_t>& ids)
    {
        return std::any_of(ids.begin(), ids.end(),
                           [&blocks](std::uint32_t id) { return id != 0 && Find(blocks, id) != nullptr; });
    }

    namespace
    {
        std::size_t RemoveFrom(std::vector<Block>& list, const std::set<std::uint32_t>& ids, bool slots)
        {
            std::size_t removed = 0;
            for (std::size_t i = 0; i < list.size();)
            {
                Block& block = list[i];
                if (block.id != 0 && ids.contains(block.id))
                {
                    ++removed;
                    if (!slots)
                    {
                        list.erase(list.begin() + static_cast<std::ptrdiff_t>(i));
                        continue;
                    }
                    block = Block{};
                }
                else
                {
                    removed += RemoveFrom(block.body, ids, false) + RemoveFrom(block.elseBody, ids, false) +
                               RemoveFrom(block.slots, ids, true) + RemoveFrom(block.inputs, ids, true);
                }
                ++i;
            }
            return removed;
        }

        void CopyRuns(const std::vector<Block>& list, const std::set<std::uint32_t>& ids,
                      std::vector<std::vector<Block>>& runs)
        {
            bool inRun = false;
            for (const Block& block : list)
            {
                if (block.id != 0 && ids.contains(block.id))
                {
                    if (!inRun)
                    {
                        runs.emplace_back();
                    }
                    runs.back().push_back(block);
                    inRun = true;
                    continue;
                }
                inRun = false;
                CopyRuns(block.body, ids, runs);
                CopyRuns(block.elseBody, ids, runs);
            }
        }
    }

    std::size_t RemoveBlocks(LogicDesc& logic, const std::set<std::uint32_t>& ids)
    {
        std::size_t removed = 0;
        for (Script& script : logic.scripts)
        {
            removed += RemoveFrom(script.blocks, ids, false);
        }
        std::erase_if(logic.scripts, [](const Script& script) { return script.blocks.empty(); });
        return removed;
    }

    std::vector<std::vector<Block>> CopyBlocks(const LogicDesc& logic, const std::set<std::uint32_t>& ids)
    {
        std::vector<std::vector<Block>> runs;
        for (const Script& script : logic.scripts)
        {
            CopyRuns(script.blocks, ids, runs);
        }
        return runs;
    }

    std::size_t CountBlocks(const LogicDesc& logic)
    {
        std::uint32_t highest = 0;
        std::size_t count = 0;
        for (const Script& script : logic.scripts)
        {
            Collect(script.blocks, highest, count);
        }
        return count;
    }

    const Variable* FindVariable(const LogicDesc& logic, std::string_view name)
    {
        const auto found = std::find_if(logic.variables.begin(), logic.variables.end(),
                                        [name](const Variable& variable) { return variable.name == name; });
        return found != logic.variables.end() ? &*found : nullptr;
    }

    bool CanAddVariable(const LogicDesc& logic, std::string_view name)
    {
        const bool blank = std::all_of(name.begin(), name.end(),
                                       [](char c) { return std::isspace(static_cast<unsigned char>(c)) != 0; });
        return !name.empty() && name.size() <= 32 && !blank && !FindVariable(logic, name);
    }

    bool RenameVariable(LogicDesc& logic, std::string_view from, std::string_view to)
    {
        if (!CanAddVariable(logic, to))
        {
            return false;
        }
        auto variable = std::find_if(logic.variables.begin(), logic.variables.end(),
                                     [from](const Variable& each) { return each.name == from; });
        if (variable == logic.variables.end())
        {
            return false;
        }
        const std::string old(from); // from may point into the variable being renamed
        variable->name = std::string(to);
        for (Script& script : logic.scripts)
        {
            Visit(script.blocks,
                  [&old, to](Block& block)
                  {
                      if (HasVariableField(block.op) && block.text == old)
                      {
                          block.text = std::string(to);
                      }
                  });
        }
        return true;
    }

    std::size_t CountUses(const LogicDesc& logic, std::string_view name)
    {
        std::size_t uses = 0;
        for (const Script& script : logic.scripts)
        {
            Visit(script.blocks,
                  [name, &uses](const Block& block)
                  {
                      if (HasVariableField(block.op) && block.text == name)
                      {
                          ++uses;
                      }
                  });
        }
        return uses;
    }

    std::optional<Property> PropertyOf(Op op)
    {
        return InfoOf(op).writes;
    }

    bool WritesVariable(Op op)
    {
        return op == SetVariable || op == ChangeVariable;
    }

    std::optional<Property> PropertyOfField(std::string_view component, std::string_view field)
    {
        const auto found =
            std::find_if(kProperties.begin(), kProperties.end(),
                         [&](const PropertyInfo& info)
                         {
                             return !info.component.empty() && info.component == component &&
                                    std::find(info.fields.begin(), info.fields.end(), field) != info.fields.end();
                         });
        if (found == kProperties.end())
        {
            return std::nullopt; // Visible and Says are not saved, and the other fields no block writes
        }
        return found->property;
    }

    std::vector<BlockAt> WritersOf(const LogicDesc& logic, std::string_view variable)
    {
        return BlocksWhere(logic, [variable](const Block& block)
                           { return WritesVariable(block.op) && block.text == variable; });
    }

    std::vector<BlockAt> WritersOf(const LogicDesc& logic, Property property)
    {
        return BlocksWhere(logic, [property](const Block& block) { return PropertyOf(block.op) == property; });
    }

    bool CanInsert(const std::vector<Block>& list, std::size_t index, const std::vector<Block>& stack, bool topLevel)
    {
        if (stack.empty() || index > list.size())
        {
            return false;
        }
        const BlockShape first = InfoOf(stack.front().op).shape;
        const BlockShape last = InfoOf(stack.back().op).shape;
        if (first == BlockShape::Condition || first == BlockShape::Value)
        {
            return false; // conditions and values only go into slots
        }
        if (first == BlockShape::Hat &&
            (!topLevel || index != 0 || (!list.empty() && InfoOf(list.front().op).shape == BlockShape::Hat)))
        {
            return false; // a hat only starts a script
        }
        if (index > 0 && InfoOf(list[index - 1].op).shape == BlockShape::Cap)
        {
            return false; // nothing goes under a cap
        }
        if (index < list.size() && last == BlockShape::Cap)
        {
            return false; // a cap can only be last
        }
        if (index == 0 && !list.empty() && InfoOf(list.front().op).shape == BlockShape::Hat)
        {
            return false; // nothing goes above a hat
        }
        return true;
    }

    bool CanFillSlot(const Block& slot, const Block& block)
    {
        return slot.id == 0 && block.id != 0 && InfoOf(block.op).shape == BlockShape::Condition;
    }

    bool CanFillInput(const Block& input, const Block& block)
    {
        return input.id == 0 && block.id != 0 && InfoOf(block.op).shape == BlockShape::Value;
    }

    std::vector<Block> TakeStack(std::vector<Block>& list, std::size_t index)
    {
        if (index >= list.size())
        {
            throw std::out_of_range("TakeStack: no block at that index");
        }
        std::vector<Block> taken(std::make_move_iterator(list.begin() + static_cast<std::ptrdiff_t>(index)),
                                 std::make_move_iterator(list.end()));
        list.erase(list.begin() + static_cast<std::ptrdiff_t>(index), list.end());
        return taken;
    }

    // 1 rather than 1.000000, 0.5 rather than 0.500000
    std::string FormatNumber(double value)
    {
        if (std::isnan(value))
        {
            return "not a number";
        }
        if (std::isinf(value))
        {
            return value > 0 ? "infinity" : "-infinity";
        }
        if (value == std::round(value) && std::abs(value) < 1e9)
        {
            return std::to_string(static_cast<long long>(value)); // -0 too reads 0
        }
        std::string text = std::format("{:.3f}", value);
        text.erase(text.find_last_not_of('0') + 1);
        if (text.back() == '.')
        {
            text.pop_back();
        }
        return text == "-0" ? "0" : text; // -0.0001 rounds to 0, not -0
    }

    std::string ValueText(VariableKind kind, double value)
    {
        if (kind == VariableKind::TrueFalse)
        {
            return value != 0.0 ? "true" : "false";
        }
        return FormatNumber(value);
    }

    std::string Describe(const Block& block)
    {
        const std::string_view label = InfoOf(block.op).label;
        std::string text;
        std::size_t number = 0;
        std::size_t slot = 0;
        for (std::size_t i = 0; i < label.size(); ++i)
        {
            if (label[i] != '%' || i + 1 >= label.size())
            {
                text += label[i];
                continue;
            }
            switch (label[++i])
            {
            case 'n':
            {
                const Block* input = number < block.inputs.size() ? &block.inputs[number] : nullptr;
                if (input && input->id != 0)
                {
                    text += "(" + Describe(*input) + ")";
                }
                else
                {
                    text += FormatNumber(number < block.numbers.size() ? block.numbers[number] : 0.0f);
                }
                ++number;
                break;
            }
            case 'k':
            case 'v': text += block.text; break;
            case 't':
                // A library block reads as its name; other text (a message, an object's name) in quotes
                text += block.op == Op::Call ? block.text : "\"" + block.text + "\"";
                break;
            case 'c':
            {
                const Block* condition = slot < block.slots.size() ? &block.slots[slot] : nullptr;
                text += condition && condition->id != 0 ? "(" + Describe(*condition) + ")" : "( )";
                ++slot;
                break;
            }
            default: text += '%'; text += label[i];
            }
        }
        return text;
    }
}
