<script lang="ts">
	import { Modal } from '$lib/components';
	import { FormButton, FormInput } from '$lib/components/shared/forms';
	import { i18n } from '$lib/i18n.svelte';
	import * as m from '$lib/paraglide/messages.js';
	import Save from '~icons/tabler/device-floppy';
	import Refresh from '~icons/tabler/refresh';
	import X from '~icons/tabler/x';

	interface Props {
		isOpen: boolean;
		saving?: boolean;
		targetPort: string;
		idleTimeout: number | '';
		errors: {
			idle_timeout_ms: boolean;
			target_port: boolean;
		};
		onClose: () => void;
		onSave: () => void;
		onTargetPortChange: (_value: string) => void;
		onIdleTimeoutChange: (_value: string) => void;
		onIdleTimeoutBlur: () => void;
	}

	let {
		isOpen,
		saving = false,
		targetPort,
		idleTimeout,
		errors,
		onClose,
		onSave,
		onTargetPortChange,
		onIdleTimeoutChange,
		onIdleTimeoutBlur
	}: Props = $props();
</script>

<Modal
	{isOpen}
	onClose={saving ? () => {} : onClose}
	title={m.usb_terminal_title({ locale: i18n.languageTag })}
>
	<div class="flex flex-col gap-4 px-1 py-4">
		<div>
			<div class="grid gap-2 sm:grid-cols-[minmax(0,1fr)_auto] sm:items-end">
				<FormInput
					id="usb-terminal-target-port"
					type="text"
					label={m.usb_terminal_target_port({ locale: i18n.languageTag })}
					placeholder={m.usb_terminal_target_port_placeholder({ locale: i18n.languageTag })}
					value={targetPort}
					oninput={(event) => onTargetPortChange((event.target as HTMLInputElement).value)}
					maxlength={31}
					error={errors.target_port
						? m.settings_validation_error({ locale: i18n.languageTag })
						: undefined}
				/>
				<FormButton
					type="button"
					variant="neutral"
					size="sm"
					icon={Refresh}
					label={m.usb_terminal_target_port_auto({ locale: i18n.languageTag })}
					title={m.usb_terminal_target_port_auto({ locale: i18n.languageTag })}
					onclick={() => onTargetPortChange('auto')}
				/>
			</div>
			<p class="mt-1 text-xs leading-tight opacity-70">
				{m.usb_terminal_target_port_desc({ locale: i18n.languageTag })}
			</p>
		</div>

		<div>
			<FormInput
				id="usb-terminal-idle-timeout"
				type="number"
				label={m.usb_terminal_idle_timeout({ locale: i18n.languageTag })}
				min="500"
				max="30000"
				step="500"
				value={idleTimeout}
				oninput={(event) => onIdleTimeoutChange((event.target as HTMLInputElement).value)}
				onblur={onIdleTimeoutBlur}
				error={errors.idle_timeout_ms
					? m.settings_validation_error({ locale: i18n.languageTag })
					: undefined}
			/>
			<p class="mt-1 text-xs leading-tight opacity-70">
				{m.usb_terminal_idle_timeout_desc({ locale: i18n.languageTag })}
			</p>
		</div>
	</div>

	{#snippet actions()}
		<div class="flex w-full justify-end gap-2">
			<FormButton
				label={m.action_cancel({ locale: i18n.languageTag })}
				icon={X}
				variant="neutral"
				onclick={onClose}
				disabled={saving}
			/>
			<FormButton
				label={m.action_save({ locale: i18n.languageTag })}
				icon={Save}
				onclick={onSave}
				loading={saving}
			/>
		</div>
	{/snippet}
</Modal>
