<script lang="ts">
	import { AdminAccessGate, GridLayout } from '$lib/components/layout';
	import { useSessionAccess } from '$lib/features/auth/useSessionAccess.svelte';
	import WebhookSettings from './WebhookSettings.svelte';
	import WebhookTest from './WebhookTest.svelte';
	import { useWebhookSettings } from './useWebhookSettings.svelte';

	const session = useSessionAccess();
	const canManage = $derived(session.canManage);
	const webhookState = useWebhookSettings({ shouldLoad: () => canManage });
</script>

<AdminAccessGate allow={canManage}>
	<GridLayout cols={2}>
		<WebhookSettings {webhookState} />
		{#if webhookState.canTest && !webhookState.loading}
			<WebhookTest {webhookState} />
		{/if}
	</GridLayout>
</AdminAccessGate>
