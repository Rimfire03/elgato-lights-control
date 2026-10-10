# Brouillon de la fiche obsproject.com (à relire et réécrire par l'auteur)

> La politique d'OBS décourage les descriptions générées par IA : ce texte est un **brouillon de travail**.
> Relisez-le, reformulez-le avec vos mots et corrigez ce qui ne correspond pas à votre usage avant de le soumettre.

**Nom de la ressource** : Elgato Lights Control
**Catégorie** : OBS Studio Plugins
**Version** : 1.0.3 | **Plateformes** : Windows (testé), macOS universel (compilé par la CI, non testé sur Mac réel)
**Licence** : GPL v2 ou ultérieure | **Code source** : https://github.com/Rimfire03/elgato-lights-control

## Description

Elgato Lights Control adds a dock to OBS Studio to control Elgato Key Light–family lights over your local network,
the same lights you manage with Elgato Control Center. It also lets you trigger light changes automatically from OBS
events (streaming, recording, virtual camera, active scene).

**Features**
- Automatic discovery of Elgato lights on your LAN, or add a light by IP address. Lights are tracked by serial
  number: if a light's IP address changes (DHCP), it is found again automatically and your groups and rules keep working
- Per-light control: on/off, brightness (1–100 %), color temperature (2900–7000 K), rename, "Identify" (blinks the light)
- Groups: control several lights at once
- Automation rules: "if *streaming / recording / streaming or recording / virtual camera is active / the active scene is X / OBS opens / OBS is closing* then set a light or a
  group to a given state", with an optional "else" state applied when the condition becomes false
- Startup update check (Windows: one-click update; macOS: opens the release page). Can be disabled in the dock.

**Installation**
- Windows: run `Install-Elgato-Lights-<version>.exe` with OBS closed. It installs to
  `C:\ProgramData\obs-studio\plugins\obs-elgato-lights` and adds an entry in *Installed apps* for clean removal.
- macOS: install the `.pkg` (unsigned; right-click > Open).
- Then open **Docks > Elgato Lights** in OBS and click *Search on network*.

**Requirements**: OBS Studio 31+ (64-bit), lights on the same local network as the PC.

**Known limitations**
- Tested with two Elgato Key Lights on Windows. Other models (Key Light Air, Ring Light, Light Strip) use the same
  API but have not been tested; Light Strip color (hue/saturation) is not supported yet.
- The group sliders do not reflect the current state of the member lights.
- macOS build is produced by CI and has not been tested on real hardware. macOS 15+ asks for "Local Network"
  permission on first discovery.

**Transparency about AI use**: this plugin was developed with the assistance of Claude Code (Anthropic), which wrote
most of the code (Elgato API client, Qt interface, rule engine, installer, build scripts) under my direction.
I defined the requirements and reviewed and tested the result with my own lights and OBS installation.

**Support / bug reports**: https://github.com/Rimfire03/elgato-lights-control/issues (or this resource's discussion tab).

This is an independent third-party plugin, not affiliated with or endorsed by the OBS Project or Elgato / Corsair.

If you find it useful, you can support the project: https://buymeacoffee.com/slide598b

## Captures d'écran à joindre

`docs/screenshots/` : `lights.png` (onglet Lumières), `groups.png` (Groupes), `automations.png` (Automatisations),
`rule-editor.png` (éditeur de règle).
