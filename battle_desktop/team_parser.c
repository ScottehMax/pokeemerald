/*
 * team_parser.c — parse Pokémon Showdown exportable team files.
 *
 * See team_parser.h for the file format description.
 */

#include "global.h"
#include "battle.h"
#include "pokemon.h"
#include "battle_desktop/team_parser.h"
#include "battle_desktop/generated/name_tables.h"
#include "constants/pokemon.h"
#include "constants/characters.h"
#include "constants/global.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* -------------------------------------------------------------------------
 * Name normalisation
 *
 * Showdown uses display names with spaces, dots, apostrophes, and hyphens
 * (e.g. "Mr. Mime", "Flamethrower", "Lum Berry").  Our lookup tables store
 * constant-style names (e.g. "MR_MIME", "FLAMETHROWER", "LUM_BERRY").
 *
 * Normalise both to the same canonical form by upper-casing and stripping
 * every character that is not A–Z or 0–9.  This makes "Mr. Mime" →
 * "MRMIME" and "MR_MIME" → "MRMIME", so they compare equal.
 * -------------------------------------------------------------------------*/
static void Normalise(const char *src, char *dst, size_t dstSize)
{
    size_t out = 0;
    for (size_t i = 0; src[i] && out + 1 < dstSize; i++)
    {
        unsigned char c = (unsigned char)src[i];
        if (isalpha(c))
            dst[out++] = (char)toupper(c);
        else if (isdigit(c))
            dst[out++] = (char)c;
        /* strip everything else (spaces, dots, apostrophes, hyphens, underscores) */
    }
    dst[out] = '\0';
}

/* Strip trailing whitespace in-place. */
static void RStripInPlace(char *s)
{
    size_t len = strlen(s);
    while (len > 0 && ((unsigned char)s[len-1] <= ' '))
        s[--len] = '\0';
}

/* Linear lookup using normalised keys.  Returns 0 (NONE) on failure. */
static u16 LookupNorm(const NameIdPair *table, int count, const char *normInput)
{
    char normKey[64];
    for (int i = 0; i < count; i++)
    {
        Normalise(table[i].name, normKey, sizeof(normKey));
        if (strcmp(normKey, normInput) == 0)
            return table[i].id;
    }
    return 0;
}

static u16 LookupSpecies(const char *name)
{
    char norm[64];
    /* Strip optional "SPECIES_" prefix before normalising */
    const char *src = (strncasecmp(name, "SPECIES_", 8) == 0) ? name + 8 : name;
    Normalise(src, norm, sizeof(norm));
    return LookupNorm(gSpeciesNamesTable, gSpeciesNamesCount, norm);
}

static u16 LookupMove(const char *name)
{
    char norm[64];
    const char *src = (strncasecmp(name, "MOVE_", 5) == 0) ? name + 5 : name;
    Normalise(src, norm, sizeof(norm));
    return LookupNorm(gMoveNamesTable, gMoveNamesCount, norm);
}

static u16 LookupItem(const char *name)
{
    char norm[64];
    const char *src = (strncasecmp(name, "ITEM_", 5) == 0) ? name + 5 : name;
    Normalise(src, norm, sizeof(norm));
    return LookupNorm(gItemNamesTable, gItemNamesCount, norm);
}

/* -------------------------------------------------------------------------
 * ASCII → GF charset conversion
 *
 * Pokémon Gen III uses a custom single-byte charset for all text.
 * This converts a printable ASCII string to that charset so we can write
 * a nickname directly with SetMonData(MON_DATA_NICKNAME).
 *
 * Only common printable characters are mapped; anything else becomes a space.
 * Output is padded to POKEMON_NAME_LENGTH bytes; any remaining bytes after
 * EOS are set to EOS (0xFF) as well.
 * -------------------------------------------------------------------------*/
static void AsciiToGfCharset(const char *ascii, u8 *gf, int maxLen)
{
    int out = 0;
    for (int i = 0; ascii[i] && out < maxLen; i++)
    {
        unsigned char c = (unsigned char)ascii[i];
        u8 gfChar;
        if      (c >= 'A' && c <= 'Z') gfChar = CHAR_A + (c - 'A');
        else if (c >= 'a' && c <= 'z') gfChar = CHAR_a + (c - 'a');
        else if (c >= '0' && c <= '9') gfChar = CHAR_0 + (c - '0');
        else if (c == ' ')             gfChar = CHAR_SPACE;
        else if (c == '.')             gfChar = CHAR_PERIOD;
        else if (c == '-')             gfChar = CHAR_HYPHEN;
        else if (c == '\'')            gfChar = CHAR_SGL_QUOTE_RIGHT;
        else if (c == '!')             gfChar = CHAR_EXCL_MARK;
        else if (c == '?')             gfChar = CHAR_QUESTION_MARK;
        else                           gfChar = CHAR_SPACE;
        gf[out++] = gfChar;
    }
    /* Pad remainder with EOS */
    while (out < maxLen)
        gf[out++] = EOS;
}

/* -------------------------------------------------------------------------
 * Stat field helpers
 * -------------------------------------------------------------------------*/

/* Showdown stat abbreviations → indices 0-5 (hp,atk,def,spa,spd,spe). */
static int ParseStatAbbrev(const char *s)
{
    char norm[16];
    Normalise(s, norm, sizeof(norm));
    if (strcmp(norm, "HP")  == 0) return 0;
    if (strcmp(norm, "ATK") == 0) return 1;
    if (strcmp(norm, "DEF") == 0) return 2;
    if (strcmp(norm, "SPA") == 0 || strcmp(norm, "SPATK") == 0 ||
        strcmp(norm, "SPECIALATTACK") == 0) return 3;
    if (strcmp(norm, "SPD") == 0 || strcmp(norm, "SPDEF") == 0 ||
        strcmp(norm, "SPECIALDEFENSE") == 0) return 4;
    if (strcmp(norm, "SPE") == 0 || strcmp(norm, "SPEED") == 0) return 5;
    return -1;
}

static const u32 sEVDataIds[6] = {
    MON_DATA_HP_EV, MON_DATA_ATK_EV, MON_DATA_DEF_EV,
    MON_DATA_SPATK_EV, MON_DATA_SPDEF_EV, MON_DATA_SPEED_EV
};
static const u32 sIVDataIds[6] = {
    MON_DATA_HP_IV, MON_DATA_ATK_IV, MON_DATA_DEF_IV,
    MON_DATA_SPATK_IV, MON_DATA_SPDEF_IV, MON_DATA_SPEED_IV
};

/* Parse "252 Atk / 4 SpD / 252 Spe" into values[6].  Missing stats keep
 * their current value (caller pre-fills the array with defaults). */
static void ParseStatLine(const char *line, u8 *values /*[6]*/)
{
    char buf[256];
    strncpy(buf, line, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    char *tok = strtok(buf, "/");
    while (tok)
    {
        while (*tok == ' ' || *tok == '\t') tok++;

        /* Expect "N StatAbbrev"; ignore trailing +/- nature markers */
        char *endptr;
        long val = strtol(tok, &endptr, 10);
        if (endptr == tok) { tok = strtok(NULL, "/"); continue; }

        while (*endptr == '+' || *endptr == '-') endptr++;
        while (*endptr == ' ' || *endptr == '\t') endptr++;

        int statIdx = ParseStatAbbrev(endptr);
        if (statIdx >= 0 && val >= 0 && val <= 255)
            values[statIdx] = (u8)val;

        tok = strtok(NULL, "/");
    }
}

/* -------------------------------------------------------------------------
 * Nature name → index (NATURE_* constants = personality % 25)
 * -------------------------------------------------------------------------*/
static const char *const sNatureNames[NUM_NATURES] = {
    "HARDY", "LONELY", "BRAVE", "ADAMANT", "NAUGHTY",
    "BOLD",  "DOCILE", "RELAXED", "IMPISH", "LAX",
    "TIMID", "HASTY", "SERIOUS", "JOLLY",  "NAIVE",
    "MODEST","MILD",  "QUIET",   "BASHFUL","RASH",
    "CALM",  "GENTLE","SASSY",   "CAREFUL","QUIRKY",
};

/* Returns NUM_NATURES (25) if not found. */
static u8 ParseNature(const char *s)
{
    char norm[32];
    Normalise(s, norm, sizeof(norm));
    for (int i = 0; i < NUM_NATURES; i++)
    {
        if (strcmp(norm, sNatureNames[i]) == 0)
            return (u8)i;
    }
    return NUM_NATURES; /* sentinel: no nature found */
}

/* -------------------------------------------------------------------------
 * Per-set state accumulated while reading lines
 * -------------------------------------------------------------------------*/
typedef struct {
    u16  species;
    char nickname[POKEMON_NAME_LENGTH + 1]; /* ASCII; empty = no custom nickname */
    u8   level;
    u16  item;
    u8   nature;        /* NUM_NATURES = unspecified */
    u16  moves[4];
    u8   moveCount;
    u8   evs[6];        /* indexed as hp,atk,def,spa,spd,spe; default 0 */
    u8   ivs[6];        /* default 31 */
    u8   happiness;
} ShowdownSet;

static void ResetSet(ShowdownSet *s)
{
    memset(s, 0, sizeof(*s));
    s->level     = 100; /* Showdown default */
    s->nature    = NUM_NATURES;
    s->happiness = 255;
    for (int i = 0; i < 6; i++) s->ivs[i] = 31;
}

/* -------------------------------------------------------------------------
 * First-line parser: "Nickname (Species) @ Item" or "Species @ Item"
 * -------------------------------------------------------------------------*/
static void ParseFirstLine(const char *line, ShowdownSet *s)
{
    char buf[256];
    strncpy(buf, line, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    /* Split on " @ " to separate name/species from item */
    char *itemPart = NULL;
    char *atSign = strstr(buf, " @ ");
    if (atSign)
    {
        *atSign = '\0';
        itemPart = atSign + 3;
        RStripInPlace(itemPart);
    }

    /* Strip gender suffix " (M)" or " (F)" */
    size_t len = strlen(buf);
    if (len >= 4 && buf[len-1] == ')' && buf[len-2] == 'M' && buf[len-3] == ' ' && buf[len-4] == '(')
        buf[len-4] = '\0';
    else if (len >= 4 && buf[len-1] == ')' && buf[len-2] == 'F' && buf[len-3] == ' ' && buf[len-4] == '(')
        buf[len-4] = '\0';

    /* Check for "Nickname (Species)" pattern: find the last " (" */
    len = strlen(buf);
    if (len > 2 && buf[len-1] == ')')
    {
        char *parenOpen = strrchr(buf, '(');
        if (parenOpen && parenOpen > buf && *(parenOpen - 1) == ' ')
        {
            /* Extract species from inside parens */
            char speciesPart[128];
            size_t speciesLen = (size_t)(&buf[len-1] - (parenOpen + 1));
            if (speciesLen > 0 && speciesLen < sizeof(speciesPart))
            {
                strncpy(speciesPart, parenOpen + 1, speciesLen);
                speciesPart[speciesLen] = '\0';
                s->species = LookupSpecies(speciesPart);
            }

            /* Extract nickname: everything before " (" */
            size_t nickLen = (size_t)(parenOpen - 1 - buf); /* exclude trailing space */
            if (nickLen > 0 && nickLen < sizeof(s->nickname))
            {
                strncpy(s->nickname, buf, nickLen);
                s->nickname[nickLen] = '\0';
            }
            goto lookup_item;
        }
    }

    /* No parens: the whole thing is the species name, no custom nickname */
    s->species = LookupSpecies(buf);

lookup_item:
    if (itemPart && *itemPart)
    {
        s->item = LookupItem(itemPart);
        if (s->item == 0)
            fprintf(stderr, "Warning: unknown item '%s'\n", itemPart);
    }
}

/* -------------------------------------------------------------------------
 * Non-first line parser (mirrors parseExportedTeamLine in battle-teams.ts)
 * -------------------------------------------------------------------------*/
static void ParseBodyLine(const char *line, ShowdownSet *s)
{
    if (strncasecmp(line, "Ability:", 8) == 0 ||
        strncasecmp(line, "Trait:",   6) == 0)
    {
        /* Ability cannot be forced in Gen III without complex personality
         * engineering; silently accept and ignore it. */
        return;
    }
    if (strncasecmp(line, "Level:", 6) == 0)
    {
        s->level = (u8)atoi(line + 6);
        return;
    }
    if (strncasecmp(line, "Happiness:", 10) == 0)
    {
        s->happiness = (u8)atoi(line + 10);
        return;
    }
    if (strncasecmp(line, "EVs:", 4) == 0)
    {
        ParseStatLine(line + 4, s->evs);
        return;
    }
    if (strncasecmp(line, "IVs:", 4) == 0)
    {
        ParseStatLine(line + 4, s->ivs);
        return;
    }
    /* "Jolly Nature" — ends with " Nature" or " nature".
     * NOTE: trailing whitespace was already stripped before this call. */
    {
        size_t lineLen = strlen(line);
        const char *suffix = " nature";
        size_t sufLen = 7;
        if (lineLen > sufLen &&
            strncasecmp(line + lineLen - sufLen, suffix, sufLen) == 0)
        {
            char natureName[64];
            size_t nameLen = lineLen - sufLen;
            if (nameLen < sizeof(natureName))
            {
                strncpy(natureName, line, nameLen);
                natureName[nameLen] = '\0';
                u8 n = ParseNature(natureName);
                if (n < NUM_NATURES)
                    s->nature = n;
            }
            return;
        }
    }
    /* Move lines start with "-" or "~" */
    if ((line[0] == '-' || line[0] == '~') && s->moveCount < 4)
    {
        const char *moveName = line + 1;
        if (*moveName == ' ') moveName++;

        /* Hidden Power [Type] → "Hidden Power" */
        char moveBuf[64];
        strncpy(moveBuf, moveName, sizeof(moveBuf) - 1);
        moveBuf[sizeof(moveBuf) - 1] = '\0';
        char *bracket = strchr(moveBuf, '[');
        if (bracket) *bracket = '\0';
        RStripInPlace(moveBuf);

        if (*moveBuf)
        {
            u16 move = LookupMove(moveBuf);
            if (move == 0)
                fprintf(stderr, "Warning: unknown move '%s'\n", moveBuf);
            s->moves[s->moveCount++] = move;
        }
        return;
    }
    if (strncasecmp(line, "Move:", 5) == 0 && s->moveCount < 4)
    {
        u16 move = LookupMove(line + 5);
        if (move == 0)
            fprintf(stderr, "Warning: unknown move '%s'\n", line + 5);
        s->moves[s->moveCount++] = move;
        return;
    }
    /* Shiny, Pokeball, Tera Type, Dynamax Level — ignored (Gen III doesn't use them) */
}

/* -------------------------------------------------------------------------
 * Commit a completed set into the party array
 * -------------------------------------------------------------------------*/
static const u32 sMoveDataIds[4] = {
    MON_DATA_MOVE1, MON_DATA_MOVE2, MON_DATA_MOVE3, MON_DATA_MOVE4
};
static const u32 sPPDataIds[4] = {
    MON_DATA_PP1, MON_DATA_PP2, MON_DATA_PP3, MON_DATA_PP4
};

static const char *const sStatNames[6] = { "HP", "Atk", "Def", "SpA", "SpD", "Spe" };

static void CommitSet(const ShowdownSet *s, struct Pokemon *mon, u8 otIdType, int slot)
{
    /* Nature → personality.  personality % 25 == nature index. */
    u8  hasFixedPersonality = (s->nature < NUM_NATURES) ? TRUE : FALSE;
    u32 personality         = hasFixedPersonality ? (u32)s->nature : 0;

    CreateMon(mon, s->species, s->level, 31,
              hasFixedPersonality, personality, otIdType, 0);

    /* Nickname */
    if (s->nickname[0] != '\0')
    {
        u8 gfName[POKEMON_NAME_LENGTH];
        AsciiToGfCharset(s->nickname, gfName, POKEMON_NAME_LENGTH);
        SetMonData(mon, MON_DATA_NICKNAME, gfName);
    }

    /* Moves */
    for (int m = 0; m < 4; m++)
    {
        u16 move = s->moves[m];
        u8  pp   = (move != 0) ? gBattleMoves[move].pp : 0;
        SetMonData(mon, sMoveDataIds[m], &move);
        SetMonData(mon, sPPDataIds[m],   &pp);
    }

    /* EVs */
    for (int i = 0; i < 6; i++)
        SetMonData(mon, sEVDataIds[i], &s->evs[i]);

    /* IVs */
    for (int i = 0; i < 6; i++)
        SetMonData(mon, sIVDataIds[i], &s->ivs[i]);

    /* Held item */
    if (s->item != 0)
    {
        u16 item = s->item;
        SetMonData(mon, MON_DATA_HELD_ITEM, &item);
    }

    /* Friendship / happiness */
    {
        u8 happiness = s->happiness;
        SetMonData(mon, MON_DATA_FRIENDSHIP, &happiness);
    }

    /* Recompute stats now that EVs/IVs are finalised */
    CalculateMonStats(mon);

    /* Human-readable summary so the user can verify what was loaded */
    printf("  [%d] %s", slot + 1,
           s->nickname[0] ? s->nickname : "(no nickname)");

    /* Show species index — species names are stubbed so just print the ID */
    printf(" (species=%u)", (unsigned)s->species);

    if (s->item)
        printf(" @ item=%u", (unsigned)s->item);
    if (s->nature < NUM_NATURES)
        printf(" | %s nature", sNatureNames[s->nature]);

    /* EVs — only print non-zero */
    bool8 firstEV = TRUE;
    for (int i = 0; i < 6; i++)
    {
        if (s->evs[i])
        {
            printf("%s%u %s", firstEV ? " | EVs: " : "/", s->evs[i], sStatNames[i]);
            firstEV = FALSE;
        }
    }

    printf("\n");
}

/* -------------------------------------------------------------------------
 * Public API
 * -------------------------------------------------------------------------*/

bool8 ParseTeamFile(const char *path, struct Pokemon *party, u8 otIdType)
{
    FILE *f = fopen(path, "r");
    if (!f)
    {
        fprintf(stderr, "Error: cannot open team file '%s'\n", path);
        return FALSE;
    }

    char line[512];
    int  monIdx   = 0;
    bool8 inSet   = FALSE;
    ShowdownSet cur;

    while (fgets(line, sizeof(line), f))
    {
        /* Strip trailing newline, carriage-return, AND trailing spaces/tabs.
         * Showdown's client exports lines with trailing "  " (markdown line
         * breaks); we must remove them before content checks like the nature
         * suffix test. */
        line[strcspn(line, "\r\n")] = '\0';
        RStripInPlace(line);

        /* Trim leading whitespace */
        char *p = line;
        while (*p == ' ' || *p == '\t') p++;

        /* Blank line or "---" → end current set */
        if (*p == '\0' || strcmp(p, "---") == 0)
        {
            if (inSet && cur.species != 0 && monIdx < PARTY_SIZE)
            {
                CommitSet(&cur, &party[monIdx], otIdType, monIdx);
                monIdx++;
            }
            inSet = FALSE;
            continue;
        }

        /* Skip "=== ... ===" team-backup format headers */
        if (strncmp(p, "===", 3) == 0)
            continue;

        if (!inSet)
        {
            ResetSet(&cur);
            inSet = TRUE;
            ParseFirstLine(p, &cur);
            if (cur.species == 0)
                fprintf(stderr, "Warning: unknown species on line '%s'\n", p);
        }
        else
        {
            ParseBodyLine(p, &cur);
        }
    }

    /* Flush the last set if the file doesn't end with a blank line */
    if (inSet && cur.species != 0 && monIdx < PARTY_SIZE)
    {
        CommitSet(&cur, &party[monIdx], otIdType, monIdx);
        monIdx++;
    }

    fclose(f);

    if (monIdx == 0)
    {
        fprintf(stderr, "Error: team file '%s' contained no valid Pokémon\n", path);
        return FALSE;
    }

    printf("Loaded %d Pokémon from '%s'\n", monIdx, path);
    return TRUE;
}
