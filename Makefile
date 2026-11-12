# das3js makefile: wraps emsdk and other build tools in a gnumake interface
#
# Dependency Tree
# ---------------
# das3js
#  |
#  +- das2C: Stream parsing, catalog request parsing (LGPL)
#      |
#      +- expat: XML parser (MIT)
#      |
#      +- fftw: Discrete Fourier Transforms (GPL)
#      |
#      +- zlib: Compressed streams, an emscripten port (zlib license)
#
# Environment
# -----------
# The emscripten toolchain must be active in the calling shell (source
# emsdk_env.sh).  Node for running wasm output is taken from EMSDK_NODE, which
# that script exports; pass NODE=... to use another.
#
# Make Targets
# ------------
# These are the top level make targets, in run order.  The two deps targets
# are optional: a plain make builds the das2C library it needs.  Run them
# after a das2C pin bump or when altering das2C code.  `make help` prints
# a one line reminder of each.
#
# make deps       Build das2C and its test programs under emsdk
#
# make deps_test  Run das2C's test suite under node
#
# make            Build the browser module (das3js.mjs, das2c.mjs, das2c.wasm)
#                 and our node test program, runs nothing.  Use DEBUG=yes
#                 to retain debug info, not recommended, it increases module
#                 size by a factor of 5.
#
# make test       Run das3js test programs under node.js
#
# make headless_test
#                 Run the module in a headless browser against local test
#                 programs and data.  Browser is set with TEST_BROWSER=firefox,
#                 the only one supported so far.  The browser is run in an
#                 independent profile.
#
# make live_test  Run in a headless browser, but against the live dasFlex
#                 server at space.physics.uiowa.edu
#
# make pack       Write the npm package, build.wasm32/das3js-VERSION.tgz,
#                 the file `npm publish` would upload
#
# make pack_test  Pack the npm tarball and install it into the consumer
#                 projects under test/npm (Vite, webpack, a bare import map
#                 page), build each and run it in a headless browser, and
#                 run the Vite app under Vite's dev server too.  A new
#                 consumer needs one `npm install` in its directory to
#                 download its tools and write its lockfile.
#
# make server     Start a node server at http://127.0.0.1:9099 with an index
#                 of the test pages and tools, Ctrl-C stops the server.
#                 Add RUNFLAGS=--dasflex to run the sibling ../dasflex
#                 locally and point the pages at it.
#
# make dist       Stage the test pages, module and fixtures as a static site
#                 in build.wasm32/site, ready to copy to a web server.  Add
#                 HTPASSWD=/server/path/.htpasswd for a basic-auth password.
#
# make clean      Delete das3js build output and das2C's, keeping das2C's
#                 downloaded dependencies
#
# make distclean  Remove all build products, and all downloads in
#                 dependencies.
#

DAS2C=deps/das2C

# Named build.<arch> as in das2C, whose wasm objects land in
# deps/das2C/build.wasm32
WBD=build.wasm32

EMCC=emcc

ifeq ($(NODE),)
ifeq ($(EMSDK_NODE),)
NODE:=node
else
NODE:=$(EMSDK_NODE)
endif
endif

# npm from beside that node: the consumer tools (Vite 8 wants node 20.19 or
# later) run under whichever node is first on PATH, so put that one first
# too.  A bare `node` means nothing to prepend.
ifeq ($(findstring /,$(NODE)),)
NPM:=npm
NODE_PATH_PFX:=
else
NPM:=$(dir $(NODE))npm
NODE_PATH_PFX:=PATH=$(dir $(NODE)):$$PATH
endif

# das2C's wasm build is its own makefile, use -C first, so the -f path and
# everything in it resolve from the das2C tree.
#
# Only the arguments are kept here: make treats a recipe line as a recursive
# make, honoring -n and sharing -j job slots with it, only when $(MAKE)
# appears in the line itself, not inside another variable.
DAS2C_WASM_ARGS=-C $(DAS2C) -f buildfiles/emscripten.mak

DAS2C_WASM_LIB=$(DAS2C)/build.wasm32/libdas3.a

# das2C's dependency builds, in link order after libdas3.a
DAS2C_FFTW_LIB=$(DAS2C)/deps.wasm32/fftw/lib/libfftw3.a
DAS2C_EXPAT_LIB=$(DAS2C)/deps.wasm32/expat/lib/libexpat.a
DAS2C_DEP_LIBS=$(DAS2C_FFTW_LIB) $(DAS2C_EXPAT_LIB)

# No -g here: whether DWARF ships is decided at link time, per output below.
# The libdas3.a and libexpat.a objects keep theirs either way.
WASM_CFLAGS=-std=c99 -Wall -Werror -O2 -I$(DAS2C) -Iglue -Isrc --use-port=zlib

# NODERAWFS mounts the real filesystem so the test reads fixtures in place,
# nothing embedded.
#
# EXIT_RUNTIME turns main()'s return into the exit status.
#
# -g keeps DWARF in the .wasm, which makes emcc warn that wasm-opt ran fewer
#    passes
#
# -Werror on the same command line would turn that into a failed link, so the
#    warning is waived here.
#
# (The browser module uses none of these)
WASM_TEST_LFLAGS=--use-port=zlib -sENVIRONMENT=node -sNODERAWFS=1 \
 -sEXIT_RUNTIME=1 -sALLOW_MEMORY_GROWTH=1 -sSTACK_SIZE=1048576 \
 -g -Wno-limited-postlink-optimizations

# The shipped module carries no debug info.
#
# -g at this link would keep the DWARF from every sub-module, making das2c.wasm
# several times larger.  DEBUG=yes puts it back for stepping through
# the C in a browser debugger, and is why the module link keeps the warning
# waiver too.
#
# Run make clean first: nothing here notices DEBUG changing.
ifeq ($(DEBUG),yes)
MOD_DEBUG=-g
endif

# Browser module link
#
# MODULARIZE + EXPORT_ES6 turns the loader into an ES module exporting one
# factory function (EXPORT_NAME) that resolves to the runtime once the
# .wasm is instantiated; nothing runs at import time.
#
# No filesystem: streams arrive through DasWasmRdr_feed, never fopen.
#
# The C export surface is marked EMSCRIPTEN_KEEPALIVE in glue/wasm.h, so only
# the allocator the JS side needs for its scratch buffers is listed here.  The
# HEAP* views are what the JS wraps TypedArrays around, hence exported.
#
# Only the runtime helpers the JS actually uses are exported:
#
#  * cwrap to bind the glue functions once at load time
#  * HEAPU8 and HEAP32 to reach the heap.
#
# The other typed views (HEAP16, HEAPF64, HEAP64...) are not exported because
# das3js.mjs builds its own TypedArrays over HEAPU8.buffer, so BigInt64Array
# over TT2000 values needs nothing extra here, not even WASM_BIGINT (no glue
# signature carries an int64 across the call boundary).
#
# ccall is left out on purpose: it re-marshals on every call and would hand
#   downstream a slow path, don't give them a footgun.
#
# The 1 MiB stack stays: das2C has large automatics.
#
# The catalog functions below are exported straight from libdas3.a, no glue.
# das2C's node, walk and source API is already flat (pointers, sizes, C
# strings), and wrapping each in a one line daswasm_ call would only make a
# second copy of its docs.  Their declarations are in das3/node.h,
# das3/source.h and das3/req.h.

DAS2C_EXPORTS=\
 new_DasNode_json del_DasNode \
 new_DasNodeWalk new_DasNodeWalk_seed del_DasNodeWalk DasNodeWalk_state \
 DasNodeWalk_numUrls DasNodeWalk_urlAt DasNodeWalk_uri DasNodeWalk_feed \
 DasNodeWalk_fail DasNodeWalk_result DasNodeWalk_nearest \
 new_DasSrc_node del_DasSrc DasSrc_title DasSrc_needsAuth DasSrc_authRealm \
 DasSrc_numExamples DasSrc_exampleLblAt new_DasReq_src DasSrc_mime \
 DasSrc_reqUrl \
 del_DasReq DasReq_set DasSrc_check

# emcc wants one comma separated list with a leading underscore on each name
EMPTY:=
SPACE:=$(EMPTY) $(EMPTY)
COMMA:=,
MOD_EXPORTS=$(subst $(SPACE),$(COMMA),$(strip _malloc _free $(addprefix _,$(DAS2C_EXPORTS))))

MODULE=$(WBD)/das2c.mjs
JS_SRCS=$(wildcard src/*.mjs)
JS_OUT=$(patsubst src/%,$(WBD)/%,$(JS_SRCS))
WASM_MOD_LFLAGS=--use-port=zlib -sMODULARIZE=1 -sEXPORT_ES6=1 \
 -sEXPORT_NAME=createDas3Module -sENVIRONMENT=web,worker -sFILESYSTEM=0 \
 -sALLOW_MEMORY_GROWTH=1 -sSTACK_SIZE=1048576 \
 -sEXPORTED_FUNCTIONS=$(MOD_EXPORTS) \
 -sEXPORTED_RUNTIME_METHODS=cwrap,HEAPU8,HEAP32 \
 -Wno-limited-postlink-optimizations

## Default ###################################################################

# A bare make builds the deliverable and our test program, runs nothing.
# das2C's test programs are deps' job.
all: module $(WBD)/TestWasm.js $(WBD)/TestRender.js

# One line per target in the header above, in the same order.  Hand kept:
# update both when a target changes.
help:
	@echo 'make targets, in run order (the deps steps are optional):'
	@echo
	@echo '  make deps            build das2C, its expat and FFTW, its test programs'
	@echo '  make deps_test       run das2C test suite under node'
	@echo '  make                 build the browser module and our test program'
	@echo '  make test            run our tests under node'
	@echo '  make headless_test   run the module in headless Firefox, local fixtures'
	@echo '  make live_test       same, against space.physics.uiowa.edu'
	@echo '  make pack            write the npm package, $(PACK)'
	@echo '  make pack_test       install the package into test/npm consumers, run them'
	@echo '  make server          serve test pages on http://127.0.0.1:9099'
	@echo '  make dist            stage the pages as a static site in $(WBD)/site'
	@echo '  make clean           remove our and das2C build output, keep downloads'
	@echo '  make distclean       clean, plus downloaded dependencies'

# Everything of ours that runs under node alone: no browser, and no network
# beyond the dependency downloads of a first build.  TestWasm reads stream
# fixtures by paths relative to the das2C checkout, so it runs from there.
test: $(WBD)/TestWasm.js $(WBD)/TestRender.js
	cd $(DAS2C) && $(NODE) $(CURDIR)/$(WBD)/TestWasm.js
	cd $(DAS2C) && $(NODE) $(CURDIR)/$(WBD)/TestRender.js

$(WBD)/TestWasm.js: test/TestWasm.c glue/wasm.c glue/wasm.h $(DAS2C_WASM_LIB) $(DAS2C_DEP_LIBS) | $(WBD)
	$(EMCC) $(WASM_CFLAGS) test/TestWasm.c glue/wasm.c $(DAS2C_WASM_LIB) $(DAS2C_DEP_LIBS) $(WASM_TEST_LFLAGS) -o $@

# Pass a directory to also get PPM images, e.g. from the das2C checkout:
#   node ../../build.wasm32/TestRender.js ../../tmp
$(WBD)/TestRender.js: test/TestRender.c src/render.c src/render.h glue/wasm.c glue/wasm.h $(DAS2C_WASM_LIB) $(DAS2C_DEP_LIBS) | $(WBD)
	$(EMCC) $(WASM_CFLAGS) test/TestRender.c src/render.c glue/wasm.c $(DAS2C_WASM_LIB) $(DAS2C_DEP_LIBS) $(WASM_TEST_LFLAGS) -o $@

# Always recurse: das2C's own dependency tracking decides what to rebuild
$(DAS2C_WASM_LIB): FORCE
	$(MAKE) $(DAS2C_WASM_ARGS) build

# The empty recipe says the recursion above made them, so make neither looks
# for another way to build them nor, under -j, checks for them too early
$(DAS2C_DEP_LIBS): $(DAS2C_WASM_LIB) ;

## Wasm ######################################################################

module: $(MODULE) $(JS_OUT)

$(MODULE): glue/wasm.c glue/wasm.h src/render.c src/render.h $(DAS2C_WASM_LIB) $(DAS2C_DEP_LIBS) | $(WBD)
	$(EMCC) $(WASM_CFLAGS) $(MOD_DEBUG) glue/wasm.c src/render.c $(DAS2C_WASM_LIB) $(DAS2C_DEP_LIBS) $(WASM_MOD_LFLAGS) -o $@

# The ES module sits beside the loader it imports, so one directory is the
# whole deliverable
$(WBD)/%.mjs: src/%.mjs | $(WBD)
	cp $< $@

# The headless runner drives Firefox only so far
TEST_BROWSER?=firefox
ifneq ($(filter headless_test live_test local_test,$(MAKECMDGOALS)),)
ifneq ($(TEST_BROWSER),firefox)
$(error TEST_BROWSER=$(TEST_BROWSER): only firefox is supported so far)
endif
endif

headless_test: module
	$(NODE) test/browser/run.mjs
	$(NODE) test/browser/run.mjs 'Catalog.html?check=1'

## Package ###################################################################

# What a downstream developer gets from npm.  `npm pack` writes the tarball
# the registry would serve, from the "files" list in package.json: npm's
# counterpart of a Python wheel, prebuilt wasm and all, and the file
# `npm publish` uploads.  It is a build product, so it lands in $(WBD) and
# make clean removes it.  The version comes from package.json, which is
# the one place it is written down.
VERSION:=$(shell sed -n 's/^[[:space:]]*"version":[[:space:]]*"\([^"]*\)".*/\1/p' package.json)
PACK=$(WBD)/das3js-$(VERSION).tgz

pack: $(PACK)

$(PACK): $(MODULE) $(JS_OUT) package.json README.md LICENSE | $(WBD)
	rm -f $(WBD)/das3js-*.tgz
	$(NODE_PATH_PFX) $(NPM) pack --silent --pack-destination $(WBD) > /dev/null

# Each consumer installs the tarball without recording it (--no-save), so
# package.json and the lockfile there list only the consumer's own tools
# and stay unchanged from run to run.  npm ci installs exactly what the
# lockfile says and fails without one, hence the one time `npm install` per
# consumer noted in the header.
NPM_CONSUMERS=vite webpack importmap

pack_test: $(PACK)
	for c in $(NPM_CONSUMERS); do \
	  (cd test/npm/$$c && $(NODE_PATH_PFX) $(NPM) ci --silent && \
	   $(NODE_PATH_PFX) $(NPM) install --no-save --silent $(CURDIR)/$(PACK)) || exit 1; \
	done
	cd test/npm/vite && $(NODE_PATH_PFX) $(NPM) run --silent build
	cd test/npm/webpack && $(NODE_PATH_PFX) $(NPM) run --silent build
	$(NODE) test/browser/run.mjs index.html --app=test/npm/vite/dist
	rm -rf test/npm/vite/node_modules/.vite
	$(NODE) test/browser/run.mjs index.html --vite-dev=test/npm/vite
	$(NODE) test/browser/run.mjs index.html --app=test/npm/webpack
	$(NODE) test/browser/run.mjs index.html --app=test/npm/importmap

# Same runner, real server: needs the network and a public dasFlex that is up
live_test: module
	$(NODE) test/browser/run.mjs TestLive.html

# Same page against the dasFlex checkout next door, served through the CGI
# shim in test/dasflex_cgi.py; no network, no Apache.  A sibling checkout
# rather than a submodule on purpose: a test server should track head.
DASFLEX?=../dasflex
local_test: module
	$(NODE) test/browser/run.mjs TestLive.html --dasflex=$(DASFLEX)

# No headless Firefox: the runner prints a URL and waits.  Load it, read the
# page and the devtools console, edit, reload.  Needed because file:// cannot
# load ES modules or .wasm, so "just open the html" does not work.
# PAGE=Perf.html and the like opens straight onto one page.
PAGE?=index.html
server: module
	$(NODE) test/browser/run.mjs $(PAGE) --serve $(RUNFLAGS)

## Static site ###############################################################

# The same three trees run.mjs mounts, so the pages' relative paths
# (../build, ../das2C/examples) resolve the same on any server, at any URL
# prefix.  Only the fixtures the pages fetch are copied: das2C's examples
# directory is otherwise mostly dead weight.
#
# Deploy with a plain copy, e.g.
#   rsync -a --delete build.wasm32/site/ host:/var/www/html/das3js/
#
# HTPASSWD is a path on the SERVER; Apache reads it there.  Create the file
# there too, with: htpasswd -c /that/path/.htpasswd someuser
SITE=$(WBD)/site
SITE_FIXTURES=ex01_polar_mfe.d2t ex08_cassini_rpws_survey.d2t \
 ex09_juno_waves_survey.d2t ex17_vector_noframe.d3b ex19_cassini_ragged_wfrm.d3t \
 ex35_strings_rank2.d3b

dist: module
	rm -rf $(SITE)
	mkdir -p $(SITE)/build $(SITE)/test/img $(SITE)/das2C/examples $(SITE)/das2C/test/das_fetch
	cp $(MODULE) $(WBD)/das2c.wasm $(JS_OUT) $(SITE)/build/
	cp test/browser/*.html test/browser/*.css $(SITE)/test/
	cp $(filter-out test/browser/run.mjs,$(wildcard test/browser/*.mjs)) $(SITE)/test/
	cp test/browser/img/*.png test/browser/img/*.svg $(SITE)/test/img/
	cp $(addprefix $(DAS2C)/examples/,$(SITE_FIXTURES)) $(SITE)/das2C/examples/
	cp $(DAS2C)/test/das_fetch/juno_survey_0.6.json $(SITE)/das2C/test/das_fetch/
	cp -r test/catalog $(SITE)/catalog
	cp deploy/index.html $(SITE)/
	cp deploy/htaccess $(SITE)/.htaccess
ifneq ($(HTPASSWD),)
	sed 's|@HTPASSWD@|$(HTPASSWD)|' deploy/htauth >> $(SITE)/.htaccess
endif

## Dependencies ##############################################################

# The prerequisite on DAS2C_WASM_LIB is for make -j: it keeps this from
# recursing into das2C while the library build is still in there
deps: $(DAS2C_WASM_LIB)
	$(MAKE) $(DAS2C_WASM_ARGS) build_test

# On deps for the same make -j reason
deps_test: deps
	$(MAKE) $(DAS2C_WASM_ARGS) test

## Housekeeping ##############################################################

$(WBD):
	mkdir -p $@

# Clean das2C too: its objects don't notice compile flag changes, and they
# rebuild quickly.  Keep the downloads.
clean:
	rm -rf build.*
	$(MAKE) $(DAS2C_WASM_ARGS) clean

# Also the expat and FFTW downloads and builds
distclean: clean
	$(MAKE) $(DAS2C_WASM_ARGS) distclean

FORCE:
.PHONY: all help test module headless_test live_test local_test pack pack_test server dist deps deps_test clean distclean FORCE
