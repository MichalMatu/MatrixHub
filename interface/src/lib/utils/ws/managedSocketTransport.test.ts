import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest';
import { ManagedSocketTransport } from './managedSocketTransport';

class MockWebSocket {
	static CONNECTING = 0;
	static OPEN = 1;
	static CLOSING = 2;
	static CLOSED = 3;
	static instances: MockWebSocket[] = [];

	onopen: (() => void) | null = null;
	onclose: (() => void) | null = null;
	onmessage: ((event: MessageEvent) => void) | null = null;
	onerror: ((event: Event) => void) | null = null;
	readyState = MockWebSocket.CONNECTING;
	binaryType = 'blob';
	close = vi.fn(() => {
		this.readyState = MockWebSocket.CLOSED;
		this.onclose?.();
	});

	constructor(public readonly url: string) {
		MockWebSocket.instances.push(this);
	}

	emitOpen() {
		this.readyState = MockWebSocket.OPEN;
		this.onopen?.();
	}

	emitClose() {
		this.readyState = MockWebSocket.CLOSED;
		this.onclose?.();
	}
}

describe('ManagedSocketTransport', () => {
	beforeEach(() => {
		MockWebSocket.instances = [];
		vi.useFakeTimers();
		vi.stubGlobal('WebSocket', MockWebSocket);
	});

	afterEach(() => {
		vi.restoreAllMocks();
		vi.unstubAllGlobals();
		vi.useRealTimers();
	});

	it('creates a socket with the configured url and binary type', () => {
		const onOpen = vi.fn();
		const transport = new ManagedSocketTransport({
			getUrl: () => 'ws://localhost/test',
			binaryType: 'arraybuffer',
			onOpen
		});

		transport.connect();

		const socket = MockWebSocket.instances[0];
		expect(socket.url).toBe('ws://localhost/test');
		expect(socket.binaryType).toBe('arraybuffer');

		socket.emitOpen();
		expect(onOpen).toHaveBeenCalledOnce();
	});

	it('schedules reconnect after unexpected close when allowed', async () => {
		const onClose = vi.fn();
		const transport = new ManagedSocketTransport({
			getUrl: () => 'ws://localhost/test',
			reconnectDelayMs: 3000,
			onClose,
			shouldReconnect: ({ intentional }) => !intentional
		});

		transport.connect();
		MockWebSocket.instances[0].emitClose();

		expect(onClose).toHaveBeenCalledOnce();

		await vi.advanceTimersByTimeAsync(3000);
		expect(MockWebSocket.instances).toHaveLength(2);
	});

	it('reports reconnect intent and next reconnect attempt in close context', () => {
		const onClose = vi.fn();
		const transport = new ManagedSocketTransport({
			getUrl: () => 'ws://localhost/test',
			reconnectDelayMs: 3000,
			onClose,
			shouldReconnect: ({ intentional }) => !intentional
		});

		transport.connect();
		MockWebSocket.instances[0].emitClose();

		expect(onClose).toHaveBeenCalledWith(
			expect.objectContaining({
				intentional: false,
				willReconnect: true,
				nextReconnectAttempt: 1
			})
		);
	});

	it('does not reconnect after an intentional disconnect', async () => {
		const transport = new ManagedSocketTransport({
			getUrl: () => 'ws://localhost/test',
			reconnectDelayMs: 3000,
			shouldReconnect: ({ intentional }) => !intentional
		});

		transport.connect();
		const socket = MockWebSocket.instances[0];
		transport.disconnect(true);

		expect(socket.close).toHaveBeenCalledOnce();

		await vi.advanceTimersByTimeAsync(3000);
		expect(MockWebSocket.instances).toHaveLength(1);
	});

	it('skips connection and reconnect work while hidden', async () => {
		const transport = new ManagedSocketTransport({
			getUrl: () => 'ws://localhost/test',
			reconnectDelayMs: 3000,
			shouldReconnect: () => true
		});

		transport.setVisible(false);
		transport.connect();
		expect(MockWebSocket.instances).toHaveLength(0);

		transport.setVisible(true);
		transport.connect();
		MockWebSocket.instances[0].emitClose();

		transport.setVisible(false);
		await vi.advanceTimersByTimeAsync(3000);
		expect(MockWebSocket.instances).toHaveLength(1);
	});

	it('uses reconnect delay profiles based on the next attempt count', async () => {
		const transport = new ManagedSocketTransport({
			getUrl: () => 'ws://localhost/test',
			reconnectDelayMs: (attempt) => attempt * 1000,
			shouldReconnect: () => true
		});

		transport.connect();
		MockWebSocket.instances[0].emitClose();

		await vi.advanceTimersByTimeAsync(999);
		expect(MockWebSocket.instances).toHaveLength(1);

		await vi.advanceTimersByTimeAsync(1);
		expect(MockWebSocket.instances).toHaveLength(2);
	});

	it('notifies connect timeout before closing a stalled socket', async () => {
		const onConnectTimeout = vi.fn();
		const transport = new ManagedSocketTransport({
			getUrl: () => 'ws://localhost/test',
			connectTimeoutMs: 2500,
			onConnectTimeout
		});

		transport.connect();
		const socket = MockWebSocket.instances[0];

		await vi.advanceTimersByTimeAsync(2499);
		expect(onConnectTimeout).not.toHaveBeenCalled();

		await vi.advanceTimersByTimeAsync(1);
		expect(onConnectTimeout).toHaveBeenCalledOnce();
		expect(socket.close).toHaveBeenCalledOnce();
	});
});
