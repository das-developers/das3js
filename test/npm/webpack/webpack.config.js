/* The least configuration a webpack user writes: one entry, one output.
 * Nothing in here mentions das3js or WebAssembly.  Webpack 5 recognizes
 * `new URL('das2c.wasm', import.meta.url)` inside the package on its own,
 * emits the .wasm as an asset beside main.js, and rewrites the URL to it;
 * this consumer checks that it does.
 *
 * ES module syntax here because package.json says "type": "module". */

import path from 'node:path';
import {fileURLToPath} from 'node:url';

const HERE = path.dirname(fileURLToPath(import.meta.url));

export default {
	mode: 'production',
	entry: './main.js',
	output: {
		path: path.join(HERE, 'dist'),
		filename: 'main.js',
		clean: true,     /* empty dist/ first, so no stale .wasm passes the test */
	},
};
