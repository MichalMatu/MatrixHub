<script lang="ts">
	import ContentBox from '$lib/components/layout/ContentBox.svelte';
	import BaseCard from '$lib/components/layout/BaseCard.svelte';
	import { FormButton } from '$lib/components/shared/forms';
	import Network from '~icons/tabler/network';
	import Send from '~icons/tabler/send';
	import * as m from '$lib/paraglide/messages.js';
	import { i18n } from '$lib/i18n.svelte';
	import WorkerStats from '$lib/components/notifications/WorkerStats.svelte';

	let { udpState, udpTest } = $props<{
		udpState: ReturnType<typeof import('./useUdpSettings.svelte').useUdpSettings>;
		udpTest: ReturnType<typeof import('./useUdpTest.svelte').useUdpTest>;
	}>();

	const savedPort = $derived(udpState.savedSettings?.port ?? udpState.settings.port);
</script>

<BaseCard title={m.udp_test_title({ locale: i18n.languageTag })} icon={Network} class="h-full">
	<ContentBox>
		<p class="text-sm opacity-70 leading-relaxed mb-4">
			{m.udp_test_desc({ locale: i18n.languageTag })}
		</p>

		<FormButton
			class="btn-primary w-full"
			disabled={udpTest.blocked}
			onclick={() => void udpTest.sendTest()}
			loading={udpTest.sending}
			icon={Send}
			label={udpTest.sending
				? m.udp_test_sending({ locale: i18n.languageTag })
				: m.udp_test_btn({ locale: i18n.languageTag })}
		/>
	</ContentBox>

	<WorkerStats type="udp" />

	<ContentBox class="mt-4">
		<div class="text-sm opacity-70 leading-relaxed space-y-1">
			<p>
				<strong>{m.udp_test_note_title({ locale: i18n.languageTag })}</strong>
				{m.udp_test_note_desc({ port: String(savedPort) }, { locale: i18n.languageTag })}
			</p>
		</div>
	</ContentBox>
</BaseCard>
