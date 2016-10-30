// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "common/math_util.h"
namespace Layout {
/// Describes the layout of the window framebuffer (size and top/bottom screen positions)
struct FramebufferLayout {
    unsigned width;
    unsigned height;
    bool top_screen_enabled;
    bool bottom_screen_enabled;
    MathUtil::Rectangle<unsigned> top_screen;
    MathUtil::Rectangle<unsigned> bottom_screen;
};

/**
 * Factory method for constructing a default FramebufferLayout
 * @param width Window framebuffer width in pixels
 * @param height Window framebuffer height in pixels
 * @return Newly created FramebufferLayout object with default screen regions initialized
 */
FramebufferLayout DefaultFrameLayout(unsigned width, unsigned height, bool is_swapped);

/**
 * Factory method for constructing a FramebufferLayout with only the top screen
 * @param width Window framebuffer width in pixels
 * @param height Window framebuffer height in pixels
 * @return Newly created FramebufferLayout object with default screen regions initialized
 */
FramebufferLayout SingleFrameLayout(unsigned width, unsigned height, bool is_swapped);

/**
 * Factory method for constructing a Frame with the a 4x size Top screen with a 1x size bottom screen on the right
 * This is useful in particular because it matches well with a 1920x1080 resolution monitor
 * @param width Window framebuffer width in pixels
 * @param height Window framebuffer height in pixels
 * @return Newly created FramebufferLayout object with default screen regions initialized
 */
FramebufferLayout LargeFrameLayout(unsigned width, unsigned height, bool is_swapped);
}