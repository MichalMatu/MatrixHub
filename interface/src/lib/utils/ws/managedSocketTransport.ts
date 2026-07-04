interface ManagedSocketCloseBaseContext {
	readonly socket: WebSocket;
	readonly intentional: boolean;
	readonly visible: boolean;
}

export interface ManagedSocketCloseContext extends ManagedSocketCloseBaseContext {
	readonly willReconnect: boolean;
	readonly nextReconnectAttempt: number;
}

type ReconnectDelayResolver = number | ((attempt: number) => number | null);

interface ManagedSocketTransportOptions {
	getUrl: () => string;
	canConnect?: () => boolean;
	reconnectDelayMs?: ReconnectDelayResolver;
	connectTimeoutMs?: number;
	binaryType?: BinaryType;
	closeSocket?: (socket: WebSocket) => void;
	onBeforeConnect?: () => void;
	onOpen?: (socket: WebSocket) => void;
	onMessage?: (event: MessageEvent, socket: WebSocket) => void;
	onError?: (event: Event, socket: WebSocket) => void;
	onClose?: (context: ManagedSocketCloseContext) => void;
	onConnectTimeout?: (socket: WebSocket) => void;
	shouldReconnect?: (context: ManagedSocketCloseBaseContext) => boolean;
}

export class ManagedSocketTransport {
	private socket: WebSocket | null = null;
	private reconnectTimer: ReturnType<typeof setTimeout> | null = null;
	private connectTimer: ReturnType<typeof setTimeout> | null = null;
	private intentionalDisconnect = false;
	private visible = true;
	private reconnectAttemptCount = 0;

	constructor(private readonly options: ManagedSocketTransportOptions) {}

	get ws() {
		return this.socket;
	}

	get reconnectAttempts() {
		return this.reconnectAttemptCount;
	}

	set reconnectAttempts(value: number) {
		this.reconnectAttemptCount = Math.max(0, value);
	}

	setVisible(visible: boolean) {
		this.visible = visible;
		if (!visible) {
			this.clearReconnectTimer();
		}
	}

	connect() {
		if (typeof window === 'undefined') return;

		this.clearReconnectTimer();
		this.clearConnectTimer();

		if (!this.visible || !this.canConnect()) return;

		if (
			this.socket &&
			(this.socket.readyState === WebSocket.OPEN || this.socket.readyState === WebSocket.CONNECTING)
		) {
			return;
		}

		if (this.socket) {
			this.detachSocket(this.socket);
			this.socket = null;
		}

		this.options.onBeforeConnect?.();
		const socket = new WebSocket(this.options.getUrl());
		if (this.options.binaryType) {
			socket.binaryType = this.options.binaryType;
		}

		this.socket = socket;
		this.intentionalDisconnect = false;

		if (this.options.connectTimeoutMs) {
			this.connectTimer = setTimeout(() => {
				if (this.socket === socket && socket.readyState === WebSocket.CONNECTING) {
					this.options.onConnectTimeout?.(socket);
					socket.close();
				}
			}, this.options.connectTimeoutMs);
		}

		socket.onopen = () => {
			if (this.socket !== socket) return;

			this.clearReconnectTimer();
			this.clearConnectTimer();
			this.reconnectAttemptCount = 0;
			this.options.onOpen?.(socket);
		};

		socket.onmessage = (event) => {
			if (this.socket !== socket) return;
			this.options.onMessage?.(event, socket);
		};

		socket.onerror = (event) => {
			if (this.socket !== socket) return;
			this.options.onError?.(event, socket);
		};

		socket.onclose = () => {
			if (this.socket !== socket) return;

			this.socket = null;
			this.clearConnectTimer();
			const baseContext = {
				socket,
				intentional: this.intentionalDisconnect,
				visible: this.visible
			};
			const shouldReconnect = this.options.shouldReconnect?.(baseContext) ?? false;
			const context = {
				...baseContext,
				willReconnect: shouldReconnect,
				nextReconnectAttempt: shouldReconnect ? this.reconnectAttemptCount + 1 : 0
			} satisfies ManagedSocketCloseContext;

			this.options.onClose?.(context);
			this.intentionalDisconnect = false;

			if (shouldReconnect) {
				this.scheduleReconnect();
			}
		};
	}

	disconnect(intentional = true) {
		this.intentionalDisconnect = intentional;
		if (intentional) {
			this.reconnectAttemptCount = 0;
		}
		this.clearReconnectTimer();
		this.clearConnectTimer();

		if (!this.socket) return;

		const socket = this.socket;
		this.socket = null;
		this.detachSocket(socket);

		if (this.options.closeSocket) {
			this.options.closeSocket(socket);
			return;
		}

		if (socket.readyState === WebSocket.OPEN || socket.readyState === WebSocket.CONNECTING) {
			socket.close();
		}
	}

	destroy() {
		this.disconnect(true);
	}

	private canConnect() {
		return this.options.canConnect?.() ?? true;
	}

	private scheduleReconnect() {
		if (this.reconnectTimer || !this.visible || !this.canConnect()) {
			return;
		}

		const nextAttempt = this.reconnectAttemptCount + 1;
		const delay = this.resolveReconnectDelay(nextAttempt);
		if (delay === null || delay === undefined || delay <= 0) {
			return;
		}

		this.reconnectAttemptCount = nextAttempt;
		this.reconnectTimer = setTimeout(() => {
			this.reconnectTimer = null;
			this.connect();
		}, delay);
	}

	private resolveReconnectDelay(attempt: number) {
		const { reconnectDelayMs } = this.options;
		if (typeof reconnectDelayMs === 'function') {
			return reconnectDelayMs(attempt);
		}
		return reconnectDelayMs ?? null;
	}

	private clearReconnectTimer() {
		if (!this.reconnectTimer) return;
		clearTimeout(this.reconnectTimer);
		this.reconnectTimer = null;
	}

	private clearConnectTimer() {
		if (!this.connectTimer) return;
		clearTimeout(this.connectTimer);
		this.connectTimer = null;
	}

	private detachSocket(socket: WebSocket) {
		socket.onopen = null;
		socket.onclose = null;
		socket.onmessage = null;
		socket.onerror = null;
	}
}
