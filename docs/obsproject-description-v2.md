# Fiche obsproject.com, version 2 (courte, à relire et ajuster avec vos mots)

**Nom** : Elgato Lights Control | **Version** : 1.0.3 | **Licence** : GPL v2+
**Code source** : https://github.com/Rimfire03/elgato-lights-control

## Description (EN)

I stream on Twitch with two Elgato Key Lights and got tired of switching them by hand or leaving the Control Center
window open. This plugin adds an "Elgato Lights" dock to OBS to control them, and to change them automatically when
OBS does something (going live, recording, switching scene...).

**What it does**
- Finds Elgato lights on your network (or add one by IP). A light is remembered by its serial number, so your groups
  and rules keep working if the router gives it a new IP address.
- Per light: on/off, brightness, color temperature, rename, "Identify" (blinks it).
- Groups of lights controlled together.
- Rules: "if streaming / recording / streaming or recording / virtual camera on / scene is X / OBS opens / OBS closes,
  then set this light or group to this state", with an optional "else" state.
- Checks for a new plugin version at startup (can be disabled).

**Install**: Windows installer or zip, macOS .pkg, from the GitHub release. Then open Docks > Elgato Lights.
Needs OBS 31+ and the lights on the same network as the PC.

**Tested**: two Key Lights on Windows; the macOS build was tested on another Mac. Other Elgato models use the same
API but are untested, and Light Strip color is not supported yet.

**How it was made**: I designed and tested this plugin, and the code was written with Claude Code (an AI assistant),
which also handles the build and release scripts. The full source is on GitHub if you want to review it.

Bug reports: https://github.com/Rimfire03/elgato-lights-control/issues

Independent plugin, not affiliated with or endorsed by the OBS Project or Elgato / Corsair.
