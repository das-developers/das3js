/* A downstream app as a webpack user writes one: import das3js by its
 * package name and nothing else.  The question this answers is whether
 * webpack finds das2c.wasm through the module's own
 * `new URL('das2c.wasm', import.meta.url)`, emits it into dist/ and points
 * the URL at the copy.
 *
 * The rest is the test protocol of test/browser/run.mjs: POST log lines to
 * /log and the verdict to /done, and read the stream fixture from /das2C/,
 * all on the same server as the app.  The same check is in each consumer
 * under test/npm. */

import {load} from 'das3js';

const WHO = 'webpack';
const FIXTURE = '/das2C/examples/ex08_cassini_rpws_survey.d2t';
const DATASETS = 13;

async function log(line)
{
	console.log(line);
	await fetch('/log', {method: 'POST', body: line});
}

async function main()
{
	let pass = false;
	try{
		const das3 = await load();
		const strm = await das3.fetchStream(FIXTURE);
		const nDs = strm.length;
		const nRecs = strm.dataset(0).shape[0];
		strm.free();

		pass = (nDs === DATASETS) && (nRecs > 0);
		await log(`${pass ? 'INFO' : 'ERROR'}: ${WHO}: ${nDs} datasets `
			+ `(expected ${DATASETS}), ${nRecs} records in the first`);
	}
	catch(e){
		await log(`ERROR: ${WHO}: ${e.message}`);
	}
	await fetch('/done', {method: 'POST', body: JSON.stringify({pass})});
}

main();
