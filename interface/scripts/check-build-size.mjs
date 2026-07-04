import { readFile, stat } from 'node:fs/promises';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';
import { gzipSync } from 'node:zlib';
import { globSync } from 'glob';

const rootDir = dirname(dirname(fileURLToPath(import.meta.url)));
const configPath = join(rootDir, '.size-limit.json');
const buildDir = join(rootDir, 'build');

const UNIT_BYTES = {
	B: 1,
	KB: 1024,
	MB: 1024 * 1024
};

function parseLimit(limit) {
	const match = /^(?<value>\d+(?:\.\d+)?)\s*(?<unit>B|KB|MB)$/i.exec(limit.trim());
	if (!match?.groups) {
		throw new Error(`Unsupported size limit format: ${limit}`);
	}

	const unit = match.groups.unit.toUpperCase();
	return Math.round(Number(match.groups.value) * UNIT_BYTES[unit]);
}

function formatBytes(bytes) {
	if (bytes >= UNIT_BYTES.MB) {
		return `${(bytes / UNIT_BYTES.MB).toFixed(2)} MB`;
	}

	if (bytes >= UNIT_BYTES.KB) {
		return `${(bytes / UNIT_BYTES.KB).toFixed(2)} KB`;
	}

	return `${bytes} B`;
}

async function getFileSize(file, gzip) {
	const absolutePath = join(rootDir, file);
	if (gzip) {
		// Paths come from a repo-local glob over build artifacts.
		// eslint-disable-next-line security/detect-non-literal-fs-filename
		return gzipSync(await readFile(absolutePath)).length;
	}

	// Paths come from a repo-local glob over build artifacts.
	// eslint-disable-next-line security/detect-non-literal-fs-filename
	return (await stat(absolutePath)).size;
}

async function getBudgetSize(pattern, gzip, exclude = [], mode = 'total') {
	const files = globSync(pattern, {
		cwd: rootDir,
		nodir: true,
		ignore: exclude
	}).sort();

	if (files.length === 0) {
		throw new Error(`No build artifacts matched "${pattern}". Run npm run build first.`);
	}

	let total = 0;
	let largestFile = '';

	for (const file of files) {
		const fileSize = await getFileSize(file, gzip);

		if (mode === 'largest') {
			if (fileSize > total) {
				total = fileSize;
				largestFile = file;
			}
		} else if (mode === 'total') {
			total += fileSize;
		} else {
			throw new Error(`Unsupported size budget mode: ${mode}`);
		}
	}

	return { files, largestFile, total };
}

async function main() {
	await stat(buildDir).catch(() => {
		throw new Error('Missing build directory. Run npm run build first.');
	});

	const budgets = JSON.parse(await readFile(configPath, 'utf8'));
	let hasFailures = false;

	for (const budget of budgets) {
		const limit = parseLimit(budget.limit);
		const mode = budget.mode ?? 'total';
		const { files, largestFile, total } = await getBudgetSize(
			budget.path,
			Boolean(budget.gzip),
			Array.isArray(budget.exclude) ? budget.exclude : [],
			mode
		);
		const status = total <= limit ? 'PASS' : 'FAIL';
		const detail = mode === 'largest' && largestFile ? `, largest ${largestFile}` : '';

		console.log(
			`${status} ${budget.name}: ${formatBytes(total)} / ${budget.limit} (${files.length} files${detail})`
		);

		if (total > limit) {
			hasFailures = true;
		}
	}

	if (hasFailures) {
		process.exit(1);
	}
}

main().catch((error) => {
	console.error(error instanceof Error ? error.message : String(error));
	process.exit(1);
});
