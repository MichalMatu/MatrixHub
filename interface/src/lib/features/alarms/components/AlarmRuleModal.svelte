<script lang="ts">
	import Save from '~icons/tabler/device-floppy';
	import X from '~icons/tabler/x';
	import { Modal } from '$lib/components';
	import { FormButton } from '$lib/components/shared/forms';
	import type { AlarmRule } from '$lib/types/domain/alarms';
	import { MAX_ALARM_NAME_LENGTH } from '$lib/types/domain/alarms';
	import { i18n } from '$lib/i18n.svelte';
	import * as m from '$lib/paraglide/messages.js';
	import { useAlarmRuleForm } from './useAlarmRuleForm.svelte';

	import AlarmNameInput from './fields/AlarmNameInput.svelte';
	import AlarmConditionFields from './fields/AlarmConditionFields.svelte';
	import AlarmThresholdSlider from './fields/AlarmThresholdSlider.svelte';
	import AlarmSeveritySelect from './fields/AlarmSeveritySelect.svelte';
	import AlarmNotifyCheckboxes from './fields/AlarmNotifyCheckboxes.svelte';
	import AlarmShellySelect from './fields/AlarmShellySelect.svelte';
	import AlarmCooldownSlider from './fields/AlarmCooldownSlider.svelte';
	import BleDeviceSelect from './fields/BleDeviceSelect.svelte';
	import GpioChannelSelect from './fields/GpioChannelSelect.svelte';

	interface Props {
		isOpen: boolean;
		rule: AlarmRule | null;
		isSaving?: boolean;
		onSave: (_rule: AlarmRule) => void;
		onClose: () => void;
	}

	let { isOpen, rule, isSaving = false, onSave, onClose }: Props = $props();

	const form = useAlarmRuleForm(
		() => rule,
		() => isOpen
	);

	function handleSubmit() {
		const savedRule = form.submitRule();
		if (!savedRule) return;
		onSave(savedRule);
	}

	function preventDefault(fn: () => void) {
		return function (event: Event) {
			event.preventDefault();
			fn();
		};
	}
</script>

<Modal {isOpen} {onClose} widthClass="w-11/12 max-w-2xl">
	<form
		onsubmit={preventDefault(() => handleSubmit())}
		novalidate
		class="py-2 px-1 max-h-[70vh] overflow-y-auto overflow-x-hidden"
	>
		<div class="flex flex-col gap-4">
			<!-- Name -->
			<AlarmNameInput bind:name={form.formData.name} maxlength={MAX_ALARM_NAME_LENGTH} />

			<!-- Main Grid -->
			<div class="grid grid-cols-1 md:grid-cols-2 gap-6 items-start">
				<!-- Left Column: Trigger & Severity -->
				<div class="flex flex-col gap-3">
					<div
						class="bg-base-200/30 p-3 rounded-xl flex flex-col gap-3 h-full border border-base-200/50"
					>
						<div class="flex items-center justify-between">
							<span class="text-xs font-bold opacity-70 uppercase tracking-wide">
								{m.alarms_field_condition({ locale: i18n.languageTag })}
							</span>
						</div>

						<AlarmConditionFields
							bind:source={form.formData.source}
							bind:operator={form.formData.operator}
							onSourceChange={form.handleSourceChange}
							onOperatorChange={form.handleOperatorChange}
						/>

						{#if form.isBleSource}
							<BleDeviceSelect
								bind:bleDeviceMac={form.formData.ble_device_mac}
								source={form.formData.source}
							/>
						{/if}

						{#if form.isGpioSource}
							<GpioChannelSelect bind:gpioId={form.formData.gpio_id} />
						{/if}

						<AlarmThresholdSlider
							bind:threshold={form.formData.threshold}
							source={form.formData.source}
							min={form.thresholdConfig.min}
							max={form.thresholdConfig.max}
							step={form.thresholdConfig.step}
							onchange={form.handleThresholdChange}
						/>

						<div class="pt-1">
							<AlarmSeveritySelect bind:severity={form.formData.severity} />
						</div>
					</div>
				</div>

				<!-- Right Column: Notifications -->
				<div class="flex flex-col gap-3">
					<div
						class="bg-base-200/30 p-4 rounded-xl flex flex-col gap-3 h-full border border-base-200/50"
					>
						<span class="text-xs font-bold block opacity-70 uppercase tracking-wide">
							{m.alarms_field_actions({ locale: i18n.languageTag })}
						</span>

						{#if form.needsCooldown}
							<AlarmCooldownSlider bind:cooldownSeconds={form.formData.cooldown_seconds} />
						{/if}

						<div class="pt-1">
							<AlarmNotifyCheckboxes
								bind:notifyChannels={form.formData.notify_channels}
								onchange={() => form.handleNotifyChannelsChange(form.formData.notify_channels)}
							/>
						</div>
					</div>
				</div>
			</div>

			<!-- Bottom: Shelly Devices -->
			{#if form.formData.shelly_device_ids !== undefined}
				<div class="pt-2 border-t border-base-200">
					<AlarmShellySelect
						shellyDeviceIds={form.formData.shelly_device_ids}
						onShellyChange={form.handleShellyChange}
					/>
				</div>
			{/if}
		</div>
	</form>

	<!-- Actions -->
	{#snippet actions()}
		<div class="flex justify-end gap-2 w-full">
			<FormButton
				label={m.alarms_form_cancel({ locale: i18n.languageTag })}
				icon={X}
				type="button"
				class="btn-neutral"
				onclick={onClose}
				disabled={isSaving}
			/>
			<FormButton
				label={form.isEditing
					? m.action_save({ locale: i18n.languageTag })
					: m.alarms_form_create({ locale: i18n.languageTag })}
				icon={Save}
				type="button"
				class="btn-primary"
				onclick={() => handleSubmit()}
				disabled={!form.formValid || isSaving}
				loading={isSaving}
			/>
		</div>
	{/snippet}
</Modal>
