# Life Puck

A wireless-free life counter for Star Wars Unlimited, built on the
**Waveshare ESP32-S3-Touch-LCD-1.28-B** round display module.

---

## Hardware

| Component | Part |
|---|---|
| MCU | ESP32-S3 |
| Display | GC9A01 240x240 round, SPI |
| Touch | CST816S capacitive single-touch, I2C |
| IMU | QMI8658C 6-axis accelerometer, I2C |
| Battery | 200 mAh LiPo (JST connector on board) |

All components are integrated on the Waveshare board. No external wiring required.

---

## Library Dependencies

This project is built with **PlatformIO + VS Code**. Library versions are
locked in `platformio.ini`; you do not have to install anything by hand.

| Library | Version tested | Source |
|---|---|---|
| LVGL | 9.5.x | PlatformIO registry (`lvgl/lvgl`) |
| TFT_eSPI | 2.5.43 | PlatformIO registry (`bodmer/TFT_eSPI`) |
| CST816S | 1.3.0 | PlatformIO registry (`fbiego/CST816S`) |
| Preferences | bundled | Arduino-ESP32 core (pioarduino fork) |

See [BUILDING.md](BUILDING.md) for first-time setup, including why this
project pins to `pioarduino/platform-espressif32` rather than the
official PlatformIO Espressif platform.

---

## Configuration

All build-time configuration lives in version-controlled files inside
this repo. There is nothing to copy into `Arduino/libraries/` anymore.

### `src/lv_conf.h`

LVGL's configuration. The build flags `-D LV_CONF_INCLUDE_SIMPLE -I src`
in `platformio.ini` tell LVGL to pull it from here. Key settings:

```c
#define LV_MEM_SIZE        (96U * 1024U)   // 96 KB heap for LVGL
#define LV_FONT_MONTSERRAT_24  1
#define LV_FONT_MONTSERRAT_48  1
```

### TFT_eSPI pin assignments

The TFT_eSPI library is configured via `build_flags` in `platformio.ini`
(`-D USER_SETUP_LOADED=1` plus all the `-D TFT_*` defines). The current
values are for the Waveshare ESP32-S3-Touch-LCD-1.28-B:

```ini
-D GC9A01_DRIVER=1
-D USE_HSPI_PORT
-D TFT_MOSI=11
-D TFT_SCLK=10
-D TFT_CS=9
-D TFT_DC=8
-D TFT_RST=14
-D TFT_BL=2
```

If you wire to a different board, edit `platformio.ini`, do not edit
any file inside `.pio/libdeps/`.

### Custom Fonts

The sketch requires two custom fonts generated from your chosen typeface:

| Symbol | File | Used for |
|---|---|---|
| `life_font_96` | `src/life_font_96.c` | 1P life counter (large) |
| `life_font_72` | `src/life_font_72.c` | 2P life counters (smaller) |

Generate them with the LVGL font converter at https://lvgl.io/tools/fontconverter.
Include only the digits 0-9 to keep file size small. Drop both `.c` files
into `src/`; PlatformIO will pick them up automatically.

---

## Controls

### Game (1P mode)

| Gesture | Action |
|---|---|
| Tap top half | +1 life |
| Tap bottom half | -1 life |
| Swipe up | +5 life |
| Swipe down | -5 life |
| Swipe left | Arm undo (see [Undo](#undo) below) |
| Shake the puck | Reset to starting life (after a confirm dwell) |
| Hold centre | Open radial menu |
| Two-finger tap | Toggle 1P / 2P mode |

### Game (2P mode)

Left half of screen = Player 1 (normal orientation).
Right half = Player 2 (rotated 180 degrees so the opponent can read it).

| Gesture | Player affected |
|---|---|
| Tap/swipe left half | P1 |
| Tap/swipe right half | P2 (directions are mirrored) |
| Swipe inward (left on P1, right on P2) | Arm undo for that player |
| Shake | Reset both players (with confirm dwell) |
| Hold centre | Open radial menu |
| Two-finger tap | Toggle 1P / 2P mode |

A shake triggers a brief on-screen confirmation overlay; holding for
`RESET_HOLD_MS` (~0.8 s) commits the reset, lifting earlier cancels it.
This is what stops a hard accidental knock from wiping the table.

### Undo

The undo flow is the same arm + hold-confirm grammar as reset, so accidents
need two deliberate gestures to take effect.

1. **Arm**: swipe horizontally on the player's half (left in 1P or on
   P1's side in 2P; right on P2's side, because P2's view is rotated).
   The affected counter dims and shows the delta that an undo would
   restore (e.g. `+3` if your last bundle subtracted 3). A faint orange
   confirmation ring appears around the rim.
2. **Confirm**: hold the centre of the screen. The ring fills over
   `RESET_HOLD_MS`. Lifting before it fills empties the ring but leaves
   the undo armed; you have `4 s` (`UndoPending::TIMEOUT_MS`) to try
   again before the armed state silently cancels.

The undo history is up to `UNDO_HISTORY_DEPTH` (default **8**) bundles
deep per counter, so several miscounts in a row can be walked back,
arm and confirm again for each step. When the buffer is full the oldest
bundle is silently dropped.

What counts as a "bundle": every life change within `BUNDLE_MS` (~1.5 s)
of the previous one is collapsed into a single undo step. So three rapid
`-1` taps undo together as one `+3`, but a `-1`, a pause, and then a `-2`
are two separate undos. A reset (shake-confirm) and a fresh deep-sleep
restore both clear the undo history, they are not gameplay actions
you'd want to walk back.

### Two-finger tap

Resting two fingers anywhere on the screen briefly (~60–600 ms) and
lifting both fires the same action as the radial menu's PLAYER_TOGGLE:
toggle 1P / 2P mode, with the usual confirmation toast and reset
animation. It is ignored while the radial menu is open, during a
defeat overlay, or while a reset / undo confirmation is armed.

### Defeat overlay

When a player's life reaches zero in count-down mode (or `2 * STARTING_LIFE`
in count-up mode), a full-screen "BASE LOST" overlay takes over until the
user taps to restart the round. Healing the dying player back above the
threshold within the overlay's animation also dismisses it.

---

## Radial Menu

Open by holding the centre of the screen for `CENTER_HOLD_MS` (~0.45 s).
The radial menu is the only settings UI; there is no separate settings
screen.

### Layout

A donut of six segments around a central dead-zone. Each segment is 60°:

| Position | Action |
|---|---|
| 12 o'clock | Toggle 1P / 2P mode |
| 2 o'clock  | Toggle Count Up / Count Down |
| 4 o'clock  | Battery (opens sub-radial) |
| 6 o'clock  | Brightness (opens circular slider) |
| 8 o'clock  | Deep sleep |
| 10 o'clock | Base-life selector (opens sub-radial) |

### Interaction model (two-stage dwell)

1. Rest a finger on a segment. After `MENU_DWELL_REVEAL_MS` (~0.9 s) the
   segment highlights and the centre area shows that segment's icon, name,
   and current value.
2. Keep holding. After a further `MENU_DWELL_COMMIT_MS` (~0.9 s) the
   segment commits:
   - **1P/2P, Count direction** fire immediately and the menu stays open.
   - **Sleep** opens a confirm choice (left = sleep, right = cancel).
   - **Battery** opens the battery sub-radial.
   - **Brightness** opens the circular brightness slider.
   - **Base-life selector** opens the base-life sub-radial.
3. Sliding your finger to a different segment cancels the dwell and restarts on
   the new one.
4. Lifting before commit cancels (menu stays open at top level).
5. **Tap the centre dead-zone** (radius `MENU_INNER_RADIUS`, ~56 px) to back
   out: from a sub-view returns to the top level; from the top level closes
   the menu.

The main game UI (life numbers, divider, battery icon) is hidden while the
menu is open so the radial UI is uncluttered.

### Battery sub-radial

Four segments around the ring: **HIDE**, **AUTO**, **ALWAYS**, and a
fourth **%** toggle. The current mode is fully opaque; the others are
translucent. Lifting a finger on a segment commits that mode (persisted to
NVS); the sub-radial stays open so you can flip again if needed. Centre
tap returns to the top-level ring.

| Mode | Behaviour |
|---|---|
| HIDE | Battery widget always hidden |
| AUTO | Widget appears when level is at or below the AUTO threshold |
| ALWAYS | Widget always visible |
| % | Toggle: show numeric percentage alongside the icon |

There is no charging indicator. Detecting USB power on the ESP32-S3 reliably
requires a hardware modification (a wire from VBUS through a voltage divider
to a free GPIO); the chip's USB PHY cannot detect VBUS in software. The
firmware does not attempt to fake this through battery-voltage heuristics
because a freshly-connected charged battery is indistinguishable from one
being charged.

### Brightness sub-view

A full-circle arc slider. Drag your finger around the ring; the value (0-100 %)
updates live both on the arc and as a number in the centre. Lifting your finger
commits the value (persisted to NVS) but does **not** close the sub-view, so
you can keep adjusting. Centre tap returns to the top-level ring.

### Base-life selector

A small ring of preset starting-life values (20, 25, 30, 40). Dwell to
commit; the new value is persisted to NVS and applied to the next round.

Both players' life totals are reset when switching 1P/2P, count direction,
or base life.

---

## Power Management

The puck uses a four-stage idle timeout chain designed to maximise battery life
during a game night without requiring manual intervention.

| Stage | Trigger | Action |
|---|---|---|
| Dim | `DIM_TIMEOUT_MS` (30 s idle) | Backlight drops to ~6 % (still faintly readable) |
| Pre-off | `SCREEN_PRE_OFF_MS` | Backlight drops further to signal screen-off is imminent |
| Screen off | `SCREEN_OFF_MS` (90 s idle) | Backlight fully off |
| Deep sleep | `DEEP_SLEEP_MS` (3 min idle) | ESP32 enters deep sleep (~0.02 mA) |

Any touch resets the idle timer and immediately restores the screen.
If the screen was fully off, the waking touch is discarded so it does not
accidentally change life totals.

### Deep sleep

Deep sleep saves all state to RTC memory (survives sleep, cleared on power-off):
life totals, count-up mode, 2P mode, and touch lock.
Brightness, battery-display mode, and base-life values are stored in NVS
flash, so they survive full power-off as user preferences.

The device wakes on any touch via the touch INT pin (RTC-capable).
On wakeup, the exact game state from before sleep is restored instantly,
and the first waking touch is silently consumed so it cannot trigger a
spurious +1 / -1.

### Manual sleep

Select **Sleep** in the radial menu (8 o'clock) and confirm to go to deep
sleep immediately regardless of idle time.

### Energy-saving defaults

| Setting | Value | Reason |
|---|---|---|
| Default brightness | 25 % (75/255) | Comfortable for indoor play and roughly halves backlight current vs. 40 % |
| Default battery mode | AUTO | Out of the box the battery icon only appears when it actually matters |
| WiFi and Bluetooth | Disabled at boot | Saves ~20-30 mA |
| Battery ADC interval | 2 minutes | Reduces ADC wakeups from 1800/hr to 30/hr |
| LVGL tick | 5 ms | Reduces CPU wakeups vs the common 1 ms interval |
| IMU poll | 50 ms | Throttles I2C reads to 20/s instead of 200/s |

Estimated active runtime at 25% brightness on a 150 mAh battery: **3-4 hours**.

---

## Code Architecture

The sketch is no longer a single monolithic file. Logic is split into
focused modules so each piece can be read, changed, and tested in
isolation. `main.cpp` itself is now ~150 lines and contains only `setup()`
and `loop()`.

```
life_puck/
└─ src/
   ├─ main.cpp              setup() and loop() only (was life_puck.ino)
   ├─ lv_conf.h             LVGL configuration
   ├─ Config.h              constants, enums, types, FontAwesome glyphs, UiText
   ├─ Clock.h               Clock::now/elapsed/tick helpers
   ├─ Animation.h           TimedAnimation helper used by HoldConfirmation and the radial dwell ring
   ├─ HoldConfirmation.h    Shared arm + hold-to-confirm state used by ResetPending and UndoPending
   ├─ Hardware.{h,cpp}      TFT, CST816S, LVGL display, draw buffer, flush cb, raw I2C finger-count read
   ├─ Imu.{h,cpp}           QMI8658C driver + shake detection
   ├─ NvmSettings.h         Preferences/NVS-backed user prefs (header-only)
   ├─ Backlight.{h,cpp}     PWM level, dim/preOff/off chain, idle-sleep callback
   ├─ Battery.{h,cpp}       ADC read, %, LVGL widget, display modes
   ├─ FlashManager.{h,cpp}  Six feedback flash arcs
   ├─ LifeCounter.{h,cpp}   One player's life value, label, defeat callback, undo history ring
   ├─ GameUi.{h,cpp}        Layout of both LifeCounters, divider, restore
   ├─ ModeToast.{h,cpp}     Brief centre toast on menu commits
   ├─ DefeatOverlay.{h,cpp} BASE LOST overlay + dismissal
   ├─ ResetPending.{h,cpp}  Hold-to-confirm reset overlay (composes HoldConfirmation)
   ├─ UndoPending.{h,cpp}   Hold-to-confirm undo overlay (composes HoldConfirmation)
   ├─ TouchRouter.{h,cpp}   Touch swallow / first-touch-after-wake handling + two-finger tap detector
   ├─ RadialMenu.{h,cpp}    Settings UI: top ring + 4 sub-views
   ├─ StartupIntro.{h,cpp}  Cinematic boot intro
   ├─ PowerManager.{h,cpp}  Deep-sleep entry, RTC-state save
   └─ App.{h,cpp}           Singleton wiring; per-tick handlers; UI builders
```

### Dependency direction

App is the only module that knows about everything. Hardware sits at the
bottom, every feature module is unaware of pins or specific chips.
`Backlight::checkTimeout()` deliberately fires deep sleep via an injected
callback instead of including PowerManager, so the dependency graph stays
acyclic and Backlight stays trivially testable.

```
                     ┌──── App ────┐
                     │             │
          GameUi  RadialMenu   PowerManager
          │  │       │   │         │
    LifeCounter  ModeToast  …      │
          │       │                │
       FlashManager                │
                     │             │
                  Battery        Backlight ──(callback)──> PowerManager
                     │             │
                   Hardware ───────┘
                   Clock, Config (no dependencies)
```

### Naming conventions

The code favours explicit names over short abbreviations. Examples:

| Old style | Current style |
|---|---|
| `bri` | `brightness` |
| `bat` | `battery` |
| `p1` / `p2` | `playerOneCounter` / `playerTwoCounter` |
| `reverseMode` | `countUpMode` |
| `divider2P` | `twoPlayerDivider` |
| `BL_*` | `BACKLIGHT_*` |
| `BAT_*` | `BATTERY_*` |

The Arduino-provided `PI` macro is used for all angle math; no module
defines its own local `PI` constant.

### Clock namespace

```cpp
Clock::now()               // wraps millis()
Clock::elapsed(since, ms)  // true if ms have passed since 'since'
Clock::tick(last, ms)      // true + updates 'last' if interval elapsed
```

Using `Clock::elapsed()` instead of raw `millis() - x >= y` arithmetic makes
timeout conditions read like plain English and keeps all timer logic
consistent.

### Modules

| Module | Responsibility |
|---|---|
| `IMU` | QMI8658C driver. Detects real shaking by counting direction reversals on the dominant horizontal axis within a short window, so a single pickup or jolt never triggers a reset. |
| `Battery` | ADC read, linear 0..100 percentage, LVGL widget that shows according to `BatteryMode`. |
| `Backlight` | PWM level, dim/pre-off/off state, idle timeout chain. Fires deep sleep via an injected callback. |
| `FlashManager` | Six feedback arc sprites that flash briefly on life change. |
| `LifeCounter` | One player's life value and LVGL label. Calls `FlashManager` on change. Fires a defeat callback when the player hits the lose condition. Keeps a per-counter ring of up to `UNDO_HISTORY_DEPTH` recent bundle origins, so several miscounted bundles can be walked back in turn. |
| `GameUi` | Builds the always-visible game layer (two `LifeCounter` instances plus the 2P divider) and handles 1P↔2P layout transitions. |
| `ModeToast` | Brief centre toast that confirms a menu commit (e.g. "1P", "Count up"). |
| `DefeatOverlay` | Full-screen BASE LOST overlay shown when a `LifeCounter` defeat callback fires. |
| `HoldConfirmation` | Shared arm + hold-to-confirm state machine. Owns the `active / fingerDown / timeout / progress / complete` contract that both `ResetPending` and `UndoPending` use, so the two flows have one source of truth for the hold grammar. |
| `ResetPendingOverlay` / `ResetPending` | Hold-to-confirm overlay used by shake-to-reset. The state struct composes `HoldConfirmation`. |
| `UndoPendingOverlay` / `UndoPending` | Hold-to-confirm overlay used by swipe-to-undo. The state struct composes `HoldConfirmation` and carries the affected player index. |
| `TouchRouter` | One-shot "swallow next touch" flag used both for wake-from-sleep and for the menu's tap-to-back. Also owns the two-finger tap detector: polls the CST816S finger-count register each tick and classifies a brief two-finger contact as a tap. |
| `RadialMenu` | Single-screen settings UI. Six-segment top-level ring (`TopRingView`), two-stage dwell commit, plus sub-views `BatterySubView`, `BrightnessView`, `BaseSelectorView`, and a generic `ChoiceView` for the sleep confirm. Centre dead-zone tap = back/close. Owns its own touch routing via `handleTouch` / `notifyFingerLifted` / `tick`. |

### App namespace

`App.cpp` owns the singletons (`imu`, `nvm`, `backlight`, `battery`,
`flashMgr`, `gameUi`, `modeToast`, `radialMenu`, `resetPendingOverlay`,
`resetPending`, `defeatOverlay`, `undoPending`, `undoPendingOverlay`,
`touchRouter`, `game`) plus the per-tick handlers `handleTouch`,
`handleResetPending`, `handleUndoPending`, `handleShake`, and
`drainPendingMenuAction`. `main.cpp` does nothing beyond `setup()`
(bring-up) and `loop()` (call those handlers).

`handleTouch` also drives the two-finger tap detector via
`TouchRouter::pollTwoFinger()` each tick, dispatching to
`executeMenuAction(MenuAction::PLAYER_TOGGLE)` when a tap is taken.

### PowerManager namespace

```cpp
PowerManager::deepSleep()
```

Saves runtime game state to the `PersistentState rtcState` struct in RTC
memory, then calls `esp_deep_sleep_start()`. Restored in `setup()` on
`ESP_SLEEP_WAKEUP_EXT0`. User preferences (brightness, battery mode,
base-life) are handled by `NvmSettings` via `Preferences` / NVS.

---

## Adding a Radial Menu Action

1. Add a new value to `enum class MenuAction` in `Config.h` and bump
   `MENU_ACTION_COUNT` so the six top-ring slots include it (or replace
   one of the existing actions in `TopRingView::build`'s `kActions[]`).
2. In `RadialMenu.cpp`, update the `kActions[]`, `kIcons[]`, and `kColors[]`
   parallel arrays inside `TopRingView::build()`. The arrays must be the
   same length as `MENU_ACTION_COUNT`.
3. Pick how the action commits:
   - **Fire-and-stay-open** (like 1P/2P, Count direction): handle in
     `App::executeMenuAction()` in `App.cpp`. The menu stays open after
     firing so the user can change other settings without re-opening.
   - **Fire-and-close** (like Sleep): wire the close in the touch handler
     in `App.cpp` before firing the action.
   - **Sub-view** (like Battery, Brightness, Base-life): add a new
     `View` enum value, subclass `MenuView`, register it on the
     `MenuHost`, and route `commitTopLevel()` to call
     `requestView(...)` on your new view.
4. If the segment should display a current value while hovered, add a
   case to `iconForAction()` and to the centre-render path used by your
   view.

Segment hit-testing is automatic: `topActionAt()` divides the ring into
`MENU_ACTION_COUNT` equal arcs starting from 12 o'clock, in `kActions[]`
order.

---

## Tuning Constants

All user-tuneable values are `constexpr` in `Config.h`.

| Constant | Default | Effect |
|---|---|---|
| `STARTING_LIFE` | 30 | Starting life total (overridable via Base-life selector) |
| `CENTER_HOLD_MS` | 450 ms | How long to hold the centre before the menu opens |
| `RESET_HOLD_MS` | 800 ms | Hold time to confirm a shake-triggered reset (also reused as the hold time for undo confirmation) |
| `UNDO_HISTORY_DEPTH` | 8 | Per-counter ring buffer size for undoable bundles. Increase if you want a deeper "walk back" history; cost is 4 bytes per slot per counter. |
| `TWO_FINGER_HOLD_MIN_MS` | 60 ms | Minimum two-finger contact duration before a release counts as a two-finger tap. Raise this if you see false positives during single-finger swipes. |
| `TWO_FINGER_HOLD_MAX_MS` | 600 ms | Maximum two-finger contact duration. Beyond this, a release is treated as a deliberate hold, not a tap. |
| `SHAKE_THRESHOLD` | 1.6 g | Per-axis delta-g that counts as one swing direction |
| `SHAKE_REVERSALS_REQUIRED` | 4 | Number of direction reversals needed within the window. A single pickup creates at most one reversal, so it cannot trigger a reset. |
| `SHAKE_WINDOW_MS` | 1200 ms | Reversals must accumulate within this window |
| `SHAKE_COOLDOWN_MS` | 3000 ms | Minimum time between two successive shake resets |
| `DIM_TIMEOUT_MS` | 30 s | Idle time before dim |
| `SCREEN_OFF_MS` | 90 s | Idle time before screen off |
| `DEEP_SLEEP_MS` | 3 min | Idle time before deep sleep |
| `MENU_DWELL_REVEAL_MS` | 900 ms | Hover this long on a segment to highlight it and reveal its current value in the centre |
| `MENU_DWELL_COMMIT_MS` | 900 ms | Hover this long after reveal to commit (fire action or open sub-view) |
| `MENU_DWELL_BACK_MS` | 900 ms | Hold the centre dead-zone this long to back out of a sub-view |
| `MENU_INNER_RADIUS` | 56 px | Radius of the centre dead-zone (used for back-tap) |
| `BACKLIGHT_DEFAULT_LEVEL` | 75 | Starting brightness (0-255, ~25 % on the UI scale) |
| `BATTERY_UPDATE_MS` | 120 000 ms | ADC read interval |
| `IMU_POLL_MS` | 50 ms | Accelerometer poll interval |
