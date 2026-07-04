import dns from 'node:dns/promises';

// Node 18+ has native fetch
// process.env.NODE_TLS_REJECT_UNAUTHORIZED = '0'; // Global disable for fetch

async function testConnection() {
	const hostname = 'matrixhub.local';
	console.log(`Testing resolution for ${hostname}...`);

	try {
		const res = await dns.lookup(hostname);
		console.log('DNS Lookup success (default):', res);
	} catch (e) {
		console.error('DNS Lookup failed (default):', e);
	}

	try {
		const res = await dns.lookup(hostname, { family: 4 });
		console.log('DNS Lookup success (IPv4):', res);
	} catch (e) {
		console.error('DNS Lookup failed (IPv4):', e);
	}

	console.log(`\nTesting fetch to https://${hostname}/api/system/status...`);
	try {
		const start = Date.now();
		// Custom agent or just ignore certs globally for this script
		process.env.NODE_TLS_REJECT_UNAUTHORIZED = '0';

		const response = await fetch(`https://${hostname}/api/system/status`);
		const time = Date.now() - start;

		console.log(`Fetch status: ${response.status}`);
		console.log(`Time taken: ${time}ms`);
		const text = await response.text();
		console.log(`Body length: ${text.length}`);
	} catch (e) {
		console.error('Fetch failed:', e);
		if (e.cause) console.error('Cause:', e.cause);
	}
}

testConnection();
