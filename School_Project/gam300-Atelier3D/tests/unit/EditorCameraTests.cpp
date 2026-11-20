#include "editor/camera/EditorCamera.hpp"

#include <gtest/gtest.h>

using Editor::Camera::EditorCamera;
using Engine::Graphics::Camera3D;
using namespace Engine::Input;

namespace
{
    constexpr float kTolerance = 1e-3f;

    // A camera at the origin looking down -Z (yaw 0, pitch 0)
    Camera3D StartCamera()
    {
        Camera3D camera;
        camera.SetPosition(glm::vec3(0.0f));
        camera.SetOrientation(glm::vec3(0.0f, 0.0f, -1.0f));
        return camera;
    }

    // An editor camera attached to StartCamera, moving immediately (no easing) so results are exact
    EditorCamera Attached(Camera3D& camera)
    {
        EditorCamera editor;
        editor.SetSmoothing(false);
        editor.Attach(camera);
        return editor;
    }

    // One camera update from this input, with the default bindings unless others are given
    void Step(EditorCamera& editor, double deltaSeconds, const InputSystem& input, Camera3D& camera,
              const InputActions& actions = InputActions::Defaults())
    {
        editor.Update(deltaSeconds, GameInput(input, actions), camera);
    }

    void ExpectNear(const glm::vec3& actual, const glm::vec3& expected)
    {
        EXPECT_NEAR(actual.x, expected.x, kTolerance);
        EXPECT_NEAR(actual.y, expected.y, kTolerance);
        EXPECT_NEAR(actual.z, expected.z, kTolerance);
    }
}

// Attaching picks up the camera's position and direction (looking down +X = yaw 90), and the orbit pivot ends up
// straight ahead of it.
TEST(EditorCamera, AttachesToTheCameraItIsGiven)
{
    Camera3D camera;
    camera.SetPosition(glm::vec3(1.0f, 2.0f, 3.0f));
    camera.SetOrientation(glm::vec3(1.0f, 0.0f, 0.0f)); // looking down +X

    EditorCamera editor;
    editor.Attach(camera);

    EXPECT_TRUE(editor.IsAttached());
    ExpectNear(editor.Position(), {1.0f, 2.0f, 3.0f});
    EXPECT_NEAR(editor.YawDegrees(), 90.0f, kTolerance);
    EXPECT_NEAR(editor.PitchDegrees(), 0.0f, kTolerance);
    // The pivot it orbits is straight ahead
    ExpectNear(editor.Pivot(), glm::vec3(1.0f, 2.0f, 3.0f) + glm::vec3(EditorCamera::kStartDistance, 0.0f, 0.0f));
}

// A frame with no input must not move or turn the camera.
TEST(EditorCamera, NoInputKeepsTheCameraWhereItIs)
{
    Camera3D camera = StartCamera();
    EditorCamera editor = Attached(camera);
    const InputSystem input;

    Step(editor, 1.0, input, camera);

    ExpectNear(camera.GetPosition(), glm::vec3(0.0f));
    EXPECT_NEAR(camera.GetForward().z, -1.0f, kTolerance);
}

// W on its own is the Move gizmo shortcut, so the camera only flies once right mouse is held too.
TEST(EditorCamera, WasdOnlyFliesWhileTheRightMouseButtonIsHeld)
{
    Camera3D camera = StartCamera();
    EditorCamera editor = Attached(camera);
    InputSystem input;
    input.SetKey(Key_W, KeyState::Down);

    Step(editor, 1.0, input, camera); // W alone is left to the editor (the Move tool)
    ExpectNear(camera.GetPosition(), glm::vec3(0.0f));
    EXPECT_FALSE(editor.IsFlying());

    input.SetMouseButton(Mouse_Right, KeyState::Down);
    Step(editor, 1.0, input, camera);
    EXPECT_TRUE(editor.IsFlying());
    EXPECT_NEAR(camera.GetPosition().z, -editor.Speed(), kTolerance);
}

// D moves right and E moves up, one Speed() per second each.
TEST(EditorCamera, StrafeAndVertical)
{
    Camera3D camera = StartCamera();
    EditorCamera editor = Attached(camera);
    InputSystem input;
    input.SetMouseButton(Mouse_Right, KeyState::Down);
    input.SetKey(Key_D, KeyState::Down);
    input.SetKey(Key_E, KeyState::Down);

    Step(editor, 1.0, input, camera);

    EXPECT_NEAR(camera.GetPosition().x, editor.Speed(), kTolerance); // right
    EXPECT_NEAR(camera.GetPosition().y, editor.Speed(), kTolerance); // up
}

// Holding Shift multiplies the fly speed by kFastMultiplier.
TEST(EditorCamera, ShiftFliesFaster)
{
    Camera3D camera = StartCamera();
    EditorCamera editor = Attached(camera);
    InputSystem input;
    input.SetMouseButton(Mouse_Right, KeyState::Down);
    input.SetKey(Key_W, KeyState::Down);
    input.SetKey(Key_LeftShift, KeyState::Down);

    Step(editor, 1.0, input, camera);

    EXPECT_NEAR(camera.GetPosition().z, -editor.Speed() * EditorCamera::kFastMultiplier, kTolerance);
}

// After binding MoveForward to Up, W does nothing and Up flies forward instead.
TEST(EditorCamera, RemappedActionsUseTheNewKeys)
{
    Camera3D camera = StartCamera();
    EditorCamera editor = Attached(camera);
    InputActions actions = InputActions::Defaults();
    actions.Bind(Action::MoveForward, {*ParseBinding("Up")});

    InputSystem input;
    input.SetMouseButton(Mouse_Right, KeyState::Down);
    input.SetKey(Key_W, KeyState::Down); // no longer bound
    Step(editor, 1.0, input, camera, actions);
    EXPECT_NEAR(camera.GetPosition().z, 0.0f, kTolerance);

    input.SetKey(Key_W, KeyState::Up);
    input.SetKey(Key_Up, KeyState::Down);
    Step(editor, 1.0, input, camera, actions);
    EXPECT_NEAR(camera.GetPosition().z, -editor.Speed(), kTolerance);
}

// Right drag turns the camera on the spot (yaw changes, position stays).
TEST(EditorCamera, RightMouseDragLooksAroundFromWhereTheCameraIs)
{
    Camera3D camera = StartCamera();
    EditorCamera editor = Attached(camera);
    InputSystem input;
    input.SetMouseButton(Mouse_Right, KeyState::Down);
    input.SetMousePosition(100.0f, 0.0f); // moved 100 px right this frame

    Step(editor, 0.016, input, camera);

    EXPECT_NEAR(editor.YawDegrees(), 100.0f * EditorCamera::kLookDegreesPerPixel, kTolerance);
    ExpectNear(camera.GetPosition(), glm::vec3(0.0f)); // turned in place
}

// Alt + left drag orbits: 300 px = 90 degrees round the pivot, staying the same distance away.
TEST(EditorCamera, AltLeftDragOrbitsAroundThePivot)
{
    Camera3D camera = StartCamera();
    EditorCamera editor = Attached(camera);
    const glm::vec3 pivot = editor.Pivot();
    InputSystem input;
    input.SetKey(Key_LeftAlt, KeyState::Down);
    input.SetMouseButton(Mouse_Left, KeyState::Down);
    input.SetMousePosition(300.0f, 0.0f); // 300 px * 0.3 = 90 degrees round

    Step(editor, 0.016, input, camera);

    ExpectNear(editor.Pivot(), pivot); // the pivot stays
    EXPECT_NEAR(glm::length(camera.GetPosition() - pivot), EditorCamera::kStartDistance, kTolerance);
    EXPECT_NEAR(editor.YawDegrees(), 90.0f, kTolerance);
    // Now looking along +X at the pivot, from its -X side
    ExpectNear(camera.GetPosition(), pivot - glm::vec3(EditorCamera::kStartDistance, 0.0f, 0.0f));
}

// A plain left drag belongs to selection and the gizmos, so the camera ignores it.
TEST(EditorCamera, LeftDragWithoutAltDoesNotMoveTheCamera)
{
    Camera3D camera = StartCamera();
    EditorCamera editor = Attached(camera);
    InputSystem input;
    input.SetMouseButton(Mouse_Left, KeyState::Down); // left drag is for gizmos and selection
    input.SetMousePosition(100.0f, 50.0f);

    Step(editor, 0.016, input, camera);

    EXPECT_NEAR(editor.YawDegrees(), 0.0f, kTolerance);
    ExpectNear(camera.GetPosition(), glm::vec3(0.0f));
}

// Middle drag slides the camera and its pivot together without turning.
TEST(EditorCamera, MiddleDragPansTheViewAndThePivot)
{
    Camera3D camera = StartCamera();
    EditorCamera editor = Attached(camera);
    editor.SetViewportHeight(500.0f);
    const glm::vec3 pivot = editor.Pivot();
    InputSystem input;
    input.SetMouseButton(Mouse_Middle, KeyState::Down);
    input.SetMousePosition(50.0f, 0.0f); // dragged right: the scene follows, so the view moves left

    Step(editor, 0.016, input, camera);

    EXPECT_LT(camera.GetPosition().x, 0.0f);
    EXPECT_NEAR(editor.Pivot().x - pivot.x, camera.GetPosition().x, kTolerance); // both moved together
    EXPECT_NEAR(editor.YawDegrees(), 0.0f, kTolerance);
}

// Each scroll step multiplies the distance to the pivot by kZoomPerScrollStep; the pivot stays put.
TEST(EditorCamera, ScrollZoomsTowardsThePivot)
{
    Camera3D camera = StartCamera();
    EditorCamera editor = Attached(camera);
    const glm::vec3 pivot = editor.Pivot();
    InputSystem input;
    input.SetScrollDelta(2.0f);

    Step(editor, 0.016, input, camera);

    ExpectNear(editor.Pivot(), pivot);
    EXPECT_NEAR(editor.Distance(), EditorCamera::kStartDistance * std::pow(EditorCamera::kZoomPerScrollStep, 2.0f),
                kTolerance);
}

// Zooming in past kMinDistance keeps moving the camera forward instead of getting stuck.
TEST(EditorCamera, ScrollingInPastTheMinimumMovesForward)
{
    Camera3D camera = StartCamera();
    EditorCamera editor = Attached(camera);
    InputSystem input;
    input.SetScrollDelta(100.0f);

    Step(editor, 0.016, input, camera);

    EXPECT_NEAR(editor.Distance(), EditorCamera::kMinDistance, kTolerance);
    EXPECT_LT(camera.GetPosition().z, 0.0f); // kept going forward instead of stopping
}

// While flying, the wheel changes the fly speed instead of zooming, and the speed stays in its limits.
TEST(EditorCamera, ScrollWhileFlyingChangesTheSpeedWithinLimits)
{
    Camera3D camera = StartCamera();
    EditorCamera editor = Attached(camera);
    const float start = editor.Speed();

    InputSystem input;
    input.SetMouseButton(Mouse_Right, KeyState::Down);
    input.SetScrollDelta(1.0f);
    Step(editor, 0.016, input, camera);
    EXPECT_GT(editor.Speed(), start);
    EXPECT_NEAR(editor.Distance(), EditorCamera::kStartDistance, kTolerance); // no zoom while flying

    input.SetScrollDelta(-100.0f);
    Step(editor, 0.016, input, camera);
    EXPECT_NEAR(editor.Speed(), EditorCamera::kMinSpeed, kTolerance);
}

// With the mouse over an editor panel, dragging and scrolling must not move the camera.
TEST(EditorCamera, MouseIsIgnoredWhileItIsOverEditorUi)
{
    Camera3D camera = StartCamera();
    EditorCamera editor = Attached(camera);
    InputSystem input;
    input.SetMouseButton(Mouse_Right, KeyState::Down);
    input.SetMousePosition(100.0f, 0.0f);
    input.SetScrollDelta(3.0f);
    input.SetCaptureMouse(true);

    Step(editor, 0.016, input, camera);

    EXPECT_NEAR(editor.YawDegrees(), 0.0f, kTolerance);
    EXPECT_NEAR(editor.Distance(), EditorCamera::kStartDistance, kTolerance);
}

// A huge drag upwards stops at kMaxPitch instead of flipping over.
TEST(EditorCamera, PitchIsClamped)
{
    Camera3D camera = StartCamera();
    EditorCamera editor = Attached(camera);
    InputSystem input;
    input.SetMouseButton(Mouse_Right, KeyState::Down);
    input.SetMousePosition(0.0f, -10000.0f); // a huge drag upwards

    Step(editor, 0.016, input, camera);

    EXPECT_NEAR(editor.PitchDegrees(), EditorCamera::kMaxPitch, kTolerance);
}

// Typing W into a text box must not fly the camera.
TEST(EditorCamera, KeysAreIgnoredWhileTypingInTheUi)
{
    Camera3D camera = StartCamera();
    EditorCamera editor = Attached(camera);
    InputSystem input;
    input.SetMouseButton(Mouse_Right, KeyState::Down);
    input.SetKey(Key_W, KeyState::Down);
    input.SetCaptureInput(true);

    Step(editor, 1.0, input, camera);

    EXPECT_NEAR(camera.GetPosition().z, 0.0f, kTolerance);
}

// F / Focus moves the pivot onto the point and backs off far enough to see it, looking at it.
TEST(EditorCamera, FocusFramesAPoint)
{
    Camera3D camera = StartCamera();
    EditorCamera editor = Attached(camera);
    const InputSystem input;

    editor.Focus(glm::vec3(10.0f, 2.0f, -4.0f), 1.0f);
    Step(editor, 0.016, input, camera);

    ExpectNear(editor.Pivot(), {10.0f, 2.0f, -4.0f});
    EXPECT_GT(editor.Distance(), 1.0f); // far enough to see all of it
    const glm::vec3 toPoint = glm::normalize(glm::vec3(10.0f, 2.0f, -4.0f) - camera.GetPosition());
    ExpectNear(camera.GetForward(), toPoint);
}

// The view cube's Right and Top views point the camera down -X and straight down.
TEST(EditorCamera, SetViewLooksFromTheGivenSide)
{
    Camera3D camera = StartCamera();
    EditorCamera editor = Attached(camera);
    const InputSystem input;

    editor.SetView(-90.0f, 0.0f); // Right: from +X, looking down -X
    Step(editor, 0.016, input, camera);
    ExpectNear(camera.GetForward(), {-1.0f, 0.0f, 0.0f});

    editor.SetView(0.0f, -90.0f); // Top: looking down
    Step(editor, 0.016, input, camera);
    EXPECT_LT(camera.GetForward().y, -0.99f);
    EXPECT_NEAR(editor.PitchDegrees(), -EditorCamera::kMaxPitch, kTolerance);
}

// Going to yaw 350 from 0 turns -10 degrees, not +350.
TEST(EditorCamera, SetViewTakesTheShortWayRound)
{
    Camera3D camera = StartCamera();
    EditorCamera editor = Attached(camera);
    const InputSystem input;

    editor.SetView(350.0f, 0.0f); // the same direction as -10
    Step(editor, 0.016, input, camera);

    EXPECT_NEAR(editor.YawDegrees(), -10.0f, kTolerance);
}

// With smoothing on, the camera is partway there after one frame and arrives after about 2 seconds.
TEST(EditorCamera, SmoothingEasesTowardsTheTarget)
{
    Camera3D camera = StartCamera();
    EditorCamera editor;
    editor.Attach(camera); // smoothing on (the default)
    const InputSystem input;

    editor.SetView(90.0f, 0.0f);
    Step(editor, 0.016, input, camera);
    EXPECT_GT(editor.YawDegrees(), 0.0f);  // on its way...
    EXPECT_LT(editor.YawDegrees(), 90.0f); // ...not there yet

    for (int i = 0; i < 120; ++i)
    {
        Step(editor, 0.016, input, camera);
    }
    EXPECT_NEAR(editor.YawDegrees(), 90.0f, 0.01f); // arrived
}

// Orbit() (used when dragging the view cube) turns around the pivot the same way Alt + drag does.
TEST(EditorCamera, OrbitTurnsAroundThePivotLikeAltDrag)
{
    Camera3D camera = StartCamera();
    EditorCamera editor = Attached(camera);
    const glm::vec3 pivot = editor.Pivot();
    const InputSystem input;

    editor.Orbit(300.0f, 0.0f); // what dragging the view cube does
    Step(editor, 0.016, input, camera);

    ExpectNear(editor.Pivot(), pivot);
    EXPECT_NEAR(editor.YawDegrees(), 300.0f * EditorCamera::kOrbitDegreesPerPixel, kTolerance);
    EXPECT_NEAR(glm::length(camera.GetPosition() - pivot), EditorCamera::kStartDistance, kTolerance);
}
