# Graphismes et fontes d'origine

Aperçu général : `overview.png`.

## D'où viennent ces images
Le programme principal garde ses images **compactées** et les décompacte au moment de les afficher (« realtime picture depacking » dans le texte de la démo).
La démo a donc été arrêtée dans Hatari à chaque écran (`tools/schedule.py` + `tools/hrun.sh`), et les pixels ont été relus **directement dans la mémoire vidéo**.
Ce sont les indices de couleur exacts, en basse résolution 320 px de large et 4 plans, et non une capture recolorée.
La palette est déduite de la capture d'écran de Hatari, **ligne par ligne**, car presque tous les écrans changent les couleurs en cours de balayage (rasters).
`tools/stscreen.py` aligne la RAM vidéo sur la capture : 0 pixel incohérent pour les logos et le menu.

## Formats
Pour chaque image `<nom>` :
| Fichier | Contenu |
|---|---|
| `<nom>_indexed.png` | **pixels d'origine** (PNG 16 couleurs indexé). La palette est la couleur la plus fréquente de chaque index sur l'image. |
| `<nom>.png` | rendu **tel qu'affiché**, rasters compris |
| `<nom>.json` | adresse RAM source, cadre de découpe, palette (`#rrggbb` + mots couleur STE `$RGB`), indices « raster », et `per_line_palette` (couleurs des indices raster pour chaque ligne) |
| `<nom>.pi1` | écrans complets uniquement : fichier **Degas** (palette + 32 000 octets planaires), lisible par les outils ST |

## Contenu
- `pictures/`
  - `loading_screen*` : l'écran de chargement (l'oreille, « by Aeon »), 16 couleurs. Les mots LOADING et DECRUNCHING sont dans la même image, sur les index 13 à 15, révélés par raster.
  - `logo_an_aura_production`, `logo_high_fidelity_dreams`, `logo_code_and_design_aeon` : les trois logos découpés.
    Le dégradé or de « AEON » et le bleu de « CODE AND DESIGN » sont des rasters sur les mêmes index, voir le JSON.
  - `fullscreen/` : les écrans complets correspondants (avec les filets), en PNG et en `.pi1`.
- `menu/`
  - `menu_select_song` : le menu entier, **218 lignes**, car la démo ouvre la bordure basse.
    L'entrée en cours de lecture est dessinée avec le plan 3 en plus (index +8), ce qui lui donne la palette rose.
    Le curseur (les deux filets) est un raster sur la couleur 0, pas des pixels.
  - `words/` : chaque mot séparé (SELECT SONG, les 8 titres, ENIGMA en version normale et en version « en lecture », LOADING, DECRUNCHING).
    Les écrans « LOADING / DECRUNCHING + nom du morceau » sont composés à partir de ces mêmes mots (exemple : `screen_*_act_of_impulse`).
- `fonts/font8x8.*` : la fonte du text-spreader, **44 glyphes 8×8 sur 1 plan**, dans l'ordre ASCII de 48 `0` à 91 `[`.
  - `font8x8.bin` : 8 octets par glyphe.
  - `font8x8_strip.png` : bande de 352×8 pixels.
  - `font8x8_sheet.png` : planche agrandie, avec chaque code et le caractère affiché.
  - En RAM, la fonte est stockée **pré-décalée** : en $1EC9E pour les décalages 0 à 7 et en $2029E pour les décalages 8 à 15, soit 128 octets par caractère.
    Les grands titres CREDITS, FEATURES, AURA et LET'S WRAP ne sont pas des images : ils sont composés avec cette fonte, à partir de l'art en points présent dans le texte (`texts/hfd_texts.txt`).
- `sprites/`
  - `bubble16.*` : la bulle 16×16 sur 1 plan, pré-décalée en RAM à $2189E.
  - `disconnect_module_msg.*` : le message 128×6 « DISCONNECT MODULE! », affiché par le stage 2 du boot si une cartouche est branchée.
- `textpage/quads_frame_*` : une frame du fond des pages de texte.
  Les « 4 big vector-quaders with voice-analysers » annoncés dans le texte sont ces **4 bandes**.
  Ce sont des quads qui tournent autour de l'axe horizontal, dessinés en temps réel dans les plans 0 et 3.
  Les rayures verticales sont les analyseurs des 4 voies et les couleurs sont des rasters. C'est du code, pas une image : la frame sert de référence visuelle.

## Refaire une capture
```bash
tools/schedule.py out/x "9000:0x39" "9300"
```
Cette commande prépare l'appui sur ESPACE au VBL 9000 (entrée dans le menu) et une capture au VBL 9300.
```bash
tools/hrun.sh out/x 9400 "" ""
```
Elle lance la démo. Voir ensuite `tools/export_pic.py` (fonction `export`) pour produire les PNG, le JSON et le `.pi1`.
