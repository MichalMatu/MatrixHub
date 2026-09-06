import type { GpioChannelConfig, GpioConfig, GpioMode, GpioPull } from '$lib/types/domain/gpio';
import { i18n } from '$lib/i18n.svelte';
import * as m from '$lib/paraglide/messages.js';

export const GPIO_DEBOUNCE_MIN_MS = 0;
export const GPIO_DEBOUNCE_MAX_MS = 5000;

export function cloneGpioConfig(config: GpioConfig): GpioConfig {
	return {
		channels: config.channels.map((channel) => ({ ...channel }))
	};
}

function normalizeDebounce(value: number): number {
	if (!Number.isFinite(value)) return 50;
	return Math.min(GPIO_DEBOUNCE_MAX_MS, Math.max(GPIO_DEBOUNCE_MIN_MS, Math.round(value)));
}

export function normalizeChannel(channel: GpioChannelConfig): GpioChannelConfig {
	const normalized = {
		...channel,
		debounce_ms: normalizeDebounce(channel.debounce_ms)
	};

	if (normalized.mode !== 'input') {
		normalized.pull = 'none';
	}

	return normalized;
}

export function normalizeConfig(config: GpioConfig): GpioConfig {
	return {
		channels: config.channels.map(normalizeChannel)
	};
}

export function gpioConfigsEqual(left: GpioConfig | null, right: GpioConfig | null): boolean {
	if (!left || !right) return left === right;
	return JSON.stringify(normalizeConfig(left)) === JSON.stringify(normalizeConfig(right));
}

export function getGpioModeLabel(mode: GpioMode): string {
	switch (mode) {
		case 'disabled':
			return m.gpio_mode_disabled({ locale: i18n.languageTag });
		case 'input':
			return m.gpio_mode_input({ locale: i18n.languageTag });
		case 'output':
			return m.gpio_mode_output({ locale: i18n.languageTag });
	}
}

function getGpioPullLabel(pull: GpioPull): string {
	switch (pull) {
		case 'none':
			return m.gpio_pull_none({ locale: i18n.languageTag });
		case 'up':
			return m.gpio_pull_up({ locale: i18n.languageTag });
		case 'down':
			return m.gpio_pull_down({ locale: i18n.languageTag });
	}
}

export function createGpioModeOptions() {
	return (['disabled', 'input', 'output'] as const).map((mode) => ({
		value: mode,
		label: getGpioModeLabel(mode)
	}));
}

export function createGpioPullOptions(
	options: { allowPullup?: boolean; allowPulldown?: boolean } = {}
) {
	return [
		{ value: 'none', label: getGpioPullLabel('none') },
		{ value: 'up', label: getGpioPullLabel('up'), disabled: options.allowPullup === false },
		{ value: 'down', label: getGpioPullLabel('down'), disabled: options.allowPulldown === false }
	];
}
