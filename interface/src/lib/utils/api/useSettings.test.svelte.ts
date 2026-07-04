import { describe, expect, it, vi } from 'vitest';
import { useSettings } from './useSettings.svelte';

vi.mock('$lib/utils', () => ({
	getRequestAbortKind: vi.fn(() => null),
	toUserRequestErrorMessage: vi.fn((error: unknown, options?: { fallbackMessage?: string }) => {
		if (error instanceof Error && error.message) return error.message;
		return options?.fallbackMessage ?? 'unknown error';
	})
}));

interface TestSettings {
	enabled: boolean;
	token: string;
	icons?: number[][];
}

interface TestErrors {
	token: boolean;
}

const DEFAULT_SETTINGS: TestSettings = {
	enabled: true,
	token: 'abc'
};

const DEFAULT_ERRORS: TestErrors = {
	token: false
};

function createDeferred<T>() {
	let resolve!: (value: T) => void;
	let reject!: (reason?: unknown) => void;
	const promise = new Promise<T>((res, rej) => {
		resolve = res;
		reject = rej;
	});
	return { promise, resolve, reject };
}

describe('useSettings', () => {
	it('skips validation and clears stale errors when shouldValidate returns false', async () => {
		let cleanup: (() => void) | undefined;
		const load = vi.fn().mockResolvedValue(DEFAULT_SETTINGS);
		const save = vi.fn(async (settings: TestSettings) => ({
			enabled: settings.enabled,
			token: settings.token
		}));

		await new Promise<void>((resolve, reject) => {
			cleanup = $effect.root(() => {
				const hook = useSettings<TestSettings, TestErrors>(DEFAULT_SETTINGS, DEFAULT_ERRORS, {
					load,
					save,
					shouldValidate: (settings) => settings.enabled,
					validate: (settings, errors) => {
						if (!settings.token.trim()) {
							errors.token = true;
							return true;
						}

						return false;
					}
				});

				void hook
					.loadSettings()
					.then(async () => {
						hook.setSettings({
							enabled: true,
							token: ''
						});
						await Promise.resolve();

						expect(await hook.saveSettingsNow()).toBe(false);
						expect(hook.errors.token).toBe(true);
						expect(save).not.toHaveBeenCalled();

						hook.setSettings({
							enabled: false,
							token: ''
						});
						await Promise.resolve();

						expect(await hook.saveSettingsNow()).toBe(true);
						expect(hook.errors.token).toBe(false);
						expect(save).toHaveBeenCalledOnce();
						resolve();
					})
					.catch(reject);
			});
		});

		cleanup?.();
	});

	it('keeps the newest load result when an older request resolves later', async () => {
		let cleanup: (() => void) | undefined;
		const firstLoad = createDeferred<TestSettings>();
		const secondLoad = createDeferred<TestSettings>();
		const load = vi
			.fn<() => Promise<TestSettings>>()
			.mockReturnValueOnce(firstLoad.promise)
			.mockReturnValueOnce(secondLoad.promise);
		const save = vi.fn();

		await new Promise<void>((resolve, reject) => {
			cleanup = $effect.root(() => {
				const hook = useSettings<TestSettings, TestErrors>(DEFAULT_SETTINGS, DEFAULT_ERRORS, {
					load,
					save
				});

				void (async () => {
					const olderRequest = hook.loadSettings();
					const newerRequest = hook.loadSettings();

					secondLoad.resolve({
						enabled: false,
						token: 'new-value'
					});
					await newerRequest;

					firstLoad.resolve({
						enabled: true,
						token: 'old-value'
					});
					await olderRequest;

					expect(hook.settings).toEqual({
						enabled: false,
						token: 'new-value'
					});
					expect(hook.savedSettings).toEqual({
						enabled: false,
						token: 'new-value'
					});
					resolve();
				})().catch(reject);
			});
		});

		cleanup?.();
	});

	it('does not overwrite a newer draft when save resolves with an older snapshot', async () => {
		let cleanup: (() => void) | undefined;
		const saveDeferred = createDeferred<TestSettings>();
		const load = vi.fn().mockResolvedValue(DEFAULT_SETTINGS);
		const save = vi.fn(() => saveDeferred.promise);

		await new Promise<void>((resolve, reject) => {
			cleanup = $effect.root(() => {
				const hook = useSettings<TestSettings, TestErrors>(DEFAULT_SETTINGS, DEFAULT_ERRORS, {
					load,
					save
				});

				void hook
					.loadSettings()
					.then(async () => {
						hook.updateSetting('token', 'first-draft');
						const pendingSave = hook.saveSettingsNow();

						hook.updateSetting('token', 'second-draft');

						saveDeferred.resolve({
							enabled: true,
							token: 'first-draft'
						});

						expect(await pendingSave).toBe(true);
						expect(save).toHaveBeenCalledWith({
							enabled: true,
							token: 'first-draft'
						});
						expect(hook.savedSettings).toEqual({
							enabled: true,
							token: 'first-draft'
						});
						expect(hook.settings).toEqual({
							enabled: true,
							token: 'second-draft'
						});
						expect(hook.hasChanges).toBe(true);
						resolve();
					})
					.catch(reject);
			});
		});

		cleanup?.();
	});

	it('blocks a second save while the first one is still in flight', async () => {
		let cleanup: (() => void) | undefined;
		const saveDeferred = createDeferred<TestSettings>();
		const load = vi.fn().mockResolvedValue(DEFAULT_SETTINGS);
		const save = vi.fn(() => saveDeferred.promise);

		await new Promise<void>((resolve, reject) => {
			cleanup = $effect.root(() => {
				const hook = useSettings<TestSettings, TestErrors>(DEFAULT_SETTINGS, DEFAULT_ERRORS, {
					load,
					save
				});

				void hook
					.loadSettings()
					.then(async () => {
						hook.updateSetting('token', 'pending-save');

						const firstSave = hook.saveSettingsNow();
						const secondSave = hook.saveSettingsNow();

						expect(await secondSave).toBe(false);
						expect(save).toHaveBeenCalledTimes(1);

						saveDeferred.resolve({
							enabled: true,
							token: 'pending-save'
						});

						expect(await firstSave).toBe(true);
						resolve();
					})
					.catch(reject);
			});
		});

		cleanup?.();
	});

	it('accepts plain objects that contain nested $state arrays', async () => {
		let cleanup: (() => void) | undefined;
		const load = vi.fn().mockResolvedValue(DEFAULT_SETTINGS);
		const save = vi.fn(async (settings: TestSettings) => settings);

		await new Promise<void>((resolve, reject) => {
			cleanup = $effect.root(() => {
				const hook = useSettings<TestSettings, TestErrors>(DEFAULT_SETTINGS, DEFAULT_ERRORS, {
					load,
					save
				});

				void hook
					.loadSettings()
					.then(async () => {
						const proxiedIcons = $state([
							[1, 2, 3],
							[4, 5, 6]
						]);

						expect(() => {
							hook.setSettings({
								enabled: false,
								token: 'with-icons',
								icons: proxiedIcons
							});
						}).not.toThrow();

						expect(hook.settings).toEqual({
							enabled: false,
							token: 'with-icons',
							icons: [
								[1, 2, 3],
								[4, 5, 6]
							]
						});

						expect(await hook.saveSettingsNow()).toBe(true);
						expect(save).toHaveBeenCalledWith({
							enabled: false,
							token: 'with-icons',
							icons: [
								[1, 2, 3],
								[4, 5, 6]
							]
						});
						resolve();
					})
					.catch(reject);
			});
		});

		cleanup?.();
	});

	it('supports silent saves without firing success feedback', async () => {
		let cleanup: (() => void) | undefined;
		const load = vi.fn().mockResolvedValue(DEFAULT_SETTINGS);
		const save = vi.fn(async (settings: TestSettings) => settings);
		const onSaveSuccess = vi.fn();

		await new Promise<void>((resolve, reject) => {
			cleanup = $effect.root(() => {
				const hook = useSettings<TestSettings, TestErrors>(DEFAULT_SETTINGS, DEFAULT_ERRORS, {
					load,
					save,
					feedback: {
						onSaveSuccess
					}
				});

				void hook
					.loadSettings()
					.then(async () => {
						hook.updateSetting('token', 'silent-save');

						expect(await hook.saveSettingsSilentlyNow()).toBe(true);
						expect(save).toHaveBeenCalledWith({
							enabled: true,
							token: 'silent-save'
						});
						expect(onSaveSuccess).not.toHaveBeenCalled();
						resolve();
					})
					.catch(reject);
			});
		});

		cleanup?.();
	});
});
