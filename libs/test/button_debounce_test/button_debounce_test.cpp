#include <gtest/gtest.h>

#include <cstring>
#include <string>

extern "C" {
#include "button_debounce.h"
}

namespace {

// The Wi-Fi button's debounce, fed one millisecond at a time as SysTick feeds
// it on the board (bsp_stm32.c, BSP_ButtonTick).
//
// Each row is a trace: one character per millisecond, '#' the pin reads
// pressed, '.' released. Under it, '^' marks the sample that completes a
// press -- the moment BSP_Wifi_TakeButtonPress would start saying yes. Written
// in groups of ten so a column can be counted at a glance.

static_assert(BUTTON_DEBOUNCE_MS == 20u, "the rows below are drawn for 20 ms");

// Where the count starts: from nothing, as bsp_stm32.c starts it -- so a
// button held through a reset is a press -- or full, which a caller could use
// to make a button already down wait to be let go.
constexpr uint8_t kFresh = 0u;
constexpr uint8_t kFull  = BUTTON_DEBOUNCE_MS;

std::string Presses(uint8_t start, const char* samples)
{
    uint8_t     low_ms = start;
    std::string out;

    for (const char* s = samples; *s != '\0'; s++)
    {
        out += ButtonDebounce_Sample(&low_ms, *s == '#') ? '^' : ' ';

        EXPECT_LE(low_ms, BUTTON_DEBOUNCE_MS) << "the count runs past its end";
    }

    return out;
}

TEST(ButtonDebounceTest, APressIsTwentyMillisecondsDownWithoutABreak)
{
    struct Row {
        const char* what;
        uint8_t     start;
        const char* samples;
        const char* presses;
    };

    const Row rows[] = {
        {"a tap of exactly 20 ms is a press", kFresh,
         "##########" "##########" ".....",
         "          " "         ^" "     "},

        {"19 ms is not", kFresh,
         "##########" "#########." ".....",
         "          " "          " "     "},

        {"noise pulses, a millisecond or two each, are not", kFresh,
         "..#......." "#....##..." ".....",
         "          " "          " "     "},

        {"bounce on the way down: counted from the last break", kFresh,
         "#.#.##.###" "##########" "##########",
         "          " "          " "      ^   "},

        {"a break part-way through starts the count over", kFresh,
         "##########" "#####.####" "##########" "##########",
         "          " "          " "          " "     ^    "},

        {"held: one press, not one every 20 ms", kFresh,
         "##########" "##########" "##########" "##########" "##########" "##########",
         "          " "         ^" "          " "          " "          " "          "},

        {"bounce on the way up: still one press", kFresh,
         "##########" "##########" "#####.#.##" ".#........",
         "          " "         ^" "          " "          "},

        {"pressed twice", kFresh,
         "##########" "##########" ".....#####" "##########" "#####.....",
         "          " "         ^" "          " "          " "    ^     "},

        {"down since before counting began: not until let go and pressed again", kFull,
         "##########" "....######" "##########" "####......",
         "          " "          " "          " "   ^      "},
    };

    for (const Row& row : rows)
    {
        ASSERT_EQ(std::strlen(row.samples), std::strlen(row.presses)) << row.what << ": the row does not line up";
        EXPECT_EQ(row.presses, Presses(row.start, row.samples)) << row.what;
    }
}

TEST(ButtonDebounceTest, AStuckButtonIsOnePressHoweverLongItStaysDown)
{
    // An hour of samples, pressed throughout: the count stops at its end and
    // stays there, so the one press is all there ever is.
    uint8_t low_ms  = kFresh;
    int     presses = 0;

    for (int ms = 0; ms < 60 * 60 * 1000; ms++)
    {
        presses += ButtonDebounce_Sample(&low_ms, true) ? 1 : 0;
    }

    EXPECT_EQ(1, presses);
    EXPECT_EQ(BUTTON_DEBOUNCE_MS, low_ms);
}

} // namespace
