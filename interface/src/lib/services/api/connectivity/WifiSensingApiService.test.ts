import { beforeEach, describe, expect, it, vi } from 'vitest';
import { WifiSensingApiService } from './WifiSensingApiService';

const mockClient = {
	get: vi.fn(),
	post: vi.fn()
};

vi.mock('$lib/utils', () => ({
	createApiClient: () => mockClient
}));

describe('WifiSensingApiService', () => {
	let service: WifiSensingApiService;

	beforeEach(() => {
		vi.clearAllMocks();
		service = new WifiSensingApiService({ bearerToken: 'token' });
	});

	it('gets runtime status from the diagnostics endpoint', async () => {
		const status = {
			schema: 'wifisensing.status.v1',
			enabled: true,
			running: true,
			active: true,
			connectedSSID: 'MatrixHub',
			connectedChannel: 6,
			motionDetected: false,
			variance_threshold: 4,
			sample_interval_ms: 1000,
			stats: {
				current: -49,
				filtered: -50,
				min: -52,
				max: -48,
				avg: -50,
				variance: 1.25,
				sampleCount: 14,
				windowMs: 13000
			},
			csi: {
				enabled: true,
				runtime_fault: false,
				runtime_reconcile_pending: false,
				queue_allocated: true,
				queue_metrics_valid: true,
				active_consumer_mask: 1,
				consumer_count: 1,
				frontend_consumer_active: true,
				alarm_consumer_active: false,
				boot_consumer_active: false,
				matrix_visualization_consumer_active: false,
				diagnostic_capture_consumer_active: false,
				queue_depth: 0,
				queue_capacity: 16,
				queue_drops_total: 0,
				queue_drops_last_sec: 0,
				rx_frames_total: 10,
				rx_accepted_total: 9,
				rx_throttled_total: 1,
				queued_packets_total: 9,
				dequeued_packets_total: 9,
				packets_forwarded_total: 9,
				batches_forwarded_total: 3,
				batches_dropped_total: 0,
				packets_per_sec: 9,
				batches_per_sec: 3,
				last_packet_ms: 123,
				last_batch_ms: 124,
				motion_control_epoch: 2,
				calibration_count: 9,
				calibration_target: 100,
				calibration_state: 'collecting',
				motion: {
					enabled: true,
					state: 'monitoring',
					baseline_ready: true,
					detected: false,
					decision_valid: true,
					has_frame: true,
					data_fresh: true,
					last_frame_ms: 123,
					frame_age_ms: 0,
					noisy: false,
					needs_calibration: false,
					score: 0,
					confidence: 0,
					frames_seen: 42,
					width: 64,
					band_count: 1,
					selected_carriers: 8,
					valid_carriers: 8,
					last_reset_reason: 'width_change'
				},
				ws_client_count: 1,
				ws_queue_enabled: true,
				capture: {
					client_count: 0,
					queue_enabled: true,
					starting: false,
					accepting: false,
					stopping: false,
					session_id: 0,
					start_exclusive_sequence: 0,
					stop_inclusive_sequence: 0,
					records_offered: 0,
					records_enqueued: 0,
					records_dropped: 0,
					truncated_records: 0
				}
			}
		};
		mockClient.get.mockResolvedValue(status);

		const result = await service.getStatus();

		expect(mockClient.get).toHaveBeenCalledWith(
			'/api/wifisensing/status',
			expect.objectContaining({ signal: expect.any(AbortSignal) })
		);
		expect(result).toBe(status);
	});

	it('gets and saves settings through the existing config endpoint', async () => {
		const settings = {
			enabled: true,
			sample_interval_ms: 1000,
			variance_threshold: 4,
			csi_alarm: {
				enabled: true,
				bands: [{ start: 10, end: 17 }],
				baseline_frames: 150,
				top_k: 8,
				enter_threshold: 6,
				clear_threshold: 3,
				hold_ms: 1200,
				clear_hold_ms: 2500,
				min_noise: 4,
				min_energy: 4,
				noisy_threshold: 80,
				auto_recalibration: true,
				sensitivity: 1 as const
			}
		};
		mockClient.get.mockResolvedValue(settings);
		mockClient.post.mockResolvedValue(settings);

		await service.getSettings();
		await service.saveSettings({ enabled: false });

		expect(mockClient.get).toHaveBeenCalledWith(
			'/api/wifisensing/config',
			expect.objectContaining({ signal: expect.any(AbortSignal) })
		);
		expect(mockClient.post).toHaveBeenCalledWith(
			'/api/wifisensing/config',
			{ enabled: false },
			expect.objectContaining({ signal: expect.any(AbortSignal) })
		);
	});

	it('requests CSI alarm calibration through the guarded endpoint', async () => {
		mockClient.post.mockResolvedValue({ ok: true, state: 'calibrating' });

		await expect(service.calibrateCsiAlarm()).resolves.toEqual({
			ok: true,
			state: 'calibrating'
		});
		expect(mockClient.post).toHaveBeenCalledWith(
			'/api/wifisensing/csi/calibrate',
			{},
			expect.objectContaining({ signal: expect.any(AbortSignal) })
		);
	});
});
