import { describe, it, expect, vi } from 'vitest';
import { useNetworkList } from './useNetworkList.svelte';

const { mockModals } = vi.hoisted(() => ({
	mockModals: {
		open: vi.fn(),
		close: vi.fn()
	}
}));

vi.mock('svelte-modals', () => ({
	modals: mockModals
}));

vi.mock('$lib/components', () => ({
	ConfirmDialog: {},
	InfoDialog: {}
}));

vi.mock('$lib/utils/ui/dialogs', () => ({
	confirm: vi.fn((options: Record<string, unknown>) => mockModals.open({}, options)),
	info: vi.fn((options: Record<string, unknown>) => mockModals.open({}, options))
}));

vi.mock('$lib/utils/ui/modal', () => ({
	openModal: vi.fn((component: unknown, props?: Record<string, unknown>) =>
		mockModals.open(component as never, props as never)
	)
}));

vi.mock('./EditNetwork.svelte', () => ({
	default: {}
}));

vi.mock('./Scan.svelte', () => ({
	default: {}
}));

vi.mock('$lib/i18n.svelte', () => ({
	i18n: {
		languageTag: 'en'
	}
}));

vi.mock('$lib/paraglide/messages.js', () => ({
	wifi_network_limit_title: () => 'Reached Maximum Networks',
	wifi_network_limit_msg: () => 'Too many networks',
	action_confirm: () => 'OK',
	wifi_network_add_title: () => 'Add network',
	wifi_network_edit_title: () => 'Edit network',
	wifi_network_delete_title: () => 'Delete network',
	wifi_network_delete_msg: ({ ssid }: { ssid: string }) => `Delete ${ssid}?`,
	action_cancel: () => 'Cancel',
	action_delete: () => 'Delete'
}));

describe('useNetworkList', () => {
	it('propagates reordered networks when moving up', () => {
		const onNetworksChange = vi.fn();
		const networks = [
			{ ssid: 'A', password: '', static_ip_config: false },
			{ ssid: 'B', password: '', static_ip_config: false },
			{ ssid: 'C', password: '', static_ip_config: false }
		];

		const list = useNetworkList(
			() => networks,
			() => onNetworksChange
		);

		list.moveNetwork(1, 'up');

		expect(onNetworksChange).toHaveBeenCalledWith([
			{ ssid: 'B', password: '', static_ip_config: false },
			{ ssid: 'A', password: '', static_ip_config: false },
			{ ssid: 'C', password: '', static_ip_config: false }
		]);
	});

	it('does not emit changes when move would exceed bounds', () => {
		const onNetworksChange = vi.fn();
		const networks = [
			{ ssid: 'A', password: '', static_ip_config: false },
			{ ssid: 'B', password: '', static_ip_config: false }
		];

		const list = useNetworkList(
			() => networks,
			() => onNetworksChange
		);

		list.moveNetwork(0, 'up');
		list.moveNetwork(1, 'down');

		expect(onNetworksChange).not.toHaveBeenCalled();
	});
});
