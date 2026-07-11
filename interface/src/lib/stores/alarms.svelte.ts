import { createSystemChannelSubscription } from './system/channelSubscription.svelte';
import type { SystemEvent, AlarmEventData } from './system/types';
import type { AlarmRule, AlarmRulesConfig } from '$lib/types/domain/alarms';

interface AlarmsSnapshotStore {
	subscribeChannel(channel: string): void;
	unsubscribeChannel(channel: string): void;
	getSnapshot<TSnapshot>(channel: string): TSnapshot | null;
	requestSnapshot?(channel: string): void;
	subscribeEvents?(run: (value: SystemEvent | null) => void): () => void;
}

interface AlarmsEventsBus {
	subscribe(run: (value: SystemEvent | null) => void): () => void;
}

interface AlarmsStoreDeps {
	statusStore?: AlarmsSnapshotStore;
	eventBus?: AlarmsEventsBus;
}

export interface AlarmTransitionConflict {
	id: string;
	transitionSeq: number;
	bootId: string | undefined;
	currentTriggered: boolean | undefined;
	incomingTriggered: boolean;
	source: 'event' | 'snapshot';
}

const MAX_UINT32 = 0xffffffff;
const BOOLEAN_ALARM_SOURCES = new Set(['wifi_csi_motion', 'imu_tamper', 'gpio_digital']);

function validTransitionSeq(value: unknown): value is number {
	return Number.isInteger(value) && Number(value) >= 0 && Number(value) <= MAX_UINT32;
}

function validBootId(value: unknown): value is string {
	return typeof value === 'string' && /^[0-9a-f]{16}$/.test(value) && value !== '0000000000000000';
}

function isNewerTransitionSeq(candidate: number, current: number) {
	if (candidate === current || candidate === 0) return false;
	if (current === 0) return true;
	return (candidate - current) >>> 0 < 0x80000000;
}

function runtimeStatusFrom(rule: AlarmRule) {
	return {
		triggered: rule.triggered,
		last_triggered: rule.last_triggered,
		current_value: rule.current_value,
		transition_seq: rule.transition_seq,
		device_millis: rule.device_millis,
		boot_id: rule.boot_id
	};
}

function hasSameRuntimeIdentity(current: AlarmRule, incoming: AlarmRule) {
	if (!current.enabled || !incoming.enabled || current.id !== incoming.id) return false;
	if (current.source !== incoming.source) return false;
	if (current.source.startsWith('ble_') && current.ble_device_mac !== incoming.ble_device_mac) {
		return false;
	}
	if (current.source === 'gpio_digital' && current.gpio_id !== incoming.gpio_id) return false;
	if (BOOLEAN_ALARM_SOURCES.has(current.source)) return true;
	return (
		Number.isFinite(current.threshold) &&
		Number.isFinite(incoming.threshold) &&
		current.operator === incoming.operator &&
		current.threshold === incoming.threshold
	);
}

export class AlarmsStore {
	private _rules = $state<AlarmRule[]>([]);
	private _loading = $state(true);
	private _errorMessage = $state<string | null>(null);
	private _transitionConflict = $state<AlarmTransitionConflict | null>(null);
	private _subscriptionCount = 0;
	private _subscription: ReturnType<
		typeof createSystemChannelSubscription<AlarmRulesConfig>
	> | null = null;
	private readonly statusStore?: AlarmsSnapshotStore;
	private readonly eventBus?: AlarmsEventsBus;

	constructor(deps: AlarmsStoreDeps = {}) {
		this.statusStore = deps.statusStore;
		this.eventBus = deps.eventBus;
	}

	start() {
		this._subscriptionCount++;
		if (this._subscriptionCount === 1) {
			this._subscription = createSystemChannelSubscription<AlarmRulesConfig>({
				channel: 'alarms',
				onSnapshot: (snapshot) => this.applySnapshot(snapshot),
				onEvent: (event) => this.applyEvent(event),
				onReset: () => this.clearTransitionOrdering(),
				systemStatusStore: this.statusStore,
				systemEventsBus: this.eventBus
			});
			return this._subscription.subscribe();
		}
		return this._rules.length > 0;
	}

	stop() {
		if (this._subscriptionCount > 0) {
			this._subscriptionCount--;
		}

		if (this._subscriptionCount === 0 && this._subscription) {
			this.reset();
		}
	}

	get rules() {
		return this._rules;
	}

	get loading() {
		return this._loading;
	}

	get errorMessage() {
		return this._errorMessage;
	}

	get transitionConflict() {
		return this._transitionConflict;
	}

	refresh() {
		this._subscription?.refresh();
	}

	applySnapshot(snapshot: AlarmRulesConfig) {
		const currentById = new Map(this._rules.map((rule) => [rule.id, rule]));
		this._rules = (snapshot.rules ?? []).map((incoming) => {
			const current = currentById.get(incoming.id);
			if (!current) return incoming;

			const currentSeq = current.transition_seq;
			const incomingSeq = incoming.transition_seq;
			const currentBootId = current.boot_id;
			const incomingBootId = incoming.boot_id;
			const currentHasBootId = validBootId(currentBootId);
			const incomingHasBootId = validBootId(incomingBootId);
			if (incomingHasBootId && (!currentHasBootId || incomingBootId !== currentBootId)) {
				// A fresh snapshot is authoritative for switching boot epochs.
				// Events are deliberately not allowed to perform this switch.
				return incoming;
			}
			if (!hasSameRuntimeIdentity(current, incoming)) {
				// Firmware resets runtime observability when trigger semantics (or
				// enabled state) changes. Accept that new epoch even when a legacy
				// snapshot has no ordering suffix or sequence zero is numerically old.
				return incoming;
			}
			if (currentHasBootId && !incomingHasBootId) {
				return { ...incoming, ...runtimeStatusFrom(current) };
			}
			if (validTransitionSeq(currentSeq) && !validTransitionSeq(incomingSeq)) {
				// A delayed legacy-shaped snapshot cannot roll back runtime data
				// that already has an ordering proof. Its configuration fields are
				// still useful, so merge those with the sequenced live status.
				return { ...incoming, ...runtimeStatusFrom(current) };
			}
			if (!validTransitionSeq(currentSeq)) {
				// Two legacy states keep last-frame-wins behavior. A sequenced
				// incoming snapshot also establishes the first ordering baseline.
				return incoming;
			}
			if (!validTransitionSeq(incomingSeq)) return incoming;

			if (incomingSeq === currentSeq) {
				if (
					current.triggered !== undefined &&
					incoming.triggered !== undefined &&
					current.triggered !== incoming.triggered
				) {
					this.recordTransitionConflict(current, incoming.triggered, incomingSeq, 'snapshot');
					return { ...incoming, ...runtimeStatusFrom(current) };
				}
				return {
					...incoming,
					current_value: incoming.current_value ?? current.current_value,
					last_triggered: incoming.last_triggered ?? current.last_triggered,
					device_millis: incoming.device_millis ?? current.device_millis
				};
			}

			if (isNewerTransitionSeq(incomingSeq, currentSeq)) {
				return incoming;
			}

			// The snapshot may have been captured before a live event whose queue
			// delivery won the race. Refresh configuration fields, but never roll
			// the displayed runtime state back to an older sequence.
			return { ...incoming, ...runtimeStatusFrom(current) };
		});
		this._loading = false;
		this._errorMessage = null;
	}

	applyEvent(event: SystemEvent | null) {
		if (event?.type !== 'alarm') return;
		this.updateRuleFromEvent(event.data);
	}

	setRules(rules: AlarmRule[]) {
		this._rules = rules;
		this._loading = false;
		this._errorMessage = null;
	}

	clearRules() {
		this._rules = [];
		this._loading = false;
	}

	setLoading(value: boolean) {
		this._loading = value;
	}

	setError(message: string | null) {
		this._errorMessage = message;
		if (message) {
			this._loading = false;
		}
	}

	reset() {
		this._subscription?.destroy();
		this._subscription = null;
		this._subscriptionCount = 0;
		this._rules = [];
		this._loading = true;
		this._errorMessage = null;
		this._transitionConflict = null;
	}

	private updateRuleFromEvent(data: AlarmEventData) {
		if (!data.id || !Number.isFinite(data.current_value)) return;

		this._rules = this._rules.map((rule) => {
			if (rule.id !== data.id) {
				return rule;
			}

			const currentSeq = rule.transition_seq;
			const incomingSeq = data.transition_seq;
			const currentBootId = rule.boot_id;
			const incomingBootId = data.boot_id;
			const currentHasBootId = validBootId(currentBootId);
			const incomingHasBootId = validBootId(incomingBootId);
			if (currentHasBootId) {
				if (!incomingHasBootId || incomingBootId !== currentBootId) {
					// Only an authoritative snapshot can switch boot epochs. This also
					// drops delayed events from the previous boot after reconnect.
					return rule;
				}
			} else if (incomingHasBootId) {
				return this.applyEventRuntime(rule, data);
			}
			if (validTransitionSeq(currentSeq)) {
				if (!validTransitionSeq(incomingSeq)) {
					// Once a sequenced baseline exists, an unordered legacy event is
					// not allowed to mutate runtime display state.
					return rule;
				}

				if (incomingSeq === currentSeq) {
					if (rule.triggered !== undefined && rule.triggered !== data.triggered) {
						this.recordTransitionConflict(rule, data.triggered, incomingSeq, 'event');
						return rule;
					}

					return {
						...rule,
						current_value: data.current_value,
						device_millis: data.device_millis ?? rule.device_millis
					};
				}

				if (!isNewerTransitionSeq(incomingSeq, currentSeq)) {
					return rule;
				}
			}

			return this.applyEventRuntime(rule, data);
		});
	}

	private applyEventRuntime(rule: AlarmRule, data: AlarmEventData): AlarmRule {
		return {
			...rule,
			triggered: data.triggered,
			current_value: data.current_value,
			transition_seq: data.transition_seq ?? rule.transition_seq,
			device_millis: data.device_millis ?? rule.device_millis,
			boot_id: validBootId(data.boot_id) ? data.boot_id : rule.boot_id
		};
	}

	private clearTransitionOrdering() {
		this._rules = this._rules.map((rule) => ({
			...rule,
			transition_seq: undefined,
			device_millis: undefined,
			boot_id: undefined
		}));
		this._transitionConflict = null;
	}

	private recordTransitionConflict(
		current: AlarmRule,
		incomingTriggered: boolean,
		transitionSeq: number,
		source: AlarmTransitionConflict['source']
	) {
		this._transitionConflict = {
			id: current.id,
			transitionSeq,
			bootId: current.boot_id,
			currentTriggered: current.triggered,
			incomingTriggered,
			source
		};
	}
}

export const alarmsStore = new AlarmsStore();
