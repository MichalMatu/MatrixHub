import { describe, it, expect, vi, beforeEach } from 'vitest';
import { render, screen, fireEvent, waitFor } from '@testing-library/svelte';
import AlarmsWidget from '$lib/features/dashboard/widgets/alarms/AlarmsWidget.svelte';
import { systemEvents } from '$lib/stores/systemStatus.svelte';

// Mocks
const { mockApi } = vi.hoisted(() => ({
	mockApi: {
		getRules: vi.fn(),
		saveRules: vi.fn()
	}
}));

const { mockUser, mockPage } = vi.hoisted(() => ({
	mockUser: {
		bearer_token: 'test-token',
		admin: true,
		isValid: true
	},
	mockPage: {
		data: {
			features: {
				security: true
			}
		}
	}
}));

vi.mock('$lib/services/api/monitoring/AlarmsApiService', async (importOriginal) => {
	const actual = await importOriginal();
	return {
		// @ts-expect-error - partial mock
		...actual,
		AlarmsApiService: class {
			constructor() {
				return mockApi;
			}
		}
	};
});

vi.mock('$lib/services/core/Logger', () => ({
	Logger: {
		error: vi.fn()
	}
}));

vi.mock('$lib/stores/user', () => ({
	user: mockUser
}));

vi.mock('$app/state', () => ({
	page: mockPage
}));

// Mock usePolling to execute immediately and not loop in tests
vi.mock('$lib/utils/api/usePolling.svelte', () => ({
	usePolling: (fn: () => Promise<void>) => {
		// Execute once on mount
		fn();
		return { isFetching: false };
	}
}));

// Mock i18n
vi.mock('$lib/i18n.svelte', () => ({
	i18n: {
		languageTag: 'en'
	}
}));

vi.mock('$lib/paraglide/messages.js', () => ({
	widget_alarms_title: () => 'Active Alarms',
	widget_alarms_no_alarms: () => 'No alarms configured',
	widget_alarms_label_alarm: () => 'ALARM',
	source_temperature: () => 'Temperature',
	source_co2: () => 'CO2',
	source_humidity: () => 'Humidity',
	source_wifi_motion: () => 'WiFi Motion',
	source_wifi_csi_motion: () => 'CSI Motion',
	source_imu_tamper: () => 'IMU Tamper',
	source_ble_temperature: () => 'BLE Temperature',
	source_ble_humidity: () => 'BLE Humidity',
	source_ble_battery: () => 'BLE Battery',
	source_ble_rssi: () => 'BLE RSSI',
	alarm_boolean_csi_motion_detected: () => 'CSI motion detected',
	alarm_boolean_imu_tamper_detected: () => 'IMU tamper detected',
	error_prefix: ({ error }: { error: string }) => `Error: ${error}`,
	alarms_error_fetch: () => 'Failed to fetch',
	alarms_error_load_timeout: () => 'Load timeout',
	alarms_error_load_fallback: () => 'Load failed',
	alarms_error_save_timeout: () => 'Save timeout',
	alarms_error_save_fallback: () => 'Save failed',
	alarms_error_update: () => 'Failed to update',
	toast_request_timeout: () => 'request timeout',
	restart_error_cancelled: () => 'request cancelled',
	request_error_network: () => 'network error',
	request_error_failed: () => 'request failed',
	toast_message: ({ message }: { message: string }) => message,
	common_loading: () => 'Loading...'
}));

describe('AlarmsWidget', () => {
	beforeEach(() => {
		vi.clearAllMocks();
		vi.useRealTimers();
		mockUser.admin = true;
		systemEvents.set(null);
	});

	it('should render loading state initially', () => {
		mockApi.getRules.mockReturnValue(new Promise(() => {})); // Never resolves
		render(AlarmsWidget);
		// BaseWidget handles loading, we assume it shows a skeleton or similar.
		// Since we can't easily check internal state of BaseWidget without implementation details,
		// we mainly check that it doesn't crash.
		const title = screen.getByText('Active Alarms');
		expect(title).toBeTruthy();
	});

	it('should render empty state when no rules', async () => {
		render(AlarmsWidget);
		systemEvents.set({
			type: 'snapshot',
			channel: 'alarms',
			data: { schema_version: 1, rules: [] }
		});

		await waitFor(() => {
			expect(screen.getByText('No alarms configured')).toBeTruthy();
		});
	});

	it('should render alarms list', async () => {
		const mockRules = [
			{
				id: '1',
				name: 'High Temp',
				enabled: true,
				triggered: false,
				source: 'temperature',
				threshold: 30,
				operator: 'above',
				current_value: 25
			},
			{
				id: '2',
				name: 'Critical CO2',
				enabled: true,
				triggered: true,
				source: 'co2',
				threshold: 1000,
				operator: 'above',
				current_value: 1200
			}
		];
		render(AlarmsWidget);
		systemEvents.set({
			type: 'snapshot',
			channel: 'alarms',
			data: { schema_version: 1, rules: mockRules }
		});

		await waitFor(() => {
			expect(screen.getByText('High Temp')).toBeTruthy();
			expect(screen.getByText('Critical CO2')).toBeTruthy();
		});

		// Check values display
		expect(screen.getByText('25.0')).toBeTruthy();
		expect(screen.getByText('1200')).toBeTruthy(); // CO2 has 0 decimals
	});

	it('renders IMU tamper as a boolean condition in the widget', async () => {
		render(AlarmsWidget);
		systemEvents.set({
			type: 'snapshot',
			channel: 'alarms',
			data: {
				schema_version: 1,
				rules: [
					{
						id: 'imu',
						name: 'IMU Alarm',
						enabled: true,
						triggered: false,
						source: 'imu_tamper',
						threshold: 0.5,
						operator: 'above',
						current_value: 0
					}
				]
			}
		});

		await waitFor(() => {
			expect(screen.getByText('IMU Alarm')).toBeTruthy();
			expect(screen.getByText('IMU tamper detected')).toBeTruthy();
		});

		expect(screen.queryByText('CSI motion detected')).toBeNull();
		expect(screen.queryByText('0.5')).toBeNull();
	});

	it('should not render alarm badge for triggered rules', async () => {
		const mockRules = [
			{
				id: '1',
				name: 'Fire',
				enabled: true,
				triggered: true,
				source: 'temperature',
				threshold: 50,
				operator: 'above',
				current_value: 60
			}
		];
		render(AlarmsWidget);
		systemEvents.set({
			type: 'snapshot',
			channel: 'alarms',
			data: { schema_version: 1, rules: mockRules }
		});

		await waitFor(() => {
			expect(screen.getByText('Fire')).toBeTruthy();
		});

		expect(screen.queryByTitle('ALARM')).toBeNull();
	});

	it('should update the matching rule from alarm events by id', async () => {
		const mockRules = [
			{
				id: 'rule-a',
				name: 'High Temp',
				enabled: true,
				triggered: false,
				source: 'temperature',
				threshold: 30,
				operator: 'above',
				current_value: 25
			},
			{
				id: 'rule-b',
				name: 'CO2 Watch',
				enabled: true,
				triggered: false,
				source: 'co2',
				threshold: 900,
				operator: 'above',
				current_value: 800
			}
		];

		render(AlarmsWidget);
		systemEvents.set({
			type: 'snapshot',
			channel: 'alarms',
			data: { schema_version: 1, rules: mockRules }
		});

		await waitFor(() => {
			expect(screen.getByText('High Temp')).toBeTruthy();
			expect(screen.getByText('CO2 Watch')).toBeTruthy();
		});

		systemEvents.set({
			type: 'alarm',
			data: {
				id: 'rule-b',
				triggered: true,
				current_value: 1234,
				severity: 2
			}
		});

		await waitFor(() => {
			expect(screen.getByText('1234')).toBeTruthy();
		});

		expect(screen.queryByText('800')).toBeNull();
		expect(screen.getByText('25.0')).toBeTruthy();
	});

	it('should handle toggle interaction', async () => {
		const mockRules = [
			{
				id: '1',
				name: 'Test Alarm',
				enabled: false, // Initially disabled
				triggered: false,
				source: 'temperature',
				threshold: 30,
				operator: 'above'
			}
		];
		mockApi.saveRules.mockResolvedValue({});

		render(AlarmsWidget);
		systemEvents.set({
			type: 'snapshot',
			channel: 'alarms',
			data: { schema_version: 1, rules: mockRules }
		});

		// Wait for load
		await waitFor(() => {
			expect(screen.getByText('Test Alarm')).toBeTruthy();
		});

		const toggle = screen.getByRole('checkbox') as HTMLInputElement;
		expect(toggle.checked).toBe(false);

		// Click toggle
		await fireEvent.click(toggle);

		// API should be called
		expect(mockApi.saveRules).toHaveBeenCalledWith(
			expect.objectContaining({
				rules: expect.arrayContaining([
					expect.objectContaining({
						id: '1',
						enabled: true,
						updated_at: expect.any(Number)
					})
				])
			})
		);
		expect(mockApi.saveRules.mock.calls[0][0].rules[0]).not.toHaveProperty('updatedAt');

		// Optimistic update check
		expect(toggle.checked).toBe(true);
	});

	it('stays websocket-first when the initial snapshot is delayed', async () => {
		vi.useFakeTimers();

		render(AlarmsWidget);

		await vi.advanceTimersByTimeAsync(5000);
		expect(mockApi.getRules).not.toHaveBeenCalled();
	});

	it('should revert toggle if API fails', async () => {
		const consoleErrorSpy = vi.spyOn(console, 'error').mockImplementation(() => {});
		const mockRules = [
			{
				id: '1',
				name: 'Test Alarm',
				enabled: false,
				triggered: false,
				source: 'temperature',
				threshold: 30,
				operator: 'above'
			}
		];
		mockApi.getRules.mockResolvedValue({
			schema_version: 1,
			rules: [
				{
					...mockRules[0],
					current_value: 44
				}
			]
		});
		mockApi.saveRules.mockImplementation(async () => {
			await new Promise((resolve) => setTimeout(resolve, 100));
			throw new Error('Update failed');
		});

		render(AlarmsWidget);
		systemEvents.set({
			type: 'snapshot',
			channel: 'alarms',
			data: { schema_version: 1, rules: mockRules }
		});

		await waitFor(() => {
			expect(screen.getByText('Test Alarm')).toBeTruthy();
		});

		const toggle = screen.getByRole('checkbox') as HTMLInputElement;

		// Click toggle
		await fireEvent.click(toggle);

		// Expect optimistic True
		await waitFor(() => {
			expect(toggle.checked).toBe(true);
		});

		// Wait for async rejection handling
		await waitFor(() => {
			// Should revert to False
			expect(toggle.checked).toBe(false);
		});

		expect(mockApi.getRules).toHaveBeenCalledWith({ includeStatus: true });
		expect(screen.getByText('44.0')).toBeTruthy();
		consoleErrorSpy.mockRestore();
	});

	it('should disable alarm toggle for non-admin users', async () => {
		mockUser.admin = false;
		const mockRules = [
			{
				id: 'user-1',
				name: 'User Alarm',
				enabled: true,
				triggered: false,
				source: 'temperature',
				threshold: 30,
				operator: 'above'
			}
		];

		render(AlarmsWidget);
		systemEvents.set({
			type: 'snapshot',
			channel: 'alarms',
			data: { schema_version: 1, rules: mockRules }
		});

		await waitFor(() => {
			expect(screen.getByText('User Alarm')).toBeTruthy();
		});

		const toggle = screen.getByRole('checkbox') as HTMLInputElement;
		expect(toggle.disabled).toBe(true);

		await fireEvent.click(toggle);

		expect(mockApi.saveRules).not.toHaveBeenCalled();
	});
});
