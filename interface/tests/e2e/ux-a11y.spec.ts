import { test, expect, type Page, type Route, type WebSocketRoute } from '@playwright/test';

type JsonObject = Record<string, unknown>;

const matrixSettings = {
	brightness: 20,
	alarm_mode: 1,
	rotation: 0,
	auto_rotate: false,
	effect_enabled: true,
	effect_engine: 0,
	effect_mode: 2,
	effect_speed: 1000,
	effect_color: 0x00ff00,
	effect_color_2: 0xff0000,
	effect_color_3: 0x0000ff,
	effect_reactivity_provider: 0,
	effect_reactivity_gain: 80,
	menu_enabled: true,
	menu_text_color: 0xffffff,
	menu_scroll_speed: 20,
	custom_icons: [[], [], []]
};

const notificationSettings = {
	telegram_enabled: false,
	webhook_enabled: false,
	bot_token: '',
	chat_id: '',
	commands_enabled: false,
	webhook_url: '',
	pushover_enabled: false,
	pushover_user: '',
	pushover_token: '',
	is_configured: false
};

const systemInfo = {
	esp_platform: 'ESP32-S3',
	firmware_version: '2.0.0',
	firmware_name: 'MatrixHub',
	firmware_built_target: 'waveshare_esp32s3_matrix',
	firmware_commit: '0123456789abcdef0123456789abcdef01234567',
	firmware_dirty: false,
	cpu_freq_mhz: 240,
	cpu_type: 'ESP32-S3',
	cpu_rev: 1,
	cpu_cores: 2,
	sketch_size: 2359296,
	free_sketch_space: 1048576,
	sdk_version: '5.5.1',
	arduino_version: '3.3.4',
	flash_chip_size: 16777216,
	flash_chip_speed: 80000000,
	cpu_reset_reason: 1,
	max_alloc_heap: 128000,
	psram_size: 8388608,
	free_psram: 6291456,
	used_psram: 2097152,
	free_heap: 210000,
	used_heap: 120000,
	total_heap: 330000,
	min_free_heap: 180000,
	core_temp: 46.5,
	fs_total: 1048576,
	fs_used: 262144,
	lp_sram_used: 0,
	lp_sram_free: 0,
	lp_sram_total: 0,
	uptime: 123456,
	mac_address: 'AA:BB:CC:DD:EE:FF',
	compile_date: 'Jun 19 2026',
	compile_time: '12:00:00',
	storage: {
		filesystem: { total_bytes: 1048576, used_bytes: 262144, free_bytes: 786432 },
		active_backend: 'littlefs',
		active_path: '/',
		active: {
			backend: 'littlefs',
			available: true,
			mounted: true,
			total_bytes: 1048576,
			used_bytes: 262144,
			free_bytes: 786432
		},
		littlefs: {
			backend: 'littlefs',
			available: true,
			mounted: true,
			total_bytes: 1048576,
			used_bytes: 262144,
			free_bytes: 786432
		},
		sdcard: {
			backend: 'sdcard',
			available: false,
			mounted: false,
			total_bytes: 0,
			used_bytes: 0,
			free_bytes: 0
		}
	}
};

const healthDiagnostics = {
	healthy: true,
	issues: [],
	heap: {
		free: 210000,
		min: 180000,
		largest: 128000,
		fragmentation: 8
	},
	wifi: {
		connected: true,
		rssi: -55,
		reconnects: 0,
		ssid: 'MatrixHub',
		ip: '192.168.0.30',
		lastDisconnectReason: 0,
		healthy: true,
		state: 'connected',
		configuredMode: 'sta',
		mode: 'sta',
		apActive: false,
		lastRecoveryReason: '',
		lastIpChangeMs: 1000,
		disconnectedSinceMs: 0,
		stableConnectedSinceMs: 1000,
		savedStaticIp: '',
		mac: 'AA:BB:CC:DD:EE:FF',
		channel: 6,
		bssid: '11:22:33:44:55:66',
		gateway: '192.168.0.1',
		subnet: '255.255.255.0',
		dns: '1.1.1.1'
	},
	runtime: {
		uptimeMs: 123456,
		uptimeHours: 0.03,
		loopCount: 4200,
		slowLoops: 0,
		maintenanceSleeps: 0,
		maintenanceSleepActive: false
	},
	ap: {
		active: false,
		stationNum: 0,
		ip: '192.168.4.1',
		mac: 'AA:BB:CC:DD:EE:00'
	},
	http: {
		activeClients: 1,
		peakClients: 2,
		opens: 4,
		closes: 3,
		wsActiveClients: 1,
		wsPeakClients: 1,
		wsOpens: 1,
		wsCloses: 0,
		wsForcedRemovals: 0,
		wsQueueDrops: 0,
		wsHeapFallbacks: 0
	},
	forwarding: {
		ready: true,
		requiresStaticIp: false,
		savedStaticIpConfigured: false,
		savedStaticIpMatches: true,
		httpsPort: 443
	}
};

const powerStatus = {
	sleep_enabled: false,
	inactivity_timeout_ms: 600000,
	grace_after_boot_ms: 30000,
	wake_reason: 'poweron',
	wake_cause_raw: 0,
	wake_gpio_mask: '0x0',
	wake_ext1_mask: '0x0',
	sleep_requested: false,
	sleep_eta_ms: 0,
	wake_interval_ms: 0,
	last_activity_ms: 1000,
	thermal_state: 'normal',
	thermal_temp_c: 46.5,
	thermal_cpu_mhz: 240,
	thermal_throttled: false,
	thermal_matrix_limit: 255,
	thermal_soft_c: 70,
	thermal_hard_c: 80,
	thermal_critical_c: 90,
	uptime_ms: 120000
};

const bleSettings = {
	enabled: true,
	sensors: [{ mac: 'AA:BB:CC:DD:EE:01', alias: 'Desk sensor' }]
};

const bleStatus = {
	enabled: true,
	running: true,
	scanner_active: false,
	metrics: {
		adv_total: 12,
		valid_packets: 10,
		parser_errors: 0,
		cache_drops: 0,
		mutex_timeouts: 0,
		scanner_running: true
	},
	devices: [
		{
			mac: 'AA:BB:CC:DD:EE:01',
			temp: 23.1,
			humid: 45.2,
			batt: 91,
			rssi: -61,
			last_seen: 0,
			saved: true
		}
	]
};

const wifiSensingSettings = {
	enabled: true,
	sample_interval_ms: 250,
	variance_threshold: 4
};

const wifiSensingStatus = {
	schema: 'wifisensing.status.v1',
	enabled: true,
	running: true,
	active: true,
	connectedSSID: 'MatrixHub',
	connectedChannel: 6,
	stats: {
		current: -55,
		filtered: -54.5,
		min: -70,
		max: -45,
		avg: -56,
		variance: 1.2,
		sampleCount: 128,
		windowMs: 30000
	},
	motionDetected: false,
	variance_threshold: 4,
	sample_interval_ms: 250,
	samples: [
		{ rssi: -55, timestamp: 1, variance: 1.1 },
		{ rssi: -54, timestamp: 2, variance: 1.2 }
	],
	csi: {
		enabled: true,
		queue_allocated: true,
		active_consumer_mask: 1,
		consumer_count: 1,
		frontend_consumer_active: true,
		alarm_consumer_active: false,
		boot_consumer_active: false,
		queue_depth: 0,
		queue_capacity: 64,
		queue_drops_total: 0,
		queue_drops_last_sec: 0,
		rx_frames_total: 100,
		rx_accepted_total: 100,
		rx_throttled_total: 0,
		queued_packets_total: 100,
		dequeued_packets_total: 100,
		packets_forwarded_total: 100,
		batches_forwarded_total: 10,
		batches_dropped_total: 0,
		packets_per_sec: 12,
		batches_per_sec: 2,
		last_packet_ms: 1000,
		last_batch_ms: 1000,
		calibration_count: 1,
		calibration_target: 48,
		calibration_state: 'stable',
		motion: {
			enabled: true,
			state: 'monitoring',
			baseline_ready: true,
			detected: false,
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
		ws_queue_enabled: true
	}
};

const airMouseStatus = {
	movement_enabled: false,
	click_enabled: false,
	running: false,
	calibrating: false,
	sensitivity_x: 1,
	sensitivity_y: 1,
	deadzone: 0.05,
	acceleration_enabled: false,
	acceleration_factor: 1,
	tap_threshold_g: 1.8,
	click_debounce_ms: 120,
	double_click_window_ms: 350,
	click_source: 0,
	single_click_action: 0,
	double_click_action: 0,
	triple_click_action: 0,
	single_click_script: '',
	double_click_script: '',
	triple_click_script: '',
	euro_min_cutoff: 1,
	euro_beta: 0.01,
	euro_d_cutoff: 1,
	gyro_offset_x: 0,
	gyro_offset_y: 0,
	gyro_offset_z: 0,
	last_delta_g: 0,
	jiggler: {
		mode: 0,
		interval: 60,
		move_distance: 16,
		random_interval: false
	},
	imu: {
		gx: 0,
		gy: 0,
		gz: 0,
		ax: 0,
		ay: 0,
		az: 1
	}
};

const macroStatus = {
	current_script: '',
	status: 'IDLE',
	current_line: 0,
	uptime_ms: 0,
	last_error: ''
};

const macroSettings = {
	enabled: false,
	boot_script: '',
	boot_delay: 0
};

const websocketSnapshots: Record<string, unknown> = {
	system_status: {
		system_info: systemInfo,
		diagnostics: healthDiagnostics,
		dashboard_widgets: {
			ble: { enabled: true, sensor_count: 1 },
			shelly: { device_count: 0 },
			alarms: { rule_count: 0 },
			wifi_sensing: { enabled: true }
		},
		wifi_ap_mode: false,
		config: { logging: { level: 'verbose' } }
	},
	telemetry: {
		co2: 612,
		temp: 23.4,
		humid: 44.5,
		lastReadOk: true,
		history: {
			co2: [
				{ t: 1000, v: 600 },
				{ t: 1001, v: 612 }
			],
			temp: [
				{ t: 1000, v: 23.3 },
				{ t: 1001, v: 23.4 }
			],
			humid: [
				{ t: 1000, v: 44.1 },
				{ t: 1001, v: 44.5 }
			]
		}
	},
	ble: {
		...bleStatus,
		settings: bleSettings
	},
	sensing: {
		enabled: true,
		variance_threshold: 4
	},
	shelly: [],
	alarms: {
		schema_version: 1,
		rules: []
	},
	airmouse: airMouseStatus
};

function createTestToken() {
	const header = Buffer.from(JSON.stringify({ alg: 'HS256', typ: 'JWT' })).toString('base64');
	const payload = Buffer.from(JSON.stringify({ username: 'admin', admin: true })).toString(
		'base64'
	);
	return `${header}.${payload}.matrixhub-e2e`;
}

async function fulfillJson(route: Route, payload: unknown, status = 200) {
	await route.fulfill({
		status,
		contentType: 'application/json',
		body: JSON.stringify(payload)
	});
}

async function fulfillText(route: Route, payload: string, contentType = 'text/plain') {
	await route.fulfill({
		status: 200,
		contentType,
		body: payload
	});
}

async function installAuthenticatedSession(page: Page) {
	const token = createTestToken();
	await page.addInitScript((accessToken) => {
		window.localStorage.setItem(
			'user',
			JSON.stringify({
				username: 'admin',
				admin: true,
				bearer_token: accessToken
			})
		);
		document.cookie = `access_token=${encodeURIComponent(accessToken)}; path=/ws; SameSite=Strict`;
	}, token);
}

function apiPayload(pathname: string): JsonObject | JsonObject[] {
	switch (pathname) {
		case '/rest/features':
			return { ntp: true, sleep: true, file_manager: true };
		case '/api/matrix/settings':
			return matrixSettings;
		case '/rest/power/status':
		case '/rest/power/config':
			return powerStatus;
		case '/rest/wifiStatus':
			return {
				status: 3,
				local_ip: '192.168.0.30',
				mac_address: 'AA:BB:CC:DD:EE:FF',
				rssi: -55,
				ssid: 'MatrixHub',
				bssid: '11:22:33:44:55:66',
				channel: 6,
				subnet_mask: '255.255.255.0',
				gateway_ip: '192.168.0.1',
				dns_ip_1: '1.1.1.1',
				dns_ip_2: '8.8.8.8'
			};
		case '/rest/wifiSettings':
			return {
				hostname: 'matrixhub',
				mode: 'sta',
				wifi_networks: [
					{
						ssid: 'MatrixHub',
						password: 'password123',
						static_ip_config: false
					}
				]
			};
		case '/rest/listNetworks':
			return { networks: [], scan_state: 'idle' };
		case '/rest/apStatus':
			return {
				status: 0,
				ip_address: '192.168.4.1',
				mac_address: 'AA:BB:CC:DD:EE:00',
				station_num: 0
			};
		case '/rest/apSettings':
			return {
				ssid: 'MatrixHub-AP',
				password: 'password123',
				channel: 6,
				ssid_hidden: false,
				max_clients: 4,
				local_ip: '192.168.4.1',
				gateway_ip: '192.168.4.1',
				subnet_mask: '255.255.255.0'
			};
		case '/rest/ntpStatus':
			return {
				status: 1,
				time_valid: true,
				utc_time: '2026-06-19T10:00:00Z',
				local_time: '2026-06-19 12:00:00',
				server: 'pool.ntp.org',
				uptime: 123456
			};
		case '/rest/ntpSettings':
			return {
				enabled: true,
				server: 'pool.ntp.org',
				tz_label: 'Europe/Warsaw',
				tz_format: 'CET-1CEST,M3.5.0,M10.5.0/3'
			};
		case '/api/notifications/settings':
			return notificationSettings;
		case '/api/config':
			return { logging: { level: 'verbose' } };
		case '/api/system/info':
			return systemInfo;
		case '/api/system/tasks':
			return {
				watchdog: { initialized: true, timeoutSec: 8 },
				taskCount: 2,
				detailsIncluded: true,
				tasks: [
					{
						name: 'loopTask',
						priority: 1,
						stackHighWaterMark: 4096,
						state: 'running',
						coreId: 1
					}
				],
				memory: {
					freeHeap: 210000,
					minFreeHeap: 180000,
					freePsram: 6291456
				}
			};
		case '/api/system/wifi/recover':
			return { success: true, accepted: true, connected: true, ip: '192.168.0.30', rssi: -55 };
		case '/rest/logs/tail':
			return {
				capacity: 128,
				lines: [
					{ t: 1000, l: 'I', g: 'E2E', m: 'frontend smoke log line' },
					{ t: 1001, l: 'V', g: 'E2E', m: 'verbose log line' }
				]
			};
		case '/api/logs':
			return { total_size: 0, months: [] };
		case '/rest/fs/list':
			return {
				files: [
					{ name: '/config', size: 0, directory: true },
					{ name: '/readme.txt', size: 42, directory: false }
				]
			};
		case '/api/alarms/rules':
			return { schema_version: 1, rules: [] };
		case '/api/ble/status':
			return bleStatus;
		case '/api/ble/settings':
			return bleSettings;
		case '/api/wifisensing/config':
			return wifiSensingSettings;
		case '/api/wifisensing/status':
			return wifiSensingStatus;
		case '/api/shelly/devices':
			return [];
		case '/api/udp':
			return {
				enabled: false,
				host: '192.168.0.10',
				port: 9000,
				format: 'json',
				interval_ms: 5000
			};
		case '/api/heartbeat':
			return {
				interval_ms: 60000,
				slots: [
					{ enabled: false, name: 'primary', url: '', allow_insecure: false },
					{ enabled: false, name: 'backup', url: '', allow_insecure: false }
				]
			};
		case '/api/compensation':
			return {
				enabled: true,
				base_temp_offset: 4,
				reference_cpu_temp: 45,
				temp_offset_per_cpu_degree: 0.08,
				min_temp_offset: 0,
				max_temp_offset: 8
			};
		case '/rest/securitySettings':
			return {
				jwt_secret: '',
				jwt_secret_configured: true,
				users: [{ username: 'admin', password: '', admin: true }]
			};
		case '/api/macros':
			return [];
		case '/api/macros/status':
			return macroStatus;
		case '/api/macros/settings':
			return macroSettings;
		case '/api/airmouse/status':
		case '/api/airmouse/config':
			return airMouseStatus;
		case '/api/keyboard/config':
			return { enabled: false };
		case '/api/usbterminal/config':
			return {
				enabled: false,
				idle_timeout_ms: 300000,
				target_port: '/dev/ttyACM0'
			};
		case '/rest/verifyAuthorization':
			return { authorized: true };
		default:
			return {};
	}
}

async function installDeviceApiMocks(page: Page) {
	await page.route(/^https?:\/\/[^/]+\/(api|rest)\//, async (route) => {
		const request = route.request();
		const url = new URL(request.url());
		const pathname = url.pathname;

		if (pathname === '/api/macros/content') {
			await fulfillText(route, '# MatrixHub smoke macro\nWAIT 100\n');
			return;
		}

		if (pathname === '/api/logs/download' || pathname === '/rest/fs/download') {
			await fulfillText(route, '', 'application/octet-stream');
			return;
		}

		let postBody: JsonObject = {};
		try {
			postBody = (request.postDataJSON() ?? {}) as JsonObject;
		} catch {
			postBody = {};
		}

		const basePayload = apiPayload(pathname);
		const payload =
			request.method() === 'POST' && !Array.isArray(basePayload)
				? { ...basePayload, ...postBody }
				: basePayload;
		await fulfillJson(route, payload);
	});
}

function sendSnapshot(ws: WebSocketRoute, channel: string) {
	if (!(channel in websocketSnapshots)) return;
	ws.send(
		JSON.stringify({
			type: 'snapshot',
			channel,
			data: websocketSnapshots[channel]
		})
	);
}

async function installDeviceWebSocketMocks(page: Page) {
	await page.routeWebSocket(/\/ws\/system(?:\?|$)/, (ws) => {
		const warmup = setTimeout(() => {
			sendSnapshot(ws, 'system_status');
			sendSnapshot(ws, 'telemetry');
			sendSnapshot(ws, 'sensing');
			sendSnapshot(ws, 'ble');
		}, 25);

		ws.onMessage((message) => {
			if (typeof message !== 'string') return;

			try {
				const request = JSON.parse(message) as Record<string, unknown>;
				for (const key of ['subscribe', 'snapshot']) {
					const channel = request[key];
					if (typeof channel === 'string') {
						sendSnapshot(ws, channel);
					}
				}
			} catch {
				// Ignore non-JSON frames from future protocol extensions.
			}
		});

		ws.onClose(() => clearTimeout(warmup));
	});

	await page.routeWebSocket(/\/ws\/csi(?:\?|$)/, (ws) => {
		ws.onMessage(() => {
			// Keep the CSI socket open for layout smoke without streaming firmware data.
		});
	});
}

async function expectNoHorizontalOverflow(page: Page) {
	const overflow = await page.evaluate(() => {
		const root = document.documentElement;
		const offenders = Array.from(document.body.querySelectorAll<HTMLElement>('*'))
			.filter((element) => {
				const rect = element.getBoundingClientRect();
				const style = window.getComputedStyle(element);
				return (
					style.display !== 'none' &&
					style.visibility !== 'hidden' &&
					rect.width > 0 &&
					rect.right > window.innerWidth + 1
				);
			})
			.slice(0, 5)
			.map((element) => ({
				tag: element.tagName.toLowerCase(),
				className: element.className.toString().slice(0, 120),
				right: Math.round(element.getBoundingClientRect().right)
			}));

		return {
			scrollWidth: root.scrollWidth,
			clientWidth: root.clientWidth,
			offenders
		};
	});

	expect(overflow, JSON.stringify(overflow.offenders, null, 2)).toEqual(
		expect.objectContaining({
			scrollWidth: expect.any(Number),
			clientWidth: expect.any(Number)
		})
	);
	expect(overflow.scrollWidth).toBeLessThanOrEqual(overflow.clientWidth + 1);
}

async function expectInteractiveControlsHaveNames(page: Page) {
	const unnamedControls = await page.evaluate(() => {
		function hasAssociatedLabel(element: HTMLElement) {
			const id = element.id;
			if (id && document.querySelector(`label[for="${CSS.escape(id)}"]`)) return true;
			if (element.closest('label')) return true;
			const ariaLabelledBy = element.getAttribute('aria-labelledby');
			return Boolean(ariaLabelledBy && document.getElementById(ariaLabelledBy));
		}

		function isVisibleControl(element: HTMLElement) {
			if (element.closest('[hidden], [aria-hidden="true"]')) return false;
			const style = window.getComputedStyle(element);
			if (style.display === 'none' || style.visibility === 'hidden') return false;
			const rect = element.getBoundingClientRect();
			return rect.width > 0 && rect.height > 0;
		}

		return Array.from(document.querySelectorAll<HTMLElement>('button, input, select, textarea'))
			.filter((element) => {
				if (!isVisibleControl(element)) return false;
				if (element instanceof HTMLInputElement && element.type === 'hidden') return false;

				const text = element.innerText?.trim();
				const ariaLabel = element.getAttribute('aria-label')?.trim();
				const title = element.getAttribute('title')?.trim();
				return !(text || ariaLabel || title || hasAssociatedLabel(element));
			})
			.slice(0, 10)
			.map((element) => element.outerHTML.slice(0, 180));
	});

	expect(unnamedControls).toEqual([]);
}

async function expectRouteRendered(page: Page) {
	const content = page.locator('.drawer-content').first();
	await expect(content).toBeVisible({ timeout: 20_000 });
	await expect(
		content.locator('.card, form, canvas, .alert, button, input, select, textarea').first()
	).toBeVisible({ timeout: 20_000 });

	const bodyText = (await content.innerText()).trim();
	expect(bodyText.length).toBeGreaterThan(0);
	expect(bodyText).not.toMatch(/Internal Error|Not Found|Cannot GET|Unhandled/i);
}

test.describe('frontend a11y and responsive smoke', () => {
	test.beforeEach(async ({ page }) => {
		await installAuthenticatedSession(page);
		await installDeviceApiMocks(page);
		await installDeviceWebSocketMocks(page);
	});

	const routes = [
		'/',
		'/system/status',
		'/logs',
		'/charts',
		'/system/file-manager',
		'/system/matrix',
		'/system/power',
		'/wifi/sta',
		'/wifi/ap',
		'/bluetooth',
		'/wifisensing',
		'/wifisensing/csi',
		'/alarms',
		'/shelly',
		'/settings/integrations/udp',
		'/settings/notifications/heartbeat',
		'/settings/sensors/compensation',
		'/connections/ntp',
		'/usb-features/airmouse',
		'/usb-features/jiggler',
		'/usb-features/keyboard',
		'/usb-features/macros',
		'/usb-features/terminal',
		'/user'
	];

	for (const routePath of routes) {
		test(`route ${routePath} has named controls and no mobile overflow`, async ({ page }) => {
			const pageErrors: string[] = [];
			page.on('pageerror', (error) => pageErrors.push(error.message));

			await page.setViewportSize({ width: 1366, height: 900 });
			await page.goto(routePath, { waitUntil: 'domcontentloaded' });
			expect(new URL(page.url()).pathname).toBe(routePath);

			for (const viewport of [
				{ width: 1366, height: 900 },
				{ width: 360, height: 740 }
			]) {
				await page.setViewportSize(viewport);
				expect(new URL(page.url()).pathname).toBe(routePath);
				await expectRouteRendered(page);
				await expectInteractiveControlsHaveNames(page);
				await expectNoHorizontalOverflow(page);
			}

			expect(pageErrors).toEqual([]);
		});
	}
});
