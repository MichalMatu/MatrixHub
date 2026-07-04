<script lang="ts">
	import MatrixSectionPage from './MatrixSectionPage.svelte';
	import MatrixDataVisualization from './MatrixDataVisualization.svelte';
	import { MATRIX_DATA_VISUALIZATION_SETTING_KEYS } from './matrixModel';
</script>

<MatrixSectionPage trackedKeys={MATRIX_DATA_VISUALIZATION_SETTING_KEYS}>
	{#snippet children(store, canManage)}
		<MatrixDataVisualization {store} {canManage} />
	{/snippet}
</MatrixSectionPage>
