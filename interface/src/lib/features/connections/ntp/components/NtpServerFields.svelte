<script lang="ts">
	import { TIME_ZONES } from '$lib/data/timezones';
	import { i18n } from '$lib/i18n.svelte';
	import * as m from '$lib/paraglide/messages.js';

	import { FormInput, FormSelect } from '$lib/components/shared/forms';
	import ContentBox from '$lib/components/layout/ContentBox.svelte';

	interface Props {
		server: string;
		tzLabel: string;
		serverError: boolean;
		onServerChange: (_server: string) => void;
		onTimezoneChange: (_tzLabel: string) => void;
	}

	let { server, tzLabel, serverError, onServerChange, onTimezoneChange }: Props = $props();

	let timezoneOptions = $derived(Object.keys(TIME_ZONES).map((tz) => ({ value: tz, label: tz })));

	function handleServerInput(e: Event) {
		const target = e.target as HTMLInputElement;
		onServerChange(target.value);
	}

	function handleTimezoneChange(e: Event) {
		const target = e.target as HTMLSelectElement;
		onTimezoneChange(target.value);
	}
</script>

<ContentBox>
	<FormInput
		id="server"
		label={m.ntp_label_server_input({ locale: i18n.languageTag })}
		value={server}
		oninput={handleServerInput}
		error={serverError ? m.ntp_error_server_invalid({ locale: i18n.languageTag }) : undefined}
		required
		minlength={3}
		maxlength={64}
	/>
</ContentBox>

<ContentBox>
	<FormSelect
		id="tz"
		label={m.ntp_label_timezone({ locale: i18n.languageTag })}
		value={tzLabel}
		onchange={handleTimezoneChange}
		options={timezoneOptions}
	/>
</ContentBox>
