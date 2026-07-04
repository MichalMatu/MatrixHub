import type { KnownNetworkItem } from '$lib/types/connectivity/wifi';
import { confirm, info } from '$lib/utils/ui/dialogs';
import { openModal } from '$lib/utils/ui/modal';
import EditNetwork from './EditNetwork.svelte';
import ScanNetworks from './Scan.svelte';
import { i18n } from '$lib/i18n.svelte';
import * as m from '$lib/paraglide/messages.js';

// Icons
import Check from '~icons/tabler/check';
import Cancel from '~icons/tabler/x';
import Delete from '~icons/tabler/trash';

export function useNetworkList(
	getNetworks: () => KnownNetworkItem[],
	onNetworksChangeFn: () => (networks: KnownNetworkItem[]) => void
) {
	function isNetworkListTooLong(): boolean {
		const networks = getNetworks();
		if (networks.length >= 5) {
			info({
				title: m.wifi_network_limit_title({ locale: i18n.languageTag }),
				message: m.wifi_network_limit_msg({ locale: i18n.languageTag }),
				dismiss: { label: m.action_confirm({ locale: i18n.languageTag }), icon: Check }
			});
			return true;
		}
		return false;
	}

	function scanForNetworks() {
		openModal(ScanNetworks, {
			storeNetwork: (ssid: string) => {
				handleNewNetwork(ssid);
			}
		});
	}

	function handleNewNetwork(ssid?: string) {
		const networks = getNetworks();
		openModal(EditNetwork, {
			title: m.wifi_network_add_title({ locale: i18n.languageTag }),
			networkEditable: {
				ssid: ssid || '',
				password: '',
				static_ip_config: false,
				local_ip: undefined,
				subnet_mask: undefined,
				gateway_ip: undefined,
				dns_ip_1: undefined,
				dns_ip_2: undefined
			},
			onSaveNetwork: async (newNetwork: KnownNetworkItem) => {
				onNetworksChangeFn()([...networks, newNetwork]);
			}
		});
	}

	function handleEdit(index: number) {
		const networks = getNetworks();
		openModal(EditNetwork, {
			title: m.wifi_network_edit_title({ locale: i18n.languageTag }),
			networkEditable: { ...networks[index] }, // Deep copy
			onSaveNetwork: async (editedNetwork: KnownNetworkItem) => {
				const updated = [...networks];
				updated[index] = editedNetwork;
				onNetworksChangeFn()(updated);
			}
		});
	}

	function confirmDelete(index: number) {
		const networks = getNetworks();
		confirm({
			title: m.wifi_network_delete_title({ locale: i18n.languageTag }),
			message: m.wifi_network_delete_msg(
				{ ssid: networks[index].ssid },
				{ locale: i18n.languageTag }
			),
			labels: {
				cancel: { label: m.action_cancel({ locale: i18n.languageTag }), icon: Cancel },
				confirm: { label: m.action_delete({ locale: i18n.languageTag }), icon: Delete }
			},
			onConfirm: () => {
				const updated = networks.filter((_, i) => i !== index);
				onNetworksChangeFn()(updated);
			}
		});
	}

	function moveNetwork(index: number, direction: 'up' | 'down') {
		const networks = getNetworks();
		if (
			(direction === 'up' && index === 0) ||
			(direction === 'down' && index === networks.length - 1)
		)
			return;

		const newNetworks = [...networks];
		const targetIndex = direction === 'up' ? index - 1 : index + 1;

		[newNetworks[index], newNetworks[targetIndex]] = [newNetworks[targetIndex], newNetworks[index]];
		onNetworksChangeFn()(newNetworks);
	}

	return {
		isNetworkListTooLong,
		scanForNetworks,
		handleNewNetwork,
		handleEdit,
		confirmDelete,
		moveNetwork
	};
}
