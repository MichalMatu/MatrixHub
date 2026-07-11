import { beforeEach, describe, expect, it } from 'vitest';
import type { AlarmRule } from '$lib/types/domain/alarms';
import { AlarmsStore } from './alarms.svelte';

function makeRule(overrides: Partial<AlarmRule> = {}): AlarmRule {
	return {
		id: 'alarm-1',
		enabled: true,
		name: 'Alarm one',
		source: 'wifi_csi_motion',
		operator: 'above',
		threshold: 0.5,
		severity: 'warning',
		notify_channels: [],
		cooldown_seconds: 60,
		triggered: false,
		current_value: 0,
		...overrides
	};
}

function alarmEvent(
	overrides: Partial<{
		id: string;
		triggered: boolean;
		current_value: number;
		severity: number;
		transition_seq: number;
		device_millis: number;
		boot_id: string;
	}> = {}
) {
	return {
		type: 'alarm' as const,
		data: {
			id: 'alarm-1',
			triggered: true,
			current_value: 1,
			severity: 1,
			...overrides
		}
	};
}

describe('AlarmsStore transition ordering', () => {
	let store: AlarmsStore;

	beforeEach(() => {
		store = new AlarmsStore();
	});

	it('accepts newer events and rejects stale events without regressing display state', () => {
		store.setRules([
			makeRule({
				triggered: false,
				transition_seq: 4,
				device_millis: 400,
				boot_id: '0000000000000002',
				current_value: 0
			})
		]);

		store.applyEvent(
			alarmEvent({ transition_seq: 5, device_millis: 500, boot_id: '0000000000000002' })
		);
		expect(store.rules[0]).toMatchObject({
			triggered: true,
			current_value: 1,
			transition_seq: 5,
			device_millis: 500,
			boot_id: '0000000000000002'
		});

		store.applyEvent(
			alarmEvent({
				triggered: false,
				current_value: 0,
				transition_seq: 4,
				device_millis: 450,
				boot_id: '0000000000000002'
			})
		);
		expect(store.rules[0]).toMatchObject({
			triggered: true,
			current_value: 1,
			transition_seq: 5,
			device_millis: 500,
			boot_id: '0000000000000002'
		});
	});

	it('latches an equal-sequence state conflict and preserves the displayed state', () => {
		store.setRules([
			makeRule({
				triggered: true,
				transition_seq: 9,
				device_millis: 900,
				boot_id: '0000000000000003',
				current_value: 1
			})
		]);

		store.applyEvent(
			alarmEvent({
				triggered: false,
				current_value: 0,
				transition_seq: 9,
				device_millis: 901,
				boot_id: '0000000000000003'
			})
		);

		expect(store.rules[0]).toMatchObject({
			triggered: true,
			current_value: 1,
			transition_seq: 9,
			device_millis: 900,
			boot_id: '0000000000000003'
		});
		expect(store.transitionConflict).toEqual({
			id: 'alarm-1',
			transitionSeq: 9,
			bootId: '0000000000000003',
			currentTriggered: true,
			incomingTriggered: false,
			source: 'event'
		});
	});

	it('accepts a reboot snapshot baseline and rejects a delayed prior-boot event', () => {
		store.setRules([
			makeRule({
				triggered: true,
				transition_seq: 9,
				device_millis: 900,
				boot_id: '0000000000000005',
				current_value: 1
			})
		]);

		store.applySnapshot({
			schema_version: 1,
			rules: [
				makeRule({
					triggered: false,
					transition_seq: 0,
					device_millis: 0,
					boot_id: '0000000000000006',
					current_value: 0
				})
			]
		});
		expect(store.rules[0]).toMatchObject({
			triggered: false,
			transition_seq: 0,
			device_millis: 0,
			boot_id: '0000000000000006'
		});

		store.applyEvent(
			alarmEvent({
				triggered: true,
				transition_seq: 10,
				device_millis: 950,
				boot_id: '0000000000000005'
			})
		);
		expect(store.rules[0]).toMatchObject({
			triggered: false,
			transition_seq: 0,
			boot_id: '0000000000000006'
		});
	});

	it('merges stale snapshot configuration without rolling back newer runtime state', () => {
		store.setRules([
			makeRule({ triggered: true, transition_seq: 7, device_millis: 700, current_value: 1 })
		]);

		store.applySnapshot({
			schema_version: 1,
			rules: [
				makeRule({
					name: 'Renamed alarm',
					triggered: false,
					transition_seq: 6,
					device_millis: 600,
					current_value: 0
				})
			]
		});

		expect(store.rules[0]).toMatchObject({
			name: 'Renamed alarm',
			triggered: true,
			current_value: 1,
			transition_seq: 7,
			device_millis: 700
		});
	});

	it('detects an equal-sequence snapshot conflict while keeping its configuration update', () => {
		store.setRules([
			makeRule({ triggered: true, transition_seq: 11, device_millis: 1100, current_value: 1 })
		]);

		store.applySnapshot({
			schema_version: 1,
			rules: [
				makeRule({
					name: 'Snapshot rename',
					triggered: false,
					transition_seq: 11,
					device_millis: 1100,
					current_value: 0
				})
			]
		});

		expect(store.rules[0]).toMatchObject({
			name: 'Snapshot rename',
			triggered: true,
			current_value: 1,
			transition_seq: 11
		});
		expect(store.transitionConflict).toMatchObject({
			id: 'alarm-1',
			transitionSeq: 11,
			incomingTriggered: false,
			source: 'snapshot'
		});
	});

	it('accepts a sequence reset when alarm trigger semantics change', () => {
		store.setRules([
			makeRule({
				source: 'temperature',
				threshold: 30,
				triggered: true,
				transition_seq: 14,
				device_millis: 1400,
				current_value: 31
			})
		]);

		store.applySnapshot({
			schema_version: 1,
			rules: [
				makeRule({
					source: 'temperature',
					threshold: 35,
					triggered: false,
					transition_seq: 0,
					device_millis: 0,
					current_value: 31
				})
			]
		});

		expect(store.rules[0]).toMatchObject({
			threshold: 35,
			triggered: false,
			transition_seq: 0,
			device_millis: 0
		});
	});

	it('accepts a metadata-less reset when alarm trigger semantics change', () => {
		store.setRules([
			makeRule({
				source: 'temperature',
				threshold: 30,
				triggered: true,
				transition_seq: 14,
				device_millis: 1400,
				boot_id: '0000000000000004',
				current_value: 31
			})
		]);

		store.applySnapshot({
			schema_version: 1,
			rules: [
				makeRule({
					source: 'temperature',
					threshold: 35,
					triggered: false,
					current_value: 31
				})
			]
		});

		expect(store.rules[0]).toMatchObject({
			threshold: 35,
			triggered: false
		});
		expect(store.rules[0]?.transition_seq).toBeUndefined();
		expect(store.rules[0]?.boot_id).toBeUndefined();
	});

	it('accepts a same-boot reset when a GPIO selector changes remotely', () => {
		store.setRules([
			makeRule({
				source: 'gpio_digital',
				gpio_id: 'gpio1',
				triggered: true,
				transition_seq: 6,
				device_millis: 600,
				boot_id: '0000000000000004',
				current_value: 1
			})
		]);

		store.applySnapshot({
			schema_version: 1,
			rules: [
				makeRule({
					source: 'gpio_digital',
					gpio_id: 'gpio2',
					triggered: false,
					transition_seq: 0,
					device_millis: 0,
					boot_id: '0000000000000004',
					current_value: 0
				})
			]
		});

		expect(store.rules[0]).toMatchObject({
			gpio_id: 'gpio2',
			triggered: false,
			transition_seq: 0,
			device_millis: 0,
			boot_id: '0000000000000004'
		});
	});

	it('does not let metadata-less legacy frames overwrite a sequenced baseline', () => {
		store.setRules([
			makeRule({
				source: 'temperature',
				threshold: 30,
				triggered: true,
				transition_seq: 3,
				device_millis: 300,
				current_value: 31
			})
		]);

		store.applyEvent(alarmEvent({ triggered: false, current_value: 0 }));
		expect(store.rules[0]).toMatchObject({
			triggered: true,
			transition_seq: 3,
			device_millis: 300
		});

		store.applySnapshot({
			schema_version: 1,
			rules: [
				makeRule({
					name: 'Legacy config rename',
					source: 'temperature',
					threshold: 30,
					triggered: false,
					current_value: 31
				})
			]
		});
		expect(store.rules[0]).toMatchObject({
			name: 'Legacy config rename',
			threshold: 30,
			triggered: true,
			current_value: 31,
			transition_seq: 3,
			device_millis: 300
		});
	});

	it('keeps legacy last-frame-wins behavior and accepts the first sequenced baseline', () => {
		store.setRules([makeRule({ triggered: false })]);

		store.applyEvent(alarmEvent());
		expect(store.rules[0]).toMatchObject({ triggered: true, current_value: 1 });
		expect(store.rules[0]?.transition_seq).toBeUndefined();

		store.applyEvent(
			alarmEvent({
				triggered: false,
				current_value: 0,
				transition_seq: 1,
				device_millis: 100
			})
		);
		expect(store.rules[0]).toMatchObject({
			triggered: false,
			transition_seq: 1,
			device_millis: 100
		});
	});

	it('accepts the reserved wrap transition from UINT32_MAX to one', () => {
		store.setRules([
			makeRule({ triggered: false, transition_seq: 0xffffffff, device_millis: 100 })
		]);

		store.applyEvent(alarmEvent({ transition_seq: 1, device_millis: 200 }));

		expect(store.rules[0]).toMatchObject({
			triggered: true,
			transition_seq: 1,
			device_millis: 200
		});
	});

	it('clears cached ordering metadata when the websocket live state resets', () => {
		let emit: (value: null) => void = () => {};
		store = new AlarmsStore({
			statusStore: {
				subscribeChannel: () => {},
				unsubscribeChannel: () => {},
				getSnapshot: () => null,
				subscribeEvents: (run) => {
					emit = run as (value: null) => void;
					return () => {};
				}
			}
		});
		store.setRules([
			makeRule({
				transition_seq: 4,
				device_millis: 400,
				boot_id: '0000000000000008',
				triggered: true
			})
		]);
		store.start();

		emit(null);

		expect(store.rules[0]?.triggered).toBe(true);
		expect(store.rules[0]?.transition_seq).toBeUndefined();
		expect(store.rules[0]?.device_millis).toBeUndefined();
		expect(store.rules[0]?.boot_id).toBeUndefined();
	});
});
