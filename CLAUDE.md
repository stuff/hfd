# High Fidelity Dreams — notes pour reprendre le travail

Étude et portages du music disk Atari ST *High Fidelity Dreams* (Aura, 1992). La documentation et les échanges sont en français.

## Où regarder
- `README.md` : vue d'ensemble et pistes pour la suite.
- `rip/SPEC.md` : **le comportement de la démo**, mesuré et indépendant de la plateforme (timings à la VBL, fondus, menu, pages de texte, replay, notes Amiga et Windows/Mac). À lire avant tout portage.
- `rip/README.md` : l'extraction (disquette, musiques, graphismes, code 68000) et les outils Hatari.
- `ports/web/README.md` : le portage web (complet), son architecture et ses tests.

## Méthode
- La référence est la démo elle-même, enregistrée dans Hatari image par image : `rip/out/rec1/video.mkv` (image n = VBL n, cadre 416×276, écran ST en (48, 29)). Ce fichier n'est pas versionné. Pour le recréer :
  1. `rip/tools/setup_hatari.sh` (compile Hatari, environ 2 min 30) ;
  2. puis les trois commandes de « Vidéo de référence » dans `rip/README.md`.
- Pour vérifier un portage, rendre les scènes hors écran et les comparer image par image à la vidéo. Dans le port web : `ports/web/scripts/render.ts` puis `scripts/compare.py`, qui masque le voyant disque de Hatari. L'intro (4 753 images) et le menu (1 396 images) donnent 0 pixel de différence.
- Pour comprendre une routine, lire le code plutôt que deviner depuis la vidéo : le profileur de Hatari (`profile on` puis `profile save`) donne toutes les adresses exécutées, et `rip/tools/rdis.py` les désassemble.

## Pièges connus
- Pour enregistrer une vidéo, Hatari doit tourner avec `--frameskips 0`. En mode accéléré, il ne rafraîchit sinon l'image qu'une VBL sur 6.
- Hatari n'est pas déterministe d'une exécution à l'autre pour la musique (2 lignes de partition d'écart observées). Les analyseurs des pages de texte ne se comparent donc pas image par image ; tout le reste, si.
- Cette build de Hatari n'a pas libpng : enregistrer en AVI BMP (environ 21 Go), puis convertir avec `rip/tools/avisplit.py`.
- La démo a besoin du son émulé, sinon elle se bloque au décompactage (`hrun.sh` s'en charge).
- Le replay NoisePacker de la démo range ses voies à l'envers (la bande d'analyseur k suit la voie 3 − k), et son effet 7 correspond au A de ProTracker.
