import { spawnSync } from 'node:child_process';
import { fileURLToPath } from 'node:url';

const nodeMajor = Number.parseInt(process.versions.node.split('.')[0] ?? '0', 10);
const disableNodeWebStorage = nodeMajor >= 25;
const vitestCli = fileURLToPath(new URL('../node_modules/vitest/vitest.mjs', import.meta.url));
const nodeArgs = disableNodeWebStorage ? ['--no-experimental-webstorage'] : [];
const inheritedNodeOptions = process.env.NODE_OPTIONS?.trim() ?? '';
const nodeOptions = [
	inheritedNodeOptions,
	disableNodeWebStorage ? '--no-experimental-webstorage' : ''
]
	.filter(Boolean)
	.join(' ');
const env = nodeOptions ? { ...process.env, NODE_OPTIONS: nodeOptions } : process.env;
const result = spawnSync(process.execPath, [...nodeArgs, vitestCli, ...process.argv.slice(2)], {
	stdio: 'inherit',
	env
});

if (result.error) {
	console.error(result.error);
	process.exit(1);
}

process.exit(result.status ?? 1);
