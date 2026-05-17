// PowerManager.h
//
// Deep-sleep save/restore.
//
// `deepSleepManual()` / `deepSleepIdle()` both persist the runtime game
// state to the PersistentState `rtcState` struct in RTC memory and then
// trigger `esp_deep_sleep_start()`. They differ only in what they stamp
// onto `rtcState.lastSleepReason`, so that the boot path on wake can
// decide whether to replay the startup intro.
//
// State is restored in setup() on ESP_SLEEP_WAKEUP_EXT0.

#pragma once

namespace PowerManager {

// User explicitly chose Sleep from the radial menu (MenuAction::SLEEP_OFF).
// On the next wake the startup intro WILL play.
void deepSleepManual();

// Backlight idle chain reached DEEP_SLEEP_MS with no activity.
// On the next wake the startup intro WILL NOT play — the device returns
// silently to the life counter, as if it had never slept.
void deepSleepIdle();

}  // namespace PowerManager
