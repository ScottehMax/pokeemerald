#ifndef GUARD_MONEY_H
#define GUARD_MONEY_H

static inline u32 GetMoney(u32 *moneyPtr) { return moneyPtr ? *moneyPtr : 0; }
static inline void SetMoney(u32 *moneyPtr, u32 newValue) { if (moneyPtr) *moneyPtr = newValue; }
static inline bool8 IsEnoughMoney(u32 *moneyPtr, u32 cost) { return moneyPtr ? (*moneyPtr >= cost) : FALSE; }
static inline void AddMoney(u32 *moneyPtr, u32 toAdd) { if (moneyPtr) *moneyPtr += toAdd; }
static inline void RemoveMoney(u32 *moneyPtr, u32 toSub) { if (moneyPtr) *moneyPtr -= toSub; }
static inline bool8 IsEnoughForCostInVar0x8005(void) { return FALSE; }
static inline void SubtractMoneyFromVar0x8005(void) {}
static inline void PrintMoneyAmountInMoneyBox(u8 windowId, int amount, u8 speed) {}
static inline void PrintMoneyAmount(u8 windowId, u8 x, u8 y, int amount, u8 speed) {}
static inline void PrintMoneyAmountInMoneyBoxWithBorder(u8 windowId, u16 tileStart, u8 pallete, int amount) {}
static inline void ChangeAmountInMoneyBox(int amount) {}
static inline void DrawMoneyBox(int amount, u8 x, u8 y) {}
static inline void HideMoneyBox(void) {}
static inline void AddMoneyLabelObject(u16 x, u16 y) {}
static inline void RemoveMoneyLabelObject(void) {}

#endif /* GUARD_MONEY_H */
