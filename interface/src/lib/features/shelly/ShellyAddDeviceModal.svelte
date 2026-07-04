<script lang="ts">
	import { i18n } from '$lib/i18n.svelte';
	import * as m from '$lib/paraglide/messages.js';
	import { Modal } from '$lib/components';
	import { FormButton, FormInput, FormSelect } from '$lib/components/shared/forms';

	// Icons
	import Save from '~icons/tabler/device-floppy';
	import X from '~icons/tabler/x';
	import AlertCircle from '~icons/tabler/alert-circle';

	interface Props {
		isOpen: boolean;
		editing?: boolean;
		saving?: boolean;
		deviceName: string;
		deviceIp: string;
		deviceRelay: number;
		deviceGeneration: number;
		deviceCount: number;
		errorMessage?: string | null;
		onClose: () => void;
		onSave: () => void;
		onNameChange: (_value: string) => void;
		onIpChange: (_value: string) => void;
		onRelayChange: (_value: number) => void;
		onGenerationChange: (_value: number) => void;
	}

	let {
		isOpen,
		editing = false,
		saving = false,
		deviceName,
		deviceIp,
		deviceRelay,
		deviceGeneration,
		deviceCount,
		errorMessage = null,
		onClose,
		onSave,
		onNameChange,
		onIpChange,
		onRelayChange,
		onGenerationChange
	}: Props = $props();

	import { MAX_SHELLY_DEVICES } from './shellyModel';
	let isLimitReached = $derived(!editing && deviceCount >= MAX_SHELLY_DEVICES);

	const generationOptions = $derived([
		{ value: 2, label: m.shelly_generation_gen2({ locale: i18n.languageTag }) },
		{ value: 1, label: m.shelly_generation_gen1({ locale: i18n.languageTag }) }
	]);

	function filterAsciiPrintable(value: string): string {
		// Keep only printable ASCII (0x20..0x7E)
		return value.replace(/[^\x20-\x7E]/g, '');
	}
</script>

<Modal
	{isOpen}
	onClose={saving ? () => {} : onClose}
	title={editing
		? m.shelly_modal_title_edit({ locale: i18n.languageTag })
		: m.shelly_modal_title({ locale: i18n.languageTag })}
>
	<!-- Main Content -->
	{#if errorMessage || isLimitReached}
		<div class="alert {isLimitReached ? 'alert-warning' : 'alert-error'} mt-4 shadow-sm">
			<AlertCircle class="h-6 w-6 shrink-0" />
			<span
				>{isLimitReached
					? m.shelly_error_limit({ count: MAX_SHELLY_DEVICES }, { locale: i18n.languageTag })
					: errorMessage}</span
			>
		</div>
	{/if}

	<div class="py-4 px-1 flex flex-col gap-4">
		<FormInput
			id="dev-name"
			label={m.shelly_input_name({ locale: i18n.languageTag })}
			value={deviceName}
			maxlength={63}
			oninput={(e) => onNameChange(filterAsciiPrintable((e.target as HTMLInputElement).value))}
			placeholder={m.shelly_input_name_placeholder({ locale: i18n.languageTag })}
		/>

		<FormInput
			id="dev-ip"
			label={m.shelly_input_ip({ locale: i18n.languageTag })}
			value={deviceIp}
			maxlength={15}
			oninput={(e) => onIpChange((e.target as HTMLInputElement).value)}
			placeholder={m.shelly_input_ip_placeholder({ locale: i18n.languageTag })}
			help={m.shelly_input_ip_hint({ locale: i18n.languageTag })}
			class="font-mono"
		/>

		<FormInput
			id="dev-relay"
			type="number"
			label={m.shelly_input_relay({ locale: i18n.languageTag })}
			value={String(deviceRelay)}
			oninput={(e) => onRelayChange(parseInt((e.target as HTMLInputElement).value))}
			min={0}
			max={3}
			help={m.shelly_input_relay_hint({ locale: i18n.languageTag })}
		/>

		<FormSelect
			id="dev-generation"
			label={m.shelly_input_generation({ locale: i18n.languageTag })}
			value={deviceGeneration}
			options={generationOptions}
			onchange={(e) => onGenerationChange(parseInt((e.target as HTMLSelectElement).value))}
		/>
	</div>

	<!-- Actions -->
	{#snippet actions()}
		<div class="flex justify-end gap-2 w-full">
			<FormButton
				label={m.action_cancel({ locale: i18n.languageTag })}
				icon={X}
				onclick={onClose}
				class="btn-neutral"
				disabled={saving}
			/>
			<FormButton
				label={m.action_save({ locale: i18n.languageTag })}
				icon={Save}
				onclick={onSave}
				disabled={isLimitReached || saving}
				loading={saving}
			/>
		</div>
	{/snippet}
</Modal>
