import type { ShellyDevice } from '$lib/services/api/integrations/ShellyApiService';
import { useShellySource } from '$lib/features/shelly/useShellySource.svelte';

export function useShellyWidgetManagement() {
	const source = useShellySource();

	let toggling = $state<Record<string, boolean>>({});

	const hasDevices = $derived(source.devices.length > 0);
	const canControl = $derived(source.canManage);

	async function toggleDevice(device: ShellyDevice, event: MouseEvent) {
		event.preventDefault();
		event.stopPropagation();
		if (!canControl) return;
		if (toggling[device.id]) return;

		toggling[device.id] = true;
		try {
			await source.toggleDevice(device.id, !device.isOn);
		} finally {
			toggling[device.id] = false;
		}
	}

	return {
		get devices() {
			return source.devices;
		},
		get loading() {
			return source.loading;
		},
		get toggling() {
			return toggling;
		},
		get errorMessage() {
			return source.errorMessage;
		},
		get hasDevices() {
			return hasDevices;
		},
		get canControl() {
			return canControl;
		},
		toggleDevice
	};
}
