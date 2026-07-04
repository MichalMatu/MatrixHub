<script lang="ts">
	import { PowerApiService } from '$lib/services/api/core/PowerApiService';
	import { usePowerManagement } from '$lib/features/system/power/usePowerManagement.svelte';
	import PowerConfigCard from '$lib/features/system/power/PowerConfigCard.svelte';
	import PowerActionsCard from '$lib/features/system/power/PowerActionsCard.svelte';
	import { useApiClient } from '$lib/utils/api/useApiClient.svelte';
	import { appFeatures } from '$lib/stores/appFeatures.svelte';

	// API Service
	const apiClient = useApiClient();
	let api = $derived(apiClient.createService(PowerApiService));
	const features = $derived(appFeatures.features);
	const featuresResolved = $derived(appFeatures.resolved);

	const powerState = usePowerManagement(() => api);
</script>

<div class="grid grid-cols-1 md:grid-cols-2 gap-3 md:gap-4">
	<PowerConfigCard
		status={powerState.status}
		loading={powerState.loading}
		error={powerState.error}
		errors={powerState.errors}
		bind:localSleepEnabled={powerState.localSleepEnabled}
		bind:localInactivityTimeoutMs={powerState.localInactivityTimeoutMs}
		bind:localGraceAfterBootMs={powerState.localGraceAfterBootMs}
		bind:localWakeTimerEnabled={powerState.localWakeTimerEnabled}
		bind:localWakeButtonEnabled={powerState.localWakeButtonEnabled}
		bind:localWakeTouchEnabled={powerState.localWakeTouchEnabled}
		bind:localWakeIntervalMs={powerState.localWakeIntervalMs}
		bind:localTimerWakeAwakeMs={powerState.localTimerWakeAwakeMs}
		bind:localButtonWakeAwakeMs={powerState.localButtonWakeAwakeMs}
		bind:localWakeTouchGpio={powerState.localWakeTouchGpio}
		bind:localWakeTouchThreshold={powerState.localWakeTouchThreshold}
		hasChanges={powerState.hasChanges}
		saving={powerState.saving}
		onSave={powerState.saveSettings}
		onReset={powerState.resetSettings}
	/>

	<PowerActionsCard
		sleepEnabled={featuresResolved && features.sleep}
		onRestart={powerState.restart}
		onFactoryReset={powerState.factoryReset}
		onSleep={powerState.requestSleep}
		onHygieneSleep={powerState.requestHygieneSleep}
	/>
</div>
