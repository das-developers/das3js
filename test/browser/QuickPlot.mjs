/* A quick look at a das stream: painted as it downloads, with rough axes,
 * and panned by dragging.
 *
 * Test page code, a first draft of what may become a component.  The
 * painting is DasRender (src/render.h), which makes pixels and nothing
 * else; this file adds the frame around them.  The ticks are a stand-in:
 * eleven evenly spaced per axis, every other one labeled by straight
 * interpolation, no rounding to nice values yet.
 *
 * The page stays live during a download, though everything here runs on
 * one thread.  fetchStream() awaits each chunk from the network, and the
 * browser handles clicks (Stop included) and redraws while it waits.  A
 * feed() holds the thread only for one chunk's parse, a few ms.  Stop
 * aborts the fetch through an AbortSignal; the pending read rejects with
 * an AbortError and the loop unwinds through its finally blocks.
 *
 * Pan: drag the data area sideways.  While the button is down the last
 * frame slides under the pointer, a plain canvas copy.  On release the
 * renderer moves its grid by the same whole pixels (DasRender.pan) and
 * only the exposed time span is requested and painted in.  The plot
 * remembers which span it holds complete, so a gap fetch that was stopped
 * is asked for again on the next pan.
 *
 * Usage:
 *
 *   const qp = new QuickPlot(das, someDiv);    // shown on the first start()
 *   qp.start(node.source(), form.settings, 'A title');   // a promise
 *   qp.stop();
 *
 * The plot owns the source it is given and frees it when replaced.  While
 * a fetch is going its header carries a Stop button, upper right.
 * onchange, if set, hears true when a fetch starts and false when it ends.
 */

import {DasError} from '../build/das3js.mjs';

/* Margins around the data area in CSS pixels: tick labels left and below */
const M = {l: 66, r: 14, t: 10, b: 40};

/* Data area shape, the desktop tools' 800:232 */
const ASPECT = 232 / 800;

/* Frame period while downloading.  Painting happens inside feed(); a
 * frame only copies pixels out, so 15 Hz is plenty. */
const FRAME_MS = 66;

const NTICK = 10;   /* intervals, so NTICK + 1 tick marks */

function el(tag, props = {}, ...kids)
{
	const e = Object.assign(document.createElement(tag), props);
	for(const k of kids) e.append(k);
	return e;
}

function fmtBytes(n)
{
	if(n < 1024) return `${n} bytes`;
	if(n < 1048576) return `${(n / 1024).toFixed(1)} KiB`;
	return `${(n / 1048576).toFixed(1)} MiB`;
}

/* A number for a tick label: short, exponent form outside 0.01 to 1e4 */
function fmtVal(v)
{
	if(v === 0) return '0';
	const a = Math.abs(v);
	return (a >= 1e4 || a < 1e-2) ? v.toExponential(1) : String(Number(v.toPrecision(3)));
}

/* "1.2e-7 to 3.4e-3 V/m, log" for the caption */
function rangeText(a)
{
	if(Number.isNaN(a.min)) return '?';
	return `${fmtVal(a.min)} to ${fmtVal(a.max)} ${a.units}${a.log ? ', log' : ''}`;
}

/* IDL style codes (!U, !N ...) in stream labels; drawn plain for now */
function plainUnits(s){ return (s || '').replace(/!(.)/g, (m, c) => c === '!' ? '!' : ''); }

/* Epoch ms to ISO-C, to the ms and without a zone: what das2C and the
 * servers read for a request.  Date's own format ends in Z. */
const isoMs = ms => new Date(ms).toISOString().slice(0, 23);

/* The header's range line after a pan: minutes are plenty for a span of
 * hours, seconds for a short one */
function spanText(x)
{
	const n = (x.t1 - x.t0 > 7200e3) ? 16 : 19;
	return `${isoMs(x.t0).slice(0, n)} to ${isoMs(x.t1).slice(0, n)} UTC`;
}

/* x moved n device pixels of an area dw wide; positive n is the data
 * moving right, so earlier time comes into view */
function shifted(x, n, dw)
{
	const ms = n * (x.t1 - x.t0) / dw;
	return {t0: x.t0 - ms, t1: x.t1 - ms};
}

const STOPPED = {state: 'stopped', kind: 'none', bytes: 0, message: ''};

export class QuickPlot {
	#das;
	#box;
	#v = null;      /* the plot on screen, see #panel() */
	#ctl = null;    /* AbortController of the fetch in progress */
	#run = null;    /* promise of the latest queued job, see #queue() */
	#seq = 0;       /* bumped by every job and by stop(), see #queue() */

	/** Called with true when a fetch starts and false when it ends */
	onchange = null;

	constructor(das, box)
	{
		this.#das = das;
		this.#box = box;
	}

	get running(){ return this.#ctl !== null; }

	/** The latest start's or pan's promise, see start() */
	get done(){ return this.#run; }

	/** The time range on screen, epoch ms {t0, t1}, or null */
	get range(){ return (this.#v && this.#v.x) ? {...this.#v.x} : null; }

	/** Abort the fetch in progress, if any, and anything queued behind
	 * it.  Its promise resolves with state 'stopped'. */
	stop()
	{
		++this.#seq;
		if(this.#ctl) this.#ctl.abort();
	}

	/** Stop, empty the box and hide it */
	clear()
	{
		this.#box.replaceChildren();
		this.#box.hidden = true;
		this.#queue(() => { this.#drop(); return null; });
	}

	/** Fetch a request and paint it, x running over its time range.
	 *
	 * @param {DasCatSource} src The source to request from, owned by the
	 *        plot from here on: pans build their requests from it after
	 *        the page has moved on to another node.  Pass a fresh copy
	 *        (DasCatNode.source()).
	 * @param {object} settings The request, keys to values as the form
	 *        gives them; the time range is replaced for each pan
	 * @param {string} [title] What is plotted, shown above it
	 * @returns {Promise<object>} never rejects: {state, kind, bytes,
	 *        message}, state 'done', 'stopped' or 'failed'
	 */
	start(src, settings, title = '')
	{
		/* The panel goes up at once, the fetch waits its turn */
		const v = this.#panel(src, title);
		return this.#queue(seq => {
			this.#drop();
			this.#v = v;
			if(seq !== this.#seq){
				v.status.textContent = 'stopped';
				v.stop.hidden = true;
				return STOPPED;
			}
			return this.#open(v, settings);
		});
	}

	/* Jobs (start, pan, clear) run one at a time, each after the last has
	 * unwound.  That's what makes it safe to move or free the renderer in
	 * a job: the previous fetch has freed its reader, which points at the
	 * renderer, by the time its promise settles.  A job queued behind
	 * another reads seq to learn whether something newer, or Stop, came
	 * along while it waited. */
	#queue(job)
	{
		if(this.#ctl) this.#ctl.abort();
		const prev = this.#run;
		const seq = ++this.#seq;
		this.#run = (async () => {
			if(prev) await prev;
			return job(seq);
		})();
		return this.#run;
	}

	/* Free what the plot on screen owns */
	#drop()
	{
		const v = this.#v;
		if(v === null) return;
		this.#v = null;
		if(v.ren) v.ren.free();
		v.src.free();
	}

	/* The elements of one plot, shown right away.  The returned object is
	 * the plot's whole state; the renderer and axis are added by #open(). */
	#panel(src, title)
	{
		const canvas = el('canvas', {className: 'qp-canvas'});
		const status = el('p', {className: 'qp-status'}, 'requesting...');
		const caption = el('p', {className: 'qp-caption'});
		/* Stop sits in the header, upper right, and only while fetching */
		const stop = el('button', {type: 'button', className: 'qp-stop'}, 'Stop');
		stop.addEventListener('click', () => this.stop());
		const range = el('p', {className: 'qp-range'});
		const head = el('div', {className: 'qp-head'},
			el('div', {}, el('h3', {className: 'qp-title'}, title || 'Quick look'), range),
			stop);
		const wrap = el('div', {className: 'qp'}, head, canvas, status, caption);
		this.#box.replaceChildren(wrap);
		this.#box.hidden = false;
		this.#box.scrollIntoView({block: 'nearest', behavior: 'smooth'});

		return {
			src, wrap, canvas, status, caption, stop, range,
			ctx: null, g: null, ren: null,
			x: null,         /* time on screen, epoch ms {t0, t1} */
			cover: null,     /* the span painted complete, [c0, c1] ms */
			base: null,      /* the settings minus the time range */
			kMin: null, kMax: null,
			drag: null,      /* the drag in progress, see #grab() */
			held: 0,         /* pans released but not yet applied */
			last: 0,         /* time of the last frame */
		};
	}

	/* Rounded box under the plot for what the server or das2C said */
	#say(v, text, isErr)
	{
		v.canvas.after(el('p', {className: isErr ? 'qp-msg qp-err' : 'qp-msg'}, text));
	}

	/* Check the request, set up the renderer and axes, and fetch */
	#open(v, settings)
	{
		let url;
		try{
			const res = v.src.check(settings);
			if(!res.ok){
				const bad = res.entries.filter(e => e.verdict === 'bad').map(e => `${e.key}: ${e.why}`);
				throw new DasError(`the request isn't complete: ${bad.join('; ')}`);
			}
			url = res.url;

			/* The x range is the one requested, read through das2C's keys: a
			 * 2018 node files it under coords.time.minimum, a current one
			 * under coords.time.min.  Pans replace it, so any spelling of it
			 * in the settings is left out of the base they start from. */
			const s = v.src;
			v.kMin = s.key('time.min');
			v.kMax = s.key('time.max');
			const val = k => res.entries.find(e => e.key === k)?.value;
			const beg = val(v.kMin), end = val(v.kMax);
			v.base = {};
			for(const [k, x] of Object.entries(settings)){
				const full = s.key(k);
				if(full !== v.kMin && full !== v.kMax) v.base[k] = x;
			}

			v.x = this.#timeAxis(beg, end);
			v.range.textContent = `${beg} to ${end} UTC`;
			v.g = this.#layout(v.wrap, v.canvas);
			v.ctx = v.canvas.getContext('2d');
			v.ren = this.#das.newRender(v.g.dw, v.g.dh);
			v.ren.setX(beg, end);
		}
		catch(e){
			v.status.textContent = 'failed';
			v.stop.hidden = true;
			v.canvas.hidden = true;
			this.#say(v, e.message, true);
			return {state: 'failed', kind: 'none', bytes: 0, message: e.message};
		}

		const {ctx, g} = v;
		ctx.fillStyle = '#000';
		ctx.fillRect(g.dl, g.dt, g.dw, g.dh);
		this.#axes(ctx, g, v.x, v.ren.axis(false));
		this.#grab(v);
		return this.#fetch(v, url, v.x.t0, v.x.t1, false);
	}

	/* Copy the renderer's pixels to the canvas, at most every FRAME_MS
	 * unless forced.  Not while a drag shows its own picture, nor between
	 * a release and the pan it asked for. */
	#paint(v, force)
	{
		if(v.drag || v.held || !v.ren) return;
		const now = performance.now();
		if(!force && (now - v.last < FRAME_MS)) return;
		v.last = now;
		if(v.ren.dirty) v.ctx.putImageData(v.ren.imageData(), v.g.dl, v.g.dt);
		this.#axes(v.ctx, v.g, v.x, v.ren.axis(false));
		v.caption.textContent = describe(v.ren);
	}

	/* One fetch into the plot's renderer, covering ms a to b.  isGap marks
	 * a pan's fill-in: the server saying there is nothing there is normal
	 * for those, so it goes in the status line, not a message box. */
	async #fetch(v, url, a, b, isGap)
	{
		const ctl = new AbortController();
		this.#ctl = ctl;
		if(this.onchange) this.onchange(true);
		v.stop.hidden = false;
		const what = isGap ? `${spanText({t0: a, t1: b})}: ` : '';
		v.status.textContent = `${what}requesting...`;

		let bytes = 0, exc = '';
		const t0 = performance.now();
		try{
			const strm = await this.#das.fetchStream(url, {
				signal: ctl.signal,
				onReader: rdr => v.ren.attach(rdr),
				onChunk: (rdr, n) => {
					bytes = n;
					exc = rdr.exception;
					v.status.textContent = `${what}${fmtBytes(n)}...`;
					this.#paint(v, false);
				},
			});
			strm.free();   /* only the picture is wanted here */
			this.#paint(v, true);

			/* Complete, so the span is known even if it held nothing.  Gaps
			 * touch the covered span, so the union stays one interval. */
			v.cover = v.cover ? [Math.min(v.cover[0], a), Math.max(v.cover[1], b)] : [a, b];

			const sec = ((performance.now() - t0) / 1000).toFixed(1);
			v.status.textContent = `${what}${fmtBytes(bytes)} in ${sec} s` + ((isGap && exc) ? `, ${exc}` : '');
			const kind = v.ren.kind;
			if(!isGap){
				if(exc) this.#say(v, exc, false);
				if(kind === 'none' && !exc) this.#say(v, 'The stream held no data to paint.', false);
				else if(kind === 'unusable') this.#say(v, `Can't paint this stream: ${v.ren.note}`, true);
			}
			return {state: 'done', kind, bytes, message: exc};
		}
		catch(e){
			this.#paint(v, true);   /* whatever arrived stays on screen */
			const kind = v.ren.kind;
			if(e.name === 'AbortError'){
				v.status.textContent = `${what}stopped after ${fmtBytes(bytes)}`;
				return {state: 'stopped', kind, bytes, message: ''};
			}
			/* A server's own words beat ours: "No data in interval" */
			const msg = (e instanceof DasError && e.exception) ? e.exception : e.message;
			v.status.textContent = `${what}` + (bytes ? `failed after ${fmtBytes(bytes)}` : 'failed');
			if(isGap && e instanceof DasError && e.exception) v.status.textContent += `, ${msg}`;
			else{
				if(bytes === 0 && !isGap) v.canvas.hidden = true;
				this.#say(v, msg, true);
			}
			return {state: 'failed', kind, bytes, message: msg};
		}
		finally{
			v.stop.hidden = true;
			if(this.#ctl === ctl){
				this.#ctl = null;
				if(this.onchange) this.onchange(false);
			}
		}
	}

	/* Drag to pan.  Pointer events cover mouse, pen and touch alike;
	 * pointer capture keeps the moves coming when the pointer leaves the
	 * canvas mid-drag.  touch-action pan-y leaves vertical swipes to the
	 * page's scrolling and gives sideways ones to us. */
	#grab(v)
	{
		const {canvas, ctx, g} = v;
		canvas.style.cursor = 'grab';
		canvas.style.touchAction = 'pan-y';

		canvas.addEventListener('pointerdown', e => {
			if(e.button !== 0 || this.#v !== v) return;
			const r = canvas.getBoundingClientRect();
			const px = e.clientX - r.left, py = e.clientY - r.top;
			const L = g.dl / g.s, T = g.dt / g.s;
			if(px < L || px > L + g.dw / g.s || py < T || py > T + g.dh / g.s) return;
			try{ canvas.setPointerCapture(e.pointerId); }catch{ /* synthetic events have no live pointer */ }
			/* The picture as shown now; a fetch still running keeps feeding
			 * the renderer but isn't drawn until the drag ends */
			v.drag = {x0: e.clientX, n: 0, snap: ctx.getImageData(g.dl, g.dt, g.dw, g.dh)};
			canvas.style.cursor = 'grabbing';
			e.preventDefault();
		});

		canvas.addEventListener('pointermove', e => {
			const dr = v.drag;
			if(!dr) return;
			const n = Math.round((e.clientX - dr.x0) * g.s);   /* device px */
			if(n === dr.n) return;
			dr.n = n;
			/* The overhang into the margins is cleared by #axes() */
			ctx.fillStyle = '#000';
			ctx.fillRect(g.dl, g.dt, g.dw, g.dh);
			ctx.putImageData(dr.snap, g.dl + n, g.dt);
			const x = shifted(v.x, n, g.dw);
			this.#axes(ctx, g, x, v.ren.axis(false));
			v.range.textContent = spanText(x);
		});

		const end = () => {
			const dr = v.drag;
			if(!dr) return;
			v.drag = null;
			canvas.style.cursor = 'grab';
			if(dr.n === 0){ this.#paint(v, true); return; }   /* a click */
			this.#pan(v, dr.n);
		};
		canvas.addEventListener('pointerup', end);
		canvas.addEventListener('pointercancel', end);
	}

	/* Move the picture n device pixels and fetch what came into view */
	#pan(v, n)
	{
		++v.held;   /* keep the dragged picture up until the pan lands */
		return this.#queue(async seq => {
			if(this.#v !== v) return STOPPED;
			const g = v.g;

			/* Always applied, even when stale: the pixels on screen were
			 * dragged, the renderer has to follow them */
			v.ren.pan(n);
			v.x = shifted(v.x, n, g.dw);
			--v.held;
			v.range.textContent = spanText(v.x);
			for(const m of v.wrap.querySelectorAll('.qp-msg')) m.remove();
			v.canvas.hidden = false;
			this.#paint(v, true);
			if(seq !== this.#seq) return STOPPED;   /* another pan, or Stop */

			/* What's on screen and known complete, then the rest.  Under a
			 * pixel isn't worth a request. */
			const {t0, t1} = v.x;
			const px = (t1 - t0) / g.dw;
			let c = v.cover ? [Math.max(v.cover[0], t0), Math.min(v.cover[1], t1)] : null;
			if(c && (c[1] - c[0] < px)) c = null;
			v.cover = c;
			let gaps;
			if(c === null){
				/* Nothing known complete: ask for the whole window.  A
				 * spectrogram keeps the peak per pixel, so painting a record
				 * twice is harmless and the moved picture can stay.  Density
				 * counts samples, which would double, so clear it first: a
				 * full width pan out and back. */
				if(v.ren.kind === 'density'){ v.ren.pan(g.dw); v.ren.pan(-g.dw); this.#paint(v, true); }
				gaps = [[t0, t1]];
			}
			else gaps = [[t0, c[0]], [c[1], t1]].filter(([a, b]) => b - a >= px);

			let r = {state: 'done', kind: v.ren.kind, bytes: 0, message: ''};
			for(const [a, b] of gaps){
				if(seq !== this.#seq) return {...r, state: 'stopped'};
				let url;
				try{
					const res = v.src.check({...v.base, [v.kMin]: isoMs(a), [v.kMax]: isoMs(b)});
					if(!res.ok) throw new DasError(`can't request ${isoMs(a)} to ${isoMs(b)}`);
					url = res.url;
				}
				catch(e){
					this.#say(v, e.message, true);
					return {state: 'failed', kind: v.ren.kind, bytes: 0, message: e.message};
				}
				r = await this.#fetch(v, url, a, b, true);
				if(r.state !== 'done') break;
			}
			return {...r, gaps: gaps.length};
		});
	}

	/* Size the canvas to its box in device pixels, so a big screen gets
	 * detail rather than a stretched picture.  Returns the geometry in
	 * device pixels: data area offset (dl, dt) and size (dw, dh), and
	 * the scale (s) for drawing in CSS pixels. */
	#layout(wrap, canvas)
	{
		const s = window.devicePixelRatio || 1;
		const cssW = Math.max(320, wrap.clientWidth || 640);
		const dw = Math.min(2400, Math.round((cssW - M.l - M.r) * s));
		const dh = Math.round(dw * ASPECT);
		canvas.width = Math.round(cssW * s);
		canvas.height = dh + Math.round((M.t + M.b) * s);
		canvas.style.width = `${cssW}px`;
		canvas.style.height = `${canvas.height / s}px`;
		return {s, dl: Math.round(M.l * s), dt: Math.round(M.t * s), dw, dh,
			w: cssW, h: canvas.height / s};
	}

	/* The time axis as epoch milliseconds.  das2C reads the text, so any
	 * form the form accepted works here; isoTime() returns '' if not. */
	#timeAxis(beg, end)
	{
		if(!beg || !end) throw new DasError('the request has no time range to plot against');
		const ms = text => {
			const iso = this.#das.isoTime(text, 3);
			return iso ? Date.parse(iso + 'Z') : NaN;
		};
		const t0 = ms(beg), t1 = ms(end);
		if(!(t1 > t0)) throw new DasError(`can't plot the time range '${beg}' to '${end}'`);
		return {t0, t1};
	}

	/* Frame, ticks and labels around the data area, redrawn every frame:
	 * the vertical range can change as data arrive.  Only the margins are
	 * cleared, the data area belongs to putImageData. */
	#axes(ctx, g, x, y)
	{
		const cs = getComputedStyle(this.#box);
		const ink = cs.getPropertyValue('--qp-ink').trim() || cs.color;
		const line = cs.getPropertyValue('--qp-line').trim() || ink;

		ctx.save();
		ctx.setTransform(g.s, 0, 0, g.s, 0, 0);   /* draw in CSS pixels */
		const L = g.dl / g.s, T = g.dt / g.s, W = g.dw / g.s, H = g.dh / g.s;
		ctx.clearRect(0, 0, g.w, T);
		ctx.clearRect(0, 0, L, g.h);
		ctx.clearRect(L + W, 0, g.w - L - W, g.h);
		ctx.clearRect(0, T + H, g.w, g.h - T - H);

		ctx.strokeStyle = line;
		ctx.fillStyle = ink;
		ctx.lineWidth = 1;
		ctx.font = '11px system-ui, sans-serif';
		ctx.strokeRect(L - 0.5, T - 0.5, W + 1, H + 1);

		/* Time: labels on the even ticks.  Within two days, clock time with
		 * the date beneath where it changes; beyond, dates. */
		const span = x.t1 - x.t0;
		let lastDay = '';
		ctx.textBaseline = 'top';
		for(let i = 0; i <= NTICK; ++i){
			const px = L + W * i / NTICK;
			ctx.beginPath();
			ctx.moveTo(px, T + H);
			ctx.lineTo(px, T + H + 5);
			ctx.stroke();
			if(i % 2) continue;
			const iso = new Date(x.t0 + span * i / NTICK).toISOString();
			const day = iso.slice(0, 10);
			let top, bot = '';
			if(span <= 2 * 86400e3){
				top = iso.slice(11, span < 600e3 ? 19 : 16);
				if(day !== lastDay) bot = day;
			}
			else top = day;
			lastDay = day;
			ctx.textAlign = (i === 0) ? 'left' : (i === NTICK) ? 'right' : 'center';
			ctx.fillText(top, px, T + H + 8);
			if(bot) ctx.fillText(bot, px, T + H + 22);
		}

		/* Vertical: the renderer's range, once known */
		ctx.textAlign = 'right';
		ctx.textBaseline = 'middle';
		const known = !Number.isNaN(y.min);
		const lo = y.log ? Math.log10(y.min) : y.min;
		const hi = y.log ? Math.log10(y.max) : y.max;
		for(let i = 0; i <= NTICK; ++i){
			const py = T + H - H * i / NTICK;
			ctx.beginPath();
			ctx.moveTo(L - 5, py);
			ctx.lineTo(L, py);
			ctx.stroke();
			if(!known || (i % 2)) continue;
			const f = lo + (hi - lo) * i / NTICK;
			ctx.fillText(fmtVal(y.log ? 10 ** f : f), L - 8, py);
		}
		if(known && y.units){
			ctx.save();
			ctx.translate(10, T + H / 2);
			ctx.rotate(-Math.PI / 2);
			ctx.textAlign = 'center';
			ctx.fillText(plainUnits(y.units), 0, 0);
			ctx.restore();
		}
		ctx.restore();
	}
}

/* What was painted, for the line under the plot */
function describe(ren)
{
	let s = ren.kind;
	if(ren.kind === 'spectrogram') s += `, color ${rangeText(ren.axis(true))}`;
	else if(ren.kind === 'density') s += ', color is log(samples per pixel)';
	if(ren.skipped) s += `; ${ren.skipped} dataset(s) of another shape skipped`;
	if(ren.note) s += `; ${ren.note}`;
	return s;
}
