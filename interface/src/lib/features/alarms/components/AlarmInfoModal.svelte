<script lang="ts">
	import X from '~icons/tabler/x';
	import { Modal } from '$lib/components';
	import { FormButton } from '$lib/components/shared/forms';
	import ContentBox from '$lib/components/layout/ContentBox.svelte';
	import { i18n } from '$lib/i18n.svelte';
	import * as m from '$lib/paraglide/messages.js';
	import { MAX_ALARM_RULES } from '$lib/types/domain/alarms';

	interface Props {
		isOpen: boolean;
		onClose: () => void;
	}

	let { isOpen, onClose }: Props = $props();
</script>

<Modal
	{isOpen}
	{onClose}
	title={m.alarms_title({ locale: i18n.languageTag })}
	widthClass="w-11/12 max-w-2xl"
>
	<div class="px-1 py-2">
		<div class="flex flex-col gap-4">
			<!-- How Alarms Work -->
			<ContentBox title={m.alarms_how_work_title({ locale: i18n.languageTag })}>
				<ul class="list-disc ml-5 space-y-1 text-sm opacity-80 leading-relaxed">
					<li>{m.alarms_how_work_1({ locale: i18n.languageTag })}</li>
					<li>{m.alarms_how_work_2({ locale: i18n.languageTag })}</li>
					<li>{m.alarms_how_work_3({ locale: i18n.languageTag })}</li>
					<li>{m.alarms_how_work_4({ max: MAX_ALARM_RULES }, { locale: i18n.languageTag })}</li>
				</ul>
			</ContentBox>

			<!-- Notification Channels -->
			<ContentBox title={m.alarms_channels_title({ locale: i18n.languageTag })}>
				<ul class="space-y-2 text-sm leading-relaxed">
					<li>
						<b class="text-base-content">{m.menu_telegram({ locale: i18n.languageTag })}</b>
						<span class="opacity-70"
							>– {m.alarms_channels_1({
								locale: i18n.languageTag
							})}</span
						>
					</li>
					<li>
						<b class="text-base-content">LED</b>
						<span class="opacity-70">– {m.alarms_channels_2({ locale: i18n.languageTag })}</span>
					</li>
					<li>
						<b class="text-base-content">{m.menu_webhook({ locale: i18n.languageTag })}</b>
						<span class="opacity-70"
							>– {m.alarms_channels_3({
								locale: i18n.languageTag
							})}</span
						>
					</li>
					<li>
						<b class="text-base-content">{m.menu_pushover({ locale: i18n.languageTag })}</b>
						<span class="opacity-70"
							>– {m.alarms_channels_4({
								locale: i18n.languageTag
							})}</span
						>
					</li>
					<li>
						<b class="text-base-content">{m.menu_shelly({ locale: i18n.languageTag })}</b>
						<span class="opacity-70"
							>– {m.alarms_channels_5({
								locale: i18n.languageTag
							})}</span
						>
					</li>
				</ul>
			</ContentBox>
		</div>
	</div>

	<!-- Close Button -->
	{#snippet actions()}
		<div class="flex justify-end w-full">
			<FormButton
				label={m.alarms_form_cancel({ locale: i18n.languageTag })}
				icon={X}
				type="button"
				class="btn-neutral"
				onclick={onClose}
			/>
		</div>
	{/snippet}
</Modal>
