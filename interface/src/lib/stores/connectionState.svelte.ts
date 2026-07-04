import { Logger } from '$lib/services/core/Logger';

export type ConnectionStatus = 'connected' | 'connecting' | 'disconnected' | 'error';

export class ConnectionStateStore {
	// Internal state
	private _apiHealthy = $state(true);
	private _reconnectAttempt = $state(0);
	private _lastError = $state<string | null>(null);
	private _lastErrorTime = $state<number | null>(null);
	private _errorCount = $state(0);

	private _lastLogTime = 0;
	private readonly LOG_THROTTLE_MS = 10000;
	private _errorTimeout: ReturnType<typeof setTimeout> | null = null;

	// Derived status
	status = $derived.by<ConnectionStatus>(() => {
		if (!this._apiHealthy && this._reconnectAttempt > 0) {
			return 'connecting';
		} else if (!this._apiHealthy) {
			return 'disconnected';
		} else if (this._lastError) {
			return 'error';
		}
		return 'connected';
	});

	// Getters for public access
	get apiHealthy() {
		return this._apiHealthy;
	}
	get reconnectAttempt() {
		return this._reconnectAttempt;
	}
	get lastError() {
		return this._lastError;
	}
	get lastErrorTime() {
		return this._lastErrorTime;
	}
	get errorCount() {
		return this._errorCount;
	}

	// Setters and Actions
	setApiHealthy(healthy: boolean) {
		this._apiHealthy = healthy;
	}

	setReconnectAttempt(attempt: number) {
		this._reconnectAttempt = attempt;
	}

	logConnectionError(message: string, error?: unknown) {
		const now = Date.now();

		this._lastError = message;
		this._lastErrorTime = now;
		this._errorCount++;

		if (now - this._lastLogTime >= this.LOG_THROTTLE_MS) {
			Logger.warn(`[Connection] ${message}`, error || '');
			this._lastLogTime = now;
		}

		// Auto-clear error status after 10s so UI returns to normal
		if (this._errorTimeout) clearTimeout(this._errorTimeout);
		this._errorTimeout = setTimeout(() => {
			this._lastError = null;
			this._lastErrorTime = null;
			this._errorTimeout = null;
		}, 10000);
	}

	clearConnectionErrors() {
		if (this._errorTimeout) {
			clearTimeout(this._errorTimeout);
			this._errorTimeout = null;
		}
		this._lastError = null;
		this._lastErrorTime = null;
		this._errorCount = 0;
		this._reconnectAttempt = 0;
	}
}

export const connectionState = new ConnectionStateStore();
