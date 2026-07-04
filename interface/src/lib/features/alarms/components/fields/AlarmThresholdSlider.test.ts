import { cleanup, render, screen } from '@testing-library/svelte';
import { describe, expect, it, vi } from 'vitest';
import AlarmThresholdSlider from './AlarmThresholdSlider.svelte';

vi.mock('$lib/i18n.svelte', () => ({
	i18n: {
		languageTag: 'en'
	}
}));

vi.mock('$lib/paraglide/messages.js', () => ({
	alarms_field_threshold: () => 'Threshold Value',
	alarm_boolean_csi_motion_detected: () => 'CSI motion detected',
	alarm_boolean_imu_tamper_detected: () => 'IMU tamper detected',
	alarm_boolean_gpio_true: () => 'GPIO input true'
}));

describe('AlarmThresholdSlider', () => {
	it('uses the CSI boolean label for CSI motion', () => {
		render(AlarmThresholdSlider, {
			props: {
				threshold: 0.5,
				source: 'wifi_csi_motion',
				min: 0,
				max: 1,
				step: 0.5
			}
		});

		expect(screen.getByText('CSI motion detected')).toBeTruthy();
		expect(screen.queryByText('IMU tamper detected')).toBeNull();
	});

	it('uses the IMU boolean label for IMU tamper', () => {
		cleanup();

		render(AlarmThresholdSlider, {
			props: {
				threshold: 0.5,
				source: 'imu_tamper',
				min: 0,
				max: 1,
				step: 0.5
			}
		});

		expect(screen.getByText('IMU tamper detected')).toBeTruthy();
		expect(screen.queryByText('CSI motion detected')).toBeNull();
	});

	it('uses the GPIO boolean label for GPIO digital', () => {
		cleanup();

		render(AlarmThresholdSlider, {
			props: {
				threshold: 0.5,
				source: 'gpio_digital',
				min: 0,
				max: 1,
				step: 0.5
			}
		});

		expect(screen.getByText('GPIO input true')).toBeTruthy();
		expect(screen.queryByText('CSI motion detected')).toBeNull();
	});
});
