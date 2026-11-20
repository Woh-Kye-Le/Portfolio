#include "editor/camera/View2D.hpp"

#include <gtest/gtest.h>

#include <cmath>

using Editor::Camera::View2D;

namespace
{
    constexpr float kEpsilon = 1e-3f;

    void ExpectNear(const glm::vec2& actual, const glm::vec2& expected, float epsilon = kEpsilon)
    {
        EXPECT_NEAR(actual.x, expected.x, epsilon);
        EXPECT_NEAR(actual.y, expected.y, epsilon);
    }

    // A view placed somewhere inside a window, as a docked Viewport is
    View2D MakeView()
    {
        View2D view;
        view.SetRect({100.0f, 40.0f}, {800.0f, 600.0f});
        return view;
    }
}

// Whatever point is set as the centre shows up in the middle of the view rect.
TEST(View2D, CenterOfTheViewShowsTheCenterPoint)
{
    View2D view = MakeView();
    ExpectNear(view.WorldToScreen({0.0f, 0.0f}), {500.0f, 340.0f});

    view.SetCenter({3.0f, -2.0f});
    ExpectNear(view.WorldToScreen({3.0f, -2.0f}), view.RectCenter());
}

// World +Y goes up the screen and +X to the right, one zoom-worth of pixels per unit.
TEST(View2D, WorldYGoesUpTheScreen)
{
    const View2D view = MakeView();
    const glm::vec2 origin = view.WorldToScreen({0.0f, 0.0f});
    const glm::vec2 up = view.WorldToScreen({0.0f, 1.0f});
    const glm::vec2 right = view.WorldToScreen({1.0f, 0.0f});

    EXPECT_NEAR(up.y, origin.y - view.Zoom(), kEpsilon);
    EXPECT_NEAR(right.x, origin.x + view.Zoom(), kEpsilon);
}

// Converting to screen and back gives the same point at any pan and zoom.
TEST(View2D, ScreenToWorldUndoesWorldToScreen)
{
    View2D view = MakeView();
    view.SetCenter({12.5f, -7.25f});
    view.SetZoom(33.0f);
    for (const glm::vec2 world : {glm::vec2(0.0f), glm::vec2(1.0f, 2.0f), glm::vec2(-40.0f, 17.5f)})
    {
        ExpectNear(view.ScreenToWorld(view.WorldToScreen(world)), world);
    }
}

// Panning moves the world by exactly the mouse movement.
TEST(View2D, PanMovesTheWorldWithTheMouse)
{
    View2D view = MakeView();
    const glm::vec2 before = view.WorldToScreen({2.0f, 1.0f});

    view.Pan({30.0f, -12.0f});

    ExpectNear(view.WorldToScreen({2.0f, 1.0f}), before + glm::vec2(30.0f, -12.0f));
}

// Zooming in or out with the wheel keeps the point under the mouse in the same place.
TEST(View2D, ZoomKeepsThePointUnderTheMouseInPlace)
{
    View2D view = MakeView();
    view.SetCenter({1.0f, 1.0f});
    const glm::vec2 mouse(720.0f, 150.0f);
    const glm::vec2 underMouse = view.ScreenToWorld(mouse);

    view.ZoomAt(mouse, 2.5f);
    ExpectNear(view.ScreenToWorld(mouse), underMouse);
    view.ZoomAt(mouse, 0.1f);
    ExpectNear(view.ScreenToWorld(mouse), underMouse);
}

// Zoom stops at kMinZoom / kMaxZoom.
TEST(View2D, ZoomStaysInRange)
{
    View2D view = MakeView();
    view.ZoomAt(view.RectCenter(), 1e6f);
    EXPECT_FLOAT_EQ(view.Zoom(), View2D::kMaxZoom);
    view.ZoomAt(view.RectCenter(), 1e-6f);
    EXPECT_FLOAT_EQ(view.Zoom(), View2D::kMinZoom);
}

// Resizing the Viewport keeps the same point in the middle and does not change the zoom.
TEST(View2D, ResizingKeepsTheCenterPointCentered)
{
    View2D view = MakeView();
    view.SetCenter({4.0f, 3.0f});

    view.SetRect({0.0f, 0.0f}, {1920.0f, 400.0f});

    ExpectNear(view.WorldToScreen({4.0f, 3.0f}), {960.0f, 200.0f});
    EXPECT_FLOAT_EQ(view.Zoom(), View2D::kDefaultZoom); // resizing does not change the scale
}

// The grid spacing is always a nice number (1, 2 or 5 times a power of ten) and never closer than the minimum pixel
// gap.
TEST(View2D, GridStepIsOneTwoOrFiveTimesAPowerOfTen)
{
    View2D view = MakeView();
    view.SetZoom(48.0f);
    EXPECT_FLOAT_EQ(view.GridStep(16.0f), 0.5f); // 0.5 units = 24 pixels; 0.2 would be under 16
    view.SetZoom(10.0f);
    EXPECT_FLOAT_EQ(view.GridStep(16.0f), 2.0f);
    view.SetZoom(1000.0f);
    EXPECT_NEAR(view.GridStep(16.0f), 0.02f, 1e-6f);

    for (const float zoom : {3.0f, 17.0f, 48.0f, 250.0f, 3999.0f})
    {
        view.SetZoom(zoom);
        const float step = view.GridStep(16.0f);
        EXPECT_GE(step * zoom, 16.0f - kEpsilon) << "zoom " << zoom;
        EXPECT_LT(step * zoom, 16.0f * 2.5f + kEpsilon) << "zoom " << zoom; // never coarser than needed
    }
}

// However far you zoom or pan, one grid line always sits exactly on each axis line.
TEST(View2D, AGridLinePassesThroughTheAxesAtAnyZoomAndPan)
{
    // The grid is drawn at whole multiples of the step through WorldToScreen, like the axes, so line 0 is the axis
    View2D view = MakeView();
    for (const float zoom : {5.0f, 48.0f, 731.0f})
    {
        for (const glm::vec2 center : {glm::vec2(0.0f), glm::vec2(0.37f, -12.9f), glm::vec2(-250.5f, 99.1f)})
        {
            view.SetZoom(zoom);
            view.SetCenter(center);
            const float step = view.GridStep(16.0f);
            const glm::vec2 axes = view.WorldToScreen({0.0f, 0.0f});
            for (int i = -3; i <= 3; ++i)
            {
                const float gridX = view.WorldToScreen({static_cast<float>(i) * step, 0.0f}).x;
                EXPECT_NEAR(gridX - axes.x, static_cast<float>(i) * step * zoom, 1e-2f);
            }
        }
    }
}

// Snap rounds each axis to the nearest multiple of the step.
TEST(View2D, SnapGoesToTheNearestGridPoint)
{
    ExpectNear(View2D::Snap({1.26f, -0.74f}, 0.5f), {1.5f, -0.5f});
    ExpectNear(View2D::Snap({-3.0f, 7.9f}, 2.0f), {-4.0f, 8.0f});
    ExpectNear(View2D::Snap({0.049f, 0.051f}, 0.1f), {0.0f, 0.1f});
}

// A snapped point ends up exactly on a drawn grid line, not just near one.
TEST(View2D, SnappedPointsLandOnGridLines)
{
    View2D view = MakeView();
    view.SetZoom(64.0f);
    view.SetCenter({0.3f, -1.7f});
    const float step = view.GridStep(16.0f);

    const glm::vec2 snapped = View2D::Snap(view.ScreenToWorld({612.0f, 97.0f}), step);
    const glm::vec2 onScreen = view.WorldToScreen(snapped);
    const glm::vec2 lineX = view.WorldToScreen({std::round(snapped.x / step) * step, 0.0f});
    const glm::vec2 lineY = view.WorldToScreen({0.0f, std::round(snapped.y / step) * step});

    EXPECT_NEAR(onScreen.x, lineX.x, kEpsilon);
    EXPECT_NEAR(onScreen.y, lineY.y, kEpsilon);
}

// The visible world area is the view size divided by the zoom, centred on the centre point.
TEST(View2D, VisibleAreaMatchesTheRect)
{
    View2D view = MakeView();
    view.SetZoom(50.0f);
    ExpectNear(view.VisibleMin(), {-8.0f, -6.0f});
    ExpectNear(view.VisibleMax(), {8.0f, 6.0f});
}
