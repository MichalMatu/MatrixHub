<script lang="ts">
	import { untrack } from 'svelte';
	import { slide } from 'svelte/transition';
	import { cubicOut } from 'svelte/easing';
	import type { KnownNetworkItem } from '$lib/types/connectivity/wifi';
	import { Modal } from '$lib/components';
	import { FormButton, FormInput, FormToggle } from '$lib/components/shared/forms';
	import { closeModalLayers } from '$lib/utils/ui/modal';
	import StaticIPFields from './StaticIPFields.svelte';
	import { useZodForm } from '$lib/utils/validation/zodForm.svelte';
	import { WifiNetworkSchema } from '$lib/features/wifi/wifiValidation';

	import { i18n } from '$lib/i18n.svelte';
	import * as m from '$lib/paraglide/messages.js';

	// Icons
	import Cancel from '~icons/tabler/x';
	import Set from '~icons/tabler/check';

	interface Props {
		isOpen: boolean;
		title: string;
		networkEditable?: KnownNetworkItem;
		onSaveNetwork: (network: KnownNetworkItem) => void;
	}

	let {
		isOpen,
		title,
		networkEditable: _networkEditable = {
			ssid: '',
			password: '',
			static_ip_config: false,
			local_ip: '',
			subnet_mask: '',
			gateway_ip: '',
			dns_ip_1: '',
			dns_ip_2: ''
		} as KnownNetworkItem,
		onSaveNetwork
	}: Props = $props();

	const form = useZodForm({
		schema: WifiNetworkSchema,
		initialValues: untrack(() => ({
			ssid: _networkEditable.ssid || '',
			password: _networkEditable.password || '',
			static_ip_config: !!_networkEditable.static_ip_config,
			local_ip: _networkEditable.local_ip || '',
			subnet_mask: _networkEditable.subnet_mask || '',
			gateway_ip: _networkEditable.gateway_ip || '',
			dns_ip_1: _networkEditable.dns_ip_1 || '',
			dns_ip_2: _networkEditable.dns_ip_2 || ''
		})),
		onSubmit: (values) => {
			onSaveNetwork(values as KnownNetworkItem);
			closeModalLayers(1);
		}
	});

	// Use this to directly access the form's DOM element
	let formElement: HTMLFormElement | undefined = $state();

	function preventDefault(fn: () => void) {
		return function (event: Event) {
			event.preventDefault();
			fn();
		};
	}
</script>

<Modal
	{isOpen}
	onClose={() => closeModalLayers(1)}
	{title}
	widthClass="w-full md:w-[28rem] min-w-fit max-w-md"
>
	<form
		class="fieldset"
		onsubmit={preventDefault(() => form.submit())}
		novalidate
		bind:this={formElement}
	>
		<div
			class="grid w-full grid-cols-1 content-center gap-4 px-4 sm:grid-cols-2"
			transition:slide|local={{ duration: 300, easing: cubicOut }}
		>
			<div>
				<FormInput
					label={m.ap_ssid_label({ locale: i18n.languageTag })}
					id="ssid"
					value={form.values.ssid}
					oninput={(e) => form.handleInput('ssid', (e.target as HTMLInputElement).value)}
					onblur={() => form.handleBlur('ssid')}
					minlength={1}
					maxlength={32}
					required
					autocomplete="off"
					error={form.errors.ssid}
				/>
			</div>
			<div>
				<FormInput
					label={m.ap_password_label({ locale: i18n.languageTag })}
					id="pwd"
					type="password"
					value={form.values.password}
					oninput={(e) => form.handleInput('password', (e.target as HTMLInputElement).value)}
					onblur={() => form.handleBlur('password')}
					maxlength={63}
					autocomplete="current-password"
					error={form.errors.password}
				/>
			</div>
			<div class="sm:col-span-2">
				<FormToggle
					label={m.wifi_static_ip_config_label({ locale: i18n.languageTag })}
					description={form.values.static_ip_config
						? m.wifi_static_ip_config_desc_manual({ locale: i18n.languageTag })
						: m.wifi_static_ip_config_desc_auto({ locale: i18n.languageTag })}
					checked={form.values.static_ip_config}
					onchange={(e) =>
						form.handleInput('static_ip_config', (e.target as HTMLInputElement).checked)}
				/>
			</div>
		</div>

		{#if form.values.static_ip_config}
			<StaticIPFields
				network={form.values}
				errors={{
					local_ip: form.errors.local_ip,
					gateway_ip: form.errors.gateway_ip,
					subnet_mask: form.errors.subnet_mask,
					dns_1: form.errors.dns_ip_1,
					dns_2: form.errors.dns_ip_2
				}}
				onInputChange={(field, value) => form.handleInput(field as keyof typeof form.values, value)}
				onFieldBlur={(field) => form.handleBlur(field as keyof typeof form.values)}
			/>
		{/if}
	</form>

	<!-- Actions -->
	{#snippet actions()}
		<div class="flex justify-end gap-2 w-full">
			<FormButton
				label={m.action_cancel({ locale: i18n.languageTag })}
				icon={Cancel}
				type="button"
				class="btn-neutral"
				onclick={() => closeModalLayers(1)}
			/>
			<FormButton
				label={m.action_save({ locale: i18n.languageTag })}
				icon={Set}
				type="submit"
				disabled={form.isSubmitting}
				onclick={() => formElement?.requestSubmit()}
			/>
		</div>
	{/snippet}
</Modal>
