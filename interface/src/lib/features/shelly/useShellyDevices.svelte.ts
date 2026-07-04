import type { ShellyDevice } from '$lib/services/api/integrations/ShellyApiService';
import { useSessionAccess, type SessionAccess } from '$lib/features/auth/useSessionAccess.svelte';
import { Logger } from '$lib/services/core/Logger';
import { getRequestAbortKind, toUserRequestErrorMessage } from '$lib/utils';
import { confirm } from '$lib/utils/ui/dialogs';
import Trash from '~icons/tabler/trash';
import Cancel from '~icons/tabler/x';
import { notifications } from '$lib/components/toasts/notifications.svelte';
import { i18n } from '$lib/i18n.svelte';
import * as m from '$lib/paraglide/messages.js';
import {
	MAX_SHELLY_DEVICES,
	createShellyDeviceDraft,
	createShellyDeviceDraftFromDevice,
	createShellyPendingDevice,
	generateShellyDeviceId,
	normalizeShellyGeneration,
	normalizeShellyRelayIndex,
	type ShellyDeviceDraft
} from './shellyModel';
import { sanitizeShellyIpv4Input, validateShellyDraft } from './shellyValidation';
import { useShellySource } from './useShellySource.svelte';

export interface ShellyDevicesState {
	devices: ShellyDevice[];
	loading: boolean;
	adding: boolean;
	editingDeviceId: string | null;
	saving: boolean;
	newDevice: ShellyDeviceDraft;
	error: string | null;
}

type ShellyDevicesDeps = {
	initialDevicesFn?: () => ShellyDevice[];
	access?: Pick<SessionAccess, 'apiOptions' | 'canRead' | 'canManage'>;
};

export function useShellyDevices(deps: ShellyDevicesDeps = {}) {
	const access = deps.access ?? useSessionAccess();
	const source = useShellySource({
		initialDevicesFn: deps.initialDevicesFn,
		access
	});

	function createDefaultDraft() {
		return createShellyDeviceDraft(m.shelly_default_name({ locale: i18n.languageTag }));
	}

	let adding = $state(false);
	let editingDeviceId = $state<string | null>(null);
	let saving = $state(false);
	let newDevice = $state<ShellyDeviceDraft>(createDefaultDraft());
	let localError = $state<string | null>(null);

	const state = $derived.by<ShellyDevicesState>(() => ({
		devices: source.devices,
		loading: source.loading,
		adding,
		editingDeviceId,
		saving,
		newDevice,
		error: localError ?? source.errorMessage
	}));

	function getEditingDevice(): ShellyDevice | undefined {
		if (!editingDeviceId) return undefined;
		return source.devices.find((device) => device.id === editingDeviceId);
	}

	async function saveDevice() {
		if (!access.canManage) return;
		if (saving) return;
		localError = null;

		const isEditing = editingDeviceId !== null;
		if (!isEditing && source.devices.length >= MAX_SHELLY_DEVICES) {
			localError = m.shelly_error_limit(
				{ count: MAX_SHELLY_DEVICES },
				{ locale: i18n.languageTag }
			);
			return;
		}

		const validation = validateShellyDraft(newDevice);
		if (!validation.valid) {
			localError = m.shelly_error_ip_required({ locale: i18n.languageTag });
			newDevice = validation.normalizedDraft;
			return;
		}

		newDevice = validation.normalizedDraft;
		saving = true;
		const currentDevice = getEditingDevice();

		const pendingDevice = createShellyPendingDevice(
			{
				...validation.normalizedDraft,
				id: editingDeviceId ?? generateShellyDeviceId(),
				ip: validation.normalizedDraft.ip,
				relay_index: normalizeShellyRelayIndex(validation.normalizedDraft.relay_index),
				generation: normalizeShellyGeneration(validation.normalizedDraft.generation)
			},
			{ enabled: currentDevice?.enabled ?? true }
		);
		const optimisticDevice = currentDevice ? { ...currentDevice, ...pendingDevice } : pendingDevice;

		try {
			await source.persistDevice(pendingDevice, {
				isEditing,
				optimisticDevice
			});
			adding = false;
			editingDeviceId = null;
			newDevice = createDefaultDraft();
			localError = null;
		} catch (error) {
			Logger.error(`Failed to ${isEditing ? 'update' : 'add'} Shelly device:`, error);
			if (getRequestAbortKind(error) === 'abort') return;
			const message = toUserRequestErrorMessage(error, {
				fallbackMessage: isEditing
					? m.toast_shelly_device_update_failed({ locale: i18n.languageTag })
					: m.toast_shelly_device_add_failed({ locale: i18n.languageTag })
			});
			localError = message;
			notifications.error(m.toast_message({ message }, { locale: i18n.languageTag }), 5000);
		} finally {
			saving = false;
		}
	}

	async function removeDevice(id: string) {
		if (!access.canManage) return;
		confirm({
			title: m.shelly_delete_title({ locale: i18n.languageTag }),
			message: m.shelly_delete_msg({ locale: i18n.languageTag }),
			labels: {
				cancel: { label: m.action_cancel({ locale: i18n.languageTag }), icon: Cancel },
				confirm: { label: m.action_delete({ locale: i18n.languageTag }), icon: Trash }
			},
			onConfirm: async () => {
				try {
					await source.deleteDevice(id);
					localError = null;
				} catch (error) {
					Logger.error('Failed to remove Shelly device:', error);
					if (getRequestAbortKind(error) === 'abort') return;
					const message = toUserRequestErrorMessage(error, {
						fallbackMessage: m.toast_shelly_device_delete_failed({ locale: i18n.languageTag })
					});
					localError = message;
					notifications.error(m.toast_message({ message }, { locale: i18n.languageTag }), 5000);
				}
			}
		});
	}

	async function toggleDevice(id: string, newState: boolean) {
		if (!access.canManage) return;
		await source.toggleDevice(id, newState);
	}

	function openAddModal() {
		if (!access.canManage) return;
		adding = true;
		editingDeviceId = null;
		localError = null;
		newDevice = createDefaultDraft();
	}

	function openEditModal(device: ShellyDevice) {
		if (!access.canManage) return;
		if (saving) return;
		adding = true;
		editingDeviceId = device.id;
		localError = null;
		newDevice = createShellyDeviceDraftFromDevice(device);
	}

	function closeAddModal() {
		if (saving) return;
		adding = false;
		editingDeviceId = null;
		localError = null;
		newDevice = createDefaultDraft();
	}

	function updateNewDeviceName(value: string) {
		newDevice.name = value;
	}

	function updateNewDeviceIp(value: string) {
		newDevice.ip = sanitizeShellyIpv4Input(value);
	}

	function updateNewDeviceRelay(value: number) {
		newDevice.relay_index = normalizeShellyRelayIndex(value, newDevice.relay_index);
	}

	function updateNewDeviceGeneration(value: number) {
		newDevice.generation = normalizeShellyGeneration(value, newDevice.generation);
	}

	return {
		get state() {
			return state;
		},
		get canRead() {
			return access.canRead;
		},
		get canManage() {
			return access.canManage;
		},
		saveDevice,
		removeDevice,
		toggleDevice,
		openAddModal,
		openEditModal,
		closeAddModal,
		updateNewDeviceName,
		updateNewDeviceIp,
		updateNewDeviceRelay,
		updateNewDeviceGeneration
	};
}
