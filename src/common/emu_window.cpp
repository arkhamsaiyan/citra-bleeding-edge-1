// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <cmath>
#include "common/assert.h"

#include "emu_window.h"
#include "input_core/input_core.h"
#include "video_core/video_core.h"

/**
 * Check if the given x/y coordinates are within the touchpad specified by the framebuffer layout
 * @param layout FramebufferLayout object describing the framebuffer size and screen positions
 * @param framebuffer_x Framebuffer x-coordinate to check
 * @param framebuffer_y Framebuffer y-coordinate to check
 * @return True if the coordinates are within the touchpad, otherwise false
 */
static bool IsWithinTouchscreen(const Layout::FramebufferLayout& layout, unsigned framebuffer_x,
                                unsigned framebuffer_y) {
    return (
        framebuffer_y >= layout.bottom_screen.top && framebuffer_y < layout.bottom_screen.bottom &&
        framebuffer_x >= layout.bottom_screen.left && framebuffer_x < layout.bottom_screen.right);
}

std::tuple<unsigned, unsigned> EmuWindow::ClipToTouchScreen(unsigned new_x, unsigned new_y) {
    new_x = std::max(new_x, framebuffer_layout.bottom_screen.left);
    new_x = std::min(new_x, framebuffer_layout.bottom_screen.right - 1);

    new_y = std::max(new_y, framebuffer_layout.bottom_screen.top);
    new_y = std::min(new_y, framebuffer_layout.bottom_screen.bottom - 1);

    return std::make_tuple(new_x, new_y);
}

void EmuWindow::TouchPressed(unsigned framebuffer_x, unsigned framebuffer_y) {
    if (!IsWithinTouchscreen(framebuffer_layout, framebuffer_x, framebuffer_y))
        return;

    int touch_x = VideoCore::kScreenBottomWidth *
                  (framebuffer_x - framebuffer_layout.bottom_screen.left) /
                  (framebuffer_layout.bottom_screen.right - framebuffer_layout.bottom_screen.left);
    int touch_y = VideoCore::kScreenBottomHeight *
                  (framebuffer_y - framebuffer_layout.bottom_screen.top) /
                  (framebuffer_layout.bottom_screen.bottom - framebuffer_layout.bottom_screen.top);
    touch_pressed = true;
    InputCore::SetTouchState(std::make_tuple(touch_x, touch_y, true));
    auto pad_state = InputCore::GetPadState();
    pad_state.touch.Assign(1);
    InputCore::SetPadState(pad_state);
}

void EmuWindow::TouchReleased() {
    touch_pressed = false;
    InputCore::SetTouchState(std::make_tuple(0, 0, false));
    auto pad_state = InputCore::GetPadState();
    pad_state.touch.Assign(0);
    InputCore::SetPadState(pad_state);
}

void EmuWindow::TouchMoved(unsigned framebuffer_x, unsigned framebuffer_y) {
    if (!touch_pressed)
        return;

    if (!IsWithinTouchscreen(framebuffer_layout, framebuffer_x, framebuffer_y))
        std::tie(framebuffer_x, framebuffer_y) = ClipToTouchScreen(framebuffer_x, framebuffer_y);

    TouchPressed(framebuffer_x, framebuffer_y);
}

void EmuWindow::UpdateCurrentFramebufferLayout(unsigned width, unsigned height) {
    Layout::FramebufferLayout layout;
    switch (Settings::values.layout_option) {
    case Settings::LayoutOption::SingleScreen:
        layout = Layout::SingleFrameLayout(width, height, Settings::values.swap_screen);
        break;
    case Settings::LayoutOption::LargeScreen:
        layout = Layout::LargeFrameLayout(width, height, Settings::values.swap_screen);
        break;
    case Settings::LayoutOption::Default:
    default:
        layout = Layout::DefaultFrameLayout(width, height, Settings::values.swap_screen);
        break;
    }
    NotifyFramebufferLayoutChanged(layout);
}
