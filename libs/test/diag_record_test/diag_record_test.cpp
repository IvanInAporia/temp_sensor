#include <gtest/gtest.h>

#include <initializer_list>
#include <string>

extern "C" {
#include "bsp.h"
#include "diag_record.h"
}

namespace {

constexpr uint8_t kNone     = DIAG_STATE_NONE;
constexpr uint8_t kReported = DIAG_RESET_REPORTED;

// A record written out field by field, in the order the text has them.
struct Fields {
    uint8_t  reset;
    uint16_t cycles;
    uint8_t  windows;
    uint8_t  unpaired;
    uint8_t  module_state;
    uint16_t sensor_fails;
};

DiagRecord Make(const Fields& f)
{
    DiagRecord rec;
    rec.reset        = f.reset;
    rec.cycles       = f.cycles;
    rec.windows      = f.windows;
    rec.unpaired     = f.unpaired;
    rec.module_state = f.module_state;
    rec.sensor_fails = f.sensor_fails;
    return rec;
}

std::string Format(const DiagRecord& rec)
{
    char    text[DIAG_TEXT_MAX + 8];
    uint8_t len = Diag_Format(&rec, text);
    EXPECT_LE(len, DIAG_TEXT_MAX);
    return std::string(text, len);
}

// --- The text -----------------------------------------------------------------

TEST(DiagRecordTest, FormatsEachFieldAsTheCloudLogShowsIt)
{
    struct Row {
        Fields      in;
        const char* text;
    };

    const Row rows[] = {
        // reset              cycles windows unpaired state sensor
        {{BSP_Reset_PowerOn,   1,     0,      0,      kNone, 0},     "r=por c=1 w=0 u=0 m=ff s=0"},
        {{BSP_Reset_Pin,       108,   4,      0,      0x02,  0},     "r=pin c=108 w=4 u=0 m=02 s=0"},
        {{BSP_Reset_IWatchdog, 3,     0,      0,      kNone, 0},     "r=wdg c=3 w=0 u=0 m=ff s=0"},
        {{BSP_Reset_Fault,     12,    1,      0,      0x04,  0},     "r=flt c=12 w=1 u=0 m=04 s=0"},
        {{BSP_Reset_Software,  0,     0,      0,      kNone, 0},     "r=sw c=0 w=0 u=0 m=ff s=0"},
        {{BSP_Reset_LowPower,  0,     0,      0,      kNone, 0},     "r=lp c=0 w=0 u=0 m=ff s=0"},
        {{BSP_Reset_None,      0,     0,      0,      kNone, 0},     "r=? c=0 w=0 u=0 m=ff s=0"},
        {{kReported,           25,    3,      0,      0x03,  0},     "r=- c=25 w=3 u=0 m=03 s=0"},
        {{kReported,           30,    1,      1,      0x06,  0},     "r=- c=30 w=1 u=1 m=06 s=0"},
        {{kReported,           9,     0,      0,      kNone, 9},     "r=- c=9 w=0 u=0 m=ff s=9"},
        {{0x7E,                0,     0,      0,      0xAB,  0},     "r=? c=0 w=0 u=0 m=ab s=0"},
        // the longest it gets
        {{BSP_Reset_PowerOn,   65535, 255,    255,    kNone, 65535}, "r=por c=65535 w=255 u=255 m=ff s=65535"},
    };

    for (const Row& row : rows)
    {
        EXPECT_EQ(row.text, Format(Make(row.in)));
    }
}

TEST(DiagRecordTest, AClearedRecordHasNothingToSay)
{
    DiagRecord rec = Make({BSP_Reset_Pin, 50, 3, 1, 0x02, 7});
    Diag_Clear(&rec);

    EXPECT_EQ("r=- c=0 w=0 u=0 m=ff s=0", Format(rec));
    EXPECT_FALSE(Diag_IsNotable(&rec));
}

// --- When a report carries it ---------------------------------------------------

TEST(DiagRecordTest, OnlyAResetOrAFailureIsWorthSending)
{
    struct Row {
        Fields in;
        bool   notable;
    };

    const Row rows[] = {
        // reset              cycles windows unpaired state sensor   notable
        {{kReported,           1,     0,      0,      kNone, 0},     false}, // an ordinary report
        {{kReported,           48,    0,      0,      kNone, 0},     false}, // the 4 h report: quiet is not a failure
        {{kReported,           65535, 0,      0,      kNone, 0},     false},
        {{BSP_Reset_PowerOn,   1,     0,      0,      kNone, 0},     true},  // the first report after boot
        {{BSP_Reset_Pin,       0,     0,      0,      kNone, 0},     true},
        {{BSP_Reset_None,      0,     0,      0,      kNone, 0},     true},
        {{kReported,           3,     1,      0,      0x02,  0},     true},  // a window failed
        {{kReported,           3,     1,      1,      0x01,  0},     true},  // ...found the module unpaired
        {{kReported,           3,     0,      0,      kNone, 1},     true},  // a sensor read failed
        {{kReported,           3,     0,      0,      0x02,  0},     false}, // a state with no failed window
    };

    for (const Row& row : rows)
    {
        DiagRecord rec = Make(row.in);
        EXPECT_EQ(row.notable, Diag_IsNotable(&rec)) << Format(rec);
    }
}

// --- Counting -------------------------------------------------------------------

TEST(DiagRecordTest, CountsCyclesAndFailedReadsAndSaturates)
{
    struct Row {
        uint16_t cycles_before;
        uint16_t fails_before;
        bool     have_reading;
        uint16_t cycles_after;
        uint16_t fails_after;
    };

    const Row rows[] = {
        // before      reading  after
        {0,     0,     true,    1,     0},
        {0,     0,     false,   1,     1},
        {107,   4,     true,    108,   4},
        {65534, 65534, false,   65535, 65535},
        {65535, 65535, false,   65535, 65535}, // stops, does not wrap to 0
        {65535, 0,     true,    65535, 0},
    };

    for (const Row& row : rows)
    {
        DiagRecord rec = Make({kReported, row.cycles_before, 0, 0, kNone, row.fails_before});
        Diag_CountCycle(&rec, row.have_reading);

        EXPECT_EQ(row.cycles_after, rec.cycles) << "from " << row.cycles_before;
        EXPECT_EQ(row.fails_after, rec.sensor_fails) << "from " << row.fails_before;
    }
}

TEST(DiagRecordTest, CountsFailedWindowsAndKeepsTheLastState)
{
    struct Row {
        uint8_t windows_before;
        uint8_t unpaired_before;
        bool    unpaired;
        uint8_t state;
        uint8_t windows_after;
        uint8_t unpaired_after;
    };

    const Row rows[] = {
        // before   unpaired state  after
        {0,   0,    false,   0x02,  1,   0},
        {0,   0,    true,    0x01,  1,   1},
        {3,   1,    false,   kNone, 4,   1},
        {254, 254,  true,    0x00,  255, 255},
        {255, 255,  true,    0x06,  255, 255}, // stops, does not wrap to 0
    };

    for (const Row& row : rows)
    {
        DiagRecord rec = Make({kReported, 5, row.windows_before, row.unpaired_before, 0x04, 0});
        Diag_CountFailedWindow(&rec, row.unpaired, row.state);

        EXPECT_EQ(row.windows_after, rec.windows) << "from " << int(row.windows_before);
        EXPECT_EQ(row.unpaired_after, rec.unpaired) << "from " << int(row.unpaired_before);
        EXPECT_EQ(row.state, rec.module_state); // the latest, not the first
    }
}

// --- Across a reset -------------------------------------------------------------

TEST(DiagRecordTest, ARecordComesBackAsItWasStored)
{
    const Fields records[] = {
        {BSP_Reset_Pin,     108,   4,   0,   0x02,  0},
        {kReported,         0,     0,   0,   kNone, 0},
        {BSP_Reset_PowerOn, 65535, 255, 255, 0xAB,  65535},
        {BSP_Reset_Fault,   1,     0,   0,   0x00,  1},
    };

    for (const Fields& f : records)
    {
        DiagRecord stored = Make(f);
        uint32_t   words[DIAG_WORDS];
        Diag_Store(&stored, words);

        DiagRecord back = Make({0, 0, 0, 0, 0, 0});
        ASSERT_TRUE(Diag_Restore(words, &back)) << Format(stored);
        EXPECT_EQ(Format(stored), Format(back));
    }
}

TEST(DiagRecordTest, WordsThatHoldNoRecordAreTurnedAway)
{
    DiagRecord stored = Make({BSP_Reset_Pin, 108, 4, 0, 0x02, 0});
    uint32_t   good[DIAG_WORDS];
    Diag_Store(&stored, good);

    struct Row {
        const char* what;
        uint32_t    words[DIAG_WORDS];
    };

    const Row rows[] = {
        {"all zero, as a power-on mostly leaves them", {0u, 0u, 0u}},
        {"all ones",                                   {0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu}},
        {"one bit off in the checksum",                {good[0] ^ 1u, good[1], good[2]}},
        {"one bit off in the counts",                  {good[0], good[1] ^ 0x10000u, good[2]}},
        {"one bit off in the window word",             {good[0], good[1], good[2] ^ 0x80u}},
        {"a reset between two of the writes",          {good[0], good[1] + 1u, good[2]}},
    };

    for (const Row& row : rows)
    {
        DiagRecord rec = Make({kReported, 7, 7, 7, 7, 7});
        EXPECT_FALSE(Diag_Restore(row.words, &rec)) << row.what;
        EXPECT_EQ("r=- c=7 w=7 u=7 m=07 s=7", Format(rec)) << row.what << ": left alone";
    }
}

} // namespace
