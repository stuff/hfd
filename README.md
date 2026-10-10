# High Fidelity Dreams

*High Fidelity Dreams* est un music disk Atari ST/STE d'**Aura** (code et design : Aeon), sorti en 1992 à la BMT-Party d'Essen.
Il contient 8 modules Amiga de Phenomena et Sanity.
Ce projet part de l'étude de la disquette d'origine pour porter la démo sur d'autres machines.

## Organisation

| Dossier | Contenu |
|---|---|
| [`rip/`](rip/) | Tout ce qui a été extrait de la disquette (`hifidrea.st`) : les 8 .mod d'origine, les graphismes et la fonte, les textes, le code 68000 désassemblé, et les outils pour piloter Hatari. Voir [rip/README.md](rip/README.md). |
| [`rip/SPEC.md`](rip/SPEC.md) | **La spécification du comportement de la démo**, indépendante de toute plateforme. C'est le point de départ de tout nouveau portage. |
| [`ports/web/`](ports/web/) | Le portage navigateur, en Vite + TypeScript. **Complet** : intro, pages de texte, menu, chargements, musique. Vérifié image par image contre la démo dans Hatari. Voir [ports/web/README.md](ports/web/README.md). |

Les portages ne partagent pas de code. Ce qu'ils partagent, ce sont les **données** de `rip/` (modules, bitmaps, palettes, RAM des pages de texte) et la spécification.
Chaque port lit ces données dans `rip/` ou les copie au moment du build, sans les dupliquer dans le dépôt.

## Pistes pour la suite
- **Windows / Mac** : le plus court est d'empaqueter le port web (Tauri ou Electron). Un port natif (C/C++ + SDL2) devra reprendre le lecteur MOD et la cadence 50 Hz du port web. Voir SPEC.md, section 9.
- **Amiga** : la cible la plus naturelle (50 Hz, Paula, Copper pour les rasters, blitter pour le spreader). Voir SPEC.md, section 9.
- **Vidéo de référence** : elle n'est pas versionnée (environ 390 Mo dans `rip/out/`). Elle se régénère en quelques minutes, voir « Vidéo de référence » dans rip/README.md. On en a besoin pour vérifier un port.
