<script lang="ts">
	import type { AlarmSource, AlarmOperator } from '$lib/types/domain/alarms';
	import { ALARM_SOURCES } from '$lib/types/domain/alarms';
	import * as m from '$lib/paraglide/messages.js';
	import { i18n } from '$lib/i18n.svelte';
	import { getAlarmSourceLabel } from '$lib/features/alarms/alarmLabels';

	import { FormSelect } from '$lib/components/shared/forms';

	let {
		source = $bindable(),
		operator = $bindable(),
		onSourceChange,
		onOperatorChange
	}: {
		source: AlarmSource;
		operator: AlarmOperator;
		onSourceChange?: (source: AlarmSource) => void;
		onOperatorChange?: (operator: AlarmOperator) => void;
	} = $props();

	let sourceOptions = $derived(
		Object.keys(ALARM_SOURCES).map((key) => ({
			value: key,
			label: getAlarmSourceLabel(key as AlarmSource)
		}))
	);

	let operatorOptions = $derived([
		{ value: 'above', label: m.alarms_operator_above({ locale: i18n.languageTag }) },
		{ value: 'below', label: m.alarms_operator_below({ locale: i18n.languageTag }) }
	]);
	let booleanLikeSource = $derived(ALARM_SOURCES[source].booleanLike === true);

	function handleSourceSelect(newSource: AlarmSource) {
		source = newSource;
		onSourceChange?.(newSource);
	}

	function handleOperatorSelect(newOperator: AlarmOperator) {
		operator = booleanLikeSource ? 'above' : newOperator;
		onOperatorChange?.(operator);
	}
</script>

<div class="flex flex-row items-center gap-2 w-full">
	<div class="grow min-w-0">
		<FormSelect
			value={source}
			options={sourceOptions}
			onchange={(e) => handleSourceSelect((e.target as HTMLSelectElement).value as AlarmSource)}
		/>
	</div>
	<div class="w-32 flex-none">
		<FormSelect
			value={operator}
			options={operatorOptions}
			disabled={booleanLikeSource}
			onchange={(e) => handleOperatorSelect((e.target as HTMLSelectElement).value as AlarmOperator)}
		/>
	</div>
</div>
