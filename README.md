# High Fidelity Dreams

*High Fidelity Dreams* est un music disk Atari ST/STE d'**Aura** (code et design : Aeon), sorti en 1992 à la BMT-Party d'Essen.
Il contient 8 modules Amiga de Phenomena et Sanity.
Ce projet part de l'étude de la disquette d'origine pour porter la démo sur d'autres machines.

## Organisation

| Dossier | Contenu |
|---|---|
| [`rip/`](rip/) | Tout ce qui a été extrait de la disquette (`hifidrea.st`) : les 8 .mod d'origine, les graphismes et la fonte, les textes, le code 68000 désassemblé, et les outils pour piloter Hatari. Voir [rip/README.md](rip/README.md). |
| [`ports/web/`](ports/web/) | Le portage navigateur (en cours). |

Les portages ne partagent pas de code (JavaScript, puis peut-être C ou assembleur 68000 pour l'Amiga). Ce qu'ils partagent, ce sont les **données** de `rip/` : modules, bitmaps, palettes ligne par ligne et textes.
Chaque port lit ces données dans `rip/` ou les copie au moment du build, sans les dupliquer dans le dépôt.
