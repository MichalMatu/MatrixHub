<script lang="ts">
	import { FormInput, FormToggle } from '$lib/components/shared/forms';
	import ContentBox from '$lib/components/layout/ContentBox.svelte';
	import { i18n } from '$lib/i18n.svelte';
	import * as m from '$lib/paraglide/messages.js';
	import type { ApSettings } from '$lib/types/connectivity/ap';
	import type { WifiApFormErrors } from '$lib/features/wifi/wifiValidation';

	interface Props {
		settings: ApSettings;
		errors: WifiApFormErrors;
	}

	let { settings = $bindable(), errors }: Props = $props();
</script>

<div class="grid w-full grid-cols-1 md:grid-cols-2 gap-x-3 gap-y-1">
	<ContentBox>
		<FormInput
			label={m.wifi_stat_ip({ locale: i18n.languageTag })}
			id="localIP"
			bind:value={settings.local_ip}
			minlength={7}
			maxlength={15}
			required
			error={errors.local_ip ? m.wifi_ipv4_error({ locale: i18n.languageTag }) : undefined}
		/>
	</ContentBox>

	<ContentBox>
		<FormInput
			label={m.wifi_stat_gateway({ locale: i18n.languageTag })}
			id="gateway"
			bind:value={settings.gateway_ip}
			minlength={7}
			maxlength={15}
			required
			error={errors.gateway_ip ? m.wifi_ipv4_error({ locale: i18n.languageTag }) : undefined}
		/>
	</ContentBox>

	<ContentBox>
		<FormInput
			label={m.wifi_stat_mask({ locale: i18n.languageTag })}
			id="subnet"
			bind:value={settings.subnet_mask}
			minlength={7}
			maxlength={15}
			required
			error={errors.subnet_mask ? m.wifi_ipv4_error({ locale: i18n.languageTag }) : undefined}
		/>
	</ContentBox>

	<FormToggle
		label={m.ap_hide_ssid_label({ locale: i18n.languageTag })}
		class="h-full"
		description={settings.ssid_hidden
			? m.ap_hide_ssid_desc_on({ locale: i18n.languageTag })
			: m.ap_hide_ssid_desc_off({ locale: i18n.languageTag })}
		bind:checked={settings.ssid_hidden}
	/>
</div>
