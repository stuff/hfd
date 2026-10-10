# High Fidelity Dreams — spécification du comportement

Ce document décrit ce que fait la démo, indépendamment de toute plateforme. C'est la référence pour écrire un portage (web, Windows/Mac, Amiga…).
Tout a été mesuré dans la vidéo de référence (Hatari, VBL par VBL) ou lu dans le code 68000. Le portage web (`ports/web/`) implémente tout ce qui suit, et l'on peut s'y reporter en cas de doute.

## 0. Conventions

- **Horloge** : tout avance par **VBL** de l'ST, soit 50,0533 Hz (PAL). Les numéros de VBL cités sont ceux de la vidéo de référence `out/rec1/video.mkv` : l'image n correspond à la VBL n, avec EmuTOS au démarrage. Voir « Vidéo de référence » dans [README.md](README.md).
- **Cadre** : 416×276 pixels, comme les captures de Hatari, bordures comprises. L'écran ST de 320×200 commence en (48, 29). « Ligne y » désigne une ligne de l'écran ST (y = 0 en haut des 200 lignes). Elle peut être négative (bordure du haut) ou ≥ 200 (bordure du bas).
- **Couleurs** : notées `0xRGB` avec des composantes **linéaires sur 4 bits** (0 = noir, 15 = plein), c'est-à-dire telles qu'affichées.
  Les mots de palette du STE ont un ordre de bits particulier (bit 3 = bit faible). Pour convertir un mot STE `$RGB` en linéaire, chaque quartet n devient `((n & 7) << 1) | (n >> 3)`.
  Les JSON de `graphics/` utilisent des `#rrggbb` où chaque composante vaut 17 × la valeur sur 4 bits.
- **Rasters** : la palette change d'une ligne à l'autre. Chaque ligne a donc sa propre palette de 16 couleurs. La bordure gauche/droite d'une ligne affiche la couleur 0 de cette ligne.
- **Plans** : basse résolution ST, 4 plans entrelacés (160 octets par ligne, 8 octets par groupe de 16 pixels). Index de couleur = bit du plan 0 | plan 1 << 1 | plan 2 << 2 | plan 3 << 3.

## 1. Les fondus

Un niveau de fondu j va de 0 (couleurs pleines) à 7 (noir).

- **Programme principal** (logos, menu, chargements, pages de texte) : chaque composante perd j sur l'échelle 3 bits de l'ST, `v3 = max(0, (n >> 1) − j)`, puis `n' = v3 << 1`. Le bit fin du STE est donc perdu dès que j > 0.
- **Loader** (écran de l'oreille uniquement) : même soustraction, mais le bit fin est conservé si `(n >> 1) − j ≥ 0`. Si la soustraction passe sous zéro, la composante tombe à 0.
- **Rythme** :
  - menu, chargements, pages de texte : un niveau toutes les **3 VBL** (7 → 0 ou 0 → 7, donc 21 VBL) ;
  - logos de l'intro : un niveau toutes les **2 VBL**, de 6 à 0 en entrée, et de 1 à 7 en sortie ;
  - oreille : voir 3.1.
- **Exceptions mesurées** :
  - Les **filets horizontaux** du menu et des écrans de chargement (3 lignes `fff aaa 666` sur la couleur 0) ne suivent pas la formule. Ils suivent cette table, par niveau j de 0 à 7 :
    `fff aaa 666 | ccc 888 666 | aaa 888 444 | 888 666 444 | 666 444 222 | 444 444 222 | 222 222 000 | 000 000 000`.
  - Le **contour blanc des entrées du menu** (couleur 5 = `ccc`) se comporte comme `fff` assombri d'un cran. Pendant un fondu, il vaut donc `fade(fff, max(j, 1))`, et non `fade(ccc, j)`.
- Les pages de texte ne calculent pas leur fondu : elles passent d'une table de couleurs précalculée à la suivante (voir 4).

## 2. Déroulé et commandes

| VBL (référence) | Écran |
|---|---|
| 0–545 | EmuTOS, puis chargement (rien à porter) |
| 546–2811 | Écran de l'oreille : LOADING (jusqu'à 972) puis DECRUNCHING |
| 2836 | Début de la musique (Enigma) |
| 3169–3776 | AN AURA PRODUCTION |
| 3798–4573 | HIGH FIDELITY DREAMS |
| 4704–5269 | CODE AND DESIGN AEON |
| 5295 → | Pages de texte, en boucle |

Les durées de l'oreille correspondent au chargement réel depuis la disquette. Le port web les raccourcit par défaut, en sautant des cycles entiers de pulsation (voir 3.1).

**Commandes** (c'est tout ce que la démo gère ; ÉCHAP, TAB, HELP et les autres touches ne font rien) :
- **ESPACE** fait passer des pages de texte au menu, et du menu aux pages de texte.
  - Dans les pages : ESPACE n'est lu qu'une fois le fondu d'entrée fini. Fondu de sortie de 21 VBL, 3 VBL de noir, puis le menu apparaît en fondu (21 VBL).
  - Dans le menu : fondu de sortie (21 VBL), puis les pages de texte reprennent **exactement là où elles s'étaient arrêtées**. Le texte et les lettres en vol sont figés pendant le menu, et la musique continue.
- **Menu** : ↑ / ↓ déplacent le curseur, qui réagit 1 VBL après l'appui. RETURN lance le morceau sous le curseur. Si c'est celui qui joue déjà, la démo le recharge quand même (LOADING, DECRUNCHING, puis il repart du début).

## 3. L'intro

Les images sont dans `graphics/pictures/` : PNG indexés, avec un JSON qui donne la palette et les couleurs mesurées ligne par ligne (`per_line_palette`).

### 3.1 L'oreille (`loading_screen_indexed.png`, palette `loading_screen.json`)
- Apparaît à la VBL 546. Fondu d'entrée du loader (bit fin conservé) : niveaux 6 à 546, 5 à 547, puis −1 toutes les 2 VBL (4 à 549, … 0 à 557).
- Fondu de sortie : niveau 1 à 2801, +1 toutes les 2 VBL jusqu'à 5 à 2809, puis noir à 2811.
- Les mots LOADING et DECRUNCHING sont superposés dans l'image, sur les lignes 189 à 199 : LOADING avec la couleur 15, DECRUNCHING avec la couleur 14. Le petit cadre à droite est en couleur 13 (`008`).
  - Le mot affiché **pulse** : une rampe triangulaire de 56 VBL, de `000` à `eee` puis retour, un niveau 3 bits toutes les 4 VBL, avec rouge, vert puis bleu décalés d'une VBL. Formule pour k = VBL − 546 (k ≥ 1) : `m = (k−1) mod 56`, `n = m >> 2`, `sub = m & 3`. En montée (n < 7), le niveau vaut n et chaque composante i vaut niveau + (sub ≥ i ? 1 : 0). En descente, le niveau vaut 7 − (n − 7) et chaque composante i vaut niveau − (sub ≥ i ? 1 : 0). La couleur vaut chaque composante × 2.
  - À la VBL 973, la pulsation passe de la couleur 15 à la couleur 14 sans perdre sa phase. Le mot caché prend la couleur du fond (`004`).
  - Les couleurs 13 à 15 de ces lignes **ne subissent pas le fondu** (la pulsation est écrite après lui). Seul le mot caché, qui vaut le fond, le subit.

### 3.2 AN AURA PRODUCTION (`fullscreen/screen_an_aura_production_indexed.png`)
- Trois bandes de lignes, une par mot : 53–72, 84–115 et 127–146. Le mot k apparaît en fondu à partir de la VBL 3169 + 24k (6 → 0, un niveau toutes les 2 VBL).
- Fondu de sortie commun à partir de 3763 (niveau 1, +1 toutes les 2 VBL).

### 3.3 HIGH FIDELITY DREAMS (`fullscreen/screen_high_fidelity_dreams_wide_indexed.png`, 416 px de large)
- **Ce sont les seules lignes de l'intro où la démo ouvre les bordures latérales** : les filets des lignes 14–21 et 178–185. L'image « wide » a été reconstituée bordures comprises (x = 0 au bord gauche du cadre de 416 px).
- Les filets glissent de 2 px par VBL à partir de 3798, pendant 142 VBL :
  - le bord gauche du filet du haut (pixels x 66 à 415 de l'image) est en `350 − 2t` ;
  - le bord droit du filet du bas (x 0 à 349) est en `65 + 2t`.
  - Ils apparaissent en fondu pendant le glissement (6 → 0 à partir de 3798, toutes les 2 VBL).
- Les mots HIGH (lignes 37–68), FIDELITY (84–115) et DREAMS (131–162) apparaissent en fondu à partir de 3972 + 6k.
- Fondu de sortie commun à partir de 4560.
- Couleur 8 : `000` sur les lignes des filets, `888` ailleurs (voir le JSON).

### 3.4 CODE AND DESIGN AEON (`fullscreen/screen_code_and_design_aeon_indexed.png`)
- Quatre bandes : CODE (7–38), AND (42–61, avec le filet raster des lignes 48–55 sur la couleur 0), DESIGN (65–96) et AEON (128–191, dégradé or).
- Chaque bande apparaît avec un « flash », à partir de 4704 + 12k. Les niveaux, un par VBL, sont `0 1 2 3 4 5 0 1 2 3 4 5 4 3 2 1 0`. Avant le flash, la bande est noire.
- Fondu de sortie commun à partir de 5256.

Pour chaque écran, chaque bande a son propre niveau de fondu, appliqué à ses lignes.

## 4. Les pages de texte

Le code 68000 est dans `extracted/disasm/textpage_4000.s`. La RAM de $12000 à $25B00, prise juste avant les pages (VBL 5280), est dans `extracted/textpage/` : toutes les données et l'état initial s'y trouvent, aux adresses ci-dessous. Le portage web la lit telle quelle, ce qui est le plus sûr.

Ordre des opérations à chaque VBL (interruption en $12A22) :
1. rasters ($12A36) ;
2. spreader ($1E552) ;
3. analyseurs ($1E250) ;
4. replay de la musique.

**Double tampon** : deux écrans en $60000 et $68000. Pendant une image, on voit l'écran dessiné à la VBL précédente, avec les rasters réglés à la VBL courante.

### 4.1 Les 4 bandes (rasters)
- Ce ne sont pas des polygones mais des **rasters précalculés**. Une chaîne de 12 interruptions Timer B (modèles en $12B52, 40 octets chacun) change les couleurs 0, 1 et 8 (8 = 1) après un nombre de lignes donné. Les autres couleurs (2–7, 9–15) prennent toutes la couleur du texte.
- **Enregistrement** (26 octets, pointeur en $12B32) :
  - 4 × (y, hauteur) pour les analyseurs ;
  - 1 octet : nombre de lignes avant la première interruption ;
  - puis 6 × [octet de couleurs (quartet haut = teinte de l'interruption paire, quartet bas = de l'impaire), compteur de lignes de la paire, compteur de l'impaire]. Le dernier compteur impair n'est pas dans l'enregistrement.
  - Un compteur à 0 signifie « jamais » (256 lignes). La 12ᵉ interruption arrête le timer.
- **Teintes** : une table de 38 octets = 9 paires de mots STE (couleur 0, couleur 1), puis la couleur du texte en +$24. Il y a 8 tables, du noir ($1E120) au plein ($1E22A). Le pointeur de table est en $12A54.
- **Script** ($12D30 à $12F64, entrées de 12 octets) : début, fin, nombre de répétitions, pas (signé, en octets).
  - On parcourt les enregistrements de début à fin par pas. À la fin, on recommence tant que le compteur n'est pas épuisé (`subq.w` / `bhi`), puis on passe à l'entrée suivante.
  - Une entrée de pas 0 fige un enregistrement (des pauses de 100 à 800 VBL).
  - Après la dernière entrée, le script boucle sur $12D48.
- **Fondus** : la boucle principale ajoute 38 au pointeur de table toutes les 3 VBL, 7 fois (entrée), et le retire de la même façon en sortie.

### 4.2 Le text-spreader
- **Bulle** : sprite 16×16 sur le plan 1, pré-décalé en $2189E (16 décalages × 64 octets), écrit par MOVE à (x − 4, y − 4).
  - Position lue sur un chemin de 600 points : $21C9E à $2214E, 2 octets par point (x − 32, y). Le pointeur est en $1E73C et avance d'un point par VBL.
  - Elle est dessinée à la position de la VBL précédente.
- **33 emplacements de lettre** en $1E9C8, 22 octets chacun : glyphe.l (−1 = vide), dx.l, x.l, dy.l, y.l (en virgule fixe 16.16), compteur.w.
  - À chaque VBL :
    - si le compteur ≠ 0, x += dx et y += dy ;
    - puis compteur −1.
  - À −2, l'emplacement reçoit la lettre suivante, avec le compteur à 64. Elle part de la position actuelle de la bulle avec `d = ((cible − départ) << 16) >> 6`, donc elle arrive en 64 VBL.
  - En vol, la lettre est OR-ée sur le plan 1, qui est effacé à chaque VBL. Avec un compteur ≤ 0, elle est dessinée sur le plan 2, que rien n'efface : c'est le texte posé.
  - Au départ, les compteurs valent 0, 2, 4…, ce qui donne une nouvelle lettre toutes les 2 VBL.
  - Une pause (compteur long en $1E9C4) laisse des emplacements vides. Elle vaut 10 au départ.
- **Fonte** : 8×8, pré-décalée en $1EC9E (44 caractères × 128 octets, codes 48 à 91) et en $2029E (version découpée en octets). Ce qu'il faut retenir : le glyphe est OR-é au pixel x près.
- **Flux de texte** (pointeur en $1E884, départ en $2214E, 14 765 octets) :
  - `0, x, y, p` : nouvelle ligne. p est le pas des caractères, signé (10 normalement). Si p < 0, la ligne est stockée à l'envers et s'écrit de droite à gauche, avec x = 322 − x + p.
  - `9, n` : pause de 30 × n.
  - `$BD, n` : effacement de la page (plan 2), 2 lignes par VBL à partir de la ligne 198 − n, et pause de n / 2.
  - `$FF` : retour au début du texte.
  - Un octet < $30 est une espace (pas de glyphe, mais x avance). Sinon, c'est le glyphe octet − $30.
  - La fonte affiche `;` comme « . », `<` comme « ( », `=` comme « ! », `>` comme « ) », `@` comme un bloc et `[` comme « - ».

### 4.3 Les analyseurs de voies
- Chaque bande d'analyseur a un niveau (mot en $1E2E4 + 4k, suivi de y et de la hauteur, recopiés depuis l'enregistrement).
- **La bande k suit la voie 3 − k du module** : le replay NoisePacker range ses voies à l'envers.
- À chaque VBL, d'après l'état du replay à la VBL précédente :
  - si une ligne de partition vient d'être lue et qu'elle contient une note, un instrument ou un effet C, le niveau prend le volume de la voie ;
  - sur les autres ticks, un effet 5, 6 ou A (glissements de volume) remet aussi le niveau au volume de la voie ;
  - ensuite, si le niveau ≠ 0, il perd 2 sans descendre sous 2.
- **Dessin** : sur les lignes y..y + hauteur, 8 mots longs par ligne à partir de l'octet 22 de la ligne, écrits par MOVE. Le mot haut va sur le plan 3, le bas sur le plan 0 du groupe suivant : c'est un bloc continu de 32 pixels.
  - Le motif dépend de `niveau − 1` : les bits 4–5 choisissent la largeur (4 variantes), les bits 1–3 choisissent l'un des 8 motifs de bord. Les constantes sont en $1E314–$1E3E0, et dans `ports/web/src/scenes/textpage.ts`.

## 5. Le menu

- Image : `graphics/menu/menu_select_song_indexed.png`, 218 lignes (la démo ouvre la bordure du bas).
- **Entrées** : l'entrée k occupe les lignes 37 + 23k à 37 + 23k + 19.
- **Palette de base** :
  - couleurs 1–5 des entrées : `004 226 448 66a ccc`, couleur 7 = `000` ;
  - en-tête SELECT SONG (lignes 7–26) : `840 a62 c84 ea6 fff` ;
  - filets aux lignes 2–4 et 30–32 (couleur 0) : `fff aaa 666`.
- **Entrée sous le curseur** : couleurs 1–5 = `226 448 66a 88c fff`. Sur ses lignes +7 à +12, la couleur 0 vaut `fff 226 448 66a 88c fff` (le filet du curseur, qui traverse les bordures), et les couleurs 7 et 15 valent `fff`.
- **Entrée en lecture** : dessinée avec le plan 3 en plus (index + 8), soit les couleurs 9–13 = `626 848 a6a c8c fff`. Elle est rose, qu'elle soit sous le curseur ou non. Le curseur n'y change que la couleur 0 et la couleur 15.
- Les entrées sont toutes centrées. On réutilise leurs mots tels quels pour les écrans de chargement.

## 6. Changement de morceau (RETURN)

Temps en VBL depuis l'appui :

| t | Événement |
|---|---|
| 0 | La musique baisse linéairement jusqu'au silence en 87 VBL |
| 87 | Fondu de sortie du menu |
| 108 | Écran de chargement (fondu d'entrée) : filets aux lignes 67 et 135, LOADING (orange, ligne 78, centré), nom du morceau en rose à la ligne 106 |
| 288 | LOADING s'éteint en fondu (21 VBL, seulement sur ses lignes), puis DECRUNCHING apparaît (21 VBL) |
| 852 | Fondu de sortie |
| 884 | Le nouveau morceau démarre, et le menu revient en fondu avec lui en rose |

Les temps 288 et 852 viennent du chargement réel de Just Spank It depuis la disquette. Ils varient selon la position du morceau sur la disquette. Le port web les raccourcit par défaut (`?authentic` pour les retrouver).

## 7. La musique

- Les 8 fichiers de `music/` sont les modules d'origine (voir README.md, section 1). La démo les joue avec un replay NoisePacker 2, piloté par la VBL : **un tick par VBL, à 50,05 Hz**. Aucun module n'utilise le tempo CIA.
- Son d'origine : émulation logicielle de Paula (« HiFi-Paula », 50 kHz en DMA sur STE). Stéréo dure de type Amiga : voies 1 + 4 à gauche, 2 + 3 à droite. Pas d'interpolation.
- Numérotation des effets dans le replay NoisePacker : 7 = glissement de volume (le A de ProTracker). Un D (saut de motif) repart toujours à la ligne 0 ; aucun des 8 modules n'a de D avec paramètre.
- Enigma démarre à la VBL 2836 de la référence, pendant le noir qui suit l'oreille, et ne s'arrête qu'au premier changement de morceau.
- **Avertissement** : d'une exécution de Hatari à l'autre, la musique ne tombe pas exactement à la même VBL (2 lignes de partition d'écart observées). Les analyseurs ne peuvent donc pas être comparés image par image à la référence.

## 8. Où trouver les données

| Pour | Fichiers |
|---|---|
| Musique | `music/*.mod` |
| Oreille | `graphics/pictures/loading_screen_indexed.png` + `loading_screen.json` |
| Logos | `graphics/pictures/fullscreen/*_indexed.png` (HIGH FIDELITY DREAMS : `*_wide_indexed.png`, 416 px) + `*.json` |
| Menu et chargements | `graphics/menu/menu_select_song_indexed.png`, `graphics/menu/words/word_loading_indexed.png`, `word_decrunching_indexed.png` |
| Pages de texte | `extracted/textpage/ram_012000_025b00.bin` (tout : tables, texte, fonte et bulle pré-décalées), ou séparément `graphics/fonts/font8x8.bin`, `graphics/sprites/bubble16.bin`, `texts/hfd_texts.txt` (texte lisible, sans les codes de mise en page) |

## 9. Notes par plateforme

- **Amiga** : la machine idéale pour ce portage.
  - L'écran PAL est à 50 Hz, donc aucun problème de cadence.
  - Paula joue les modules en matériel : il suffit d'un replay ProTracker. Seuls les analyseurs ont besoin de l'état des voies, dans l'ordre inverse.
  - Le Copper fait naturellement les rasters, une palette par ligne. Il peut aussi changer la couleur 0 dans les bordures, ce qui couvre les filets et les bandes qui traversent l'écran.
  - Les couleurs 12 bits de l'Amiga correspondent directement à la notation `0xRGB` de ce document.
  - Pour les filets de HIGH FIDELITY DREAMS, il faut un affichage en overscan, ou les recadrer.
  - Le blitter peut dessiner les lettres (OR sur un plan) et les effacer.
- **Windows / Mac** :
  - Deux voies possibles : empaqueter le port web (Tauri, Electron, ou simplement une page locale), ou faire un portage natif (C ou C++ avec SDL2, par exemple). Pour le natif, il faut un lecteur MOD (libopenmpt, ou une transcription de `ports/web/src/audio/mod.ts`).
  - Comme pour le web, l'écran tourne rarement à 50 Hz. Il faut garder la logique à 50,05 Hz et l'afficher sur la fréquence de l'écran (voir `ports/web/src/pacing.ts`). Avec un écran VRR ou à 100 Hz, la cadence est parfaite.
- **Dans tous les cas** : la vérification la plus efficace consiste à rendre les scènes hors écran et à les comparer image par image à `out/rec1/video.mkv`, comme le fait le port web (`scripts/render.ts` et `scripts/compare.py`).
