import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest';
import { useCsiConnection } from './useCsiConnection.svelte';

const { mockSession } = vi.hoisted(() => ({
	mockSession: {
		isAuthenticated: true
	}
}));

vi.mock('$lib/features/auth/useSessionAccess.svelte', () => ({
	useSessionAccess: () => mockSession
}));

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
	binaryType = 'blob';
	readyState = MockWebSocket.CONNECTING;
	send = vi.fn();
	close = vi.fn(() => {
		this.readyState = MockWebSocket.CLOSED;
		this.onclose?.();
	});

	constructor(public url: string) {
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

	emitMessage(data: ArrayBuffer) {
		this.onmessage?.({ data } as MessageEvent);
	}
}

function createCsiFrame({
	timestamp = 1234,
	rssi = -42,
	gainTimesTen = 15,
	motionScore = 2.5,
	isMotionDetected = true,
	iq = [3, 4, 5, 12]
}: {
	timestamp?: number;
	rssi?: number;
	gainTimesTen?: number;
	motionScore?: number;
	isMotionDetected?: boolean;
	iq?: number[];
}) {
	const buffer = new ArrayBuffer(13 + iq.length);
	const view = new DataView(buffer);
	let offset = 0;

	view.setUint32(offset, timestamp, true);
	offset += 4;
	view.setInt8(offset, rssi);
	offset += 1;
	view.setUint16(offset, iq.length, true);
	offset += 2;
	view.setUint8(offset, gainTimesTen);
	offset += 1;
	view.setFloat32(offset, motionScore, true);
	offset += 4;
	view.setUint8(offset, isMotionDetected ? 1 : 0);
	offset += 1;

	for (let index = 0; index < iq.length; index++) {
		view.setInt8(offset + index, iq[index]);
	}

	return buffer;
}

function concatFrames(...frames: ArrayBuffer[]) {
	const totalLength = frames.reduce((sum, frame) => sum + frame.byteLength, 0);
	const batch = new Uint8Array(totalLength);
	let offset = 0;

	for (const frame of frames) {
		batch.set(new Uint8Array(frame), offset);
		offset += frame.byteLength;
	}

	return batch.buffer;
}

describe('useCsiConnection', () => {
	let nowMs = 0;

	beforeEach(() => {
		nowMs = 0;
		MockWebSocket.instances = [];
		vi.useFakeTimers();
		vi.stubGlobal('WebSocket', MockWebSocket);
		vi.spyOn(performance, 'now').mockImplementation(() => nowMs);
		vi.spyOn(console, 'log').mockImplementation(() => undefined);
		vi.spyOn(console, 'warn').mockImplementation(() => undefined);
		vi.spyOn(console, 'error').mockImplementation(() => undefined);
		Object.defineProperty(document, 'hidden', {
			value: false,
			configurable: true
		});
	});

	afterEach(() => {
		vi.restoreAllMocks();
		vi.unstubAllGlobals();
		vi.useRealTimers();
	});

	it('resets fps to zero after unexpected close', async () => {
		let cleanup: (() => void) | undefined;
		let csi: ReturnType<typeof useCsiConnection> | undefined;

		cleanup = $effect.root(() => {
			csi = useCsiConnection();
		});

		const socket = await vi.waitFor(() => {
			expect(MockWebSocket.instances[0]).toBeDefined();
			return MockWebSocket.instances[0];
		});

		expect(socket.url).toContain('/ws/csi');
		expect(socket.url).not.toContain('access_token=');
		socket.emitOpen();

		nowMs = 1200;
		socket.emitMessage(createCsiFrame({}));

		expect(csi?.isConnected).toBe(true);
		expect(csi?.fps).toBe(1);

		socket.emitClose();

		expect(csi?.isConnected).toBe(false);
		expect(csi?.fps).toBe(0);

		cleanup?.();
	});

	it('restarts fps counting cleanly after reconnect', async () => {
		let cleanup: (() => void) | undefined;
		let csi: ReturnType<typeof useCsiConnection> | undefined;

		cleanup = $effect.root(() => {
			csi = useCsiConnection();
		});

		const firstSocket = await vi.waitFor(() => {
			expect(MockWebSocket.instances[0]).toBeDefined();
			return MockWebSocket.instances[0];
		});

		firstSocket.emitOpen();

		nowMs = 1200;
		firstSocket.emitMessage(createCsiFrame({ timestamp: 1 }));
		expect(csi?.fps).toBe(1);

		firstSocket.emitClose();
		expect(csi?.fps).toBe(0);

		await vi.advanceTimersByTimeAsync(3000);

		const secondSocket = MockWebSocket.instances[1];
		expect(secondSocket).toBeDefined();

		secondSocket.emitOpen();
		nowMs = 4300;
		secondSocket.emitMessage(createCsiFrame({ timestamp: 2 }));
		expect(csi?.isConnected).toBe(true);
		expect(csi?.fps).toBe(1);

		cleanup?.();
	});

	it('counts packets inside a batched CSI websocket message', async () => {
		let cleanup: (() => void) | undefined;
		let csi: ReturnType<typeof useCsiConnection> | undefined;

		cleanup = $effect.root(() => {
			csi = useCsiConnection();
		});

		const socket = await vi.waitFor(() => {
			expect(MockWebSocket.instances[0]).toBeDefined();
			return MockWebSocket.instances[0];
		});

		socket.emitOpen();
		nowMs = 1200;
		socket.emitMessage(
			concatFrames(
				createCsiFrame({ timestamp: 10, iq: [3, 4] }),
				createCsiFrame({ timestamp: 11, rssi: -60, iq: [5, 12] })
			)
		);

		expect(csi?.timestamp).toBe(11);
		expect(csi?.rssi).toBe(-60);
		expect(csi?.fps).toBe(2);
		expect(Array.from(csi?.amplitudes ?? [])).toEqual([13]);

		cleanup?.();
	});

	it('clears stale CSI frame data after unexpected close', async () => {
		let cleanup: (() => void) | undefined;
		let csi: ReturnType<typeof useCsiConnection> | undefined;

		cleanup = $effect.root(() => {
			csi = useCsiConnection();
		});

		const socket = await vi.waitFor(() => {
			expect(MockWebSocket.instances[0]).toBeDefined();
			return MockWebSocket.instances[0];
		});

		socket.emitOpen();
		socket.emitMessage(
			createCsiFrame({ timestamp: 777, motionScore: 9.5, isMotionDetected: true })
		);

		expect(csi?.timestamp).toBe(777);
		expect(csi?.motionScore).toBeCloseTo(9.5);
		expect(csi?.isMotionDetected).toBe(true);
		expect(csi?.amplitudes.length).toBeGreaterThan(0);

		socket.emitClose();

		expect(csi?.timestamp).toBe(0);
		expect(csi?.motionScore).toBe(0);
		expect(csi?.isMotionDetected).toBe(false);
		expect(csi?.amplitudes.length).toBe(0);

		cleanup?.();
	});

	it('sends STOP before closing an open CSI socket during teardown', async () => {
		let cleanup: (() => void) | undefined;

		cleanup = $effect.root(() => {
			useCsiConnection();
		});

		const socket = await vi.waitFor(() => {
			expect(MockWebSocket.instances[0]).toBeDefined();
			return MockWebSocket.instances[0];
		});

		socket.emitOpen();
		cleanup?.();

		expect(socket.send).toHaveBeenCalledWith('STOP');
		expect(socket.close).not.toHaveBeenCalled();

		await vi.advanceTimersByTimeAsync(50);
		expect(socket.close).toHaveBeenCalledOnce();
	});
});
