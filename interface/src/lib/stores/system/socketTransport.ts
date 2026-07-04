import { ManagedSocketTransport } from '$lib/utils/ws/managedSocketTransport';
import { SocketWatchdog } from '$lib/utils/ws/socketWatchdog';
import { buildWebSocketUrl } from '$lib/utils/ws/buildWebSocketUrl';

interface SystemSocketTransportOptions {
	hasValidSession: () => boolean;
	onOpen?: (socket: WebSocket) => void;
	onConnectionStateChange: (connected: boolean) => void;
	onConnectionHealthy: () => void;
	onConnectionInterrupted: (context: { reconnecting: boolean; reconnectAttempt: number }) => void;
	onGracePeriodExpired: () => void;
	onMessage: (event: MessageEvent) => void;
	onResetLiveState: () => void;
}

export class SystemSocketTransport {
	private gracePeriodTimer: ReturnType<typeof setTimeout> | null = null;
	private connected = false;

	private readonly INITIAL_RECONNECT_DELAY_MS = 1000;
	private readonly MAX_RECONNECT_DELAY_MS = 60000;
	private readonly CONNECT_TIMEOUT_MS = 10000;
	private readonly WATCHDOG_TIMEOUT_MS = 30000;
	private readonly watchdog = new SocketWatchdog({
		timeoutMs: this.WATCHDOG_TIMEOUT_MS,
		onTimeout: () => {
			const socket = this.transport.ws;
			if (!socket || socket.readyState !== WebSocket.OPEN) return;
			console.warn('[System] WebSocket watchdog timeout, closing dead socket.');
			socket.close();
		}
	});
	private readonly transport = new ManagedSocketTransport({
		getUrl: () => this.buildSocketUrl(),
		canConnect: () => this.hasAccess(),
		reconnectDelayMs: (attempt) => this.getReconnectDelayMs(attempt),
		connectTimeoutMs: this.CONNECT_TIMEOUT_MS,
		binaryType: 'arraybuffer',
		onOpen: (socket) => {
			this.clearGracePeriodTimer();
			this.setConnected(true);
			this.options.onConnectionHealthy();
			this.armWatchdog(socket);
			this.options.onOpen?.(socket);
		},
		onMessage: (event, socket) => {
			this.armWatchdog(socket);
			this.options.onMessage(event);
		},
		onError: (event) => {
			console.error('[System] WS Error', event);
		},
		onConnectTimeout: () => {
			console.warn('[System] WebSocket connect timeout, aborting stalled handshake.');
		},
		onClose: ({ intentional, willReconnect, nextReconnectAttempt }) => {
			this.setConnected(false);
			this.watchdog.clear();
			this.clearGracePeriodTimer();

			if (!intentional) {
				this.options.onConnectionInterrupted({
					reconnecting: willReconnect,
					reconnectAttempt: willReconnect ? nextReconnectAttempt : 0
				});
				this.gracePeriodTimer = setTimeout(() => {
					this.gracePeriodTimer = null;
					if (!this.connected) {
						console.error('[System] Grace period (10s) expired without reconnect.');
						this.options.onResetLiveState();
						this.options.onGracePeriodExpired();
					} else {
						console.info('[System] Grace period recovery successful.');
					}
				}, 10000);
			}
		},
		shouldReconnect: ({ intentional }) => !intentional && this.hasAccess()
	});
	constructor(private readonly options: SystemSocketTransportOptions) {}

	get ws() {
		return this.transport.ws;
	}

	get reconnectAttempts() {
		return this.transport.reconnectAttempts;
	}

	set reconnectAttempts(value: number) {
		this.transport.reconnectAttempts = value;
	}

	connect() {
		this.clearGracePeriodTimer();
		this.transport.connect();
	}

	ensureConnected() {
		if (typeof window === 'undefined') return;
		if (!this.hasAccess()) return;

		const socket = this.transport.ws;
		if (!socket) {
			this.connect();
			return;
		}

		if (socket.readyState > WebSocket.OPEN) {
			// Replacing a dead socket should keep the last live UI state until the
			// normal reconnect/grace-period flow decides whether the data is stale.
			this.disconnect({ resetLiveState: false });
			this.connect();
		}
	}

	disconnect({
		intentional = true,
		resetLiveState = true
	}: { intentional?: boolean; resetLiveState?: boolean } = {}) {
		this.clearGracePeriodTimer();
		this.watchdog.clear();
		this.transport.disconnect(intentional);
		this.setConnected(false);
		if (resetLiveState) {
			this.options.onResetLiveState();
		}
	}

	destroy() {
		this.clearGracePeriodTimer();
		this.watchdog.clear();
		this.transport.destroy();
	}

	sendJson(message: Record<string, string>) {
		const socket = this.transport.ws;
		if (socket && socket.readyState === WebSocket.OPEN) {
			socket.send(JSON.stringify(message));
		}
	}

	private hasAccess() {
		return this.options.hasValidSession();
	}

	private buildSocketUrl() {
		return buildWebSocketUrl('/ws/system');
	}

	private setConnected(connected: boolean) {
		this.connected = connected;
		this.options.onConnectionStateChange(connected);
	}

	private clearGracePeriodTimer() {
		if (!this.gracePeriodTimer) return;
		clearTimeout(this.gracePeriodTimer);
		this.gracePeriodTimer = null;
	}

	private getReconnectDelayMs(attempt: number) {
		return Math.min(
			this.INITIAL_RECONNECT_DELAY_MS * 2 ** Math.max(0, attempt - 1),
			this.MAX_RECONNECT_DELAY_MS
		);
	}

	private armWatchdog(socket: WebSocket) {
		if (this.transport.ws !== socket || socket.readyState !== WebSocket.OPEN) {
			return;
		}

		this.watchdog.arm(true);
	}
}
