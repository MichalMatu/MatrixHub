<script lang="ts">
	import MatrixSectionPage from './MatrixSectionPage.svelte';
	import MatrixEffects from './MatrixEffects.svelte';
	import { MATRIX_EFFECT_SETTING_KEYS } from './matrixModel';
</script>

<MatrixSectionPage trackedKeys={MATRIX_EFFECT_SETTING_KEYS}>
	{#snippet children(store, canManage)}
		<MatrixEffects {store} {canManage} />
	{/snippet}
</MatrixSectionPage>
