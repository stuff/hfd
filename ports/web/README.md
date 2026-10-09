# High Fidelity Dreams — portage web

Réplique exacte du music disk dans le navigateur, en Vite + TypeScript, sans dépendance à l'exécution.
Les données d'origine (modules, graphismes) sont lues directement dans [`../../rip/`](../../rip/) au moment du build.

```bash
npm install
npm run dev        # http://localhost:5173
npm run build      # site statique dans dist/
```

Paramètres d'URL :
- `?authentic` : durées d'origine des chargements, comme sur la disquette. L'écran de l'oreille dure alors 45 s au lieu de 6 s, et chaque changement de morceau 18 s au lieu de 5 s. Les fondus, eux, sont identiques dans les deux cas.
- `?menu` : saute l'intro et va directement au menu (pour le développement).
- `?debug` : expose l'application dans la console sous `hfd`.

Commandes : ↑ / ↓ pour choisir, RETURN pour lancer le morceau. À la souris, survol puis clic ; au doigt, un tap.

## État
- [x] Menu SELECT SONG, curseur, entrée en lecture, fondus
- [x] LOADING / DECRUNCHING + nom du morceau, fondu sonore
- [x] Lecteur MOD (AudioWorklet)
- [x] Intro : écran de l'oreille (LOADING / DECRUNCHING pulsés), logos AN AURA PRODUCTION / HIGH FIDELITY DREAMS (filets qui glissent dans les bordures) / CODE AND DESIGN AEON
- [ ] Pages de texte : text-spreader, 4 bandes en rotation avec analyseurs de voies, rasters

## Comment c'est fait
- **`src/st/display.ts`** : un écran ST virtuel cadré comme les captures de Hatari (416×276, bordures comprises, écran en (48, 29)).
  Les pixels sont des indices de couleur. Chaque ligne a sa propre palette de 16 couleurs (c'est ainsi que marchent les rasters), et la bordure prend la couleur 0 de sa ligne.
  Le fondu reproduit celui de la démo : au niveau j, chaque composante perd j sur l'échelle 3 bits du ST.
  Le programme principal perd au passage le bit fin du STE, alors que le loader (écran de l'oreille) le garde. `fadeLines` donne à une bande de lignes son propre niveau de fondu, et `wide` ouvre les bordures latérales d'une ligne.
- **`src/audio/mod.ts`** : un replay ProTracker sans dépendance (il tourne dans l'AudioWorklet comme dans Node).
  Il mixe comme Paula, sans interpolation, avec les voies 1+4 à gauche et 2+3 à droite. Les ticks vont à 50,05 Hz, la fréquence VBL du ST, comme le replay de la démo.
  Il remonte l'état de chaque voie à chaque tick (volume, période, crête, déclenchement), pour les analyseurs.
- **`src/scenes/`** : une scène par écran. Tout avance par VBL (1/50 s), avec des durées mesurées dans l'enregistrement de référence.
- Le plugin Vite de `vite.config.ts` transforme `…_indexed.png?indexed` en indices de palette bruts au moment du build.

## Vérification contre la démo d'origine
La référence est l'enregistrement image par image de la démo dans Hatari (voir « Vidéo de référence » dans [`rip/README.md`](../../rip/README.md)).
`scripts/render.ts` fait tourner les scènes sans navigateur, et `scripts/compare.py` les compare pixel par pixel à la vidéo :

```bash
npx vite-node scripts/render.ts -- intro /tmp/intro.rgb 4753
python3 scripts/compare.py /tmp/intro.rgb ../../rip/out/rec1/video.mkv 546
npx vite-node scripts/render.ts -- menu /tmp/menu.rgb 1396 127:ArrowDown 276:Enter
python3 scripts/compare.py /tmp/menu.rgb ../../rip/out/rec1/video.mkv 9024
```
- **Intro** : la VBL 0 du script correspond à la VBL 546 de la référence (apparition de l'oreille, après EmuTOS). Résultat : **0 pixel de différence sur 4 753 images**, de l'oreille jusqu'à la fin de CODE AND DESIGN AEON.
- **Menu** : la VBL 0 correspond à la VBL 9024 (ESPACE au VBL 9000). Le menu, le déplacement du curseur, RETURN, LOADING, DECRUNCHING et le retour au menu donnent **0 pixel de différence sur 1 396 images**.

Ces deux comparaisons se font VBL par VBL.

Pour le son, `scripts/render-audio.ts` rend un module en WAV. Comparé à libopenmpt au même tempo, l'enveloppe corrèle entre 0,94 et 0,99. Seuls quelques passages d'*Act of Impulse* (vibrato et changement d'instrument sans note) descendent à 0,7–0,85.

`scripts/smoke.mjs` lance le build dans Chrome headless (puppeteer-core), clique, change de morceau, fait des captures et relève les erreurs de console.
