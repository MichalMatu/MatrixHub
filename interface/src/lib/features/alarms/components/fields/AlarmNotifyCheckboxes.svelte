<script lang="ts">
	import type { NotifyChannel } from '$lib/types/domain/alarms';
	import { i18n } from '$lib/i18n.svelte';
	import * as m from '$lib/paraglide/messages.js';
	import { FormCheckbox } from '$lib/components/shared/forms';

	let {
		notifyChannels = $bindable(),
		onchange
	}: {
		notifyChannels: NotifyChannel[];
		onchange?: (channels: NotifyChannel[]) => void;
	} = $props();

	function handleChannelToggle(channel: NotifyChannel) {
		if (notifyChannels.includes(channel)) {
			notifyChannels = notifyChannels.filter((c) => c !== channel);
		} else {
			notifyChannels = [...notifyChannels, channel];
		}
		onchange?.(notifyChannels);
	}
</script>

<div class="flex flex-col gap-1">
	<span class="text-xs font-bold block opacity-70 uppercase tracking-wide">
		{m.alarms_field_notify_via({ locale: i18n.languageTag })}
	</span>
	<div class="flex flex-row gap-4 flex-wrap">
		<FormCheckbox
			label="Telegram"
			checked={notifyChannels.includes('telegram')}
			onchange={() => handleChannelToggle('telegram')}
		/>
		<FormCheckbox
			label="LED"
			checked={notifyChannels.includes('led')}
			onchange={() => handleChannelToggle('led')}
		/>
		<FormCheckbox
			label="Webhook"
			checked={notifyChannels.includes('webhook')}
			onchange={() => handleChannelToggle('webhook')}
		/>
		<FormCheckbox
			label="Pushover"
			checked={notifyChannels.includes('pushover')}
			onchange={() => handleChannelToggle('pushover')}
		/>
	</div>
</div>
