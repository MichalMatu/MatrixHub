import { render, screen } from '@testing-library/svelte';
import { beforeEach, describe, expect, it, vi } from 'vitest';
import Statusbar from './Statusbar.svelte';

const statusbarMock = vi.hoisted(() => {
	const defaultStatus = {
		coreTemp: 44,
		isConnected: true,
		isApMode: false,
		rssi: -60
	};

	const state = {
		status: { ...defaultStatus },
		currentDate: '18.03.2026',
		currentClock: '09:06',
		currentTitle: 'Charts',
		cpuTempStyle: 'color: currentColor',
		connectionStatus: 'connected',
		connectionClass: 'text-success',
		connectionTooltip: 'API and live status connected',
		confirmSleep: vi.fn()
	};

	return {
		state,
		defaultStatus,
		reset() {
			state.status = { ...defaultStatus };
			state.currentDate = '18.03.2026';
			state.currentClock = '09:06';
			state.currentTitle = 'Charts';
			state.cpuTempStyle = 'color: currentColor';
			state.connectionStatus = 'connected';
			state.connectionClass = 'text-success';
			state.connectionTooltip = 'API and live status connected';
			state.confirmSleep = vi.fn();
		}
	};
});

vi.mock('./useStatusbarManagement.svelte', () => ({
	useStatusbarManagement: () => statusbarMock.state
}));

vi.mock('$lib/paraglide/messages.js', () => ({
	action_open_menu: () => 'Open menu',
	statusbar_ap_mode: () => 'AP Mode',
	status_wifi_disconnected: () => 'Disconnected',
	power_btn_sleep: () => 'Sleep'
}));

function expectDaisyTooltipOnly(element: HTMLElement, tip: string) {
	expect(element.getAttribute('data-tip')).toBe(tip);
	expect(element.hasAttribute('title')).toBe(false);
}

describe('Statusbar', () => {
	beforeEach(() => {
		statusbarMock.reset();
	});

	it('uses only the DaisyUI tooltip for the live connection indicator', () => {
		render(Statusbar);

		const indicator = screen.getByRole('img', { name: 'API and live status connected' });

		expectDaisyTooltipOnly(indicator, 'API and live status connected');
	});

	it('labels the AP mode tooltip without a native title', () => {
		statusbarMock.state.status = {
			...statusbarMock.defaultStatus,
			isConnected: false,
			isApMode: true,
			rssi: -95
		};

		render(Statusbar);

		expectDaisyTooltipOnly(screen.getByRole('img', { name: 'AP Mode' }), 'AP Mode');
	});

	it('labels the disconnected Wi-Fi tooltip without a native title', () => {
		statusbarMock.state.status = {
			...statusbarMock.defaultStatus,
			isConnected: false,
			isApMode: false,
			rssi: -95
		};

		render(Statusbar);

		expectDaisyTooltipOnly(screen.getByRole('img', { name: 'Disconnected' }), 'Disconnected');
	});
});
