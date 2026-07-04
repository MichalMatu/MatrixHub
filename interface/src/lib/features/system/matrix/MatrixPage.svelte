<script lang="ts">
	import { goto } from '$app/navigation';
	import { onMount } from 'svelte';
	import MatrixSectionPage from './MatrixSectionPage.svelte';
	import MatrixSettings from './MatrixSettings.svelte';
	import { MATRIX_DISPLAY_SETTING_KEYS } from './matrixModel';
	import { MATRIX_DISPLAY_TAB_PATH, getRememberedMatrixTabPath } from './matrixNavigation';

	onMount(() => {
		const rememberedPath = getRememberedMatrixTabPath();
		if (rememberedPath && rememberedPath !== MATRIX_DISPLAY_TAB_PATH) {
			void goto(rememberedPath, { replaceState: true, noScroll: true });
		}
	});
</script>

<MatrixSectionPage trackedKeys={MATRIX_DISPLAY_SETTING_KEYS}>
	{#snippet children(store, canManage)}
		<MatrixSettings {store} {canManage} />
	{/snippet}
</MatrixSectionPage>
