import { DEFAULT_SYSTEM_STATUS, type SystemStatus } from '$lib/types/system/systemStatus';
import type { ScriptStatus } from '$lib/services/api/integrations/MacroApiService';
import { connectionState } from './connectionState';
import { user } from './user';
import { SystemSocketTransport } from './system/socketTransport';
import { parseBinaryPacket } from './system/parsers';
import type { SystemEvent } from './system/types';
import { markAppPerformance, measureAppPerformance } from '$lib/utils/performanceMarks';

export class SystemEventBus {
	private subscribers = new Set<(value: SystemEvent | null) => void>();
	private currentValue: SystemEvent | null = null;

	subscribe(run: (value: SystemEvent | null) => void) {
		this.subscribers.add(run);
		run(this.currentValue);
		return () => this.subscribers.delete(run);
	}

	set(value: SystemEvent | null) {
		this.currentValue = value;
		this.subscribers.forEach((run) => run(value));
	}
}

export class SystemStatusStore {
	private _data = $state<SystemStatus>({ ...DEFAULT_SYSTEM_STATUS });
	private _isWsConnected = $state(false);
	private _macros = $state<ScriptStatus | null>(null);
	private activeSubscriptions = new Map<string, number>();
	// Text snapshots ("system_status", "telemetry", etc.) are cached per channel
	// so newly mounted widgets can hydrate immediately without waiting for the
	// next websocket frame. This is only a frontend cache, not authoritative state.
	private lastSnapshots = new Map<string, unknown>();
	readonly events = new SystemEventBus();
	private readonly transport = new SystemSocketTransport({
		hasValidSession: () => !!user.bearer_token && !!user.isValid,
		onOpen: () => {
			this.replayActiveSubscriptions();
		},
		onConnectionStateChange: (connected) => {
			this._isWsConnected = connected;
		},
		onConnectionHealthy: () => {
			connectionState.setApiHealthy(true);
			connectionState.clearConnectionErrors();
		},
		onConnectionInterrupted: ({ reconnecting, reconnectAttempt }) => {
			connectionState.setApiHealthy(false);
			connectionState.setReconnectAttempt(reconnecting ? reconnectAttempt : 0);
		},
		onGracePeriodExpired: () => {
			connectionState.logConnectionError('System WebSocket Disconnected');
		},
		onMessage: (event) => {
			this.handleSocketMessage(event);
		},
		onResetLiveState: () => {
			this.resetLiveState();
		}
	});

	get data() {
		return this._data;
	}
	get isConnected() {
		return this._isWsConnected;
	}
	get macros() {
		return this._macros;
	}
	get ws() {
		return this.transport.ws;
	}
	get reconnectAttempts() {
		return this.transport.reconnectAttempts;
	}
	set reconnectAttempts(value: number) {
		this.transport.reconnectAttempts = value;
	}

	getSnapshot<T = unknown>(channel: string): T | null {
		return (this.lastSnapshots.get(channel) as T) ?? null;
	}

	subscribeEvents(run: (value: SystemEvent | null) => void) {
		return this.events.subscribe(run);
	}

	private resetLiveState() {
		this._data = { ...DEFAULT_SYSTEM_STATUS };
		this._macros = null;
		this.lastSnapshots.clear();
		this.events.set(null);
	}

	setMacros(status: ScriptStatus | null) {
		this._macros = status;
	}

	connect() {
		this.transport.connect();
	}

	ensureConnected() {
		this.transport.ensureConnected();
	}

	disconnect() {
		this.transport.disconnect();
	}

	destroy() {
		this.transport.destroy();
		this.activeSubscriptions.clear();
		this._isWsConnected = false;
		this.resetLiveState();
	}

	subscribeChannel(channel: string) {
		const currentCount = this.activeSubscriptions.get(channel) ?? 0;
		const nextCount = currentCount + 1;
		this.activeSubscriptions.set(channel, nextCount);
		this.transport.ensureConnected();
		if (currentCount === 0) {
			// Channel ownership stays reference-counted, but snapshot freshness uses
			// the simpler rule below: every subscriber gets one fresh snapshot.
			this.sendChannelMessage('subscribe', channel);
		}

		// A fresh snapshot is cheap compared with chasing stale cache edge-cases
		// across multiple widgets that can mount/unmount independently.
		this.sendChannelMessage('snapshot', channel);
	}

	requestSnapshot(channel: string) {
		this.transport.ensureConnected();
		this.sendChannelMessage('snapshot', channel);
	}

	unsubscribeChannel(channel: string) {
		const currentCount = this.activeSubscriptions.get(channel) ?? 0;
		if (currentCount <= 0) return;

		const nextCount = currentCount - 1;
		if (nextCount === 0) {
			this.activeSubscriptions.delete(channel);
			// Snapshot cache is only meaningful while the channel is actively
			// leased. Once the last consumer leaves, drop it so the next screen does
			// not hydrate from stale state and then skip the backend refresh.
			this.lastSnapshots.delete(channel);
			this.sendChannelMessage('unsubscribe', channel);
		} else {
			this.activeSubscriptions.set(channel, nextCount);
		}

		if (this.activeSubscriptions.size === 0) {
			this.transport.disconnect({ intentional: true, resetLiveState: false });
		}
	}

	private sendChannelMessage(type: 'subscribe' | 'unsubscribe' | 'snapshot', channel: string) {
		this.transport.sendJson({ [type]: channel });
	}

	private replayActiveSubscriptions() {
		// After a socket reconnect the backend has forgotten our previous channel
		// leases, so we must resubscribe every currently used channel from scratch.
		// Reconnect uses the same freshness rule as a normal subscribe: always ask
		// for one fresh snapshot instead of trying to trust the local cache.
		this.activeSubscriptions.forEach((_count, channel) => {
			this.sendChannelMessage('subscribe', channel);
			this.sendChannelMessage('snapshot', channel);
		});
	}

	private handleSocketMessage(event: MessageEvent) {
		if (event.data instanceof ArrayBuffer) {
			parseBinaryPacket(event.data, {
				systemEvents: this.events,
				updateStatus: (updater) => {
					this._data = updater(this._data);
				},
				updateMacros: (status) => {
					this._macros = status;
				}
			});
			return;
		}

		if (typeof event.data === 'string') {
			this.handleTextFrame(event.data);
		}
	}

	private handleTextFrame(frame: string) {
		try {
			const msg = JSON.parse(frame);
			if (msg.type === 'snapshot' && typeof msg.channel === 'string') {
				if (
					msg.channel === 'system_status' &&
					markAppPerformance('matrixhub:first-system-status', { once: true })
				) {
					measureAppPerformance(
						'matrixhub:boot-to-first-system-status',
						'matrixhub:boot:start',
						'matrixhub:first-system-status'
					);
				}

				// One incoming snapshot fans out in two directions:
				// 1. cache it for late subscribers via getSnapshot()
				// 2. publish an event so already-mounted hooks update immediately
				this.lastSnapshots.set(msg.channel, msg.data);
				this.events.set({ type: 'snapshot', channel: msg.channel, data: msg.data });
				return;
			}

			if (msg.type === 'event' || msg.type === 'sensor') {
				this.events.set(msg);
				return;
			}

			if (msg.type === 'macros') {
				this._macros = msg.data;
			}
		} catch {
			// Ignore parse errors from malformed frames.
		}
	}
}

export const systemStatus = new SystemStatusStore();
export const systemEvents = systemStatus.events;
