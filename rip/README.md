# High Fidelity Dreams (Aura, 1992) — étude de la disquette (`rip/`)

Ce dossier contient tout ce qui a été extrait de la disquette : musiques, graphismes, textes, code désassemblé et outils d'extraction.
Les portages, eux, sont dans [`../ports/`](../ports/).

*High Fidelity Dreams* est un music disk Atari ST/STE d'**Aura** (code et design : Aeon, protection : Chris),
sorti le 5.6.1992 à la BMT-Party d'Essen. Il contient 8 modules Amiga
(Phenomena / Sanity) joués par une émulation logicielle de la puce son Paula de l'Amiga (« HiFi-Paula »,
16 kHz sur ST et 50 kHz en son DMA sur STE). Les modules sont stockés au format NoisePacker 2 et compressés.

La disquette `hifidrea.st` (800 Ko, 80 pistes, 2 faces, 10 secteurs) n'a **pas de système de fichiers**.
Elle démarre sur un boot sector chiffré qui charge les étapes suivantes en accédant directement au contrôleur disquette.
Tout a été extrait en faisant tourner la démo dans Hatari (STE, 1 Mo, EmuTOS), en headless et piloté par script.

## 1. Les musiques (`music/`)

Comme convenu, ce sont les **fichiers .mod originaux** (Modland / ModArchive), pas des reconversions des données de la disquette.
Pour chaque morceau, j'ai fait jouer la démo, puis comparé le module décompacté en RAM ($26000) aux candidats trouvés en ligne :
notes de chaque pattern, ordre de lecture et samples comparés octet par octet (`tools/np2_compare.py`).

| # | Menu de la démo | Auteurs* | Fichier retenu | Source | Correspondance avec la disquette |
|---|---|---|---|---|---|
| 1 | Act of Impulse | Mantronix & Tip | `01 - Act of Impulse …mod` | ModArchive #65381 (`actofimp.mod`) | identique (la version Modland est différente : 7 patterns et 1 sample changent) |
| 2 | Chinese Dream | Firefox & Tip | `02 - Chinese Dream …mod` | Modland `Firefox/chinese dream.mod` | identique |
| 3 | Elysium | Jester (Sanity) | `03 - Elysium (Jester).mod` | Modland `Jester/elysium.mod` (format FLT4/StarTrekker) | identique |
| 4 | Enigma | Firefox & Tip | `04 - Enigma aka Hyperbased …mod` | Modland `Firefox/coop-Tip/hyperbased.mod` | identique ; c'est la musique de la démo *Enigma* de Phenomena (elle sert aussi de musique d'intro) |
| 5 | Just Spank It | Firefox & Tip | `05 - Just Spank It …mod` | Modland `Firefox/coop-Tip/yeaah just spank it.mod` | identique (le `just spank it.mod` de Modland est une autre version) |
| 6 | Macrocosm | Firefox | `06 - Macrocosm (Firefox) [repaired].mod` | Modland `Firefox/macrocosm.mod`, **réparé** | voir ci-dessous |
| 7 | Overload | Mantronix & Tip | `07 - Overload …mod` | Modland `Mantronix/coop-Tip/overload.mod` | identique, sauf 6 octets dans le sample 29 |
| 8 | The Sweat Shop | Mantronix & Tip | `08 - The Sweat Shop …mod` | Modland `Mantronix/coop-Tip/the sweat shop.mod` | identique |

\* Les auteurs viennent de Demozoo et Modland. Les crédits affichés par la démo diffèrent parfois (elle met « by Tip and Mantronix » sur presque tout).

« Identique » veut dire que les notes de tous les patterns utilisés, l'ordre de lecture et tous les samples sont les mêmes.
Les seules différences restantes viennent de l'encodage NoisePacker : patterns renumérotés, effet 8 supprimé, paramètres de volume-slide réécrits.

**Macrocosm** : le fichier Modland est un rip défectueux. Chaque sample y est décalé de 2 octets de plus que le précédent,
jusqu'à 52 octets pour le dernier, ce qui fait que chaque sample commence par la fin du précédent.
La version réparée réaligne les samples du fichier Modland lui-même et complète les 52 octets manquants à la fin du dernier sample.
Ces 52 octets sont les seules données reprises de la disquette.
L'original non modifié est dans `music/sources/modland/`.

`music/sources/` contient tous les candidats téléchargés, y compris les variantes non retenues
(a final hyperbase, the final hyperbase, just spank it, act of impulse version Modland, un autre « overload »).

## 2. Graphismes, fontes, textes et écrans

- `graphics/` contient les **graphismes d'origine**, relus pixel par pixel dans la mémoire vidéo de la démo émulée.
  On y trouve l'écran de chargement (l'oreille), les 3 logos, le menu complet et chaque mot du menu, la fonte 8×8 du text-spreader, la bulle et le message anti-cartouche.
  Chaque image existe en PNG indexé (pixels d'origine), en PNG « tel qu'affiché » (rasters compris) et en JSON (palettes, y compris ligne par ligne).
  Les écrans complets sont aussi en `.pi1` Degas.
  Les détails sont dans [graphics/README.md](graphics/README.md) et l'aperçu dans `graphics/overview.png`.

- `texts/hfd_texts.txt` contient le texte intégral de la démo, décodé. Le « text-spreader » stocke une ligne sur deux à l'envers, et la police utilise `[`→`-`, `;`→`.`, `=`→`!`, `@`→bloc.
  On y trouve les crédits, les membres d'Aura, les greetings, deux récits de parties (Shining 8, Black Sausages) et des infos techniques :
  1 041 122 octets de modules compressés en 696 357 octets, 37:16 de musique, un replay NoisePacker « Twins of Phenomena » réécrit par Aeon (5 lignes de scan au maximum),
  4 « vector-quaders » avec analyseurs de voix (ce sont les 4 bandes qui tournent derrière le texte), et 32 bobs de 8×8 pour le spreader.
  Le boot contient aussi un message caché : « HI INTRUDER ! … DARE TO CRACK IT … SPECIALLY DEDICATED TO TINA M. ».
- `screens/*.png` contient 13 captures à la résolution native (bordures comprises) : chargement (l'oreille « by Aeon »), décompactage, logos AURA, titre, CODE AND DESIGN AEON,
  pages de texte avec rasters, crédits, menu de sélection et décompactage d'un morceau.
  Les captures ne contiennent pas les palettes changées à chaque ligne (rasters) séparément : elles montrent l'image finale.

## 3. Code (`extracted/`)

### Chaîne de démarrage et protection
1. **Boot sector** (`T0 S0 s1`). Le code en $1E déchiffre par XOR $132 octets écrits **sur les vecteurs d'exception** ($8–$13A), puis saute en $10.
   Il met la palette au noir et envoie la commande « pause » au clavier.
   Pour que les calculs d'adresses tombent juste, le code suppose que `$4CE` (vbl_list) contient `$01F8` / `$0000`.
   `tools/decrypt_boot.py` reproduit ce déchiffrement.
2. **Stage 1** (en $10). Il remplit la RAM jusqu'à 4 Mo, programme le DMA et le contrôleur WD1772 à la main, lit `T0 S0 s8` en $500, le déchiffre et saute en $400.
3. **Stage 2** ($400). Il teste une cartouche en $FA0000 (anti-debugger) et affiche « DISCONNECT MODULE! » s'il en trouve une, puis lit `T0 S0 s7` en $200.
4. **Stage 3** ($200). Il redirige **toutes** les exceptions vers $272 (piège), lit 36 secteurs en $70000 et y saute.
5. **Résident** ($70000–$74800). C'est le loader et le décompacteur (« JAM and Aeon-pre-pack »), avec l'écran de l'oreille et le Timer B en $70564.
   Il charge le programme principal compacté en $3E00 et le module Enigma en $25E00, puis décompacte.
6. **Programme principal**, point d'entrée en **$4000**. VBL en $4532, HBL en $4282, TRAP #0 en $44D8.
   Le module en cours est décompacté en **$26000** au format NoisePacker 2, avec ses pointeurs de samples relocalisés et les boucles pré-déroulées pour le mixeur.
   Les écrans sont vers $58000–$7FFFF et les tables du mixeur en $74000/$7C000.

### Carte de la disquette (relevée par trace FDC dans Hatari)
| Contenu | Piste/face/secteur | Offset dans le .st | Secteurs | Chargé en |
|---|---|---|---|---|
| boot sector | T0 S0 s1 | $000000 | 1 | (buffer TOS) |
| stage 3 | T0 S0 s7 | $000C00 | 1 | $200 |
| stage 2 | T0 S0 s8 | $000E00 | 1 | $500 → $400 |
| Act of Impulse | T0 S1 s5 | $001C00 | 102 | $25E00 |
| Chinese Dream | T5 S1 s7 | $00E800 | 104 | $25E00 |
| Elysium | T11 S0 s1 | $01B800 | ~179 | $25E00 |
| Enigma | T19 S1 s9 | $031C00 | 234 | $25E00 |
| Just Spank It | T31 S1 s3 | $04F000 | ~119 | $25E00 |
| Macrocosm | T37 S1 s1 | $05DC00 | ~203 | $25E00 |
| Overload | T47 S1 s3 | $077000 | ~236 | $25E00 |
| The Sweat Shop | T59 S0 s7 | $094400 | 200 | $25E00 |
| programme principal (compacté) | T69 S0 s7 | $0AD400 | 173 | $3E00 |
| loader / décompacteur résident | T77 S1 s9 | $0C2C00 | 36 | $70000 |

Le relevé brut de toutes les lectures de secteurs est dans `extracted/fdc_reads_raw.txt`.

### Fichiers
- `extracted/memory/` contient des images RAM brutes (adresse 0 = début du fichier) :
  boot déchiffré, stage 2, RAM à l'entrée du résident ($70000), RAM à l'entrée du programme principal ($4000, code décompacté), et RAM au menu (VBL 9000).
- `extracted/disasm/*.s` contient des **désassemblages** 68000 récursifs, avec des labels, les registres hardware nommés (`mfp_*`, `snd_*`, `dma_*`…) et les zones de données en `dc.b` :
  `stage1_bootsector.s`, `stage2_0400.s`, `stage3_0200.s`, `resident_70000.s` (557 instructions), `main_4000.s` (1438 instructions).
  Ce n'est **pas** une décompilation en C. La couverture du programme principal est partielle :
  les routines appelées via des tables (`jmp (a0)`) ou par du code auto-généré (le mixeur Paula génère son code en $74000) restent en `dc.b`.
  On peut ajouter des points d'entrée en relançant `tools/rdis.py`.

## 4. Reproduire / continuer (`tools/`)

Toutes les commandes ci-dessous se lancent depuis le dossier `rip/`.

```bash
tools/setup_hatari.sh
```
Ce script compile SDL2, Hatari 2.5.0 et télécharge EmuTOS 1.3 dans `tools/.build`, sans droits root (environ 2 min 30).
Il applique `hatari-debugger-key.patch`, qui ajoute au debugger de Hatari une commande `key <scancode> d|u` pour piloter le menu au VBL près.

```bash
mkdir -p out/cd && tools/keys.py out/cd "9000:0x39,9130:0x48,9160:0x48,9400:0x1c"
```
Ces appuis correspondent à ESPACE, deux fois HAUT puis RETURN, ce qui sélectionne Chinese Dream.

```bash
tools/hrun.sh out/cd 13000 "9390 10000" "12990"
```
Cette commande prend des captures aux VBL 9390 et 10000 et un dump RAM au VBL 12990, en environ 30 s.

```bash
python3 tools/np2_compare.py out/cd/m_12990.bin music/*.mod
```

Autres outils :
- `schedule.py` : programme des appuis de touches et des captures (adresse vidéo, BMP, RAM) en chaînant les breakpoints, ce qui garde l'émulation rapide.
- `stscreen.py`, `export_pic.py` et `memview.py` : extraction des graphismes.
- `fdcmap.py` : carte des lectures disque à partir de `--trace fdc`.
- `rdis.py` : désassembleur récursif, avec capstone dans `tools/.build/pylib`.
- `modinfo.py`.
- `avisplit.py` : convertit l'AVI brut de Hatari (voir ci-dessous) en une vidéo compacte sans perte (`video.mkv`, FFV1) et un `audio.wav`.

### Vidéo de référence (pour vérifier les portages)
Hatari peut enregistrer la démo image par image, son compris. Cette build n'a pas libpng, il faut donc le codec BMP : l'AVI brut pèse environ 21 Go pour 16 000 VBL. `avisplit.py` le lit même si son en-tête est cassé au-delà de 4 Go.
```bash
tools/schedule.py out/rec1 "9000:0x39,9150:0x50,9300:0x1c" "15990"
tools/hrun.sh out/rec1 16000 "" "" --avirecord --avi-vcodec bmp --avi-file $PWD/out/rec1/demo.avi
tools/avisplit.py out/rec1/demo.avi out/rec1 && rm out/rec1/demo.avi
```
On obtient l'intro complète, le menu (ESPACE au VBL 9000), BAS au VBL 9150 et RETURN au VBL 9300 (chargement de Just Spank It), à 50 Hz.
L'image n de `video.mkv` correspond au VBL n. Le cadre fait 416×276, et l'écran ST commence en (48, 29).
Le dossier `out/` n'est pas versionné.

Remarque : la démo a besoin que le son soit émulé. Avec `--sound off`, elle se bloque au décompactage.
`hrun.sh` utilise donc le driver audio SDL « disk » sans délai, ce qui garde le son émulé sans caler la vitesse sur le temps réel.

## 5. Pistes pour un portage (PC / web / Amiga)
- **Musique** : les 8 .mod se jouent directement. Sur le web : libopenmpt (wasm), chiptune3.js ou un player MOD maison en WebAudio. Sur Amiga : le replay ProTracker natif, Paula fait le travail en matériel.
- **Visuels** : les bitmaps d'origine sont dans `graphics/`.
  Le reste est du code à refaire : le text-spreader (32 bobs 8×8 avec `font8x8.bin`, textes dans `texts/`), les rasters (palettes par ligne fournies en JSON),
  et les 4 quads en rotation avec les analyseurs de voies (une frame de référence dans `graphics/textpage/`).
  Sur Amiga, les rasters se font naturellement au Copper.
