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

export class AlarmsStore {
	private _rules = $state<AlarmRule[]>([]);
	private _loading = $state(true);
	private _errorMessage = $state<string | null>(null);
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

	refresh() {
		this._subscription?.refresh();
	}

	applySnapshot(snapshot: AlarmRulesConfig) {
		this._rules = snapshot.rules ?? [];
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
	}

	private updateRuleFromEvent(data: AlarmEventData) {
		if (!data.id || Number.isNaN(data.current_value)) return;

		this._rules = this._rules.map((rule) => {
			if (rule.id !== data.id) {
				return rule;
			}

			return {
				...rule,
				triggered: data.triggered,
				current_value: data.current_value
			};
		});
	}
}

export const alarmsStore = new AlarmsStore();
