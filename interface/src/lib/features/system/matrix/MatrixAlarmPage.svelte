<script lang="ts">
	import MatrixSectionPage from './MatrixSectionPage.svelte';
	import MatrixAlarmSettings from './MatrixAlarmSettings.svelte';
	import { MATRIX_ALARM_SETTING_KEYS } from './matrixModel';
</script>

<MatrixSectionPage trackedKeys={MATRIX_ALARM_SETTING_KEYS}>
	{#snippet children(store, canManage)}
		<MatrixAlarmSettings {store} {canManage} />
	{/snippet}
</MatrixSectionPage>
