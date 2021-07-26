/**
 * @file putchar.cpp
 * @author Keeton Feavel (keetonfeavel@cedarville.edu)
 * @brief
 * @version 0.3
 * @date 2020-08-08
 *
 * @copyright Copyright the Panix Contributors (c) 2020
 *
 */

#include <dev/graphics/font.hpp>
#include <dev/graphics/framebuffer.hpp>
#include <dev/graphics/graphics.hpp>
#include <dev/graphics/tty.hpp>
#include <lib/mutex.hpp>
#include <lib/stdio.hpp>
#include <stddef.h>

/**
 * @brief ANSI color codes for use in functions
 * like printf(). To change the color from the
 * foreground to the background, add 10 to the
 * desired color value.
 * (i.e. ANSI_Red == 31 (fore)--> 41 (back))
 *
 */
enum tty_ansi_color : uint16_t {
    ANSI_Background = 10,
    ANSI_Black = 30,
    ANSI_Red = 31,
    ANSI_Green = 32,
    ANSI_Yellow = 33,
    ANSI_Blue = 34,
    ANSI_Magenta = 35,
    ANSI_Cyan = 36,
    ANSI_White = 37,
    ANSI_BrightBlack = 90,
    ANSI_BrightRed = 91,
    ANSI_BrightGreen = 92,
    ANSI_BrightYellow = 93,
    ANSI_BrightBlue = 94,
    ANSI_BrightMagenta = 95,
    ANSI_BrightCyan = 96,
    ANSI_BrightWhite = 97,
};

static uint16_t ansi_values[8] = { 0 };
static size_t ansi_values_index = 0;
#define PUSH_VAL(VAL) ansi_values[ansi_values_index++] = (VAL)
#define POP_VAL() ansi_values[--ansi_values_index]
#define CLEAR_VALS() ansi_values_index = 0
#define ESC ('\033')
#define TAB_WIDTH 4u
// ANSI states
typedef enum ansi_state {
    Normal,
    Esc,
    Bracket,
    Value
} ansi_state_t;
// State and value storage
ansi_state_t ansi_state = Normal;
uint16_t ansi_val = 0;
// Saved cursor positions
uint32_t ansi_cursor_x = 0;
uint32_t ansi_cursor_y = 0;
// The names don't really line up, so this will need refactoring.
uint32_t ansi_vga_table[16] = {
    VGA_Black, VGA_Red, VGA_Green, VGA_Brown, VGA_Blue,
    VGA_Magenta, VGA_Cyan, VGA_LightGrey, VGA_DarkGrey, VGA_LightRed,
    VGA_LightGreen, VGA_Yellow, VGA_LightBlue, VGA_LightMagenta,
    VGA_LightCyan, VGA_White
};
// Printing mutual exclusion
Mutex putLock;

int putchar(char c)
{
    int retval;
    // must lock when writing to the screen
    putLock.Lock();
    // call the unlocked implementation of putchar
    retval = putchar_unlocked(c);
    // release the screen to be used by other tasks
    putLock.Unlock();
    return retval;
}

int putchar_unlocked(char c)
{
    // Check the ANSI state
    switch (ansi_state) {
    case Normal: // print the character out normally unless it's an ESC
        if (c != ESC)
            break;
        ansi_state = Esc;
        goto end;
    case Esc: // we got an ESC, now we need a left square bracket
        if (c != '[')
            break;
        ansi_state = Bracket;
        goto end;
    case Bracket: // we're looking for a value/command char now
        if (c >= '0' && c <= '9') {
            ansi_val = (uint16_t)(c - '0');
            ansi_state = Value;
            goto end;
        } else if (c == 's') { // Save cursor position attribute
            ansi_cursor_x = tty_coords_x;
            ansi_cursor_y = tty_coords_y;
            goto normal;
        } else if (c == 'u') { // Restore cursor position attribute
            tty_coords_x = ansi_cursor_x;
            tty_coords_y = ansi_cursor_y;
            goto normal;
        }
        break;
    case Value:
        if (c == ';') { // the semicolon is a value separator
            // enqueue the value here
            PUSH_VAL(ansi_val);
            ansi_state = Bracket;
            ansi_val = 0;
        } else if (c == 'm') { // Set color/text attributes command
            PUSH_VAL(ansi_val);
            // take action here
            // iterate through all values
            while (ansi_values_index > 0) {
                ansi_val = (uint16_t)POP_VAL();
                if (ansi_val == 0) {
                    // Reset code will just reset to whatever was specified in tty_clear().
                    color_fore = reset_fore;
                    color_back = reset_back;
                } else if (ansi_val >= ANSI_Black && ansi_val <= ANSI_White) {
                    color_fore = (tty_vga_color)ansi_vga_table[ansi_val - ANSI_Black];
                } else if (ansi_val >= (ANSI_Black + ANSI_Background) && ansi_val <= (ANSI_White + ANSI_Background)) {
                    color_back = (tty_vga_color)ansi_vga_table[ansi_val - (ANSI_Black + ANSI_Background)];
                } else if (ansi_val >= ANSI_BrightBlack && ansi_val <= ANSI_BrightWhite) {
                    color_fore = (tty_vga_color)ansi_vga_table[ansi_val - ANSI_BrightBlack + 8];
                } else if (ansi_val >= (ANSI_BrightBlack + ANSI_Background) && ansi_val <= (ANSI_BrightWhite + ANSI_Background)) {
                    color_back = (tty_vga_color)ansi_vga_table[ansi_val - (ANSI_BrightBlack + ANSI_Background) + 8];
                } // else it was an unknown code
            }
            goto normal;
        } else if (c == 'H' || c == 'f') { // Set cursor position attribute
            PUSH_VAL(ansi_val);
            // the proper order is 'line (y);column (x)'
            if (ansi_values_index > 2) {
                goto error;
            }
            tty_coords_x = (uint8_t)POP_VAL();
            tty_coords_y = (uint8_t)POP_VAL();
            goto normal;
        } else if (c == 'J') { // Clear screen attribute
            // The proper code is ESC[2J
            if (ansi_val != 2) {
                goto error;
            }
            // Clear by resetting the double buffer and swapping.
            // TODO: Find a better way to do this?
            graphics::resetDoubleBuffer();
            graphics::swap();
        } else if (c >= '0' && c <= '9') { // just another digit of a value
            ansi_val = (uint16_t)(ansi_val * 10 + (uint16_t)(c - '0'));
        } else
            break; // invald code, so just return to normal
        // we hit one of the cases so return
        goto end;
    }
    // we fell through some way or another so just reset to Normal no matter what
    ansi_state = Normal;
    switch (c) {
    // Backspace
    case 0x08:
        if (tty_coords_x > 0) {
            tty_coords_x--;
        }
        break;
    // Newline
    case '\n':
        tty_coords_x = 0;
        tty_coords_y++;
        break;
    case '\t':
        while (++tty_coords_x % TAB_WIDTH)
            ;
        break;
    // Carriage return
    case '\r':
        tty_coords_x = 0;
        break;
    }
    // Print the character
    //graphics::font::Draw(c, tty_coords_x, tty_coords_y, color_fore);
    // Move to the next line
    if (tty_coords_x >= X86_TTY_WIDTH) {
        tty_coords_x = 0;
        tty_coords_y++;
    }
    // Clear the screen
    if (tty_coords_y >= X86_TTY_HEIGHT) {
        //TODO: Shift text up and reset the bottom line
        /*
        where = x86_bios_vga_mem + ((X86_TTY_HEIGHT - 1) * X86_TTY_WIDTH - 1);
        for (size_t col = 0; col < X86_TTY_WIDTH; ++col) {
            *(++where) = (uint16_t)(' ' | (attrib << 8));
        }
        */
        tty_coords_x = 0;
        tty_coords_y = X86_TTY_HEIGHT - 1;
    }
    goto end;
error:
    // Reset stack index
    CLEAR_VALS();
    // Return to normal
    ansi_state = Normal;
    ansi_val = 0;
    return EOF;
normal:
    ansi_state = Normal;
    ansi_val = 0;
end:
    return (int)c;
}
