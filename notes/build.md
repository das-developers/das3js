# Building and testing das3js

## Prerequisites

* The submodule: `git submodule update --init` after a fresh clone.
  `deps/das2C` is the stream library.
* Network access for the first build.  das2C downloads and builds its own
  wasm dependencies, expat (from its GitHub releases) and FFTW (from
  fftw.org), checks each against a pinned SHA-256 and keeps them in
  `deps/das2C/deps.wasm32` until a `make distclean`.  If a download site
  is down, see the `*_URL` comments in das2C's `buildfiles/emscripten.mak`.
* emscripten via emsdk, last verified with 6.0.13, the version das2C's CI
  pins.  Install it anywhere user-writable, for example:

      git clone https://github.com/emscripten-core/emsdk.git ~/opt/emsdk
      cd ~/opt/emsdk && ./emsdk install 6.0.13 && ./emsdk activate 6.0.13

  emsdk bundles the Node that runs the tests; pass `NODE=...` to use another.
* For the browser tests: Firefox on PATH.  It runs headless with a throwaway
  profile, so a Firefox you have open is not disturbed.

## Toolchain into the shell

    source ~/opt/emsdk/emsdk_env.sh   # adjust to taste

emsdk needs Python 3.10 or newer.  Where the system `python3` is older (Rocky
9 ships 3.9, RHEL 8 3.6), bracket the line with two exports.  The second is
not a typo: `emsdk_env.sh` unsets the variable after using it, and `emcc`
needs it again.

    export EMSDK_PYTHON=/usr/bin/python3.12
    source ~/opt/emsdk/emsdk_env.sh
    export EMSDK_PYTHON=/usr/bin/python3.12

## Build

    make module

The deliverable is three files in `build.wasm32/`: `das3js.mjs` (the module
you import), `das2c.mjs` (its loader) and `das2c.wasm`.  Keep them together.

`make clean module DEBUG=yes` builds a version that keeps the C debug
information, for stepping through das2C in a browser debugger.  It is about
five times larger, so rebuild without it before deploying.

## Test

Each target builds what it needs.  ERROR lines inside a run are expected
when the run says they are intentional.  These are negative tests than 
are supposed to fail.  The trailing success line is the final verdict.
The command below are provided in run order, and when working only on
das3js code, you can skip the `deps` rows.  Browser tests assume you have 
firefox on your machine.

| Command              | What it runs                                  | Success line                                      |
|----------------------|-----------------------------------------------|---------------------------------------------------|
| `make deps_test`     | das2C's test suite as wasm under Node         | `All wasm test programs completed without errors` |
| `make test`          | the C glue test as wasm under Node            | `All Foreign function layer checks passed`        |
| `make headless_test` | the module in headless Firefox, fixtures      | `All module checks passed`                        |
| `make live_test`     | same, against space.physics.uiowa.edu         | `All live checks passed`                          |
| `make local_test`    | same, against the dasFlex checkout next door  | `All live checks passed`                          |
| `make pack_test`     | the npm package in Vite, webpack, plain page  | `page reported success`, once per run (four)      |

`make deps_test` is the slow one, mostly das2C's `TestFeed`, and das2C's
CI runs it at the pinned commit.  Run it when das2C is the suspect.
`make live_test` needs the network.  The headless targets take
`TEST_BROWSER=firefox`, as the default (and so far the only) choice.

To use the pages in your own browser instead, run:
```bash
make server
```
and load the URL it that it prints. On Linux, Ctrl-C stops the test sever.  
Your browser should open to an index of the test pages and two tools:

* **Structure inspector:** give it any stream URL and it prints every
  dataset, dimension and variable as JSON, without the data values.  A
  fixture loads by default; `?url=...` bookmarks a stream.
* **Performance:** fetches a live stream (Juno Waves at Iowa by default),
  then re-parses the same bytes from memory so parser throughput shows
  separately from the network.

`make server PAGE=Perf.html` opens straight onto one page.

## The npm package

    make pack

writes `build.wasm32/das3js-VERSION.tgz`, the file `npm publish` would
upload: the three module files plus README, LICENSE and package.json, as
listed under `"files"` in package.json.  The version is read from
package.json.  `npm pack --dry-run` lists the contents without writing
anything.

### Consumer tests

A downstream developer installs that tarball into their own project and
builds it with their own tools.  To assist downstream developers the
directory `test/npm/` holds one small project for three common setups:

  * `vite/` - a Vite app, run both as a production build and under
    Vite's dev server
  * `webpack/` - a webpack app
  * `importmap/` - a plain page with no build tools, as a content 
    delivery network (CDN) user would write it

Each loads das2C's `example/ex08_cassini_rpws_survey.d2t` and passes if
all 13 datasets in that stream parse.

`make pack_test` packs, installs the tarball into each project, builds
them, and runs each in headless Firefox.  The tools themselves (Vite,
webpack) come from each project's lockfile.  A new project, or a
fresh lockfile after changing a tool's version, needs one `npm install`
in its directory first, which downloads the tools.

Npm and the tools run under whichever `node` is first on PATH, and
sourcing emsdk does not put its own Node there.  Where the system Node
is too old for Vite (it wants 20.19 or newer), put emsdk's first.  The
Makefile does this itself. If you run manual commands adjust the path:

    export PATH=$(dirname $EMSDK_NODE):$PATH

Here's the steps that `make pack_test` takes, in case you'd like to
run one at a time:

    make pack
    cd test/npm/vite
    npm ci                                          # tools, from the lockfile
    npm install --no-save ../../../build.wasm32/das3js-*.tgz
    npm run build                                   # vite and webpack only
    cd ../../..

    node test/browser/run.mjs index.html --app=test/npm/vite/dist   # Vite build
    node test/browser/run.mjs index.html --vite-dev=test/npm/vite   # Vite dev server
    node test/browser/run.mjs index.html --app=test/npm/webpack
    node test/browser/run.mjs index.html --app=test/npm/importmap

Some notes on the options:

* `--no-save` keeps the tarball out of the project's package.json and
lockfile, so they list only the project's tools.  
* `--app=DIR` serves a project's page at the site root. 
* `--vite-dev=DIR` runs the project's Vite dev server inside the runner 

No matter the options, the examples under `/das2C/` are reachable.  

Note: before a dev server run you can issue `rm -rf test/npm/vite/node_modules/.vite` 
to make Vite pre-bundle das3js from scratch, as `make pack_test` does.

To observe the tests, add Add `--serve` to any of the four to watch the
page in your own browser instead: it prints the URL, the devtools network
tab shows where `das2c.wasm` came from, and Ctrl-C stops it.

Don't use `npx vite` in `test/npm/vite` for this. Vite's server doesn't know
where to find the example streams or the test reporting location so the page
fails.

## Deploying to a local web server

All build products are static files, the browser does the work.

    make dist HTPASSWD=/server/path/.htpasswd
    rsync -a --delete build.wasm32/site/ host:/web/dir/das3js/

* `build.wasm32/site/` is about 15 MB.  Most of that is the one text,
  example.  The tree has the same layout `make server` uses, so it works
  under any URL prefix.

* The generated `.htaccess` declares the `.wasm` and `.mjs` types.  The
  browser refuses the module without them.  It needs `AllowOverride FileInfo`.

* `HTPASSWD` is optional and is a path on the server.  It adds basic auth,
  which needs `AllowOverride AuthConfig`.  Create the password file on the
  server with `htpasswd -c /server/path/.htpasswd someuser`.

* The pages talk to the node runner only when served from 127.0.0.1, so a
  real server never sees their log POSTs.  TestModule's login exchange
  needs the runner's `/auth/` too and is skipped elsewhere.

* The internal test catalog, `test/catalog/`, is staged as `catalog/`, so
  `Catalog.html?internal=1` and TestModule's catalog checks work there too.

* The data presets fetch from the das servers cross origin, which works
  because those servers send `Access-Control-Allow-Origin: *`.

## Clean rebuild

    make clean       # this project's and das2C's wasm build output
    make distclean   # also das2C's downloaded dependencies

`clean` is the everyday reset, and the one to use after pulling a new das2C.
It rebuilds all of the das2C code, which is quick.  It preserves the 
expat and FFTW builds in `deps/das2C/deps.wasm32/`. 

Use `distclean` when a dependency itself is in doubt.  The next build
downloads and rebuilds them, which is several minutes, most of it compiling
FFTW.

## When something fails

* The failing check names itself in the test output.

* In the browser tests, lines prefixed `firefox:` are Firefox's console, and
  das2C's log messages appear there too.  A
  `[GFX1-]: RenderCompositorSWGL failed mapping default framebuffer` line is
  headless Firefox without a GPU, not a failure.
  
* A link error `emcc: error: ... -Werror` after an emsdk upgrade is a new
  emscripten warning, not a code problem; see the warning flags in the
  Makefile.
