# Elgato Lights Control (plugin OBS)

[![Buy Me A Coffee](https://img.shields.io/badge/Buy%20Me%20A%20Coffee-Donate-orange.svg?logo=buy-me-a-coffee&logoColor=white)](https://buymeacoffee.com/slide598b)

Plugin natif OBS Studio (C++ / Qt6) qui pilote les lampes Elgato (Key Light, Key Light Air, Ring Light, Light Strip…)
via leur API HTTP locale (port 9123, la même que celle d'Elgato Control Center).

## Fonctions

- **Lumières** : recherche automatique sur le réseau local (ou ajout par IP), puis pour chaque lampe : allumée/éteinte,
  luminosité (1–100 %), température (2900–7000 K), renommer, identifier (clignote), supprimer.
- **Groupes** : regroupe plusieurs lampes et les pilote ensemble.
- **Automatisations** : « si <condition> alors <lampe/groupe> <état> », avec « sinon » optionnel.
  Conditions : OBS est ouvert, OBS est en cours de fermeture, stream en cours, enregistrement en cours, caméra virtuelle active, scène active = X.
  Un état = allumer/éteindre/inchangé + luminosité et/ou température optionnelles.

Configuration enregistrée dans `%APPDATA%\obs-studio\plugin_config\obs-elgato-lights\config.json`.
Le dock « Elgato Lights » apparaît dans OBS (menu *Docks*).

## Support et signalement de bugs

Les bugs et demandes d'évolution se signalent dans les
[Issues du dépôt](https://github.com/Rimfire03/elgato-lights-control/issues). Merci d'indiquer la version du plugin,
la version d'OBS, le système, le modèle de lampe et, si possible, les lignes `obs-elgato-lights` du log OBS
(Aide > Fichiers de log).

## Transparence : développement assisté par IA

Ce plugin a été développé avec l'assistance de **Claude Code (Anthropic)** : l'IA a rédigé la majeure partie du code
(client de l'API Elgato, interface Qt, moteur de règles, installateur, scripts de build et de packaging) sous la
direction de l'auteur, qui a défini les besoins, relu et testé le résultat sur son installation d'OBS.

## Mentions

Plugin tiers, indépendant : il n'est ni affilié ni approuvé par OBS Project ni par Elgato / Corsair.
« OBS Studio » et « Elgato » sont des marques de leurs propriétaires respectifs.

## Soutenir le projet

Si ce plugin vous est utile, vous pouvez m'offrir un café :

<a href="https://buymeacoffee.com/slide598b" target="_blank"><img src="https://cdn.buymeacoffee.com/buttons/v2/default-yellow.png" alt="Buy Me A Coffee" height="50"></a>

## Mises à jour

Au démarrage d'OBS, le plugin interroge la dernière release GitHub (`Rimfire03/elgato-lights-control`). Si une version plus
récente existe, une fenêtre propose de mettre à jour (ou d'ignorer cette version). Sous Windows, l'installateur est
téléchargé (SHA-256 vérifié), puis il attend la fermeture d'OBS, remplace le plugin et relance OBS. Sous macOS, la
page de la release s'ouvre. Désactivable dans le dock (case « Vérifier les mises à jour »).

La release doit contenir l'asset `Install-Elgato-Lights-<version>.exe` (généré dans `dist\` par
`tools\Package-Release.ps1`) et le dépôt doit être public pour que l'API GitHub réponde sans authentification.

## macOS

Le code est portable (Qt6 + API OBS) ; le plugin se compile en binaire universel (arm64 + x86_64) avec le preset
`macos` (Xcode 16+, CMake ≥ 3.28) ou via la CI GitHub Actions (`.github/workflows`, job `macos-build`),
qui produit un installeur `.pkg`.

```bash
cmake --preset macos
cmake --build --preset macos --config RelWithDebInfo
```

Installation manuelle : copier `obs-elgato-lights.plugin` dans
`~/Library/Application Support/obs-studio/plugins/`, puis relancer OBS.

Notes macOS :
- Le plugin n'est pas signé/notarisé tant que les secrets Apple Developer (`MACOS_SIGNING_*`) ne sont pas
  configurés dans le dépôt GitHub ; passer `codesign`/`notarize` à `true` dans `build-project.yaml` ensuite.
  Sans signature, retirer la quarantaine : `xattr -dr com.apple.quarantine <plugin ou .pkg>`.
- macOS 15+ demande l'autorisation « Réseau local » à OBS à la première recherche de lampes.

## Compilation (Windows)

Prérequis : Visual Studio 2022 (ou Build Tools) avec « Développement Desktop en C++ », CMake ≥ 3.28.

```powershell
cmake --preset windows-x64
cmake --build --preset windows-x64 --config RelWithDebInfo
```

Release (zip + installateur signé) : `tools\Package-Release.ps1` → `dist\`.
Installation : l'installateur copie la DLL dans `C:\ProgramData\obs-studio\plugins\obs-elgato-lights\bin\64bit\` et
crée une entrée « Ajout/Suppression de programmes » (désinstallation propre).

