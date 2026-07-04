import { spawn } from 'node:child_process';
import { mkdir, readFile, rm, stat, writeFile } from 'node:fs/promises';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';

const rootDir = dirname(dirname(fileURLToPath(import.meta.url)));
const cacheDir = join(rootDir, 'node_modules', '.cache');
const lockDir = join(cacheDir, 'paraglide-compile.lock');
const lockInfoFile = join(lockDir, 'owner.json');
const stampFile = join(cacheDir, 'paraglide-compile.stamp');
const paraglideCli = join(rootDir, 'node_modules', '@inlang', 'paraglide-js', 'bin', 'run.js');
const compileTimeoutMs = getPositiveNumberEnv('PARAGLIDE_COMPILE_TIMEOUT_MS', 60_000);
const staleLockMs = getPositiveNumberEnv(
	'PARAGLIDE_LOCK_STALE_MS',
	Math.max(compileTimeoutMs + 15_000, 120_000)
);

let ownsLock = false;

function sleep(ms) {
	return new Promise((resolve) => setTimeout(resolve, ms));
}

function getPositiveNumberEnv(name, fallback) {
	const raw = process.env[name];
	if (!raw) return fallback;

	const parsed = Number(raw);
	return Number.isFinite(parsed) && parsed > 0 ? parsed : fallback;
}

function formatDuration(ms) {
	if (ms < 1000) return `${ms}ms`;
	return `${Math.round(ms / 1000)}s`;
}

function isProcessRunning(pid) {
	if (!Number.isInteger(pid) || pid <= 0) {
		return false;
	}

	try {
		process.kill(pid, 0);
		return true;
	} catch (error) {
		return !(error instanceof Error) || !('code' in error) || error.code !== 'ESRCH';
	}
}

async function lockExists() {
	try {
		await stat(lockDir);
		return true;
	} catch {
		return false;
	}
}

async function hasFreshStamp(requestedAt) {
	try {
		const info = await stat(stampFile);
		return info.mtimeMs >= requestedAt;
	} catch {
		return false;
	}
}

async function readLockInfo() {
	try {
		return JSON.parse(await readFile(lockInfoFile, 'utf8'));
	} catch {
		return null;
	}
}

async function writeLockInfo() {
	await writeFile(
		lockInfoFile,
		JSON.stringify(
			{
				pid: process.pid,
				createdAt: new Date().toISOString()
			},
			null,
			2
		) + '\n',
		'utf8'
	);
}

async function removeStaleLockIfNeeded() {
	let lockStat;
	try {
		lockStat = await stat(lockDir);
	} catch {
		return false;
	}

	const ageMs = Date.now() - lockStat.mtimeMs;
	const lockInfo = await readLockInfo();
	const ownerPid = Number(lockInfo?.pid);
	const ownerAlive =
		Number.isInteger(ownerPid) && ownerPid > 0 ? isProcessRunning(ownerPid) : false;
	const shouldRemove =
		(lockInfo !== null && ownerAlive === false) ||
		(lockInfo === null && ageMs >= staleLockMs) ||
		(lockInfo !== null && ageMs >= staleLockMs);

	if (!shouldRemove) {
		return false;
	}

	const reason =
		lockInfo !== null && ownerAlive === false
			? `owner pid ${ownerPid} is no longer running`
			: `lock age ${formatDuration(ageMs)} exceeded ${formatDuration(staleLockMs)}`;

	console.warn(`Removing stale Paraglide lock: ${reason}.`);
	await rm(lockDir, { recursive: true, force: true });
	return true;
}

async function waitForUnlock(requestedAt) {
	while (await lockExists()) {
		if (await removeStaleLockIfNeeded()) {
			return 'retry';
		}

		await sleep(100);
	}

	if (await hasFreshStamp(requestedAt)) {
		return 'skip';
	}

	return 'retry';
}

async function releaseLock() {
	if (!ownsLock) return;
	ownsLock = false;
	await rm(lockDir, { recursive: true, force: true });
}

function runCompile() {
	return new Promise((resolve, reject) => {
		let settled = false;
		const child = spawn(
			process.execPath,
			[paraglideCli, 'compile', '--project', './project.inlang', '--outdir', './src/lib/paraglide'],
			{
				cwd: rootDir,
				stdio: 'inherit'
			}
		);

		let forceKillTimer;
		const compileTimer = setTimeout(() => {
			child.kill('SIGTERM');
			forceKillTimer = setTimeout(() => {
				child.kill('SIGKILL');
			}, 5_000);
			forceKillTimer.unref?.();

			if (settled) return;
			settled = true;
			reject(new Error(`Paraglide compile timed out after ${formatDuration(compileTimeoutMs)}.`));
		}, compileTimeoutMs);

		const clearTimers = () => {
			clearTimeout(compileTimer);
			if (forceKillTimer) {
				clearTimeout(forceKillTimer);
			}
		};

		child.on('error', (error) => {
			clearTimers();
			if (settled) return;
			settled = true;
			reject(error);
		});
		child.on('exit', (code, signal) => {
			clearTimers();
			if (settled) return;
			settled = true;

			if (code === 0) {
				resolve();
				return;
			}

			if (signal) {
				reject(new Error(`Paraglide compile terminated with signal ${signal}.`));
				return;
			}

			reject(new Error(`Paraglide compile exited with code ${code ?? 'unknown'}.`));
		});
	});
}

for (const signal of ['SIGINT', 'SIGTERM', 'SIGHUP']) {
	process.on(signal, () => {
		void releaseLock().finally(() => process.exit(1));
	});
}

async function main() {
	const requestedAt = Date.now();
	await mkdir(cacheDir, { recursive: true });

	while (true) {
		try {
			await mkdir(lockDir);
			ownsLock = true;
			await writeLockInfo();
			break;
		} catch (error) {
			if (!(error instanceof Error) || !('code' in error) || error.code !== 'EEXIST') {
				throw error;
			}

			const action = await waitForUnlock(requestedAt);
			if (action === 'skip') {
				return;
			}
		}
	}

	try {
		await runCompile();
		await writeFile(stampFile, `${new Date().toISOString()}\n`, 'utf8');
	} finally {
		await releaseLock();
	}
}

main().catch(async (error) => {
	await releaseLock();
	console.error(error instanceof Error ? error.message : String(error));
	process.exit(1);
});
