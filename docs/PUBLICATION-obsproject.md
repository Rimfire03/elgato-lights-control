# Publier sur obsproject.com (aide-mémoire, pas un texte à coller)

Politique : https://obsproject.com/forum/threads/forum-resource-and-ip-policy.195980/
Soumission : forum OBS > Resources > catégorie « OBS Studio Plugins » > *Add resource* (compte forum requis).

La description doit être **rédigée par l'auteur, en anglais**. Les descriptions générées par IA sont déconseillées.

## Checklist avant soumission

- [ ] Compte sur le forum OBS
- [ ] Nom de la ressource sans « OBS » (ex. : « Elgato Lights Control »)
- [ ] Release stable testée (recherche réseau, groupes, règles stream/scène) - voir « Tests » ci-dessous
- [ ] 2 à 3 captures d'écran du dock (onglets Lumières, Groupes, Automatisations) ou une courte vidéo
- [ ] Lien de téléchargement : GitHub Releases (hébergeur accepté)
- [ ] Lien vers le code source (GPL v2+) : https://github.com/Rimfire03/elgato-lights-control
- [ ] Déclaration IA dans la description (outil utilisé et rôle), cf. section « Transparence » du README

## Plan de la description (à rédiger soi-même)

1. Ce que fait le plugin en 2-3 phrases (contrôle des Elgato Key Light / Light Strip depuis un dock OBS).
2. Fonctions : réglage individuel, groupes, automatisations (conditions stream / enregistrement / caméra
   virtuelle / scène).
3. Installation (Windows : installateur ; macOS : .pkg) et prérequis (OBS 31+, lampes sur le même réseau).
4. Utilisation rapide : Docks > Elgato Lights > Rechercher sur le réseau.
5. Déclaration IA.
6. Support : Issues GitHub (ou le fil de discussion de la ressource).
7. Optionnel et bref : lien Buy Me a Coffee.

## Tests à faire avant de soumettre

- Une vraie lampe : recherche automatique, on/off, luminosité, température.
- Groupe de 2 lampes ou plus.
- Règle « si stream en cours » (then / else) et règle « scène active ».
- Installation et désinstallation propres (Applications installées).
- macOS : build testé sur un vrai Mac (non fait à ce jour).
