#ifndef GUARD_MAIL_H
#define GUARD_MAIL_H

#include "main.h"
#include "constants/items.h"

#define IS_ITEM_MAIL(itemId) (FALSE)

static inline void ReadMail(struct Mail *mail, MainCallback exitCallback, bool8 hasText) {}
static inline void ClearAllMail(void) {}
static inline void ClearMail(struct Mail *mail) {}
static inline bool8 MonHasMail(struct Pokemon *mon) { return FALSE; }
static inline u8 GiveMailToMonByItemId(struct Pokemon *mon, u16 itemId) { return 0; }
static inline u16 SpeciesToMailSpecies(u16 species, u32 personality) { return 0; }
static inline u16 MailSpeciesToSpecies(u16 mailSpecies, u16 *buffer) { return 0; }
static inline u8 GiveMailToMon(struct Pokemon *mon, struct Mail *mail) { return 0; }
static inline void TakeMailFromMon(struct Pokemon *mon) {}

#endif /* GUARD_MAIL_H */
