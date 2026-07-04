import type {
	AlarmOperator,
	AlarmRule,
	AlarmSource,
	NotifyChannel
} from '$lib/types/domain/alarms';
import {
	ALARM_SOURCES,
	DEFAULT_ALARM_RULE,
	MAX_ALARM_NAME_LENGTH,
	generateId
} from '$lib/types/domain/alarms';
import { getThresholdConfig } from './alarmThresholdConfig';

type AlarmRuleDraft = Omit<AlarmRule, 'id' | 'created_at' | 'updated_at'>;

type AlarmRuleFormDeps = {
	now?: () => number;
	createId?: () => string;
};

function createNewDraft(): AlarmRuleDraft {
	return {
		// This draft inherits the shared default channel set from DEFAULT_ALARM_RULE.
		// Today that is LED-first on purpose so new rules work locally out of the box.
		...DEFAULT_ALARM_RULE,
		enabled: true,
		shelly_device_ids: [],
		ble_device_mac: '',
		gpio_id: ''
	};
}

function createDraftFromRule(rule: AlarmRule): AlarmRuleDraft {
	return {
		enabled: rule.enabled,
		name: rule.name,
		source: rule.source,
		operator: rule.operator,
		threshold: rule.threshold,
		severity: rule.severity,
		cooldown_seconds: rule.cooldown_seconds,
		notify_channels: [...rule.notify_channels],
		shelly_device_ids: [...(rule.shelly_device_ids ?? [])],
		ble_device_mac: rule.ble_device_mac ?? '',
		gpio_id: rule.gpio_id ?? ''
	};
}

function getSourceShortName(source: AlarmSource): string {
	switch (source) {
		case 'co2':
			return 'CO₂';
		case 'temperature':
			return 'Temp';
		case 'humidity':
			return 'RH';
		case 'wifi_motion':
			return 'Motion';
		case 'wifi_csi_motion':
			return 'CSI Motion';
		case 'imu_tamper':
			return 'IMU Tamper';
		case 'gpio_digital':
			return 'GPIO';
		case 'ble_temperature':
			return 'BLE Temp';
		case 'ble_humidity':
			return 'BLE RH';
		case 'ble_battery':
			return 'BLE Batt';
		case 'ble_rssi':
			return 'BLE RSSI';
		default:
			return source;
	}
}

function getActionTokens(channels: NotifyChannel[], shellyDeviceIds: string[] = []): string[] {
	const parts: string[] = [];
	// Keep these short tokens stable because they are embedded in auto-generated
	// rule names and also make it obvious during debugging which default channel
	// was applied to a freshly opened rule modal.
	if (channels.includes('led')) parts.push('LED');
	if (channels.includes('telegram')) parts.push('TG');
	if (channels.includes('webhook')) parts.push('WH');
	if (channels.includes('pushover')) parts.push('PO');
	if (shellyDeviceIds.length > 0) parts.push('Shelly');
	return parts;
}

function generateAutoName(draft: AlarmRuleDraft): string {
	const sourceMeta = ALARM_SOURCES[draft.source];
	const unit = sourceMeta ? sourceMeta.unit : '';
	const op = draft.operator === 'above' ? '>' : '<';
	const actions = getActionTokens(draft.notify_channels, draft.shelly_device_ids ?? []);
	const actionSuffix = actions.length > 0 ? ` (${actions.join(', ')})` : '';
	const baseName = isBooleanLikeSource(draft.source)
		? getBooleanSourceName(draft.source)
		: `${getSourceShortName(draft.source)} ${op} ${draft.threshold}${unit}`;
	const fullName = `${baseName}${actionSuffix}`;

	if (fullName.length <= MAX_ALARM_NAME_LENGTH) {
		return fullName;
	}

	const availableForBase = MAX_ALARM_NAME_LENGTH - actionSuffix.length;
	return `${baseName.slice(0, availableForBase)}${actionSuffix}`;
}

function isBleSource(source: AlarmSource): boolean {
	return (
		source === 'ble_temperature' ||
		source === 'ble_humidity' ||
		source === 'ble_battery' ||
		source === 'ble_rssi'
	);
}

function getDefaultOperator(source: AlarmSource): AlarmOperator {
	if (ALARM_SOURCES[source].booleanLike) return 'above';
	return source === 'ble_battery' || source === 'ble_rssi' ? 'below' : 'above';
}

function isBooleanLikeSource(source: AlarmSource): boolean {
	return ALARM_SOURCES[source].booleanLike === true;
}

function getBooleanSourceName(source: AlarmSource): string {
	if (source === 'gpio_digital') return 'GPIO input true';
	return source === 'imu_tamper' ? 'IMU tamper detected' : 'CSI motion detected';
}

function hasAnyActions(draft: AlarmRuleDraft): boolean {
	return draft.notify_channels.length > 0 || (draft.shelly_device_ids?.length ?? 0) > 0;
}

export function useAlarmRuleForm(
	getRule: () => AlarmRule | null,
	getIsOpen: () => boolean,
	deps: AlarmRuleFormDeps = {}
) {
	const now = deps.now ?? (() => Math.floor(Date.now() / 1000));
	const createId = deps.createId ?? generateId;

	let formData = $state<AlarmRuleDraft>(createNewDraft());
	let wasOpen = $state(false);
	let lastAutoName = $state('');
	let previousSource = $state<AlarmSource>('co2');

	function resetDraft() {
		const currentRule = getRule();
		const nextDraft = currentRule ? createDraftFromRule(currentRule) : createNewDraft();

		formData = nextDraft;
		if (isBooleanLikeSource(formData.source)) {
			formData.operator = 'above';
			formData.threshold = 0.5;
		}
		previousSource = nextDraft.source;

		if (!currentRule && nextDraft.name === '') {
			const autoName = generateAutoName(nextDraft);
			formData.name = autoName;
			lastAutoName = autoName;
		} else {
			lastAutoName = '';
		}
	}

	$effect.pre(() => {
		const currentIsOpen = getIsOpen();
		if (currentIsOpen && !wasOpen) {
			resetDraft();
		}
		wasOpen = currentIsOpen;
	});

	function updateAutoName() {
		if (formData.name === lastAutoName || formData.name === '') {
			const nextName = generateAutoName(formData);
			formData.name = nextName;
			lastAutoName = nextName;
		}
	}

	function handleSourceChange(nextSource: AlarmSource) {
		formData.source = nextSource;
		if (nextSource !== previousSource) {
			formData.threshold = getThresholdConfig(nextSource).default;
			formData.operator = getDefaultOperator(nextSource);
			previousSource = nextSource;
		}
		updateAutoName();
	}

	function handleOperatorChange(nextOperator: AlarmOperator) {
		if (isBooleanLikeSource(formData.source)) {
			formData.operator = 'above';
			formData.threshold = 0.5;
			updateAutoName();
			return;
		}
		formData.operator = nextOperator;
		updateAutoName();
	}

	function handleThresholdChange(nextThreshold: number) {
		if (isBooleanLikeSource(formData.source)) {
			formData.operator = 'above';
			formData.threshold = 0.5;
			updateAutoName();
			return;
		}
		formData.threshold = nextThreshold;
		updateAutoName();
	}

	function handleNotifyChannelsChange(nextChannels: NotifyChannel[]) {
		formData.notify_channels = nextChannels;
		updateAutoName();
	}

	function handleShellyChange(nextIds: string[]) {
		formData.shelly_device_ids = nextIds;
		updateAutoName();
	}

	let thresholdConfig = $derived(getThresholdConfig(formData.source));
	let isEditing = $derived(getRule() !== null);
	let currentBleSource = $derived(isBleSource(formData.source));
	let currentGpioSource = $derived(formData.source === 'gpio_digital');
	let currentBooleanLikeSource = $derived(isBooleanLikeSource(formData.source));
	let bleValid = $derived(!currentBleSource || (formData.ble_device_mac?.length ?? 0) > 0);
	let gpioValid = $derived(!currentGpioSource || (formData.gpio_id?.length ?? 0) > 0);
	// LED-only rules stay local and do not need remote-channel cooldown UX.
	// If LED should start using cooldown-specific UI later, revisit this derived
	// flag together with DEFAULT_ALARM_RULE and AlarmRule defaults.
	let needsCooldown = $derived(
		formData.notify_channels.includes('telegram') ||
			formData.notify_channels.includes('webhook') ||
			formData.notify_channels.includes('pushover')
	);
	let formValid = $derived(
		formData.name.trim().length >= 2 && hasAnyActions(formData) && bleValid && gpioValid
	);

	function submitRule(): AlarmRule | null {
		if (!formValid) return null;

		const currentRule = getRule();
		const timestamp = now();

		return {
			id: currentRule?.id ?? createId(),
			...formData,
			created_at: currentRule?.created_at ?? timestamp,
			updated_at: timestamp
		};
	}

	return {
		get formData() {
			return formData;
		},
		get thresholdConfig() {
			return thresholdConfig;
		},
		get isEditing() {
			return isEditing;
		},
		get isBleSource() {
			return currentBleSource;
		},
		get isGpioSource() {
			return currentGpioSource;
		},
		get isBooleanLikeSource() {
			return currentBooleanLikeSource;
		},
		get needsCooldown() {
			return needsCooldown;
		},
		get formValid() {
			return formValid;
		},
		updateAutoName,
		handleSourceChange,
		handleOperatorChange,
		handleThresholdChange,
		handleNotifyChannelsChange,
		handleShellyChange,
		submitRule
	};
}
