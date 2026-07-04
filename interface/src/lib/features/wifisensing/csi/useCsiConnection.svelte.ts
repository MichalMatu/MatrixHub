import { untrack } from 'svelte';
import { useSessionAccess } from '$lib/features/auth/useSessionAccess.svelte';
import { buildWebSocketUrl } from '$lib/utils/ws/buildWebSocketUrl';
import { ManagedSocketTransport } from '../../../utils/ws/managedSocketTransport';
import { SocketWatchdog } from '../../../utils/ws/socketWatchdog';
import { parseCsiFrames, type CsiAmplitudeBuffers } from './parseCsiFrame';

const WATCHDOG_TIMEOUT_MS = 15000;
const RETRY_DELAY_MS = 3000;
const CONNECT_TIMEOUT_MS = 10000;

export function useCsiConnection() {
	const session = useSessionAccess();
	let userEnabled = $state(true);
	let isConnected = $state(false);
	let pageVisible = $state(true);

	let timestamp = $state(0);
	let rssi = $state(0);
	let gain = $state(1.0);
	let subcarriers = $state(64);
	let amplitudes = $state.raw<Float32Array<ArrayBufferLike>>(new Float32Array(0));
	let motionScore = $state(0);
	let isMotionDetected = $state(false);
	let fps = $state(0);

	let frameCount = 0;
	let lastFrameTime = typeof performance !== 'undefined' ? performance.now() : 0;
	let buffers: CsiAmplitudeBuffers = {
		bufferA: new Float32Array(0),
		bufferB: new Float32Array(0),
		flip: false
	};

	function resetFpsState() {
		frameCount = 0;
		fps = 0;
		lastFrameTime = typeof performance !== 'undefined' ? performance.now() : 0;
	}

	function resetFrameState() {
		timestamp = 0;
		rssi = 0;
		gain = 1.0;
		subcarriers = 64;
		amplitudes = new Float32Array(0);
		motionScore = 0;
		isMotionDetected = false;
		buffers = {
			bufferA: new Float32Array(0),
			bufferB: new Float32Array(0),
			flip: false
		};
	}

	function resetConnectionState() {
		isConnected = false;
		resetFrameState();
		resetFpsState();
	}

	function processData(buffer: ArrayBuffer) {
		const parsedFrames = parseCsiFrames(buffer, buffers);
		if (!parsedFrames) return;

		for (const parsed of parsedFrames) {
			buffers = parsed.buffers;
			timestamp = parsed.timestamp;
			rssi = parsed.rssi;
			gain = parsed.gain;
			subcarriers = parsed.subcarriers;
			amplitudes = parsed.amplitudes;
			motionScore = parsed.motionScore;
			isMotionDetected = parsed.isMotionDetected;
		}

		frameCount += parsedFrames.length;
		const now = performance.now();
		if (now - lastFrameTime >= 1000) {
			fps = frameCount;
			frameCount = 0;
			lastFrameTime = now;
		}
	}

	const watchdog = new SocketWatchdog({
		timeoutMs: WATCHDOG_TIMEOUT_MS,
		onTimeout: () => {
			console.warn('WS Watchdog: Timeout, resetting...');
			transport.ws?.close();
		}
	});

	function resetWatchdog() {
		watchdog.arm(isConnected);
	}

	function clearWatchdog() {
		watchdog.clear();
	}

	function canConnect() {
		if (typeof window === 'undefined') return false;
		if (!userEnabled) return false;
		if (!session.isAuthenticated) return false;
		return true;
	}

	const transport = new ManagedSocketTransport({
		getUrl: () => buildWebSocketUrl('/ws/csi'),
		canConnect,
		connectTimeoutMs: CONNECT_TIMEOUT_MS,
		reconnectDelayMs: RETRY_DELAY_MS,
		binaryType: 'arraybuffer',
		closeSocket: (socket) => {
			if (socket.readyState === WebSocket.OPEN) {
				try {
					socket.send('STOP');
					setTimeout(() => {
						if (socket.readyState === WebSocket.OPEN) {
							try {
								socket.close();
							} catch {
								// ignore close errors on stale socket
							}
						}
					}, 50);
				} catch (error) {
					console.warn('Error sending STOP or closing CSI WS:', error);
				}
				return;
			}

			if (socket.readyState < WebSocket.CLOSING) {
				try {
					socket.close();
				} catch {
					// ignore close errors on stale socket
				}
			}
		},
		onOpen: () => {
			isConnected = true;
			resetFpsState();
			resetWatchdog();
		},
		onClose: ({ intentional }) => {
			resetConnectionState();
			clearWatchdog();

			if (!intentional && userEnabled && session.isAuthenticated) {
				console.warn('CSI WS Closed unexpectedly. Reconnecting...');
			}
		},
		onMessage: (event) => {
			resetWatchdog();
			if (event.data instanceof ArrayBuffer) {
				processData(event.data);
			}
		},
		onError: (event) => {
			console.error('CSI WS Error', event);
		},
		onConnectTimeout: () => {
			console.warn('CSI WS connect timeout.');
		},
		shouldReconnect: ({ intentional }) => !intentional && userEnabled && session.isAuthenticated
	});

	function connect() {
		transport.connect();
	}

	function stopConnection() {
		clearWatchdog();
		transport.disconnect(true);
		resetConnectionState();
	}

	function disconnect() {
		userEnabled = false;
		stopConnection();
	}

	function toggle() {
		if (userEnabled) {
			disconnect();
			return;
		}

		userEnabled = true;
		connect();
	}

	$effect(() => {
		if (typeof window === 'undefined') return;

		const syncVisibility = () => {
			pageVisible = !document.hidden;
		};

		syncVisibility();
		document.addEventListener('visibilitychange', syncVisibility);

		return () => {
			document.removeEventListener('visibilitychange', syncVisibility);
		};
	});

	$effect(() => {
		if (typeof window === 'undefined') return;

		// CSI is a dedicated high-rate stream, so hidden tabs should release it
		// instead of keeping the firmware producer alive in the background.
		if (!session.isAuthenticated || !userEnabled || !pageVisible) {
			stopConnection();
			return;
		}

		connect();
	});

	$effect(() => {
		untrack(() => {
			resetConnectionState();
		});

		return () => {
			stopConnection();
			watchdog.destroy();
		};
	});

	return {
		get isConnected() {
			return isConnected;
		},
		get timestamp() {
			return timestamp;
		},
		get rssi() {
			return rssi;
		},
		get gain() {
			return gain;
		},
		get subcarriers() {
			return subcarriers;
		},
		get amplitudes() {
			return amplitudes;
		},
		get motionScore() {
			return motionScore;
		},
		get isMotionDetected() {
			return isMotionDetected;
		},
		get fps() {
			return fps;
		},
		get userEnabled() {
			return userEnabled;
		},
		connect,
		disconnect,
		toggle
	};
}
