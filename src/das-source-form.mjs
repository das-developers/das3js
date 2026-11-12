/* Copyright (C) 2026 Chris Piker
 * This file is part of das3js, das stream utilities for browsers.
 * See the LICENSE file in the repository root for terms.
 */

/* <das-source-form>: a query form for one das data source, drawn from the
 * source's own interface definition.  A custom element, so it works in
 * plain HTML and inside any framework.
 *
 * Usage:
 *
 *   import {load} from 'das3js';
 *   import 'das3js/source-form';           // defines the tag
 *
 *   const form = document.querySelector('das-source-form');
 *   form.source = node.source();           // a DasCatSource
 *   form.addEventListener('das-request', ev => fetchIt(ev.detail.url));
 *
 * The page owns the source.  Set form.source = null before freeing it.
 *
 * What it draws, from DasCatSource.form:
 *
 *   - Example buttons, each of which fills the form, and a Defaults button
 *     that returns every field to the source's default.  None fetches.
 *   - Coordinates (time ...) and data variables, always shown.
 *   - Server options, then the output format, each in a collapsed section
 *     whose heading says what is chosen, so it can stay closed.  The
 *     format comes last, the choice made right before submitting.
 *
 * Each section box carries data-section="coords", "data", "options" or
 * "formats", for pages and tests that need to find one.
 *
 * Every edit is checked by das2C after a short pause in typing.  A field
 * das2C rejects is marked, with das2C's reason beside it, once the user
 * has touched it or tried to submit.  The submit button stays disabled
 * until the whole request is good.  The form never fetches anything: on
 * submit it hands the URL to the page, which decides what to do with it.
 *
 * Attributes:
 *
 *   show-url       show the request URL as it is built, a debugging aid.
 *                  The query is shown decoded, for reading, as a link to
 *                  the real URL.
 *   pickers        add a date picker beside each time field.  Off by
 *                  default; typed times are the norm in this field.
 *   expand         sections to draw open, space separated: "formats
 *                  options".  Coordinates and data are always open.
 *   submit-label   the button text, default "Get Data"
 *
 * Events, all bubbling and crossing the shadow boundary (composed):
 *
 *   das-change   detail {ok, bad, url, mime, settings}
 *                after every check.  url and mime are '' unless ok.
 *   das-request  detail {url, mime, settings}
 *                the user submitted a good request
 *
 * A page may replace the submit button with its own controls by giving
 * the element children with slot="actions"; they can read form.current
 * and call form.submit().
 *
 * Styling: the form draws in a shadow root, so page CSS doesn't reach in.
 * Colors come from these custom properties, which do inherit through it:
 *
 *   --das-form-muted   help text, units, ranges        default #888
 *   --das-form-error   rejected fields and reasons     default #d33
 *   --das-form-line    borders                         default translucent grey
 *   --das-form-accent  the chosen example              default currentColor
 *
 * and ::part(section), ::part(group), ::part(field), ::part(why),
 * ::part(url), ::part(examples) for more.
 */

/* How long typing must pause before das2C checks the form, milliseconds.
 * Long enough not to check every keystroke, short enough to feel live. */
const PAUSE_MS = 250;

/* Section headings.  das2C's section names are the keys. */
const SECTION_LABEL = {
	coords: 'Coordinates', data: 'Data', formats: 'Output Format', options: 'Options',
};

/* The order sections are drawn in.  das2C lists formats before options;
 * here the format comes last, the choice made right before submitting. */
const SECTION_ORDER = ['coords', 'data', 'options', 'formats'];
function sectionRank(sec)
{
	const i = SECTION_ORDER.indexOf(sec.name);
	return (i < 0) ? SECTION_ORDER.length : i;
}

/* Sections drawn as collapsible boxes; the rest are always open */
const COLLAPSIBLE = new Set(['formats', 'options']);

/* What an empty time field shows.  Users of this field know ISO 8601 and
 * that trailing parts may be left off; das2C's message for a time it
 * can't read gives the format again. */
const TIME_HINT = 'YYYY-MM-DDTHH:MM';

/* Coordinate properties in the order a person reads them, whatever order
 * the node lists them in: from, to, then step */
const COORD_ORDER = ['min', 'minimum', 'max', 'maximum', 'res', 'resolution', 'inter', 'interval'];
function coordRank(p)
{
	const i = COORD_ORDER.indexOf(p.name.toLowerCase());
	return (i < 0) ? COORD_ORDER.length : i;
}

/* Data sections longer than this scroll inside their box */
const SCROLL_AFTER = 12;

const STYLE = `
:host { display: block; }
fieldset, details {
	border: 1px solid var(--das-form-line, rgba(128, 128, 128, 0.4));
	border-radius: 6px; margin: 0 0 0.7em; padding: 0.4em 0.8em 0.6em;
}
legend, summary { font-weight: 600; padding: 0 0.3em; }
summary { cursor: pointer; }
summary .now { font-weight: normal; color: var(--das-form-muted, #888); }
.group { margin: 0.35em 0; }
.ghead { display: flex; align-items: baseline; gap: 0.45em; flex-wrap: wrap; }
.glabel { font-weight: 600; }
.field {
	display: flex; align-items: baseline; gap: 0.45em; flex-wrap: wrap;
	margin: 0.25em 0 0.25em 1.2em;
}
.field.top { margin-left: 0; }
.flabel { min-width: 7em; }
.muted, .units, .echo, .range { color: var(--das-form-muted, #888); font-size: 0.9em; }
.why { color: var(--das-form-error, #d33); font-size: 0.9em; flex-basis: 100%; }
.why:empty { display: none; }
input[aria-invalid=true], select[aria-invalid=true] {
	outline: 2px solid var(--das-form-error, #d33); outline-offset: 1px;
}
input[type=text] { font-family: inherit; }
.examples { display: flex; flex-wrap: wrap; gap: 0.4em; align-items: baseline; margin-bottom: 0.7em; }
.examples button[aria-pressed=true] { outline: 2px solid var(--das-form-accent, currentColor); }
.note { color: var(--das-form-muted, #888); font-size: 0.9em; margin: 0.2em 0; }
.bulk { display: flex; gap: 0.4em; margin: 0.2em 0; }
.scroll { max-height: 22em; overflow-y: auto; padding-right: 0.4em; }
.actions { margin: 0.6em 0 0.3em; }
.url {
	font-family: monospace; font-size: 0.85em; overflow-wrap: anywhere;
	color: var(--das-form-muted, #888); margin-top: 0.4em;
}
.url.ok, .url a { color: inherit; }
`;

/* Element builder, as in das-catalog-tree.mjs.  Text goes in through
 * textContent, never innerHTML: labels come from remote servers.  part and
 * role must be attributes. */
const ATTRS = new Set(['part', 'role', 'slot', 'for']);
function el(tag, props = {}, ...kids)
{
	const e = document.createElement(tag);
	for(const [k, v] of Object.entries(props)){
		if(ATTRS.has(k)) e.setAttribute(k, v);
		else e[k] = v;
	}
	for(const k of kids) if(k !== null && k !== undefined) e.append(k);
	return e;
}

/* Labels in das catalogs may carry das2's old text formatting codes, the
 * IDL convention: "B!Dac!N" is B with "ac" lowered.  !A, !U and !E raise
 * text, !B, !D and !I lower it, !N returns to normal and !! is a literal
 * "!".  Any other code is kept as written.  Returns DOM nodes, built with
 * text nodes only, so a label can never inject markup. */
function richLabel(s)
{
	const frag = document.createDocumentFragment();
	if(!s) return frag;
	let box = frag;          /* where text goes now: frag, or a sup/sub */
	let run = '';
	const flush = () => { if(run){ box.append(run); run = ''; } };

	for(let i = 0; i < s.length; ++i){
		if(s[i] !== '!' || i + 1 >= s.length){ run += s[i]; continue; }
		const c = s[i + 1].toUpperCase();
		if(c === '!'){ run += '!'; ++i; continue; }
		const tag = 'AUE'.includes(c) ? 'sup' : ('BDI'.includes(c) ? 'sub' : null);
		if(tag === null && c !== 'N'){ run += s[i]; continue; }
		flush();
		box = frag;
		if(tag !== null){
			box = document.createElement(tag);
			frag.append(box);
		}
		++i;
	}
	flush();
	return frag;
}

/* A URL with its query decoded, for people to read.  The path is left as
 * is.  Text that won't decode is returned unchanged. */
function readable(url)
{
	const q = url.indexOf('?');
	if(q < 0) return url;
	try{ return url.slice(0, q + 1) + decodeURIComponent(url.slice(q + 1)); }
	catch(e){ return url; }
}

export class DasSourceForm extends HTMLElement {
	#src = null;
	#form = null;         /* the source's form model, src.form */

	/* key -> {prop, input, kind, why, echo, picker}.  kind is 'select',
	 * 'check', 'text' or 'radio'; radio is a format's enable toggle. */
	#fields = new Map();
	#groups = [];         /* {toggle, keys, box} for groups with an enable toggle */
	#summaries = [];      /* functions that refresh the collapsed headings */

	#body;                /* everything drawn from the source */
	#urlLine;
	#submitBtn;
	#exButtons = [];

	#timer = 0;           /* the pending pause-then-check */
	#current = null;      /* the last check's result */
	#touched = new Set(); /* keys the user has edited */
	#showAll = false;     /* after a submit try or an example: show every problem */

	static observedAttributes = ['show-url', 'pickers', 'expand', 'submit-label'];

	constructor()
	{
		super();
		const root = this.attachShadow({mode: 'open'});
		root.append(el('style', {}, STYLE));

		this.#body = el('div');
		this.#submitBtn = el('button', {type: 'button', disabled: true}, this.submitLabel);
		this.#submitBtn.addEventListener('click', () => this.submit());

		/* A <slot> shows the page's own children that name it, or, when the
		 * page gives none, its own content: here the submit button */
		const actions = el('div', {className: 'actions'},
			el('slot', {name: 'actions'}, this.#submitBtn));
		this.#urlLine = el('div', {className: 'url', part: 'url', hidden: true});
		root.append(this.#body, actions, this.#urlLine);
	}

	attributeChangedCallback(name)
	{
		if(name === 'submit-label'){ this.#submitBtn.textContent = this.submitLabel; return; }
		if(name === 'show-url'){ this.#showUrl(); return; }
		/* pickers and expand change what is drawn */
		if(this.#src !== null) this.#draw();
	}

	get submitLabel(){ return this.getAttribute('submit-label') || 'Get Data'; }

	/** The DasCatSource the form is for.  Setting it redraws the form with
	 * the source's defaults; null empties it. */
	get source(){ return this.#src; }
	set source(src)
	{
		this.#src = src;
		this.#form = src ? src.form : null;
		this.#touched.clear();
		this.#showAll = false;
		this.#current = null;
		this.#draw();
	}

	/** The last check: {ok, bad, url, mime, settings}, or null before one */
	get current(){ return this.#current; }

	/** The form's values as das2C property keys, {'coords.time.min': ...} */
	get settings()
	{
		const out = {};
		for(const [key, f] of this.#fields){
			if(f.input.disabled) continue;   /* belongs to an unchosen format */
			let val;
			if(f.kind === 'radio'){
				/* Only the chosen format is named; das2C turns off the rest */
				if(!f.input.checked) continue;
				val = 'true';
			}
			else if(f.kind === 'check') val = f.input.checked ? 'true' : 'false';
			else val = f.input.value.trim();

			/* An empty optional field means "server default": leave it out.
			 * An empty required one stays, so das2C reports it missing. */
			if(val === '' && !f.prop.ctrl.required) continue;
			out[key] = val;
		}
		return out;
	}

	/** Fill the form from settings ({key: value}), after returning every
	 * field to its default.  Keys that aren't fields are ignored. */
	set settings(settings)
	{
		this.#reset();
		for(const [key, val] of Object.entries(settings)) this.#setField(key, String(val));
		this.#syncGroups();
		this.#check();
	}

	/** Load worked example i into the form */
	useExample(i)
	{
		this.settings = this.#src.example(i).settings;
		this.#showAll = true;          /* a filled form shows all its problems */
		this.#exButtons.forEach((b, j) => b.setAttribute('aria-pressed', String(i === j)));
		this.#check();
	}

	/** Return every field to the source's default */
	useDefaults(){ this.settings = {}; }

	/** Check now, and if the request is good, fire das-request.  Returns
	 * the check result. */
	submit()
	{
		this.#showAll = true;
		const res = this.#check();
		if(res && res.ok)
			this.#emit('das-request', {url: res.url, mime: res.mime, settings: res.settings});
		return res;
	}

	/* ******************************************************************** */
	/* Drawing */

	#draw()
	{
		clearTimeout(this.#timer);
		this.#fields.clear();
		this.#groups = [];
		this.#summaries = [];
		this.#exButtons = [];
		this.#body.replaceChildren();
		this.#urlLine.hidden = true;
		this.#submitBtn.disabled = true;
		if(this.#form === null) return;

		/* The worked examples, and a way back from them */
		const examples = this.#form.examples;
		const row = el('div', {className: 'examples', part: 'examples'});
		if(examples.length > 0) row.append(el('span', {className: 'muted'}, 'Examples:'));
		examples.forEach((lbl, i) => {
			const b = el('button', {type: 'button'}, lbl || `Example ${i + 1}`);
			b.setAttribute('aria-pressed', 'false');
			b.addEventListener('click', () => this.useExample(i));
			this.#exButtons.push(b);
			row.append(b);
		});
		const defs = el('button', {type: 'button', title: "Every field back to the source's default"},
			'Defaults');
		defs.addEventListener('click', () => this.useDefaults());
		row.append(defs);
		this.#body.append(row);

		const expand = new Set((this.getAttribute('expand') || '').split(/\s+/));
		const sections = [...this.#form.sections].sort((a, b) => sectionRank(a) - sectionRank(b));
		for(const sec of sections) this.#body.append(this.#section(sec, expand.has(sec.name)));

		this.#syncGroups();
		this.#check();
	}

	/* A <fieldset> for an always open section, a <details> for one that
	 * collapses.  <details> is the browser's own disclosure box: click its
	 * <summary> to open or close it, no script needed. */
	#section(sec, open)
	{
		const label = SECTION_LABEL[sec.name] || sec.name;
		let box, heading;
		if(COLLAPSIBLE.has(sec.name)){
			const now = el('span', {className: 'now'});
			heading = el('summary', {}, label, now);
			box = el('details', {part: 'section', open}, heading);
			box.dataset.section = sec.name;
			this.#summaries.push(() => { now.textContent = this.#sectionNow(sec); });
		}
		else{
			box = el('fieldset', {part: 'section'}, el('legend', {}, label));
			box.dataset.section = sec.name;
		}

		/* A data section of nothing but on/off variables gets the rule an
		 * empty choice follows, and all/none buttons when there are many */
		if(sec.name === 'data'){
			const toggles = sec.groups.filter(g => g.enable !== null && !g.xorGroup);
			if(toggles.length > 1 && toggles.length === sec.groups.length){
				box.append(el('p', {className: 'note'},
					'Check the variables to send.  With none checked, the server sends all of them.'));
				const all = el('button', {type: 'button'}, 'All');
				const none = el('button', {type: 'button'}, 'None');
				all.addEventListener('click', () => this.#setToggles(toggles, true));
				none.addEventListener('click', () => this.#setToggles(toggles, false));
				box.append(el('div', {className: 'bulk'}, all, none));
			}
		}

		const inner = (sec.groups.length > SCROLL_AFTER) ? el('div', {className: 'scroll'}) : box;
		for(const g of sec.groups) inner.append(this.#group(sec, g));
		if(inner !== box) box.append(inner);
		return box;
	}

	/* What a collapsed section's heading shows: the chosen format, or how
	 * many options differ from their defaults */
	#sectionNow(sec)
	{
		if(sec.name === 'formats'){
			const on = sec.groups.find(g => g.enable && this.#fields.get(g.enable)?.input.checked);
			return on ? `: ${on.label}` : '';
		}
		let n = 0;
		for(const g of sec.groups) for(const p of g.props){
			const f = this.#fields.get(p.key);
			if(f && this.#valueOf(f) !== this.#defaultOf(p)) ++n;
		}
		return n ? `: ${n} changed` : '';
	}

	#group(sec, g)
	{
		const box = el('div', {className: 'group', part: 'group'});
		const onProp = g.enable ? g.props.find(p => p.key === g.enable) : null;

		/* The heading: an on/off control when the group has one, a radio
		 * button when it is one of an exclusive set (formats) */
		if(onProp && onProp.ctrl){
			const kind = g.xorGroup ? 'radio' : 'check';
			const input = el('input', {type: kind === 'radio' ? 'radio' : 'checkbox'});
			if(kind === 'radio') input.name = `xor-${g.xorGroup}`;
			input.checked = (onProp.value === 'true');
			const why = el('span', {className: 'why', part: 'why'});
			this.#fields.set(onProp.key, {prop: onProp, input, kind, why, echo: null, picker: null});
			input.addEventListener('change', () => this.#edited(onProp.key, true));

			const label = el('label', {className: 'glabel'}, input, ' ', richLabel(g.label));
			const head = el('div', {className: 'ghead'}, label);
			if(g.title) head.append(el('span', {className: 'muted'}, g.title));
			head.append(why);
			box.append(head);
		}
		else if(g.name !== '' && (g.label || g.name)){
			/* The anonymous options group needs no heading, its section has one */
			const head = el('div', {className: 'ghead'}, el('span', {className: 'glabel'}, richLabel(g.label)));
			if(g.title) head.append(el('span', {className: 'muted'}, g.title));
			box.append(head);
		}

		const keys = [];
		const props = (sec.name === 'coords') ?
			[...g.props].sort((a, b) => coordRank(a) - coordRank(b)) : g.props;
		for(const p of props){
			if(p === onProp) continue;
			if(p.ctrl === null){
				/* A fixed property is information, except the units of a
				 * coordinate, which its fields show already */
				if(sec.name === 'coords' && p.name === 'units') continue;
				box.append(el('div', {className: 'field muted', part: 'field'},
					el('span', {className: 'flabel'}, richLabel(p.label || p.name)), p.value ?? ''));
				continue;
			}
			box.append(this.#field(p, g.name === ''));
			keys.push(p.key);
		}

		/* A coordinate's valid range, once, from its minimum's control */
		if(sec.name === 'coords'){
			const lo = g.props.find(p => p.ctrl && p.ctrl.range && p.type === 'time');
			if(lo){
				const [a, b] = lo.ctrl.range;
				box.append(el('div', {className: 'field range'},
					`Valid range: ${a ?? 'any'} to ${b ?? 'any'}${lo.units ? ' ' + lo.units : ''}`));
			}
		}

		if(onProp && onProp.ctrl) this.#groups.push({toggle: onProp.key, keys, box});
		return box;
	}

	/* One adjustable property: label, control, units, and room for das2C's
	 * reason when it rejects the value */
	#field(p, top)
	{
		let input, kind;
		const def = this.#defaultOf(p);

		if(p.ctrl.choices.length > 0){
			kind = 'select';
			input = el('select');
			/* No default and not required: the server decides, say so */
			if(p.value === null && !p.ctrl.required)
				input.append(el('option', {value: ''}, '(server default)'));
			for(const c of p.ctrl.choices)
				input.append(el('option', {value: c.value}, c.label ?? c.value));
			input.value = def;
		}
		else if(p.type === 'boolean'){
			kind = 'check';
			input = el('input', {type: 'checkbox', checked: def === 'true'});
		}
		else{
			kind = 'text';
			input = el('input', {type: 'text', value: def, spellcheck: false, autocomplete: 'off'});
			if(p.type === 'time'){
				input.placeholder = TIME_HINT;
				input.size = 22;
			}
			else{
				input.size = 8;
				if(p.type === 'integer') input.inputMode = 'numeric';
				if(p.type === 'real') input.inputMode = 'decimal';
			}
		}
		input.setAttribute('part', 'input');

		/* Hover text: the long description, and the allowed range if any */
		let tip = p.title || '';
		if(p.ctrl.range && p.type !== 'time'){
			const [a, b] = p.ctrl.range;
			tip += `${tip ? '\n' : ''}From ${a ?? 'any'} to ${b ?? 'any'}`;
		}
		const id = `f-${p.key}`;
		input.id = id;
		if(tip) input.title = tip;

		const why = el('span', {className: 'why', part: 'why'});
		const row = el('div', {className: top ? 'field top' : 'field', part: 'field'});
		if(kind === 'check'){
			/* A checkbox reads best with its label after it */
			row.append(el('label', {for: id}, input, ' ', richLabel(p.label || p.name)));
			if(p.title && p.title !== p.label) row.append(el('span', {className: 'muted'}, p.title));
		}
		else{
			row.append(el('label', {className: 'flabel', for: id, title: tip}, richLabel(p.label || p.name)), input);
			if(p.units && p.type !== 'time') row.append(el('span', {className: 'units'}, p.units));
			if(p.type === 'time') row.append(el('span', {className: 'units'}, p.units || 'UTC'));
		}

		let echo = null, picker = null;
		if(kind === 'text' && p.type === 'time'){
			if(this.hasAttribute('pickers')){
				/* datetime-local carries no zone; read and written as UTC */
				picker = el('input', {type: 'datetime-local', step: 1, title: 'Pick a time (UTC)'});
				picker.addEventListener('change', () => {
					input.value = picker.value;
					this.#edited(p.key, true);
				});
				row.append(picker);
			}
			echo = el('span', {className: 'echo'});
			row.append(echo);
		}
		row.append(why);

		this.#fields.set(p.key, {prop: p, input, kind, why, echo, picker});
		/* 'input' fires per keystroke, 'change' when a value is committed:
		 * picking from a list, toggling a box, leaving a text field */
		input.addEventListener('input', () => this.#edited(p.key, false));
		input.addEventListener('change', () => this.#edited(p.key, true));
		return row;
	}

	/* ******************************************************************** */
	/* Values */

	#defaultOf(p)
	{
		if(p.value === null) return (p.type === 'boolean') ? 'false' : '';
		return p.value;
	}

	#valueOf(f)
	{
		if(f.kind === 'check' || f.kind === 'radio') return f.input.checked ? 'true' : 'false';
		return f.input.value.trim();
	}

	#reset()
	{
		for(const f of this.#fields.values()){
			const def = this.#defaultOf(f.prop);
			if(f.kind === 'check' || f.kind === 'radio') f.input.checked = (def === 'true');
			else f.input.value = def;
		}
		this.#touched.clear();
		this.#showAll = false;
		this.#exButtons.forEach(b => b.setAttribute('aria-pressed', 'false'));
	}

	#setField(key, val)
	{
		const f = this.#fields.get(key);
		if(!f) return;
		if(f.kind === 'check' || f.kind === 'radio'){
			f.input.checked = /^(true|1|yes)$/i.test(val);
			return;
		}
		if(f.kind === 'select' && ![...f.input.options].some(o => o.value === val)){
			/* das2C accepted it but it isn't listed; show it rather than
			 * silently picking something else */
			f.input.append(el('option', {value: val}, val));
		}
		f.input.value = val;
	}

	#setToggles(groups, on)
	{
		for(const g of groups){
			const f = this.#fields.get(g.enable);
			if(f){ f.input.checked = on; this.#touched.add(g.enable); }
		}
		this.#syncGroups();
		this.#check();
	}

	/* Fields of a group that is off are disabled: their values would be
	 * ignored, and a disabled box says so at a glance */
	#syncGroups()
	{
		for(const g of this.#groups){
			const on = this.#fields.get(g.toggle).input.checked;
			for(const k of g.keys){
				const f = this.#fields.get(k);
				f.input.disabled = !on;
				if(f.picker) f.picker.disabled = !on;
			}
		}
	}

	/* ******************************************************************** */
	/* Checking */

	#edited(key, now)
	{
		this.#touched.add(key);
		this.#exButtons.forEach(b => b.setAttribute('aria-pressed', 'false'));
		this.#syncGroups();
		this.#echo(key);
		clearTimeout(this.#timer);
		if(now) this.#check();
		else this.#timer = setTimeout(() => this.#check(), PAUSE_MS);
	}

	/* Beside a time field, what das2C read, when that tells the user
	 * something: '2026-015' shows '= 2026-01-15T00:00:00'.  A time typed as
	 * the start of what das2C would write, '2026-01-15T12', gets no echo;
	 * only the zeros it leaves off would differ. */
	#echo(key)
	{
		const f = this.#fields.get(key);
		if(!f || !f.echo) return;
		const typed = f.input.value.trim();
		const iso = typed ? this.#src.runtime.isoTime(typed) : '';
		f.echo.textContent = (iso && !iso.startsWith(typed)) ? `= ${iso}` : '';
	}

	#check()
	{
		clearTimeout(this.#timer);
		if(this.#src === null) return null;

		const settings = this.settings;
		let res;
		try{ res = this.#src.check(settings); }
		catch(e){
			/* das2C couldn't even build the request, rare: report and stop */
			res = {ok: false, bad: 1, entries: [], url: '', mime: '', error: e.message};
		}
		res.settings = settings;
		this.#current = res;

		/* Mark fields by verdict.  Problems show for touched fields only,
		 * until a submit try or an example asks for all of them. */
		const verdicts = new Map(res.entries.map(e => [e.key, e]));
		for(const [key, f] of this.#fields){
			const v = verdicts.get(key);
			const bad = v && v.verdict === 'bad' && (this.#showAll || this.#touched.has(key));
			f.why.textContent = bad ? v.why : '';
			f.input.setAttribute('aria-invalid', String(Boolean(bad)));
			if(f.echo) this.#echo(key);
		}
		for(const refresh of this.#summaries) refresh();

		this.#submitBtn.disabled = !res.ok;
		this.#showUrl();
		this.#emit('das-change', {ok: res.ok, bad: res.bad, url: res.url, mime: res.mime, settings});
		return res;
	}

	#showUrl()
	{
		const res = this.#current;
		this.#urlLine.hidden = !this.hasAttribute('show-url') || res === null;
		if(this.#urlLine.hidden) return;
		this.#urlLine.className = res.ok ? 'url ok' : 'url';
		if(res.ok){
			/* Shown decoded, for reading: 'read.params=anode=8 apid=x2a2'.
			 * That text isn't a working URL, so it links to the real one,
			 * which copy-link and a click get.  Only http(s): the URL comes
			 * from a catalog, and a javascript: link would run on click. */
			const text = readable(res.url);
			const shown = /^https?:\/\//i.test(res.url) ?
				el('a', {href: res.url, target: '_blank', rel: 'noopener'}, text) : text;
			this.#urlLine.replaceChildren(shown, res.mime ? `  (${res.mime})` : '');
		}
		else if(res.error)
			this.#urlLine.textContent = `No request: ${res.error}`;
		else
			this.#urlLine.textContent = `No request yet: ${res.bad} field${res.bad === 1 ? '' : 's'} to fix`;
	}

	#emit(type, detail)
	{
		this.dispatchEvent(new CustomEvent(type, {detail, bubbles: true, composed: true}));
	}
}

/* Defining a tag twice throws, which a page importing this module through
 * two different URLs would otherwise hit */
if(!customElements.get('das-source-form'))
	customElements.define('das-source-form', DasSourceForm);
