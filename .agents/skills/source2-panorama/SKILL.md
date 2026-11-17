---
name: source2-panorama
description: >-
  Authoritative reverse-engineering and development reference for Source 2 Panorama UI,
  ValveResourceFormat (VRF) resource decompilation, and CS2 Panorama APIs. Use when inspecting
  decompiled Panorama assets (SteamTracking/GameTracking-CS2), decoding compiled resources
  (vxml_c, vcss_c, vjs_c, vmat_c, vpk), interfacing with DemoAPI/GameStateAPI/GameEvents,
  or developing native in-engine Panorama HUD overlays.
---

# Source 2 Panorama UI, VRF Decompilation & API Reference

This skill provides reverse-engineering standards, architecture specifications, API references, and asset decompilation guidelines for the **Source 2 Panorama UI Engine** used in **Counter-Strike 2**.

Derived from:
- [SteamTracking/GameTracking-CS2 Panorama Repository](https://github.com/SteamTracking/GameTracking-CS2/tree/master/game/csgo/pak01_dir/panorama)
- [ValveResourceFormat (VRF / Source 2 Resource Decompiler)](https://github.com/ValveResourceFormat/ValveResourceFormat)
- [Valve Developer Community CS:GO / CS2 Panorama API](https://developer.valvesoftware.com/wiki/CSGO_Panorama_API)

---

## Quick Reference Links
- [Detailed Panorama JavaScript APIs (DemoAPI, GameStateAPI, GameEvents)](./references/panorama_api_reference.md)
- [ValveResourceFormat (VRF) & Binary Block Architecture](./references/vrf_resource_decompilation.md)

---

## 1. Source 2 Panorama UI Engine Architecture

Panorama is Valve's proprietary user interface framework designed specifically for games. It is built natively into `panorama.dll`, with bindings exposed to `client.dll` through Google V8 JavaScript runtime:

```
+-------------------------------------------------------------+
|                      CS2 Game Engine                        |
|                                                             |
|   [client.dll] <---> [panorama.dll] <---> [Google V8 JS]   |
|         ^                    |                    ^         |
|         |                    v                    |         |
|   C++ Game State       Panorama DOM         Custom Scripts  |
|   (Demo, Netcon)      (XML & CSS3)          (demo_viewer.js)|
+-------------------------------------------------------------+
```

### Core Subsystems
1. **Layout (`.xml` / `_c`)**: Declarative DOM structure declaring nested `<Panel>`, `<Label>`, `<Image>`, `<Button>`, and `<Movie>` tags.
2. **Styling (`.css` / `_c`)**: GPU-accelerated styling with flexbox layouts (`flow-children: right | down | none`), transforms (`transform: translate3d(...)`), opacity masks, blur filters, and transitions.
3. **Scripting (`.js` / `_c`)**: Custom V8 execution context with direct C++ bindings (`$`, `GameEvents`, `DemoAPI`, `GameStateAPI`).

---

## 2. ValveResourceFormat (VRF) & Resource Decompilation

In production Counter-Strike 2 builds, all Panorama files in `pak01_dir.vpk` are pre-compiled into binary resource containers with the `_c` suffix:

| Source File | Compiled Resource | Decompiler Output (VRF) |
| :--- | :--- | :--- |
| `demo_viewer.xml` | `demo_viewer.vxml_c` | KeyValues3 (KV3) XML AST node tree |
| `csgostyles.css` | `csgostyles.vcss_c` | KeyValues3 serialized CSS selector rules |
| `demo_viewer.js` | `demo_viewer.vjs_c` | Bytecode / UTF-8 string tables |
| `crosshair.vmat` | `crosshair.vmat_c` | Binary Material description |
| `bullet_tracer.vpcf`| `bullet_tracer.vpcf_c` | Particle System Compute Graph |

### Resource Container Binary Format
Source 2 resources begin with a 16-byte header:
- `FileSize` (uint32)
- `HeaderVersion` (uint16 = `0x000C`)
- `Version` (uint16 = `0x0001`)
- 4-byte block directory offset pointing to four-character block tags:
  - **`REDI` (ResourceEditInfo)**: Tracks compiler arguments, source dependencies, and sub-resource pointers.
  - **`DATA`**: Primary compiled asset payload (usually encoded as binary KV3).
  - **`NTRO`**: Introspection schema for untyped struct fields.

---

## 3. Panorama JavaScript Core APIs

Panorama executes client scripts in a sandboxed V8 engine. Built-in global host objects include:

### A. Demo Playback API (`DemoAPI`)
Critical for sub-tick replay analysis and demo scrubbing:
```javascript
// Querying playback state
let currentTick = DemoAPI.GetPlaybackTick();
let totalTicks  = DemoAPI.GetTotalTicks();
let isPaused    = DemoAPI.IsPlaybackPaused();
let timescale   = DemoAPI.GetTimescale();

// Controlling transport
DemoAPI.SetPlaybackTick(targetTick);
DemoAPI.PausePlayback();
DemoAPI.ResumePlayback();
DemoAPI.SetTimescale(0.5); // Slow-mo analysis
```

### B. Game Events Listener (`GameEvents`)
Subscribes to engine network events broadcast during demo playback:
```javascript
GameEvents.Subscribe("player_hurt", function(eventData) {
    let attackerId = eventData.attacker;
    let victimId   = eventData.userid;
    let damage     = eventData.dmg_health;
    let hitgroup   = eventData.hitgroup; // 1 = Head (Apex Joint #6)
    $.Msg(`[COMBAT] Attacker: ${attackerId} hit Victim: ${victimId} for ${damage} HP (Hitgroup: ${hitgroup})`);
});

GameEvents.Subscribe("weapon_fire", function(eventData) {
    let weapon = eventData.weapon;
    let isSilenced = eventData.silenced;
    $.Msg(`[FIRE] Actuation: ${weapon} at tick ${DemoAPI.GetPlaybackTick()}`);
});
```

### C. Console & System Execution (`GameInterfaceAPI`)
Executes developer console commands or queries engine configuration:
```javascript
GameInterfaceAPI.ConsoleExec("getpos_exact");
GameInterfaceAPI.ConsoleExec("demo_gototick 27703");
let sens = GameInterfaceAPI.GetSettingString("sensitivity");
```

---

## 4. Building Native Panorama Replay Overlays

To create clean, native Source 2 in-engine visuals:

### Layout (`subtick_hud.xml`)
```xml
<root>
    <styles>
        <include src="s2r://panorama/styles/csgostyles.vcss_c" />
        <include src="s2r://panorama/styles/subtick_hud.css" />
    </styles>
    <scripts>
        <include src="s2r://panorama/scripts/subtick_hud.js" />
    </scripts>

    <Panel class="SubTickRoot" hittest="false">
        <!-- Top Sub-Tick Micro-Slice Bar (y = 96px, below scoreboard) -->
        <Panel id="SubTickBarContainer" class="SubTickCard">
            <Label id="SubTickTitle" class="CardTitle" text="SUB-TICK TIMING // 15.625 ms DISCRETE INTERVAL" />
            <Panel id="SubTickTrack" class="Track">
                <Panel id="SubTickFill" class="Fill" />
                <Panel id="SubTickNeedle" class="Needle" />
            </Panel>
            <Label id="SubTickCallout" class="Callout" text="+attack actuation @ 12.18 ms (w = 0.780)" />
        </Panel>

        <!-- Anomaly Card (Right side, y = 140px) -->
        <Panel id="AnomalyCard" class="AnomalyAlert">
            <Label id="AnomalyTitle" text="✓ KINEMATIC INTEGRITY VERIFIED" />
            <Label id="AnomalyDesc" text="sv_maxunlag: 200.0ms | Rewind: -47.6ms" />
        </Panel>
    </Panel>
</root>
```

### Styling (`subtick_hud.css`)
```css
.SubTickRoot {
    width: 100%;
    height: 100%;
}

.SubTickCard {
    horizontal-align: center;
    vertical-align: top;
    margin-top: 96px;
    width: 480px;
    background-color: #090d16ee;
    border: 1px solid #38bdf8;
    border-radius: 4px;
    padding: 8px 12px;
    flow-children: down;
}

.CardTitle {
    font-family: Stratum2, "Segoe UI", sans-serif;
    font-size: 11px;
    font-weight: bold;
    color: #818cf8;
}

.Track {
    width: 100%;
    height: 14px;
    background-color: #0f172a;
    border: 1px solid #334155;
    margin-top: 4px;
}

.Fill {
    height: 100%;
    background-color: #0284c7;
    width: 78%;
}

.Needle {
    width: 3px;
    height: 100%;
    background-color: #ffffff;
    horizontal-align: left;
    margin-left: 78%;
}
```

---

## 5. Development & Content Mounting Rules

1. **CS2 `-insecure` Mode**: Custom Panorama assets can be mounted directly into `game/csgo/panorama/` when running with `-insecure`.
2. **Panorama Debugger**: Open developer console and execute `panorama_debugger 1` to inspect live panel hierarchies, computed CSS styles, and event triggers.
3. **Hot-Reloading**: Use `panorama_reload` to reload XML and CSS changes without restarting the CS2 client.
4. **HLAE Panorama Styling**: For clean cinematic recording or disabling indicators (like TrueView text):
   ```text
   mirv_panorama panelstyle panelId=trueview_row opacity=0
   ```
