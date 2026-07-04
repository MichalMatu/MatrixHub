#!/usr/bin/env node
/* eslint-disable security/detect-non-literal-fs-filename */

import fs from 'node:fs';
import path from 'node:path';
import process from 'node:process';

const repoRoot = process.cwd();
const srcRoot = path.join(repoRoot, 'src');
const strict = process.argv.includes('--strict');

const zeroDebtBudgets = {
	settingsCardMissingReset: 0,
	settingsCardMissingDirtySource: 0,
	rawStyledControl: 0,
	cssImportant: 0,
	importantUtilityClass: 0,
	responsiveOrderUtility: 0,
	themeContract: 0
};

const budgets = zeroDebtBudgets;

const ignoredFormPrimitivePaths = [
	'src/lib/components/shared/forms/',
	'src/lib/components/shared/tables/',
	'src/lib/components/forms/FormPrimitivesHarness.svelte'
];

const importantUtilityPattern =
	/(^|\s)!(?:[a-z]+:)?(?:p|px|py|pt|pr|pb|pl|m|mx|my|mt|mr|mb|ml|min-h|min-w|max-h|max-w|h|w|text|bg|border|rounded|shadow|flex|grid|gap|opacity|block|inline|hidden|overflow|absolute|relative|static|fixed|sticky|z|inset|top|right|bottom|left)-[^\s"'`{}]+/g;

const orderUtilityPattern = /(^|\s)(?:[a-z]+:)?order-[a-z0-9-]+/g;

const svelteAttributePatterns = {
	onSave: [/\bonSave\s*=/, /\{\s*onSave\s*\}/],
	onReset: [/\bonReset\s*=/, /\{\s*onReset\s*\}/],
	dirtySourceId: [/\bdirtySourceId\s*=/, /\{\s*dirtySourceId\s*\}/]
};

const issues = {
	settingsCardMissingReset: [],
	settingsCardMissingDirtySource: [],
	rawStyledControl: [],
	cssImportant: [],
	importantUtilityClass: [],
	responsiveOrderUtility: [],
	themeContract: []
};

function walk(dir) {
	const entries = fs.readdirSync(dir, { withFileTypes: true });
	const files = [];
	for (const entry of entries) {
		if (entry.name.startsWith('.') || entry.name === 'node_modules') continue;
		const fullPath = path.join(dir, entry.name);
		if (entry.isDirectory()) {
			files.push(...walk(fullPath));
		} else if (/\.(svelte|css)$/.test(entry.name)) {
			files.push(fullPath);
		}
	}
	return files;
}

function toRepoPath(filePath) {
	return path.relative(repoRoot, filePath).replaceAll(path.sep, '/');
}

function lineNumber(text, index) {
	return text.slice(0, index).split('\n').length;
}

function compact(value) {
	return value.replace(/\s+/g, ' ').trim();
}

function pushIssue(rule, relPath, line, message, snippet) {
	issues[rule].push({
		path: relPath,
		line,
		message,
		snippet: compact(snippet).slice(0, 220)
	});
}

function isIgnoredFormPrimitive(relPath) {
	return ignoredFormPrimitivePaths.some((ignoredPath) => relPath.includes(ignoredPath));
}

function findOpeningTags(text, tagName) {
	const tags = [];
	const needle = `<${tagName}`;
	let searchFrom = 0;

	while (searchFrom < text.length) {
		const start = text.indexOf(needle, searchFrom);
		if (start === -1) break;

		let quote = '';
		let braceDepth = 0;
		for (let index = start + needle.length; index < text.length; index += 1) {
			const char = text[index];
			const previous = text[index - 1];

			if (quote) {
				if (char === quote && previous !== '\\') {
					quote = '';
				}
				continue;
			}

			if (char === '"' || char === "'" || char === '`') {
				quote = char;
				continue;
			}

			if (char === '{') {
				braceDepth += 1;
				continue;
			}

			if (char === '}') {
				braceDepth = Math.max(0, braceDepth - 1);
				continue;
			}

			if (char === '>' && braceDepth === 0) {
				tags.push({ index: start, value: text.slice(start, index + 1) });
				searchFrom = index + 1;
				break;
			}
		}

		if (searchFrom <= start) {
			searchFrom = start + needle.length;
		}
	}

	return tags;
}

function extractClassBody(element) {
	const classMatch =
		/class\s*=\s*(?:"([^"]*)"|'([^']*)'|`([^`]*)`|\{`([^`]*)`\}|\{"([^"]*)"\}|\{'([^']*)'\})/s.exec(
			element
		);
	return classMatch?.slice(1).find(Boolean) ?? '';
}

function hasSvelteAttribute(attrs, name) {
	return svelteAttributePatterns[name]?.some((pattern) => pattern.test(attrs)) ?? false;
}

function scanSettingsCards(text, relPath) {
	for (const tag of findOpeningTags(text, 'SettingsCard')) {
		const attrs = tag.value;
		if (!hasSvelteAttribute(attrs, 'onSave')) continue;
		const line = lineNumber(text, tag.index);
		if (!hasSvelteAttribute(attrs, 'onReset')) {
			pushIssue(
				'settingsCardMissingReset',
				relPath,
				line,
				'SettingsCard with onSave should expose onReset for the local discard/back action.',
				attrs
			);
		}
		if (!hasSvelteAttribute(attrs, 'dirtySourceId')) {
			pushIssue(
				'settingsCardMissingDirtySource',
				relPath,
				line,
				'SettingsCard with onSave should provide a stable dirtySourceId.',
				attrs
			);
		}
	}
}

function scanNativeStyledControls(text, relPath) {
	if (isIgnoredFormPrimitive(relPath)) return;

	const elementPattern = /<(input|select|textarea|table)\b[\s\S]*?>/g;
	let match;
	while ((match = elementPattern.exec(text))) {
		const tagName = match[1];
		const element = match[0];
		const classBody = extractClassBody(element);
		const line = lineNumber(text, match.index);
		const styledDaisyControl =
			/\b(input|input-|select|select-|textarea|textarea-)\b/.test(classBody) ||
			/\b(input|select|textarea)-(bordered|xs|sm|md|lg)\b/.test(classBody);
		const styledTable = tagName === 'table' && /\btable\b/.test(classBody);
		if (!styledDaisyControl && !styledTable) continue;
		pushIssue(
			'rawStyledControl',
			relPath,
			line,
			`Raw styled <${tagName}> should use shared form/table primitives or be explicitly paid down in the baseline.`,
			element
		);
	}
}

function scanImportantCss(text, relPath) {
	const lines = text.split('\n');
	lines.forEach((lineText, index) => {
		if (!lineText.includes('!important')) return;
		pushIssue(
			'cssImportant',
			relPath,
			index + 1,
			'Avoid !important; move the behavior into a token, component variant, or cascade layer.',
			lineText
		);
	});
}

function scanClassAttributes(text, relPath) {
	const classPattern =
		/class(?:=|:)\s*(?:"([^"]*)"|'([^']*)'|`([^`]*)`|\{`([^`]*)`\}|\{"([^"]*)"\}|\{'([^']*)'\})/gs;
	let match;
	while ((match = classPattern.exec(text))) {
		const classBody = match.slice(1).find(Boolean) ?? '';
		const line = lineNumber(text, match.index);

		const importantClasses = [...classBody.matchAll(importantUtilityPattern)].map((item) =>
			item[0].trim()
		);
		if (importantClasses.length) {
			pushIssue(
				'importantUtilityClass',
				relPath,
				line,
				`Avoid Tailwind important utilities (${importantClasses.join(', ')}).`,
				classBody
			);
		}

		const orderClasses = [...classBody.matchAll(orderUtilityPattern)].map((item) => item[0].trim());
		if (orderClasses.length) {
			pushIssue(
				'responsiveOrderUtility',
				relPath,
				line,
				`Responsive order utilities (${orderClasses.join(', ')}) need a documented layout reason.`,
				classBody
			);
		}
	}
}

function parseAvailableThemes(storeText) {
	const match = /export\s+const\s+AVAILABLE_THEMES\s*=\s*\[([\s\S]*?)\]\s+as\s+const/.exec(
		storeText
	);
	if (!match) return [];
	return [...match[1].matchAll(/['"]([^'"]+)['"]/g)].map((item) => item[1]);
}

function parseDaisyThemes(cssText) {
	const match = /themes\s*:\s*([\s\S]*?);/.exec(cssText);
	if (!match) return [];
	return match[1]
		.split(',')
		.map((entry) => entry.trim().split(/\s+/)[0])
		.filter(Boolean);
}

function scanThemeContract() {
	const stylePagePath = path.join(srcRoot, 'routes/system/styles/+page.svelte');
	const themeStorePath = path.join(srcRoot, 'lib/stores/theme.svelte.ts');
	const appCssPath = path.join(srcRoot, 'app.css');

	if (
		!fs.existsSync(stylePagePath) ||
		!fs.existsSync(themeStorePath) ||
		!fs.existsSync(appCssPath)
	) {
		pushIssue(
			'themeContract',
			'src/routes/system/styles/+page.svelte',
			1,
			'Theme settings must be backed by the shared theme store, app CSS, and styles page.',
			'Missing one of the required theme contract files.'
		);
		return;
	}

	const stylePageText = fs.readFileSync(stylePagePath, 'utf8');
	const themeStoreText = fs.readFileSync(themeStorePath, 'utf8');
	const appCssText = fs.readFileSync(appCssPath, 'utf8');
	const pageRelPath = toRepoPath(stylePagePath);

	const requiredPageFragments = [
		['SettingsCard', 'Theme settings page should use SettingsCard.'],
		['hasChanges={themeStore.hasChanges}', 'Theme settings page should expose dirty state.'],
		['onSave={() => themeStore.save()}', 'Theme settings page should save explicitly.'],
		['onReset={() => themeStore.reset()}', 'Theme settings page should reset local theme edits.'],
		[
			'dirtySourceId="theme-settings"',
			'Theme settings page should register a stable dirty source.'
		],
		['AVAILABLE_THEMES', 'Theme settings page should render the shared theme registry.']
	];

	for (const [fragment, message] of requiredPageFragments) {
		if (!stylePageText.includes(fragment)) {
			pushIssue('themeContract', pageRelPath, 1, message, fragment);
		}
	}

	if (/const\s+THEMES\s*=/.test(stylePageText)) {
		pushIssue(
			'themeContract',
			pageRelPath,
			lineNumber(stylePageText, stylePageText.search(/const\s+THEMES\s*=/)),
			'Theme settings page should not define a local theme list.',
			'const THEMES = [...]'
		);
	}

	const storeThemes = parseAvailableThemes(themeStoreText);
	const cssThemes = parseDaisyThemes(appCssText);
	if (!storeThemes.length) {
		pushIssue(
			'themeContract',
			toRepoPath(themeStorePath),
			1,
			'Theme store should export AVAILABLE_THEMES as the canonical registry.',
			'export const AVAILABLE_THEMES = [...] as const'
		);
	}

	const missingInCss = storeThemes.filter((theme) => !cssThemes.includes(theme));
	const missingInStore = cssThemes.filter((theme) => !storeThemes.includes(theme));
	if (missingInCss.length || missingInStore.length) {
		pushIssue(
			'themeContract',
			toRepoPath(appCssPath),
			1,
			'DaisyUI app.css themes must match AVAILABLE_THEMES in the theme store.',
			`missingInCss=${missingInCss.join(',') || '-'} missingInStore=${
				missingInStore.join(',') || '-'
			}`
		);
	}
}

for (const filePath of walk(srcRoot)) {
	const text = fs.readFileSync(filePath, 'utf8');
	const relPath = toRepoPath(filePath);
	scanSettingsCards(text, relPath);
	scanNativeStyledControls(text, relPath);
	scanImportantCss(text, relPath);
	scanClassAttributes(text, relPath);
}

scanThemeContract();

let hasFailure = false;
console.log('MatrixHub UI contract check');
console.log(strict ? 'Mode: strict zero-debt' : 'Mode: enforced zero-debt');

for (const [rule, foundIssues] of Object.entries(issues)) {
	const budget = budgets[rule];
	const count = foundIssues.length;
	const status = count > budget ? 'FAIL' : count < budget ? 'PAYDOWN' : 'OK';
	console.log(`${status.padEnd(7)} ${rule}: ${count}/${budget}`);
	if (count > budget) {
		hasFailure = true;
		const overflow = foundIssues.slice(budget);
		for (const issue of overflow.slice(0, 12)) {
			console.log(`  - ${issue.path}:${issue.line} ${issue.message}`);
			console.log(`    ${issue.snippet}`);
		}
		if (overflow.length > 12) {
			console.log(`  ... ${overflow.length - 12} more new issues`);
		}
	}
}

if (hasFailure) {
	console.error(
		'\nUI contract check failed. Reduce the new issue count or intentionally lower/refresh the baseline.'
	);
	process.exit(1);
}

console.log('\nUI contract check passed.');
