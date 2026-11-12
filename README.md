# das3js

Das servers typically provide data relevant to space plasma and magnetospheric
physics research.  To retrieve data, an HTTP GET request is sent to a das
server by a client program and a self-describing stream of data values covering
the requested time range, at the requested time resolution, is provided in the
response body.  This project reads and processes those streams in web browsers.


**A new wrapper around a fast core.**  The JavaScript layer is new;
the parser underneath is not.  Das3js runs on das2C and provides a 
three-file ES module (`das3js.mjs`, `das2c.mjs`, `das2c.wasm`) that
fetches a das stream, parses it as it arrives and hands back decoded
values, int64 and all.  Modules are also provided to read data source
interface and protocol definitions.  These gather user input and submit
server queries.  The core module does no plotting, though test pages
exercise a simple renderer which paints spectrogram data as they arrive.
Currently, the renderer is mostly for performance tests.

Layout:

  * `src/` - the ES modules: `das3js.mjs`, the core, and two custom
    elements, `<das-catalog-tree>` and `<das-source-form>`
  * `glue/` - a small flat C interface over das2C, the wasm export surface
  * `test/` - the glue test (wasm under node) and the browser tests
    (headless Firefox via a node runner)
  * `deps/` - das2C, pinned as a git submodule and compiled in; it fetches
    and builds expat and FFTW for wasm32 itself
  * `notes/` - build recipe, measurements

**It's fast.**  A full day of Juno Waves burst waveforms, 505 MB of
samples, parses in Firefox in under 300 ms once the bytes are in memory,
about 1.7 GB/s, and the first record is usable half a second after the
request goes out while the server spends the next 36 s producing the rest.
There's no JSON on the wire, no number conversion in JavaScript: the values
arrive in their native binary encoding and the browser reads them in place.
A screenshot of a live run is below. Method and more streams are in 
[notes/performance.md](notes/performance.md).

![notes/hammer_perf.png](notes/hammer_perf.png)

## Using das3js

Install it from npm:

    npm install das3js

Then read a stream:

```js
import {load} from 'das3js';

const das3 = await load();                  // once per page or worker
const strm = await das3.fetchStream(url);   // parses as the bytes arrive

for(let i = 0; i < strm.length; ++i){
	const ds = strm.dataset(i);
	for(const dim of ds.data){
		const v = dim.var();                  // the dimension's center values
		console.log(ds.name, dim.name, v.units, v.values());
	}
}
strm.free();                                // datasets live in wasm memory
```

`values()` returns typed arrays (`Float32Array`, `BigInt64Array`, ...)
that view the decoded data directly.  Das3js runs in browsers and web
workers; Node is not supported yet.

Two optional custom elements build on the core, for pages that let
people find and request data.  Each import defines its tag:

```js
import 'das3js/catalog-tree';    // <das-catalog-tree>: browse a das catalog
import 'das3js/source-form';     // <das-source-form>: a request form for one source

const tree = document.querySelector('das-catalog-tree');
const form = document.querySelector('das-source-form');
tree.catalog = das3.newCatalog();
tree.addEventListener('das-select', ev => {
	const node = ev.detail.node;
	if(!node?.isSource) return;
	const old = form.source;
	form.source = node.source();       // the page owns sources,
	old?.free();                       // so it frees the one replaced
});
form.addEventListener('das-request', ev => fetchOrDownload(ev.detail.url));
```

They draw in a shadow root and take their colors from `--das-*` CSS
custom properties; the comment at the top of each file lists those, the
attributes and the events.  The form never fetches; it hands the page a
checked request URL.

The package is three files that must stay together: `das3js.mjs`, its
loader `das2c.mjs`, and the compiled library `das2c.wasm`, about 225 KB
over the wire.  The loader finds the `.wasm` beside itself, and the
common setups below handle that with no configuration.  If yours moves
the `.wasm` somewhere else, tell `load()` where:
`load({locateFile: name => '/my/assets/' + name})`.

### Vite

No configuration needed; tested with Vite 8, both `vite build` and the
dev server.

If the dev server fails to load the `.wasm` (a failed request for
`das2c.wasm`, or a WebAssembly compile error in the browser console),
your Vite's dependency pre-bundling moved das3js without fixing its
path.  You can exclude it from pre-bundling in `vite.config.js`:

```js
export default {
	optimizeDeps: {exclude: ['das3js']},
};
```

To ship the license notices with your app, turn on `build.license`;
Vite then writes `.vite/license.md`, das3js's notices included:

```js
export default {
	build: {license: true},
};
```

### webpack

No configuration needed; tested with webpack 5, which emits
`das2c.wasm` as an asset beside your bundle.

During a build, webpack warns that the `.wasm` exceeds its recommended
asset size of 244 KiB.  That limit is a general hint for JavaScript
bundles; the library loads once and is compiled while it downloads.  To
silence the warning for it alone, in `webpack.config.js`:

```js
export default {
	// ...
	performance: {assetFilter: name => !name.endsWith('.wasm')},
};
```

### No bundler

A plain page can import das3js from a CDN (content delivery network) with
an import map:

```html
<script type="importmap">
{"imports": {"das3js": "https://cdn.jsdelivr.net/npm/das3js@0.1/build.wasm32/das3js.mjs"}}
</script>
<script type="module">
import {load} from 'das3js';
// ...
</script>
```

To host the files yourself, copy the three files from the package's
`build.wasm32/` directory together.  The web server must send `.wasm` as
`application/wasm` and `.mjs` as `text/javascript`; browsers refuse
either one otherwise.

### Checking a setup

`make pack_test` installs the package into a small project for each of
the setups above and runs them in headless Firefox; see
[notes/build.md](notes/build.md).

## Building

Like the other das2C bindings (das2py, das2dlm), this project statically
compiles in a selected das2C version, rather than depending on shared
objects.  The current pin is das2C master at 6a9ba9e, v3.0.4 plus a fix
for catalog option flags.  `notes/build.md` covers the toolchain, the
build and the tests; `git submodule update --init` fetches the pinned
sources.

## License

Das3js's own code, the JavaScript included, is MIT licensed.  The
compiled `das2c.wasm` contains das2C and so is distributed under the
LGPL 2.1.  Bundling and minifying with Vite, webpack and the like is
fine and does not put an application under either license; see
[LICENSE](LICENSE) for what each file is under and the notices to ship.
