from pathlib import Path


def replace_once(path: str, old: str, new: str) -> None:
    file_path = Path(path)
    text = file_path.read_text()
    count = text.count(old)
    if count != 1:
        raise SystemExit(f"{path}: expected one match, found {count}: {old!r}")
    file_path.write_text(text.replace(old, new, 1))


replace_once(
    "interface/vite.config.ts",
    "import { createLogger, type Logger, type UserConfig } from 'vite';\nimport { defineConfig } from 'vitest/config';",
    "import { createLogger, defineConfig, type Logger, type UserConfig } from 'vite';",
)
replace_once(
    "interface/vite.config.ts",
    "\t\ttest: {\n\t\t\tsetupFiles: ['./src/test/setup.ts']\n\t\t},\n",
    "",
)

package_path = Path("interface/package.json")
package_text = package_path.read_text()
script_replacements = {
    '"test": "npm run i18n:build && vitest"': '"test": "npm run i18n:build && node ./scripts/run-vitest.mjs"',
    '"test:ui": "npm run i18n:build && vitest --ui"': '"test:ui": "npm run i18n:build && node ./scripts/run-vitest.mjs --ui"',
    '"test:run": "npm run i18n:build && vitest run"': '"test:run": "npm run i18n:build && node ./scripts/run-vitest.mjs run"',
    '"test:coverage": "npm run i18n:build && vitest run --coverage"': '"test:coverage": "npm run i18n:build && node ./scripts/run-vitest.mjs run --coverage"',
}
for old, new in script_replacements.items():
    if package_text.count(old) != 1:
        raise SystemExit(f"interface/package.json: expected one match: {old}")
    package_text = package_text.replace(old, new, 1)
package_path.write_text(package_text)

Path("interface/scripts/run-vitest.mjs").write_text(
    """import { spawnSync } from 'node:child_process';
import { fileURLToPath } from 'node:url';

const nodeMajor = Number.parseInt(process.versions.node.split('.')[0] ?? '0', 10);
const disableNodeWebStorage = nodeMajor >= 25;
const vitestCli = fileURLToPath(new URL('../node_modules/vitest/vitest.mjs', import.meta.url));
const nodeArgs = disableNodeWebStorage ? ['--no-experimental-webstorage'] : [];
const inheritedNodeOptions = process.env.NODE_OPTIONS?.trim() ?? '';
const nodeOptions = [
\tinheritedNodeOptions,
\tdisableNodeWebStorage ? '--no-experimental-webstorage' : ''
]
\t.filter(Boolean)
\t.join(' ');
const env = nodeOptions ? { ...process.env, NODE_OPTIONS: nodeOptions } : process.env;
const result = spawnSync(process.execPath, [...nodeArgs, vitestCli, ...process.argv.slice(2)], {
\tstdio: 'inherit',
\tenv
});

if (result.error) {
\tconsole.error(result.error);
\tprocess.exit(1);
}

process.exit(result.status ?? 1);
"""
)
