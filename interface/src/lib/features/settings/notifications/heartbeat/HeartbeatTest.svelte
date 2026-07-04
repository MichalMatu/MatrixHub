<script lang="ts">
	import ContentBox from '$lib/components/layout/ContentBox.svelte';
	import BaseCard from '$lib/components/layout/BaseCard.svelte';
	import { FormButton } from '$lib/components/shared/forms';
	import Heartbeat from '~icons/tabler/heart-rate-monitor';
	import Send from '~icons/tabler/send';
	import * as m from '$lib/paraglide/messages.js';
	import { i18n } from '$lib/i18n.svelte';
	import WorkerStats from '$lib/components/notifications/WorkerStats.svelte';

	let { heartbeatTest } = $props<{
		heartbeatTest: ReturnType<typeof import('./useHeartbeatTest.svelte').useHeartbeatTest>;
	}>();
</script>

<BaseCard
	title={m.heartbeat_test_title({ locale: i18n.languageTag })}
	icon={Heartbeat}
	class="h-full"
>
	<ContentBox>
		<p class="text-sm opacity-70 leading-relaxed mb-4">
			{m.heartbeat_test_desc({ locale: i18n.languageTag })}
		</p>

		<FormButton
			class="btn-primary w-full"
			disabled={heartbeatTest.blocked}
			onclick={() => void heartbeatTest.sendTest()}
			loading={heartbeatTest.sending}
			icon={Send}
			label={heartbeatTest.sending
				? m.heartbeat_test_sending({ locale: i18n.languageTag })
				: m.heartbeat_test_btn({ locale: i18n.languageTag })}
		/>
	</ContentBox>

	<WorkerStats type="heartbeat" />

	<ContentBox class="mt-4">
		<div class="text-sm opacity-70 leading-relaxed space-y-1">
			<p>
				<strong>{m.heartbeat_test_note_title({ locale: i18n.languageTag })}</strong>
				{m.heartbeat_test_note_desc({ locale: i18n.languageTag })}
			</p>
		</div>
	</ContentBox>
</BaseCard>
