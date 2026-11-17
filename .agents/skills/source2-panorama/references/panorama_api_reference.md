# Counter-Strike 2 Panorama JavaScript API Reference

Authoritative API reference for client scripts running within Counter-Strike 2's V8 Panorama runtime.

---

## 1. Global Panorama Functions (`$`)

The root `$` object provides DOM manipulation, logging, timing, and event dispatch:

| Method | Signature | Description |
| :--- | :--- | :--- |
| `$.Msg` | `$.Msg(...args)` | Prints formatted debug messages to the CS2 developer console. |
| `$.Warning` | `$.Warning(...args)` | Prints yellow warning messages to developer console. |
| `$.CreatePanel` | `$.CreatePanel(type, parent, id)` | Dynamically creates a new DOM panel (e.g. `'Panel'`, `'Label'`, `'Button'`). |
| `$.Schedule` | `$.Schedule(delaySeconds, callback)` | Non-blocking timer invocation. Returns a schedule handle. |
| `$.CancelScheduled` | `$.CancelScheduled(handle)` | Cancels a previously scheduled callback. |
| `$.GetContextPanel` | `$.GetContextPanel()` | Returns the root panel for the currently executing script. |
| `$.DispatchEvent` | `$.DispatchEvent(eventName, ...args)` | Dispatches a Panorama event through the panel hierarchy. |
| `$.RegisterEventHandler` | `$.RegisterEventHandler(event, panel, callback)` | Attaches a listener to a specific panel event. |

---

## 2. Demo Playback API (`DemoAPI`)

Exposes transport controls and playback statistics during CS2 demo viewing:

```javascript
// --- Navigation & Scrubbing ---
DemoAPI.GetPlaybackTick();         // Returns current integer playback tick (e.g. 27703)
DemoAPI.GetTotalTicks();            // Returns total recorded ticks in demo
DemoAPI.SetPlaybackTick(tick);      // Jumps playback directly to target tick (equivalent to demo_gototick)
DemoAPI.GotoNextRound();            // Skips playback to the next round start tick
DemoAPI.GotoPreviousRound();        // Rewinds playback to the prior round start tick

// --- Playback State & Speed ---
DemoAPI.IsPlaybackPaused();         // Returns boolean true if playback is paused
DemoAPI.PausePlayback();            // Pauses demo playback
DemoAPI.ResumePlayback();           // Resumes demo playback
DemoAPI.GetTimescale();             // Returns float playback speed multiplier (default 1.0)
DemoAPI.SetTimescale(scale);        // Modifies playback speed (0.25 for slow-mo, 2.0 for fast-forward)

// --- Metadata & Match Info ---
DemoAPI.GetDemoFilename();          // Returns active demo filename string
DemoAPI.GetMapName();               // Returns map name (e.g. "de_cache", "de_mirage")
DemoAPI.GetDemoProtocol();          // Source 2 Wire Protocol version (e.g. 4)
```

---

## 3. Game Events Bus (`GameEvents`)

Provides event subscription to server-authoritative and client gameplay events:

```javascript
// Subscribe to an event
let listenerHandle = GameEvents.Subscribe("eventName", function(eventData) {
    // Process eventData object
});

// Unsubscribe
GameEvents.Unsubscribe(listenerHandle);
```

### Essential Event Payloads for Demo Replays

#### `player_hurt`
Fired when a player takes bullet, explosive, or world damage:
- `userid`: Entity ID / player slot of the victim
- `attacker`: Entity ID / player slot of the attacker
- `health`: Remaining health of victim
- `armor`: Remaining armor
- `dmg_health`: Damage dealt to health
- `dmg_armor`: Damage absorbed by armor
- `hitgroup`: Hitbox region:
  - `1`: Head (`Apex Joint #6`)
  - `2`: Chest
  - `3`: Stomach
  - `4`: Left Arm
  - `5`: Right Arm
  - `6`: Left Leg
  - `7`: Right Leg
- `weapon`: Weapon string (e.g. `"ssg08"`, `"ak47"`)

#### `weapon_fire`
Fired on the exact tick of weapon actuation:
- `userid`: Player slot of shooter
- `weapon`: Weapon item definition name
- `silenced`: Boolean flag for detached/attached silencer

#### `bullet_impact`
Fired when a server trace collides with geometry or players:
- `userid`: Shooter player slot
- `x`, `y`, `z`: Floating-point world coordinates of impact

#### `round_start` & `round_end`
- `timelimit`: Time limit in seconds
- `winner`: Team winning the round (2 = T, 3 = CT)
- `reason`: Win condition ID (elimination, defusal, detonation)

---

## 4. Game Interface API (`GameInterfaceAPI`)

Executes console commands and queries engine convars:

```javascript
// Execute console command
GameInterfaceAPI.ConsoleExec("cl_drawhud 1");
GameInterfaceAPI.ConsoleExec("mirv_panorama panelstyle panelId=trueview_row opacity=0");

// Read ConVar / setting
let sens = GameInterfaceAPI.GetSettingString("sensitivity");
let vol  = GameInterfaceAPI.GetSettingFloat("volume");
```

---

## 5. Match Statistics & Persona API

```javascript
// Player metadata
MyPersonaAPI.GetXuid();            // SteamID64 of current player
MyPersonaAPI.GetName();            // Player display name

// Match stats
MatchStatsAPI.GetRoundScore(2);    // Returns T score
MatchStatsAPI.GetRoundScore(3);    // Returns CT score
```
