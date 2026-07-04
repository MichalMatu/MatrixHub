// @vitest-environment jsdom
import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest';
import { SystemSocketTransport } from './socketTransport';

class MockWebSocket {
	static readonly CONNECTING = 0;
	static readonly OPEN = 1;
	static readonly CLOSING = 2;
	static readonly CLOSED = 3;
	static instances: MockWebSocket[] = [];

	readyState = MockWebSocket.CONNECTING;
	binaryType = '';
	onopen: ((event: Event) => void) | null = null;
	onclose: ((event: CloseEvent) => void) | null = null;
	onerror: ((event: Event) => void) | null = null;
	onmessage: ((event: MessageEvent) => void) | null = null;
	sent: string[] = [];

	constructor(public readonly url: string) {
		MockWebSocket.instances.push(this);
	}

	send(data: string) {
		this.sent.push(data);
	}

	close() {
		this.readyState = MockWebSocket.CLOSED;
		this.onclose?.({} as CloseEvent);
	}

	open() {
		this.readyState = MockWebSocket.OPEN;
		this.onopen?.(new Event('open'));
	}
}

describe('SystemSocketTransport', () => {
	beforeEach(() => {
		MockWebSocket.instances = [];
		vi.useFakeTimers();
		vi.unstubAllGlobals();
		vi.stubGlobal('WebSocket', MockWebSocket as unknown as typeof WebSocket);
	});

	afterEach(() => {
		vi.restoreAllMocks();
		vi.unstubAllGlobals();
		vi.useRealTimers();
	});

	it('notifies the store when the socket opens without owning channel state itself', () => {
		const onOpen = vi.fn();
		const transport = new SystemSocketTransport({
			hasValidSession: () => true,
			onOpen,
			onConnectionStateChange: vi.fn(),
			onConnectionHealthy: vi.fn(),
			onConnectionInterrupted: vi.fn(),
			onGracePeriodExpired: vi.fn(),
			onMessage: vi.fn(),
			onResetLiveState: vi.fn()
		});

		transport.connect();
		const socket = MockWebSocket.instances[0];
		socket.open();

		expect(onOpen).toHaveBeenCalledOnce();
		expect(onOpen).toHaveBeenCalledWith(socket as unknown as WebSocket);
		expect(socket.sent).toEqual([]);

		transport.destroy();
	});

	it('does not append auth query params to ws/system URLs', () => {
		const transport = new SystemSocketTransport({
			hasValidSession: () => true,
			onConnectionStateChange: vi.fn(),
			onConnectionHealthy: vi.fn(),
			onConnectionInterrupted: vi.fn(),
			onGracePeriodExpired: vi.fn(),
			onMessage: vi.fn(),
			onResetLiveState: vi.fn()
		});

		transport.connect();

		const socket = MockWebSocket.instances[0];
		expect(socket.url).toContain('/ws/system');
		expect(socket.url).not.toContain('access_token=');

		transport.destroy();
	});

	it('can send explicit JSON messages over an open socket', () => {
		const transport = new SystemSocketTransport({
			hasValidSession: () => true,
			onConnectionStateChange: vi.fn(),
			onConnectionHealthy: vi.fn(),
			onConnectionInterrupted: vi.fn(),
			onGracePeriodExpired: vi.fn(),
			onMessage: vi.fn(),
			onResetLiveState: vi.fn()
		});

		transport.connect();
		const socket = MockWebSocket.instances[0];
		socket.open();

		transport.sendJson({ snapshot: 'system_status' });
		expect(socket.sent).toEqual([JSON.stringify({ snapshot: 'system_status' })]);

		transport.destroy();
	});

	it('does not send JSON messages while the socket is closed', () => {
		const transport = new SystemSocketTransport({
			hasValidSession: () => true,
			onConnectionStateChange: vi.fn(),
			onConnectionHealthy: vi.fn(),
			onConnectionInterrupted: vi.fn(),
			onGracePeriodExpired: vi.fn(),
			onMessage: vi.fn(),
			onResetLiveState: vi.fn()
		});

		transport.connect();
		const firstSocket = MockWebSocket.instances[0];
		transport.sendJson({ subscribe: 'system_status' });
		expect(firstSocket.sent).toEqual([]);

		transport.destroy();
	});

	it('keeps live state during the reconnect grace period and resets only after expiry', async () => {
		const consoleErrorSpy = vi.spyOn(console, 'error').mockImplementation(() => {});
		const onResetLiveState = vi.fn();
		const onGracePeriodExpired = vi.fn();
		const onConnectionInterrupted = vi.fn();
		const transport = new SystemSocketTransport({
			hasValidSession: () => true,
			onConnectionStateChange: vi.fn(),
			onConnectionHealthy: vi.fn(),
			onConnectionInterrupted,
			onGracePeriodExpired,
			onMessage: vi.fn(),
			onResetLiveState
		});

		transport.connect();
		const socket = MockWebSocket.instances[0];
		socket.open();
		socket.onclose?.({} as CloseEvent);

		expect(onResetLiveState).not.toHaveBeenCalled();
		expect(onGracePeriodExpired).not.toHaveBeenCalled();
		expect(onConnectionInterrupted).toHaveBeenCalledWith({
			reconnecting: true,
			reconnectAttempt: 1
		});

		await vi.advanceTimersByTimeAsync(9999);
		expect(onResetLiveState).not.toHaveBeenCalled();

		await vi.advanceTimersByTimeAsync(1);
		expect(onResetLiveState).toHaveBeenCalledOnce();
		expect(onGracePeriodExpired).toHaveBeenCalledOnce();
		consoleErrorSpy.mockRestore();

		transport.destroy();
	});

	it('preserves live state when the socket reconnects before grace period expires', async () => {
		const onResetLiveState = vi.fn();
		const onGracePeriodExpired = vi.fn();
		const onConnectionInterrupted = vi.fn();
		const transport = new SystemSocketTransport({
			hasValidSession: () => true,
			onConnectionStateChange: vi.fn(),
			onConnectionHealthy: vi.fn(),
			onConnectionInterrupted,
			onGracePeriodExpired,
			onMessage: vi.fn(),
			onResetLiveState
		});

		transport.connect();
		const firstSocket = MockWebSocket.instances[0];
		firstSocket.open();
		firstSocket.onclose?.({} as CloseEvent);

		await vi.advanceTimersByTimeAsync(1000);
		const reconnectedSocket = MockWebSocket.instances[1];
		reconnectedSocket.open();

		await vi.advanceTimersByTimeAsync(9000);

		expect(onResetLiveState).not.toHaveBeenCalled();
		expect(onGracePeriodExpired).not.toHaveBeenCalled();
		expect(onConnectionInterrupted).toHaveBeenCalledWith({
			reconnecting: true,
			reconnectAttempt: 1
		});
		expect(reconnectedSocket.sent).toEqual([]);

		transport.destroy();
	});

	it('replaces a dead socket without resetting live state', () => {
		const onResetLiveState = vi.fn();
		const transport = new SystemSocketTransport({
			hasValidSession: () => true,
			onConnectionStateChange: vi.fn(),
			onConnectionHealthy: vi.fn(),
			onConnectionInterrupted: vi.fn(),
			onGracePeriodExpired: vi.fn(),
			onMessage: vi.fn(),
			onResetLiveState
		});

		transport.connect();
		const firstSocket = MockWebSocket.instances[0];
		firstSocket.open();
		firstSocket.readyState = MockWebSocket.CLOSED;

		transport.ensureConnected();

		expect(onResetLiveState).not.toHaveBeenCalled();
		expect(MockWebSocket.instances).toHaveLength(2);

		transport.destroy();
	});
});
