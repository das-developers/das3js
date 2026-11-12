/* The catalog browser: <das-catalog-tree> on the left, and a details pane
 * that shows a data source's request form, <das-source-form>.
 *
 * The two elements are the shippable parts, src/das-catalog-tree.mjs and
 * src/das-source-form.mjs.  This file is what an application would write
 * around them: make a catalog, hand it to the tree, hand the selected
 * source to the form, and act on the requests the form builds.
 *
 * Query parameters, so a view can be bookmarked:
 *
 *   seed=URL     start from this node instead of START below
 *   flat=1       set the tree's no-collapse attribute
 *   internal=1   read the catalog from the internal test catalog, a
 *                partial copy in test/catalog/, instead of the network
 *   check=1      also click through the tree once and report the result
 *                to the node runner (make headless_test); implies internal
 */

import {load, DasError} from '../build/das3js.mjs';
import '../build/das-catalog-tree.mjs';     /* defines <das-catalog-tree> */
import '../build/das-source-form.mjs';      /* defines <das-source-form> */

const tree = document.getElementById('tree');
const info = document.getElementById('info');
const status = document.getElementById('status');
const seedBox = document.getElementById('seed');
const internalNote = document.getElementById('internal');
const flatBox = document.getElementById('flat');

/* See Inspect.mjs: verdicts go to the node runner when one is listening */
/* The node runner listens on 127.0.0.1 only; elsewhere there is no /log */
const RUNNER = location.hostname === '127.0.0.1';

function report(line, pass)
{
	if(!RUNNER) return;
	fetch('/log', {method: 'POST', body: line}).catch(() => {});
	if(pass !== undefined)
		fetch('/done', {method: 'POST', body: JSON.stringify({pass})}).catch(() => {});
}

/* Map the real https URLs a walk asks for onto the internal test catalog,
 * ../catalog/ under make server and make dist alike */
function testCatalogFetch(url, init)
{
	return fetch(url.replace(/^https:\/\//, '../catalog/'), init);
}

/* Text through textContent only: catalog strings come from remote servers */
function el(tag, props = {}, ...kids)
{
	const e = Object.assign(document.createElement(tag), props);
	for(const k of kids) e.append(k);
	return e;
}

const das = await load({progName: 'catalog'});

/* Without a seed: das2C's compiled in root, opened at U. Iowa */
const START = 'tag:das2.org,2012:site:/uiowa';

/* Request types the structure inspector can read; anything else, a CDF
 * file for instance, is offered as a download */
const STREAM_MIME = new Set([
	'application/vnd.das2.das2stream', 'text/vnd.das2.das2stream',
	'application/vnd.das.stream', 'text/vnd.das.stream',
]);

let cat = null;        /* the DasCatalog shown */
let src = null;        /* the DasCatSource in the details pane, freed on change */
let t0 = 0;

/* ************************************************************************ */
/* Details pane */

/* A link for an http(s) URL, plain text for anything else: the URL comes
 * from a catalog, and a javascript: link would run on click */
function link(url, text = url)
{
	if(!/^https?:\/\//i.test(url || '')) return text;
	return el('a', {href: url, target: '_blank', rel: 'noopener'}, text);
}

function field(dl, name, value)
{
	if(value === null || value === undefined || value === '') return;
	dl.append(el('dt', {}, name), el('dd', {}, value));
}

function freeSource()
{
	if(src){ src.free(); src = null; }
}

/* What the pane says when no source is selected */
function placeholder()
{
	info.replaceChildren(el('p', {className: 'placeholder'}, 'Select a stream source'));
}

/* Only data sources fill the pane.  Containers leave the placeholder: the
 * tree already says what is in them. */
function showNode(node)
{
	freeSource();
	placeholder();
	if(!node.isSource) return;
	info.replaceChildren();

	info.append(el('h3', {}, node.title || node.label || node.name || '(untitled)'));
	if(node.uri) info.append(el('p', {className: 'uri'}, node.uri));
	if(node.description) info.append(el('p', {}, node.description));
	showSource(node);

	/* Where it came from, for the curious, out of the way */
	const dl = el('dl', {className: 'foot'});
	field(dl, 'node type', `${node.type} ${node.version || ''}`.trim());
	field(dl, 'loaded from', link(node.url));
	info.append(dl);
}

/* A data source: a <das-source-form> drawn from its interface, and two
 * actions on the request it builds.  The form only builds and checks
 * requests; what happens to one is this page's business. */
function showSource(node)
{
	try{ src = node.source(); }
	catch(e){
		info.append(el('p', {className: 'err'}, e.message));
		return;
	}
	if(src.needsAuth)
		info.append(el('p', {}, el('span', {className: 'badge'},
				'login may be required' + (src.authRealm ? ` (${src.authRealm})` : '')),
			'  The catalog marks this source as protected; the server decides per request.'));

	const form = el('das-source-form');
	form.toggleAttribute('show-url', true);   /* a test page: show the URL */
	/* Date pickers only where typing is awkward: a touch screen, no mouse */
	form.toggleAttribute('pickers', matchMedia('(pointer: coarse)').matches);

	/* The page's buttons replace the form's own, through its actions slot */
	const view = el('button', {type: 'button', disabled: true}, 'View Structure');
	const save = el('button', {type: 'button', disabled: true}, 'Download');
	const note = el('span', {className: 'muted'});
	const actions = el('span', {}, view, ' ', save, ' ', note);
	actions.setAttribute('slot', 'actions');
	form.append(actions);

	/* No type given: older das2 nodes, whose output is a das2 stream */
	const isStream = mime => !mime || STREAM_MIME.has(mime);
	form.addEventListener('das-change', ev => {
		const {ok, mime} = ev.detail;
		view.disabled = !(ok && isStream(mime));
		save.disabled = !ok;
	});
	view.addEventListener('click', () => {
		const res = form.submit();
		if(res && res.ok)
			window.open('Inspect.html?url=' + encodeURIComponent(res.url), '_blank', 'noopener');
	});
	save.addEventListener('click', async () => {
		const res = form.submit();
		if(!res || !res.ok) return;
		note.textContent = 'downloading...';
		try{ note.textContent = await download(res.url); }
		catch(e){ note.textContent = `download failed: ${e.message}`; }
	});

	info.append(el('h3', {}, 'Request'), form);
	form.source = src;
}

/* Fetch a URL into memory and hand it to the browser as a file to save.
 * The name is the server's, from Content-Disposition, else the last part
 * of the URL path.  Returns a line for the page to show. */
async function download(url)
{
	const resp = await fetch(url);
	if(!resp.ok) throw new Error(`HTTP ${resp.status} ${resp.statusText}`);
	const m = /filename="?([^";]+)"?/i.exec(resp.headers.get('Content-Disposition') || '');
	const name = m ? m[1] : (new URL(url).pathname.split('/').pop() || 'das_data');
	const blob = await resp.blob();

	/* An object URL names the in-memory bytes; a link with a download
	 * attribute pointed at it saves them instead of navigating */
	const href = URL.createObjectURL(blob);
	const a = el('a', {href, download: name});
	document.body.append(a);
	a.click();
	a.remove();
	setTimeout(() => URL.revokeObjectURL(href), 10000);
	return `saved ${name}, ${blob.size} bytes`;
}

/* An entry das2C can't load: the listing is all there is */
function showEntry(parent, entry)
{
	freeSource();
	const dl = el('dl');
	field(dl, 'node type', `${entry.stub.type || '?'} (not a type this das2C loads)`);
	field(dl, 'listed in', link(parent.url, parent.uri ?? parent.url));
	if(entry.urls.length)
		field(dl, 'urls', el('div', {}, ...entry.urls.map(u => el('div', {}, link(u)))));
	field(dl, 'provides', entry.provides.join(', '));
	field(dl, 'about', entry.description);
	info.replaceChildren(el('h3', {}, entry.title || entry.label), dl);
}

function showError(title, e)
{
	freeSource();
	info.replaceChildren(el('h3', {}, title),
		el('p', {className: 'err'}, (e instanceof DasError) ? e.message : `${e.name}: ${e.message}`));
}

/* ************************************************************************ */
/* The tree's events */

tree.addEventListener('das-open', ev => {
	const top = ev.detail.node;
	const ms = (performance.now() - t0).toFixed(0);
	status.textContent = `${top.uri ?? top.url}: ${top.children.length} children, ${ms} ms`;
	showNode(top);
});

tree.addEventListener('das-select', ev => {
	const {node, parent, entry} = ev.detail;
	if(node) showNode(node);
	else showEntry(parent, entry);
});

tree.addEventListener('das-error', ev => {
	const {entry, error} = ev.detail;
	if(entry === null) status.textContent = '';
	showError(entry ? (entry.title || entry.label) : 'catalog failed to open', error);
});

function open(seed, internal)
{
	freeSource();
	placeholder();
	tree.catalog = null;   /* never free a catalog the tree still holds */
	if(cat) cat.free();

	const opts = {};
	if(seed) opts.seed = seed;
	else opts.start = START;
	if(internal) opts.fetch = testCatalogFetch;
	cat = das.newCatalog(opts);

	status.textContent = 'loading...';
	t0 = performance.now();
	tree.catalog = cat;   /* the element loads and draws it */
}

/* ************************************************************************ */
/* Headless check, on the internal test catalog: open the tree, then click
 * down to the TRACERS-1 L2 MSC BAC SourceSet.  Of its flex and das2 DSDF
 * sources das2C loads only flex, so its row should collapse into that.
 * With no-collapse, the Voyager 1 PWS SpecAnalyzer-4s-Efield Collection
 * must open as a container.
 * Done the way a user would, through the shadow root's rows and click(). */

function next(type)
{
	return new Promise(resolve => tree.addEventListener(type, ev => resolve(ev), {once: true}));
}

/* Click the row labeled lbl among the rows under 'under' (a <ul>) and
 * return its das-select detail.  :scope > li pins it to that one level. */
function findRow(under, lbl)
{
	const row = Array.from(under.querySelectorAll(':scope > li > [part=row]'))
		.find(r => r.querySelector('[part=label]').textContent === lbl);
	if(!row) throw new Error(`no row '${lbl}'`);
	return row;
}

/* A leaf: no aria-expanded and no twisty painted */
function drawnAsLeaf(row)
{
	return !row.hasAttribute('aria-expanded') && row.querySelector('.twisty').textContent === '';
}

async function clickRow(under, lbl)
{
	const row = findRow(under, lbl);
	const picked = Promise.race([next('das-select'), next('das-error')]);
	row.click();
	const ev = await picked;
	if(ev.type === 'das-error') throw ev.detail.error;
	return {row, ...ev.detail};
}

/* The MSC BAC request form, as a user would drive it: examples, a typed
 * time das2C rejects, then a day-of-year time it reads */
function checkForm()
{
	const form = info.querySelector('das-source-form');
	if(!form) throw new Error('no request form for MSC BAC');
	const fr = form.shadowRoot;
	const [view, save] = info.querySelectorAll('[slot=actions] button');

	const exBtns = [...fr.querySelectorAll('[part=examples] button')].filter(b => b.textContent !== 'Defaults');
	if(exBtns.length !== 2) throw new Error(`${exBtns.length} example buttons`);
	exBtns[1].click();   /* asks for CDF, which can only be downloaded */
	if(!form.current.ok || form.current.mime !== 'application/x-cdf')
		throw new Error(`CDF example: ok ${form.current.ok}, mime '${form.current.mime}'`);
	if(!view.disabled || save.disabled) throw new Error('CDF example: want Download only');
	const fmtNow = fr.querySelector('[data-section=formats] summary .now').textContent;
	if(fmtNow !== ': CDF file') throw new Error(`format heading '${fmtNow}'`);
	exBtns[0].click();
	if(view.disabled) throw new Error('das stream example: View Structure is off');

	/* 'change' is what a browser fires when a typed value is committed */
	const tmin = fr.getElementById('f-coords.time.min');
	const type = text => { tmin.value = text; tmin.dispatchEvent(new Event('change')); };
	const row = tmin.parentElement;

	type('Tuesday');
	const why = row.querySelector('[part=why]').textContent;
	if(!why.includes('Tuesday') || tmin.getAttribute('aria-invalid') !== 'true' || !save.disabled)
		throw new Error(`bad time: why '${why}', download ${save.disabled ? 'off' : 'on'}`);

	type('2026-195T22:11');
	const echo = row.querySelector('.echo').textContent;
	if(echo !== '= 2026-07-14T22:11:00') throw new Error(`day of year echo '${echo}'`);
	if(!form.current.ok || row.querySelector('[part=why]').textContent !== '')
		throw new Error('day of year time refused');
	/* The URL line reads decoded but links to the real URL */
	const urlA = fr.querySelector('[part=url] a');
	if(!urlA || urlA.href !== form.current.url || !form.current.url.includes('read.time.min=2026-195T22%3A11')
	   || !urlA.textContent.includes('read.time.min=2026-195T22:11'))
		throw new Error(`url line '${urlA?.textContent}' -> ${urlA?.href}`);

	/* Defaults: back to the source's own, which has no times */
	const defs = [...fr.querySelectorAll('[part=examples] button')].find(b => b.textContent === 'Defaults');
	defs.click();
	if(tmin.value !== '' || form.current.ok || !save.disabled)
		throw new Error(`Defaults left time '${tmin.value}', ok ${form.current.ok}`);
	return `form: 2 examples, a bad time flagged, 2026-195T22:11 read as 2026-07-14T22:11:00, `
		+ `decoded URL linked, Defaults clears`;
}

async function selfCheck()
{
	const opened = await Promise.race([next('das-open'), next('das-error')]);
	if(opened.type === 'das-error') throw opened.detail.error;

	const top = opened.detail.node;
	if(top.uri !== START) throw new Error(`opened ${top.uri}`);
	const ul = tree.shadowRoot.querySelector('ul');
	const n = ul.querySelectorAll(':scope > li > [part=row]').length;
	if(n !== top.children.length) throw new Error(`${n} rows for ${top.children.length} children`);

	let at = ul, sel;
	for(const lbl of ['Tracers', 'TRACERS-1', 'L2']){
		sel = await clickRow(at, lbl);
		if(sel.row.getAttribute('aria-expanded') !== 'true') throw new Error(`${lbl} did not open`);
		const kids = sel.row.parentElement.querySelectorAll(':scope > ul > li > [part=row]').length;
		if(kids !== sel.node.children.length) throw new Error(`${kids} rows under ${lbl}, ${sel.node.children.length} children`);
		at = sel.row.parentElement.querySelector(':scope > ul');
	}
	if(info.querySelector('.placeholder') === null || info.childElementCount !== 1)
		throw new Error('a container filled the details pane');

	if(!drawnAsLeaf(findRow(at, 'MSC BAC'))) throw new Error('MSC BAC drawn as a container before opening');
	sel = await clickRow(at, 'MSC BAC');
	if(!sel.node.isSource) throw new Error(`MSC BAC gave a ${sel.node.kind}, not a source`);
	if(sel.parent.kind !== 'srcset' || sel.entry.name !== 'flex')
		throw new Error(`MSC BAC collapsed to ${sel.parent.kind} / ${sel.entry.name}`);
	if(sel.row.hasAttribute('aria-expanded') || sel.row.parentElement.querySelector('ul'))
		throw new Error('MSC BAC row still drawn as a container');
	const srcUri = sel.node.uri;

	const formMsg = checkForm();
	const SA = 'SpecAnalyzer-4s-Efield';

	/* no-collapse redraws the tree; the Collection must then open as one */
	const redrawn = next('das-open');
	tree.noCollapse = true;
	await redrawn;
	at = tree.shadowRoot.querySelector('ul');
	for(const lbl of ['Voyager', '1', 'PWS', SA]){
		if(lbl === SA && drawnAsLeaf(findRow(at, lbl)))
			throw new Error(`with no-collapse ${SA} drawn as a source`);
		sel = await clickRow(at, lbl);
		at = sel.row.parentElement.querySelector(':scope > ul');
	}
	if(sel.node.kind !== 'collection' || sel.row.getAttribute('aria-expanded') !== 'true')
		throw new Error(`with no-collapse ${SA} gave a ${sel.node.kind}`);
	const nColl = at.querySelectorAll(':scope > li > [part=row]').length;

	/* An entry das2C can't load: its pane links the listing and each URL */
	at = tree.shadowRoot.querySelector('ul');
	for(const lbl of ['Tracers', 'TRACERS-1', 'L2', 'MSC BAC']){
		sel = await clickRow(at, lbl);
		at = sel.row.parentElement.querySelector(':scope > ul');
	}
	sel = await clickRow(at, 'Das2 DSDF');
	if(sel.node !== null) throw new Error(`Das2 DSDF loaded as a ${sel.node.kind}`);
	const hrefs = Array.from(info.querySelectorAll('a'), a => a.href);
	if(!hrefs.some(h => h.endsWith('/l2/msc_bac.json')) || !hrefs.some(h => h.endsWith('/l2/msc_bac/das2.d2t')))
		throw new Error(`entry pane links: ${hrefs.join(' ')}`);

	/* The rare case, from the hand-made test/catalog/local/srcsets.json: a
	 * SourceSet with two usable sources is drawn as a source until opened,
	 * then gets its twisty and opens as a container.  Detach first so the
	 * attribute change has no catalog to redraw, see open(). */
	tree.catalog = null;
	tree.noCollapse = false;
	const reopened = next('das-open');
	open('https://local/srcsets.json', true);
	await reopened;
	at = tree.shadowRoot.querySelector('ul');
	for(const lbl of ['MSC BAC', 'Two das2'])
		if(!drawnAsLeaf(findRow(at, lbl))) throw new Error(`${lbl} drawn as a container before opening`);

	sel = await clickRow(at, 'Two das2');
	if(sel.node.kind !== 'srcset' || sel.row.getAttribute('aria-expanded') !== 'true')
		throw new Error(`'Two das2' gave a ${sel.node.kind}, expanded ${sel.row.getAttribute('aria-expanded')}`);
	const nTwo = sel.row.parentElement.querySelectorAll(':scope > ul > li > [part=row]').length;
	if(nTwo !== 2) throw new Error(`${nTwo} rows under 'Two das2'`);
	sel = await clickRow(at, 'MSC BAC');
	if(!sel.node.isSource || sel.entry.name !== 'flex')
		throw new Error(`seeded MSC BAC gave a ${sel.node.kind} / ${sel.entry.name}`);

	return `opened ${top.uri}, ${n} rows; down to L2, MSC BAC collapsed to ${srcUri}; ${formMsg}; `
		+ `with no-collapse ${SA} opened with ${nColl} row(s), the DSDF entry linked; `
		+ `a two-source set drawn as a source opened with ${nTwo}`;
}

/* ************************************************************************ */

document.getElementById('form').addEventListener('submit', ev => {
	ev.preventDefault();
	const q = new URLSearchParams();
	if(seedBox.value.trim()) q.set('seed', seedBox.value.trim());
	if(internal) q.set('internal', '1');
	if(flatBox.checked) q.set('flat', '1');
	history.replaceState(null, '', '?' + q);
	/* After open(): a no-collapse change redraws whatever catalog the tree
	 * holds, and the old one is freed by then */
	open(seedBox.value.trim(), internal);
	tree.noCollapse = flatBox.checked;
});

const q = new URLSearchParams(location.search);
seedBox.value = q.get('seed') || '';
const check = q.get('check') === '1';
const internal = check || q.get('internal') === '1';
internalNote.hidden = !internal;
flatBox.checked = q.get('flat') === '1';
tree.noCollapse = flatBox.checked;

if(check)
	selfCheck().then(
		msg => report(`INFO: catalog tree: ${msg}`, true),
		e => report(`ERROR: catalog tree: ${e.message}`, false));
open(seedBox.value, internal);
