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

// A record written out field by field, in the order the text has them. A row
// that stops after sensor_fails has had no pairing window.
struct Fields {
    uint8_t  reset;
    uint16_t cycles;
    uint8_t  windows;
    uint8_t  unpaired;
    uint8_t  module_state;
    uint16_t sensor_fails;
    uint8_t  pairings;
    uint8_t  pairing;
    uint8_t  pairing_sends;
};

DiagRecord Make(const Fields& f)
{
    DiagRecord rec;
    rec.reset         = f.reset;
    rec.cycles        = f.cycles;
    rec.windows       = f.windows;
    rec.unpaired      = f.unpaired;
    rec.module_state  = f.module_state;
    rec.sensor_fails  = f.sensor_fails;
    rec.pairings      = f.pairings;
    rec.pairing       = f.pairing;
    rec.pairing_sends = f.pairing_sends;
    return rec;
}

constexpr uint8_t kSilent     = DIAG_PAIRING_SILENT;
constexpr uint8_t kUnanswered = DIAG_PAIRING_UNANSWERED;
constexpr uint8_t kIgnored    = DIAG_PAIRING_IGNORED;
constexpr uint8_t kUnfinished = DIAG_PAIRING_UNFINISHED;
constexpr uint8_t kReached    = DIAG_PAIRING_REACHED;

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

TEST(DiagRecordTest, FormatsThePairingFieldAfterTheRest)
{
    struct Row {
        Fields      in;
        const char* text;
    };

    const Row rows[] = {
        // reset    cycles windows unpaired state sensor  pairings outcome      sends
        {{kReported, 3,     0,      0,      kNone, 0,      1,       kReached,    1}, "r=- c=3 w=0 u=0 m=ff s=0 p=1k1"},
        {{kReported, 3,     0,      0,      kNone, 0,      1,       kUnfinished, 1}, "r=- c=3 w=0 u=0 m=ff s=0 p=1t1"},
        {{kReported, 3,     0,      0,      kNone, 0,      1,       kIgnored,    1}, "r=- c=3 w=0 u=0 m=ff s=0 p=1i1"},
        {{kReported, 3,     0,      0,      kNone, 0,      1,       kUnanswered, 3}, "r=- c=3 w=0 u=0 m=ff s=0 p=1x3"},
        {{kReported, 3,     0,      0,      kNone, 0,      1,       kSilent,     0}, "r=- c=3 w=0 u=0 m=ff s=0 p=1n0"},
        {{kReported, 3,     0,      0,      kNone, 0,      2,       kReached,    2}, "r=- c=3 w=0 u=0 m=ff s=0 p=2k2"},
        {{BSP_Reset_Pin, 9, 1,      0,      0x02,  0,      3,       kUnfinished, 1}, "r=pin c=9 w=1 u=0 m=02 s=0 p=3t1"},
        {{kReported, 3,     0,      0,      kNone, 0,      255,     kReached,    1}, "r=- c=3 w=0 u=0 m=ff s=0 p=255k1"},
        // what only a corrupted record could hold: still one character each
        {{kReported, 3,     0,      0,      kNone, 0,      1,       0x7E,        42}, "r=- c=3 w=0 u=0 m=ff s=0 p=1?9"},
        // no pairing window: no field, whatever the rest says
        {{kReported, 3,     0,      0,      kNone, 0,      0,       kReached,    1}, "r=- c=3 w=0 u=0 m=ff s=0"},
    };

    for (const Row& row : rows)
    {
        EXPECT_EQ(row.text, Format(Make(row.in)));
    }
}

TEST(DiagRecordTest, ThePairingFieldIsDroppedRatherThanOverrunTheBuffer)
{
    // The field needs up to 8 characters, " p=255x9", so it goes on only while
    // the line is at most 32 long. Real lines are ~27.
    struct Row {
        Fields      in;
        const char* text;
    };

    const Row rows[] = {
        // 32 characters before it: exactly fits, at the full 40
        {{BSP_Reset_PowerOn, 65535, 25, 25, kNone, 0, 255, kUnanswered, 9}, "r=por c=65535 w=25 u=25 m=ff s=0 p=255x9"},
        // 33: dropped
        {{BSP_Reset_PowerOn, 65535, 255, 25, kNone, 0, 1, kReached, 1}, "r=por c=65535 w=255 u=25 m=ff s=0"},
        // the longest the rest gets
        {{BSP_Reset_PowerOn, 65535, 255, 255, kNone, 65535, 1, kReached, 1}, "r=por c=65535 w=255 u=255 m=ff s=65535"},
    };

    for (const Row& row : rows)
    {
        EXPECT_EQ(row.text, Format(Make(row.in)));
    }
}

TEST(DiagRecordTest, AClearedRecordHasNothingToSay)
{
    DiagRecord rec = Make({BSP_Reset_Pin, 50, 3, 1, 0x02, 7, 2, kUnfinished, 1});
    Diag_Clear(&rec);

    EXPECT_EQ("r=- c=0 w=0 u=0 m=ff s=0", Format(rec));
    EXPECT_FALSE(Diag_IsNotable(&rec));
}

// --- When a report carries it ---------------------------------------------------

TEST(DiagRecordTest, OnlyAResetAFailureOrAPairingIsWorthSending)
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
        {{kReported,           3,     0,      0,      kNone, 0, 1, kReached, 1}, true}, // a pairing window, even one that worked
        {{kReported,           3,     0,      0,      kNone, 0, 1, kUnfinished, 1}, true},
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

TEST(DiagRecordTest, CountsPairingWindowsAndKeepsTheLast)
{
    struct Row {
        uint8_t pairings_before;
        uint8_t outcome;
        uint8_t sends;
        uint8_t pairings_after;
    };

    const Row rows[] = {
        // before outcome      sends  after
        {0,       kReached,    1,     1},
        {0,       kSilent,     0,     1},
        {1,       kUnanswered, 3,     2},
        {254,     kIgnored,    1,     255},
        {255,     kUnfinished, 2,     255}, // stops, does not wrap to 0
    };

    for (const Row& row : rows)
    {
        DiagRecord rec = Make({kReported, 5, 2, 0, 0x02, 1, row.pairings_before, kUnfinished, 1});
        Diag_CountPairing(&rec, static_cast<DiagPairing>(row.outcome), row.sends);

        EXPECT_EQ(row.pairings_after, rec.pairings) << "from " << int(row.pairings_before);
        EXPECT_EQ(row.outcome, rec.pairing); // the latest, not the first
        EXPECT_EQ(row.sends, rec.pairing_sends);

        // and nothing else
        EXPECT_EQ(2, rec.windows);
        EXPECT_EQ(0x02, rec.module_state);
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
        {kReported,         4,     0,   0,   kNone, 0,     2,   kUnfinished, 1},
        {BSP_Reset_Pin,     12,    1,   0,   0x02,  0,     255, kUnanswered, 3},
    };

    for (const Fields& f : records)
    {
        DiagRecord stored = Make(f);
        uint32_t   words[DIAG_WORDS];
        Diag_Store(&stored, words);

        DiagRecord back = Make({0, 0, 0, 0, 0, 0, 0, 0, 0});
        ASSERT_TRUE(Diag_Restore(words, &back)) << Format(stored);
        EXPECT_EQ(Format(stored), Format(back));
        EXPECT_EQ(stored.pairing_sends, back.pairing_sends) << Format(stored);
    }
}

TEST(DiagRecordTest, WordsThatHoldNoRecordAreTurnedAway)
{
    DiagRecord stored = Make({BSP_Reset_Pin, 108, 4, 0, 0x02, 0, 1, kReached, 1});
    uint32_t   good[DIAG_WORDS];
    Diag_Store(&stored, good);

    struct Row {
        const char* what;
        uint32_t    words[DIAG_WORDS];
    };

    const Row rows[] = {
        {"all zero, as a power-on mostly leaves them", {0u, 0u, 0u, 0u}},
        {"all ones",                                   {0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu}},
        {"one bit off in the checksum",                {good[0] ^ 1u, good[1], good[2], good[3]}},
        {"one bit off in the counts",                  {good[0], good[1] ^ 0x10000u, good[2], good[3]}},
        {"one bit off in the window word",             {good[0], good[1], good[2] ^ 0x80u, good[3]}},
        {"one bit off in the pairing word",            {good[0], good[1], good[2], good[3] ^ 0x100u}},
        {"a reset between two of the writes",          {good[0], good[1] + 1u, good[2], good[3]}},
        {"the old three-word layout",                  {0xD1A60001u ^ good[1] ^ good[2], good[1], good[2], 0u}},
    };

    for (const Row& row : rows)
    {
        DiagRecord rec = Make({kReported, 7, 7, 7, 7, 7});
        EXPECT_FALSE(Diag_Restore(row.words, &rec)) << row.what;
        EXPECT_EQ("r=- c=7 w=7 u=7 m=07 s=7", Format(rec)) << row.what << ": left alone";
    }
}

} // namespace
