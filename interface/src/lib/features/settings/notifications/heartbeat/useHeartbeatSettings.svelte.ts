/**
 * @file useHeartbeatSettings.svelte.ts
 * @brief Composable hook for multi-slot heartbeat pinger settings
 */

import {
	HeartbeatApiService,
	type HeartbeatSettings,
	type HeartbeatSlot
} from '$lib/services/api/integrations/HeartbeatApiService';
import { notifications } from '$lib/components/toasts/notifications.svelte';
import { createSettingsFeedback } from '$lib/utils/api/settingsFeedback';
import { useSettings } from '$lib/utils/api/useSettings.svelte';
import { i18n } from '$lib/i18n.svelte';
import * as m from '$lib/paraglide/messages.js';
import { useApiClient } from '$lib/utils/api/useApiClient.svelte';

// Constants
const MAX_SLOTS = 4;
const DEFAULT_HEARTBEAT_INTERVAL_MS = 300000;
const NOTIFICATION_DURATION_MS = 3000;

function createDefaultSlots(): HeartbeatSlot[] {
	return Array.from({ length: MAX_SLOTS }, () => ({
		enabled: false,
		name: '',
		url: '',
		allow_insecure: false
	}));
}

const DEFAULT_SETTINGS: HeartbeatSettings = {
	interval_ms: DEFAULT_HEARTBEAT_INTERVAL_MS,
	slots: createDefaultSlots()
};

// Heartbeat doesn't use a separate errors object for slots in the same way (it shows toast on save)
// But we can use the generic error handler or just keep the toast logic in validate.
const DEFAULT_ERRORS = {};

type HeartbeatSettingsDeps = {
	api?: Pick<HeartbeatApiService, 'getSettings' | 'updateSettings'>;
	shouldLoad?: () => boolean;
};

export function useHeartbeatSettings(deps: HeartbeatSettingsDeps = {}) {
	const apiClient = deps.api ? null : useApiClient();
	const feedback = createSettingsFeedback<HeartbeatSettings>();

	function createApi() {
		return deps.api ?? apiClient!.createService(HeartbeatApiService);
	}

	function normalizeSlot(slot: HeartbeatSlot): HeartbeatSlot {
		const url = normalizeUrl(slot.url);
		return {
			enabled: slot.enabled,
			name: filterAsciiPrintable(slot.name).slice(0, 23),
			url,
			allow_insecure: url.startsWith('https://') ? Boolean(slot.allow_insecure) : false
		};
	}

	const hook = useSettings<HeartbeatSettings, typeof DEFAULT_ERRORS>(
		DEFAULT_SETTINGS,
		DEFAULT_ERRORS,
		{
			load: async () => {
				const data = await createApi().getSettings();
				const slots = createDefaultSlots();
				data.slots.forEach((slot, i) => {
					if (i < MAX_SLOTS) slots[i] = normalizeSlot(slot);
				});
				return { interval_ms: data.interval_ms, slots };
			},
			save: async (settings) => {
				const normalizedSettings: HeartbeatSettings = {
					interval_ms: settings.interval_ms,
					slots: settings.slots.map((slot) => normalizeSlot(slot))
				};
				const updated = await createApi().updateSettings(normalizedSettings);
				const slots = createDefaultSlots();
				updated.slots.forEach((slot, i) => {
					if (i < MAX_SLOTS) slots[i] = normalizeSlot(slot);
				});
				return { interval_ms: updated.interval_ms, slots };
			},
			validate: (settings) => {
				for (const slot of settings.slots) {
					if (slot.enabled && !slot.url) {
						notifications.warning(
							m.toast_heartbeat_url_required({ locale: i18n.languageTag }),
							NOTIFICATION_DURATION_MS
						);
						return true; // Error found
					}
				}
				return false;
			},
			feedback
			// No restart config -> direct save (implied by original code not using restart modal)
			// Checking original code again:
			// notifications.success('Heartbeat settings saved', NOTIFICATION_DURATION_MS);
			// It does NOT use confirmRestartAndSave.
		}
	);

	let autoLoadArmed = true;

	$effect(() => {
		const shouldLoad = deps.shouldLoad?.();
		if (shouldLoad === undefined) return;
		if (!shouldLoad) {
			autoLoadArmed = true;
			return;
		}
		if (!autoLoadArmed) return;
		autoLoadArmed = false;
		void hook.loadSettings();
	});

	let enabledCount = $derived.by(() => {
		const savedSettings = hook.savedSettings;
		return savedSettings?.slots.filter((slot) => slot.enabled && slot.url).length ?? 0;
	});
	let canTest = $derived(enabledCount > 0);

	function filterAsciiPrintable(value: string): string {
		// Keep only printable ASCII (0x20..0x7E)
		return value.replace(/[^\x20-\x7E]/g, '');
	}

	/**
	 * Normalize URL for persistence: trim + printable ASCII only.
	 */
	function normalizeUrl(value: string): string {
		return filterAsciiPrintable(value.trim());
	}

	function setSlotUrl(index: number, value: string) {
		if (index >= 0 && index < MAX_SLOTS) {
			const normalizedUrl = normalizeUrl(value);
			hook.settings.slots[index].url = normalizedUrl;
			if (!normalizedUrl.startsWith('https://')) {
				hook.settings.slots[index].allow_insecure = false;
			}
		}
	}

	function setSlotUrlRaw(index: number, value: string) {
		if (index >= 0 && index < MAX_SLOTS) {
			const asciiUrl = filterAsciiPrintable(value);
			hook.settings.slots[index].url = asciiUrl;
			if (!asciiUrl.trim().startsWith('https://')) {
				hook.settings.slots[index].allow_insecure = false;
			}
		}
	}

	function setSlotName(index: number, value: string) {
		if (index >= 0 && index < MAX_SLOTS) {
			hook.settings.slots[index].name = filterAsciiPrintable(value).slice(0, 23);
		}
	}

	function toggleSlot(index: number) {
		if (index >= 0 && index < MAX_SLOTS) {
			hook.settings.slots[index].enabled = !hook.settings.slots[index].enabled;
		}
	}

	function setSlotAllowInsecure(index: number, value: boolean) {
		if (index >= 0 && index < MAX_SLOTS) {
			hook.settings.slots[index].allow_insecure = hook.settings.slots[index].url
				.trim()
				.startsWith('https://')
				? value
				: false;
		}
	}

	// Original hook didn't expose updateSetting, so we don't strictly need to expose hook.updateSetting
	// But we can for consistency if needed.

	return {
		get settings() {
			return hook.settings;
		},
		get savedSettings() {
			return hook.savedSettings;
		},
		get loading() {
			return hook.loading;
		},
		set loading(v) {
			hook.loading = v;
		},
		get saving() {
			return hook.saving;
		},
		get hasChanges() {
			return hook.hasChanges;
		},
		get enabledCount() {
			return enabledCount;
		},
		get canTest() {
			return canTest;
		},
		loadSettings: hook.loadSettings,
		saveSettings: hook.saveSettings,
		resetSettings: hook.resetSettings,
		setSlotUrl,
		setSlotUrlRaw,
		setSlotName,
		toggleSlot,
		setSlotAllowInsecure
	};
}
