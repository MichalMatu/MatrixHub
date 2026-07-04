import { describe, it, expect, vi, beforeEach, afterEach } from 'vitest';
import type { ScriptStatus } from '$lib/services/api/integrations/MacroApiService';
import { SystemStatusStore } from './systemStatus.svelte';
import { DEFAULT_SYSTEM_STATUS } from '$lib/types/system/systemStatus';

// Mock WebSocket
class MockWebSocket {
	static CONNECTING = 0;
	static OPEN = 1;
	static CLOSING = 2;
	static CLOSED = 3;
	static instances: MockWebSocket[] = [];
	static autoOpen = true;
	static openDelayMs = 0;

	onopen: (() => void) | null = null;
	onclose: (() => void) | null = null;
	onmessage: ((ev: MessageEvent) => void) | null = null;
	onerror: ((ev: Event) => void) | null = null;
	binaryType: string = 'blob';
	readyState: number = MockWebSocket.CONNECTING;

	constructor(public url: string) {
		MockWebSocket.instances.push(this);
		if (MockWebSocket.autoOpen) {
			setTimeout(() => {
				if (this.readyState !== MockWebSocket.CONNECTING) return;
				this.readyState = MockWebSocket.OPEN;
				this.onopen?.();
			}, MockWebSocket.openDelayMs);
		}
	}

	send = vi.fn();
	close = vi.fn(() => {
		this.readyState = MockWebSocket.CLOSED;
		this.onclose?.();
	});
}

// Mock User store
vi.mock('./user', () => ({
	user: { bearer_token: 'test-token', isValid: true }
}));

// Mock i18n
vi.mock('$lib/i18n.svelte', () => ({
	i18n: {
		languageTag: 'en',
		setLocale: vi.fn()
	}
}));

// Mock connectionState to avoid dynamic import races
vi.mock('./connectionState', () => ({
	connectionState: {
		setApiHealthy: vi.fn(),
		setReconnectAttempt: vi.fn(),
		clearConnectionErrors: vi.fn(),
		logConnectionError: vi.fn()
	}
}));

vi.mock('./connectionState.svelte', () => ({
	connectionState: {
		setApiHealthy: vi.fn(),
		setReconnectAttempt: vi.fn(),
		clearConnectionErrors: vi.fn(),
		logConnectionError: vi.fn()
	}
}));

describe('SystemStatusStore', () => {
	const stores: SystemStatusStore[] = [];
	const createStore = () => {
		const store = new SystemStatusStore();
		stores.push(store);
		return store;
	};

	beforeEach(() => {
		vi.stubGlobal('WebSocket', MockWebSocket);
		vi.useFakeTimers();
		vi.clearAllMocks();
		MockWebSocket.instances = [];
		MockWebSocket.autoOpen = true;
		MockWebSocket.openDelayMs = 0;
	});

	afterEach(() => {
		for (const store of stores.splice(0)) {
			store.destroy();
		}
		vi.useRealTimers();
		vi.unstubAllGlobals();
	});

	it('should initialize with default status', () => {
		const store = createStore();
		expect(store.data).toEqual(DEFAULT_SYSTEM_STATUS);
		expect(store.isConnected).toBe(false);
	});

	it('should update macros state', () => {
		const store = createStore();
		const testMacros: ScriptStatus = {
			current_script: '',
			status: 'IDLE',
			current_line: 0,
			uptime_ms: 0,
			last_error: ''
		};
		store.setMacros(testMacros);
		expect(store.macros).toEqual(testMacros);
	});

	it('should manage WebSocket connection state', async () => {
		const store = createStore();

		store.connect();

		// Advance timers to trigger MockWebSocket onopen
		await vi.advanceTimersByTimeAsync(10);

		expect(store.isConnected).toBe(true);

		store.disconnect();
		expect(store.isConnected).toBe(false);
	});

	it('should handle subscription channels', async () => {
		const store = createStore();
		store.connect();
		await vi.advanceTimersByTimeAsync(10);

		const socketInstance = store.ws;
		expect(socketInstance).not.toBeNull();

		store.subscribeChannel('ble');
		expect(socketInstance?.send).toHaveBeenCalledWith(JSON.stringify({ subscribe: 'ble' }));
		expect(socketInstance?.send).toHaveBeenCalledWith(JSON.stringify({ snapshot: 'ble' }));

		store.unsubscribeChannel('ble');
		expect(socketInstance?.send).toHaveBeenCalledWith(JSON.stringify({ unsubscribe: 'ble' }));
		expect(store.ws).toBeNull();
	});

	it('should request a snapshot when a second subscriber joins an already-open channel without cached data', async () => {
		const store = createStore();
		store.connect();
		await vi.advanceTimersByTimeAsync(10);

		const socketInstance = store.ws as unknown as MockWebSocket;
		expect(socketInstance).not.toBeNull();

		store.subscribeChannel('system_status');
		expect(socketInstance.send).toHaveBeenNthCalledWith(
			1,
			JSON.stringify({ subscribe: 'system_status' })
		);
		expect(socketInstance.send).toHaveBeenNthCalledWith(
			2,
			JSON.stringify({ snapshot: 'system_status' })
		);

		socketInstance.send.mockClear();
		store.subscribeChannel('system_status');

		expect(socketInstance.send).toHaveBeenCalledTimes(1);
		expect(socketInstance.send).toHaveBeenCalledWith(JSON.stringify({ snapshot: 'system_status' }));
	});

	it('should keep the cached snapshot and socket lease until the last subscriber leaves', async () => {
		const store = createStore();
		store.connect();
		await vi.advanceTimersByTimeAsync(10);

		const socketInstance = store.ws as unknown as MockWebSocket;
		expect(socketInstance).not.toBeNull();

		store.subscribeChannel('system_status');
		socketInstance.onmessage?.({
			data: JSON.stringify({
				type: 'snapshot',
				channel: 'system_status',
				data: { diagnostics: { healthy: true } }
			})
		} as MessageEvent);
		store.subscribeChannel('system_status');

		expect(store.getSnapshot('system_status')).toEqual({
			diagnostics: { healthy: true }
		});

		socketInstance.send.mockClear();
		store.unsubscribeChannel('system_status');

		expect(socketInstance.send).not.toHaveBeenCalledWith(
			JSON.stringify({ unsubscribe: 'system_status' })
		);
		expect(store.ws).toBe(socketInstance as unknown as WebSocket);
		expect(store.getSnapshot('system_status')).toEqual({
			diagnostics: { healthy: true }
		});

		store.unsubscribeChannel('system_status');

		expect(socketInstance.send).toHaveBeenCalledWith(
			JSON.stringify({ unsubscribe: 'system_status' })
		);
		expect(store.ws).toBeNull();
		expect(store.getSnapshot('system_status')).toBeNull();
	});

	it('should sync subscriptions on reconnect', async () => {
		const store = createStore();
		store.subscribeChannel('telemetry');

		store.connect();
		await vi.advanceTimersByTimeAsync(10);

		const firstSocket = store.ws as unknown as MockWebSocket;
		expect(firstSocket).not.toBeNull();
		expect(firstSocket.send).toHaveBeenCalledWith(JSON.stringify({ subscribe: 'telemetry' }));
		expect(firstSocket.send).toHaveBeenCalledWith(JSON.stringify({ snapshot: 'telemetry' }));

		firstSocket.close();
		await vi.advanceTimersByTimeAsync(1000);

		const secondSocket = MockWebSocket.instances[1];
		expect(secondSocket).toBeDefined();
		await vi.advanceTimersByTimeAsync(10);
		expect(secondSocket.send).toHaveBeenCalledWith(JSON.stringify({ subscribe: 'telemetry' }));
		expect(secondSocket.send).toHaveBeenCalledWith(JSON.stringify({ snapshot: 'telemetry' }));
	});

	it('should update status from binary packet', async () => {
		const store = createStore();
		store.connect();
		await vi.advanceTimersByTimeAsync(10);

		const socketInstance = store.ws as unknown as MockWebSocket;
		expect(socketInstance).not.toBeNull();

		// Create a mock system packet (Magic 0xA5)
		const buffer = new ArrayBuffer(10);
		const view = new DataView(buffer);
		view.setUint8(0, 0xa5);
		view.setUint32(1, 1000, true); // ts
		view.setUint8(5, 3); // connected
		view.setUint8(6, 0x01); // wifi flags (sta)
		view.setInt8(7, -50); // rssi
		view.setInt16(8, 265, true); // cpu temp

		// In systemStatus.svelte.ts, we use onmessage
		if (socketInstance.onmessage) {
			// @ts-expect-error - mock event data
			socketInstance.onmessage({ data: buffer });
		}

		expect(store.data.rssi).toBe(-50);
		expect(store.data.wifiStatus).toBe(3);
	});

	it('should keep reconnecting after more than twenty failures with capped backoff', async () => {
		const consoleErrorSpy = vi.spyOn(console, 'error').mockImplementation(() => {});
		const store = createStore();
		const { connectionState } = await import('./connectionState');
		store.connect();
		await vi.advanceTimersByTimeAsync(10);

		expect(MockWebSocket.instances).toHaveLength(1);

		store.reconnectAttempts = 25;
		const firstSocket = MockWebSocket.instances[0];
		firstSocket.close();

		expect(connectionState.setApiHealthy).toHaveBeenCalledWith(false);
		expect(connectionState.setReconnectAttempt).toHaveBeenCalledWith(26);

		expect(MockWebSocket.instances).toHaveLength(1);
		await vi.advanceTimersByTimeAsync(59000);
		expect(MockWebSocket.instances).toHaveLength(1);

		await vi.advanceTimersByTimeAsync(1000);
		expect(MockWebSocket.instances).toHaveLength(2);
		consoleErrorSpy.mockRestore();
	});

	it('should abort a stalled handshake with a connect timeout instead of the live watchdog', async () => {
		MockWebSocket.autoOpen = false;
		const consoleWarnSpy = vi.spyOn(console, 'warn').mockImplementation(() => {});
		const store = createStore();

		store.connect();

		const socketInstance = store.ws as unknown as MockWebSocket;
		expect(socketInstance).not.toBeNull();
		expect(socketInstance.readyState).toBe(MockWebSocket.CONNECTING);

		await vi.advanceTimersByTimeAsync(9999);
		expect(socketInstance.close).not.toHaveBeenCalled();

		await vi.advanceTimersByTimeAsync(1);
		expect(socketInstance.close).toHaveBeenCalledTimes(1);
		expect(consoleWarnSpy).toHaveBeenCalledWith(
			'[System] WebSocket connect timeout, aborting stalled handshake.'
		);
		expect(consoleWarnSpy).not.toHaveBeenCalledWith(
			'[System] WebSocket watchdog timeout, closing dead socket.'
		);

		consoleWarnSpy.mockRestore();
	});

	it('should close an idle open socket with the live watchdog timeout', async () => {
		const consoleWarnSpy = vi.spyOn(console, 'warn').mockImplementation(() => {});
		const store = createStore();

		store.connect();
		await vi.advanceTimersByTimeAsync(10);

		const socketInstance = store.ws as unknown as MockWebSocket;
		expect(socketInstance).not.toBeNull();
		expect(socketInstance.readyState).toBe(MockWebSocket.OPEN);

		await vi.advanceTimersByTimeAsync(30000);
		expect(socketInstance.close).toHaveBeenCalledTimes(1);
		expect(consoleWarnSpy).toHaveBeenCalledWith(
			'[System] WebSocket watchdog timeout, closing dead socket.'
		);

		consoleWarnSpy.mockRestore();
	});

	it('should preserve live system status during the reconnect grace period', async () => {
		const store = createStore();
		const { connectionState } = await import('./connectionState');
		store.connect();
		await vi.advanceTimersByTimeAsync(10);

		const socketInstance = store.ws as unknown as MockWebSocket;
		expect(socketInstance).not.toBeNull();

		const buffer = new ArrayBuffer(10);
		const view = new DataView(buffer);
		view.setUint8(0, 0xa5);
		view.setUint32(1, 1000, true);
		view.setUint8(5, 3);
		view.setUint8(6, 0x01);
		view.setInt8(7, -50);
		view.setInt16(8, 280, true);

		socketInstance.onmessage?.({ data: buffer } as MessageEvent);
		expect(store.data.rssi).toBe(-50);
		expect(store.data.coreTemp).toBe(28);

		socketInstance.close();

		expect(store.isConnected).toBe(false);
		expect(connectionState.setApiHealthy).toHaveBeenCalledWith(false);
		expect(connectionState.setReconnectAttempt).toHaveBeenCalledWith(1);
		expect(store.data.rssi).toBe(-50);
		expect(store.data.coreTemp).toBe(28);

		await vi.advanceTimersByTimeAsync(9999);
		expect(store.data.rssi).toBe(-50);
		expect(store.data.coreTemp).toBe(28);
	});

	it('should clear stale system status only after the reconnect grace period expires', async () => {
		const consoleErrorSpy = vi.spyOn(console, 'error').mockImplementation(() => {});
		const store = createStore();
		store.connect();
		await vi.advanceTimersByTimeAsync(10);

		const socketInstance = store.ws as unknown as MockWebSocket;
		expect(socketInstance).not.toBeNull();

		const buffer = new ArrayBuffer(10);
		const view = new DataView(buffer);
		view.setUint8(0, 0xa5);
		view.setUint32(1, 1000, true);
		view.setUint8(5, 3);
		view.setUint8(6, 0x01);
		view.setInt8(7, -50);
		view.setInt16(8, 280, true);

		socketInstance.onmessage?.({ data: buffer } as MessageEvent);
		expect(store.data.rssi).toBe(-50);

		MockWebSocket.autoOpen = false;
		socketInstance.close();
		await vi.advanceTimersByTimeAsync(10000);

		expect(store.data).toEqual(DEFAULT_SYSTEM_STATUS);
		expect(store.macros).toBeNull();
		consoleErrorSpy.mockRestore();
	});

	it('should preserve live status if websocket reconnects before grace period expires', async () => {
		const store = createStore();
		store.subscribeChannel('system_status');
		store.connect();
		await vi.advanceTimersByTimeAsync(10);

		const firstSocket = store.ws as unknown as MockWebSocket;
		expect(firstSocket).not.toBeNull();

		const buffer = new ArrayBuffer(10);
		const view = new DataView(buffer);
		view.setUint8(0, 0xa5);
		view.setUint32(1, 1000, true);
		view.setUint8(5, 3);
		view.setUint8(6, 0x01);
		view.setInt8(7, -50);
		view.setInt16(8, 280, true);

		firstSocket.onmessage?.({ data: buffer } as MessageEvent);
		firstSocket.close();

		await vi.advanceTimersByTimeAsync(1000);
		const secondSocket = MockWebSocket.instances[1];
		expect(secondSocket).toBeDefined();
		await vi.advanceTimersByTimeAsync(10);

		expect(store.isConnected).toBe(true);
		expect(store.data.rssi).toBe(-50);
		expect(store.data.coreTemp).toBe(28);
	});

	it('should cancel pending grace period errors after intentional disconnect', async () => {
		const store = createStore();
		store.connect();
		await vi.advanceTimersByTimeAsync(10);

		const socketInstance = store.ws as unknown as MockWebSocket;
		expect(socketInstance).not.toBeNull();

		socketInstance.close();
		store.disconnect();

		const consoleErrorSpy = vi.spyOn(console, 'error').mockImplementation(() => {});
		const { connectionState } = await import('./connectionState');
		const reconnectAttemptCallsBeforeWait = vi.mocked(connectionState.setReconnectAttempt).mock
			.calls.length;

		await vi.advanceTimersByTimeAsync(10000);

		expect(consoleErrorSpy).not.toHaveBeenCalledWith(
			'[System] Grace period (10s) expired without reconnect.'
		);
		expect(connectionState.logConnectionError).not.toHaveBeenCalled();
		expect(connectionState.setReconnectAttempt).toHaveBeenCalledTimes(
			reconnectAttemptCallsBeforeWait
		);

		consoleErrorSpy.mockRestore();
	});
});
