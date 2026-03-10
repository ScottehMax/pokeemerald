/*
 * text_data.c - Real name tables for the desktop battle engine
 *
 * Provides gMoveNames, gSpeciesNames, and gTrainerClassNames from the
 * actual game data files. With MODERN=1 the _() macro stores strings as
 * plain ASCII + 0xFF (EOS), so DecodeGFString can output them directly.
 */

#include "global.h"
#include "constants/global.h"
#include "constants/moves.h"
#include "constants/species.h"

#include "src/data/text/move_names.h"
#include "src/data/text/species_names.h"
