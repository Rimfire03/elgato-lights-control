# Elgato Lights Control (plugin OBS)

Plugin natif OBS Studio (C++ / Qt6) qui pilote les lampes Elgato (Key Light, Key Light Air, Ring Light, Light Strip…)
via leur API HTTP locale (port 9123, la même que celle d'Elgato Control Center).

## Fonctions

- **Lumières** : recherche automatique sur le réseau local (ou ajout par IP), puis pour chaque lampe : allumée/éteinte,
  luminosité (1–100 %), température (2900–7000 K), renommer, identifier (clignote), supprimer.
- **Groupes** : regroupe plusieurs lampes et les pilote ensemble.
- **Automatisations** : « si <condition> alors <lampe/groupe> <état> », avec « sinon » optionnel.
  Conditions : stream en cours, enregistrement en cours, caméra virtuelle active, scène active = X.
  Un état = allumer/éteindre/inchangé + luminosité et/ou température optionnelles.

Configuration enregistrée dans `%APPDATA%\obs-studio\plugin_config\obs-elgato-lights\config.json`.
Le dock « Elgato Lights » apparaît dans OBS (menu *Docks*).

## Compilation (Windows)

Prérequis : Visual Studio 2022 (ou Build Tools) avec « Développement Desktop en C++ », CMake ≥ 3.28.

```powershell
cmake --preset windows-x64
cmake --build --preset windows-x64 --config RelWithDebInfo
```

Installation : copier `build_x64\rundir\RelWithDebInfo\obs-elgato-lights\` dans
`C:\Program Files\obs-studio\` (le dossier contient `obs-plugins\64bit\*.dll`), puis relancer OBS.
