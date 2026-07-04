<script lang="ts">
	import { AdminAccessGate, GridLayout } from '$lib/components/layout';
	import { useSessionAccess } from '$lib/features/auth/useSessionAccess.svelte';
	import TelegramSettings from './TelegramSettings.svelte';
	import TelegramTest from './TelegramTest.svelte';
	import { useTelegramSettings } from './useTelegramSettings.svelte';

	const session = useSessionAccess();
	const canManage = $derived(session.canManage);
	const telegramState = useTelegramSettings({ shouldLoad: () => canManage });
</script>

<AdminAccessGate allow={canManage}>
	<GridLayout cols={2}>
		<TelegramSettings {telegramState} />
		{#if telegramState.canTest && !telegramState.loading}
			<TelegramTest />
		{/if}
	</GridLayout>
</AdminAccessGate>
