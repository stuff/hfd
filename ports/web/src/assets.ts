// Everything comes straight from ../../rip: graphics as palette indices, modules as URLs.
import menu from '@rip/graphics/menu/menu_select_song_indexed.png?indexed';
import wordLoading from '@rip/graphics/menu/words/word_loading_indexed.png?indexed';
import wordDecrunching from '@rip/graphics/menu/words/word_decrunching_indexed.png?indexed';
import font8x8Url from '@rip/graphics/fonts/font8x8.bin?url';
import ear from '@rip/graphics/pictures/loading_screen_indexed.png?indexed';
import earMeta from '@rip/graphics/pictures/loading_screen.json';
import aura from '@rip/graphics/pictures/fullscreen/screen_an_aura_production_indexed.png?indexed';
import auraMeta from '@rip/graphics/pictures/fullscreen/screen_an_aura_production.json';
import hfd from '@rip/graphics/pictures/fullscreen/screen_high_fidelity_dreams_wide_indexed.png?indexed';
import hfdMeta from '@rip/graphics/pictures/fullscreen/screen_high_fidelity_dreams.json';
import aeon from '@rip/graphics/pictures/fullscreen/screen_code_and_design_aeon_indexed.png?indexed';
import aeonMeta from '@rip/graphics/pictures/fullscreen/screen_code_and_design_aeon.json';
import actOfImpulse from '@rip/music/01 - Act of Impulse (Mantronix & Tip).mod?url';
import chineseDream from '@rip/music/02 - Chinese Dream (Firefox & Tip).mod?url';
import elysium from '@rip/music/03 - Elysium (Jester).mod?url';
import enigma from '@rip/music/04 - Enigma aka Hyperbased (Firefox & Tip).mod?url';
import justSpankIt from '@rip/music/05 - Just Spank It (Firefox & Tip).mod?url';
import macrocosm from '@rip/music/06 - Macrocosm (Firefox) [repaired].mod?url';
import overload from '@rip/music/07 - Overload (Mantronix & Tip).mod?url';
import theSweatShop from '@rip/music/08 - The Sweat Shop (Mantronix & Tip).mod?url';

export const images = { menu, wordLoading, wordDecrunching, ear, aura, hfd, aeon };
/** Palettes of the pictures, as measured in rip/graphics (per_line_palette = rasters). */
export const pictureMeta = { ear: earMeta, aura: auraMeta, hfd: hfdMeta, aeon: aeonMeta };

/** Menu order. Words for LOADING/DECRUNCHING screens: x of the word in the menu image. */
export const SONGS = [
  { title: 'Act of Impulse', url: actOfImpulse },
  { title: 'Chinese Dream', url: chineseDream },
  { title: 'Elysium', url: elysium },
  { title: 'Enigma', url: enigma },
  { title: 'Just Spank It', url: justSpankIt },
  { title: 'Macrocosm', url: macrocosm },
  { title: 'Overload', url: overload },
  { title: 'The Sweat Shop', url: theSweatShop },
];
export const ENIGMA = 3;

/** 44 glyphs of 8×8, 1 bit per pixel, for character codes 48 ('0') to 91 ('['). */
export async function loadFont(): Promise<Uint8Array> {
  return new Uint8Array(await (await fetch(font8x8Url)).arrayBuffer());
}
