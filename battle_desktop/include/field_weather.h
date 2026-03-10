#ifndef GUARD_FIELD_WEATHER_H
#define GUARD_FIELD_WEATHER_H

/* Desktop stub for field_weather.h */

static inline u8 GetCurrentWeather(void) { return 0; }
static inline void SetWeather(u32 weather) {}
static inline void DoCurrentWeatherEffect(void) {}
static inline bool8 IsWeatherEffectActive(void) { return FALSE; }
static inline void StartWeatherChangeWithDelay(u8 weather, u16 delay) {}
static inline void GetCurrentMapWeather(void) {}
static inline u8 weatherStrength(void) { return 0; }
static inline bool8 IsWeather(u32 weather) { return FALSE; }

#endif /* GUARD_FIELD_WEATHER_H */
