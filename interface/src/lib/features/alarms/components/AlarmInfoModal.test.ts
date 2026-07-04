import { render, screen } from '@testing-library/svelte';
import { describe, expect, it, vi } from 'vitest';
import AlarmInfoModal from './AlarmInfoModal.svelte';

vi.mock('$lib/i18n.svelte', () => ({
	i18n: {
		languageTag: 'en'
	}
}));

vi.mock('$lib/paraglide/messages.js', () => ({
	alarms_title: () => 'Alarm Rules',
	alarms_how_work_title: () => 'How Alarms Work',
	alarms_how_work_1: () =>
		'Each rule watches one source and compares the current value against its threshold',
	alarms_how_work_2: () =>
		'Alarms are evaluated whenever new sensor data arrives and the UI is updated in real time',
	alarms_how_work_3: () => 'Cooldown only limits repeat notifications for the same active alarm',
	alarms_how_work_4: ({ max }: { max: number }) => `You can configure up to ${max} alarm rules`,
	alarms_channels_title: () => 'Notification Channels and Actions',
	alarms_channels_1: () => 'Telegram – Configure in Settings → Telegram',
	alarms_channels_2: () => 'LED – Built-in LED indicator flashes',
	alarms_channels_3: () => 'Webhooks – Configure in Settings → Webhook Notifications',
	alarms_channels_4: () => 'Pushover – Configure in Settings → Pushover Notifications',
	alarms_channels_5: () => 'Shelly – Turn ON selected Shelly devices when the alarm triggers',
	menu_telegram: () => 'Telegram Notifications',
	menu_webhook: () => 'Webhook Notifications',
	menu_pushover: () => 'Pushover Notifications',
	menu_shelly: () => 'Shelly Devices',
	alarms_form_cancel: () => 'Cancel',
	action_close: () => 'Close'
}));

describe('AlarmInfoModal', () => {
	it('shows the current limit and available actions', () => {
		render(AlarmInfoModal, {
			props: {
				isOpen: true,
				onClose: vi.fn()
			}
		});

		expect(screen.getByText('You can configure up to 8 alarm rules')).toBeTruthy();
		expect(screen.getByText('Pushover Notifications')).toBeTruthy();
		expect(screen.getByText(/Pushover – Configure in Settings/)).toBeTruthy();
		expect(screen.getByText('Shelly Devices')).toBeTruthy();
		expect(screen.getByText(/Shelly – Turn ON selected Shelly devices/)).toBeTruthy();
	});
});
