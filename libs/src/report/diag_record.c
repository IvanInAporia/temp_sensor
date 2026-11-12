#include "diag_record.h"

#include "bsp.h"

/* Mixed into the checksum, so all-zero words -- what a power-on mostly leaves
 * -- do not read as an empty record. Change it when the layout changes, and a
 * record in the old layout reads as none rather than as nonsense. */
#define DIAG_MAGIC 0xD1A60002u

/* The longest `p` field: " p=255x9". */
#define PAIRING_FIELD_MAX 8u

void Diag_Clear(DiagRecord* rec)
{
    rec->cycles        = 0u;
    rec->sensor_fails  = 0u;
    rec->windows       = 0u;
    rec->unpaired      = 0u;
    rec->module_state  = DIAG_STATE_NONE;
    rec->reset         = DIAG_RESET_REPORTED;
    rec->pairings      = 0u;
    rec->pairing       = DIAG_PAIRING_NONE;
    rec->pairing_sends = 0u;
}

void Diag_CountCycle(DiagRecord* rec, bool have_reading)
{
    if (rec->cycles < UINT16_MAX)
    {
        rec->cycles++;
    }

    if (!have_reading && (rec->sensor_fails < UINT16_MAX))
    {
        rec->sensor_fails++;
    }
}

void Diag_CountFailedWindow(DiagRecord* rec, bool unpaired, uint8_t module_state)
{
    if (rec->windows < UINT8_MAX)
    {
        rec->windows++;
    }

    if (unpaired && (rec->unpaired < UINT8_MAX))
    {
        rec->unpaired++;
    }

    rec->module_state = module_state;
}

void Diag_CountPairing(DiagRecord* rec, DiagPairing outcome, uint8_t sends)
{
    if (rec->pairings < UINT8_MAX)
    {
        rec->pairings++;
    }

    rec->pairing       = (uint8_t) outcome;
    rec->pairing_sends = sends;
}

bool Diag_IsNotable(const DiagRecord* rec)
{
    return (rec->reset != DIAG_RESET_REPORTED) || (rec->windows != 0u) || (rec->sensor_fails != 0u) ||
           (rec->pairings != 0u);
}

void Diag_Store(const DiagRecord* rec, uint32_t words[DIAG_WORDS])
{
    words[1] = ((uint32_t) rec->cycles << 16) | rec->sensor_fails;
    words[2] = ((uint32_t) rec->windows << 24) | ((uint32_t) rec->unpaired << 16) |
               ((uint32_t) rec->module_state << 8) | rec->reset;
    words[3] = ((uint32_t) rec->pairings << 16) | ((uint32_t) rec->pairing << 8) | rec->pairing_sends;
    words[0] = DIAG_MAGIC ^ words[1] ^ words[2] ^ words[3];
}

bool Diag_Restore(const uint32_t words[DIAG_WORDS], DiagRecord* rec)
{
    /* Also what catches a reset that landed between two of Diag_Store's
     * writes: the words then disagree with the checksum. */
    if (words[0] != (DIAG_MAGIC ^ words[1] ^ words[2] ^ words[3]))
    {
        return false;
    }

    rec->cycles        = (uint16_t) (words[1] >> 16);
    rec->sensor_fails  = (uint16_t) words[1];
    rec->windows       = (uint8_t) (words[2] >> 24);
    rec->unpaired      = (uint8_t) (words[2] >> 16);
    rec->module_state  = (uint8_t) (words[2] >> 8);
    rec->reset         = (uint8_t) words[2];
    rec->pairings      = (uint8_t) (words[3] >> 16);
    rec->pairing       = (uint8_t) (words[3] >> 8);
    rec->pairing_sends = (uint8_t) words[3];

    return true;
}

static const char* ResetName(uint8_t reset)
{
    switch (reset)
    {
    case DIAG_RESET_REPORTED: return "-";
    case BSP_Reset_PowerOn:   return "por";
    case BSP_Reset_Pin:       return "pin";
    case BSP_Reset_IWatchdog: return "wdg";
    case BSP_Reset_Fault:     return "flt";
    case BSP_Reset_Software:  return "sw";
    case BSP_Reset_LowPower:  return "lp";
    default:                  return "?";
    }
}

/* " k=" and then `value` in decimal. No printf: on this part it would cost a
 * good share of the flash that is left. */
static uint8_t PutField(char* text, uint8_t at, char key, uint16_t value)
{
    char    digits[5];
    uint8_t count = 0u;

    do
    {
        digits[count++] = (char) ('0' + (value % 10u));
        value           = (uint16_t) (value / 10u);
    } while (value != 0u);

    text[at++] = ' ';
    text[at++] = key;
    text[at++] = '=';

    while (count != 0u)
    {
        text[at++] = digits[--count];
    }

    return at;
}

uint8_t Diag_Format(const DiagRecord* rec, char* text)
{
    static const char hex[] = "0123456789abcdef";

    /* Indexed by DiagPairing. */
    static const char pairing_letters[] = "-nxitk";

    uint8_t at = 0u;

    text[at++] = 'r';
    text[at++] = '=';

    for (const char* name = ResetName(rec->reset); *name != '\0'; name++)
    {
        text[at++] = *name;
    }

    at = PutField(text, at, 'c', rec->cycles);
    at = PutField(text, at, 'w', rec->windows);
    at = PutField(text, at, 'u', rec->unpaired);

    text[at++] = ' ';
    text[at++] = 'm';
    text[at++] = '=';
    text[at++] = hex[rec->module_state >> 4];
    text[at++] = hex[rec->module_state & 0x0Fu];

    at = PutField(text, at, 's', rec->sensor_fails);

    /* Last, and only with room for its longest. Every real line has that room
     * -- the fields above reach 38 characters only with counts no device
     * runs up between two reports -- but the SDK's buffer has no bounds
     * check, so a line that would not fit goes without it. */
    if ((rec->pairings != 0u) && (at <= (DIAG_TEXT_MAX - PAIRING_FIELD_MAX)))
    {
        at = PutField(text, at, 'p', rec->pairings);

        text[at++] = (rec->pairing < (sizeof(pairing_letters) - 1u)) ? pairing_letters[rec->pairing] : '?';
        text[at++] = (char) ('0' + ((rec->pairing_sends < 9u) ? rec->pairing_sends : 9u));
    }

    return at;
}
