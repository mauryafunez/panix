/**
 * @file graphics.cpp
 * @author Keeton Feavel (keetonfeavel@cedarville.edu)
 * @author Michel (JMallone) Gomes (michels@utfpr.edu.br)
 * @brief
 * @version 0.2
 * @date 2021-07-24
 *
 * @copyright Copyright the Panix Contributors (c) 2021
 *
 * References:
 *          https://wiki.osdev.org/Double_Buffering
 */
#pragma once
#include <stdint.h>
#include <dev/vga/fb.hpp>

namespace fb {

/**
 * @brief Checks if the framebuffer has been initialized
 *
 * @return true Framebuffer is initialized
 * @return false Framebuffer is not initialized
 */
bool isInitialized();

/**
 * @brief Initializes the framebuffer (if it exists)
 *
 * @param info
 */
void init(FramebufferInfo info);

/**
 * @brief Draws a pixel at a given coordinate.
 *
 * @param x X-axis coordinate
 * @param y Y-axis coordinate
 * @param color Hex color
 */
void pixel(uint32_t x, uint32_t y, uint32_t color);

/**
 * @brief Draws and fills a rectangle of a given width and height, and color
 * at the provided coordinates.
 *
 * @param x X-axis coordinate
 * @param y Y-axis coordinate
 * @param w Width
 * @param h Height
 * @param color Hex color
 */
void putrect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color);

/**
 * @brief Fill the backbuffer with '0'.
 *
 */
void resetDoubleBuffer();

/**
 * @brief Swap the data on backbuffer to memory video buffer
 * and show in the screen.
 *
 */
void swap();

};
