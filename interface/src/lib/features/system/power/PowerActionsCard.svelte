<script lang="ts">
	import BaseCard from '$lib/components/layout/BaseCard.svelte';
	import { FormButton } from '$lib/components/shared/forms';
	import { confirm } from '$lib/utils/ui/dialogs';
	import { confirmRestartAndSave } from '$lib/utils/ui/restartConfirmation';
	import Cancel from '~icons/tabler/x';
	import Power from '~icons/tabler/reload';
	import Sleep from '~icons/tabler/zzz';
	import Zap from '~icons/tabler/bolt';
	import FactoryReset from '~icons/tabler/refresh-dot';
	import PowerIcon from '~icons/tabler/bolt';
	import { i18n } from '$lib/i18n.svelte';
	import * as m from '$lib/paraglide/messages.js';

	interface Props {
		sleepEnabled: boolean;
		onRestart: () => Promise<void>;
		onFactoryReset: () => Promise<void>;
		onSleep: () => Promise<void>;
		onHygieneSleep: () => Promise<void>;
	}

	let { sleepEnabled, onRestart, onFactoryReset, onSleep, onHygieneSleep }: Props = $props();

	function confirmRestart() {
		confirmRestartAndSave(async () => {}, {
			title: m.power_dialog_restart_title({ locale: i18n.languageTag }),
			message: m.power_dialog_restart_msg({ locale: i18n.languageTag }),
			confirmLabel: m.power_btn_restart({ locale: i18n.languageTag }),
			triggerRestart: onRestart
		});
	}

	function confirmHygieneSleep() {
		confirmRestartAndSave(async () => {}, {
			title: m.power_dialog_restart_title({ locale: i18n.languageTag }),
			message: m.power_dialog_restart_msg({ locale: i18n.languageTag }),
			confirmLabel: m.power_btn_hygiene_sleep({ locale: i18n.languageTag }),
			triggerRestart: onHygieneSleep,
			useSleepInsteadOfRestart: true
		});
	}

	function confirmReset() {
		confirmRestartAndSave(async () => {}, {
			title: m.power_dialog_reset_title({ locale: i18n.languageTag }),
			message: m.power_dialog_reset_msg({ locale: i18n.languageTag }),
			cancelLabel: m.power_dialog_restart_abort({ locale: i18n.languageTag }),
			confirmLabel: m.power_dialog_reset_confirm({ locale: i18n.languageTag }),
			triggerRestart: onFactoryReset
		});
	}

	function confirmSleep() {
		confirm({
			title: m.power_dialog_sleep_title({ locale: i18n.languageTag }),
			message: m.power_dialog_sleep_msg({ locale: i18n.languageTag }),
			labels: {
				cancel: { label: m.power_dialog_restart_abort({ locale: i18n.languageTag }), icon: Cancel },
				confirm: { label: m.power_dialog_sleep_confirm({ locale: i18n.languageTag }), icon: Sleep }
			},
			onConfirm: () => {
				onSleep();
			}
		});
	}
</script>

<BaseCard title={m.power_actions_title({ locale: i18n.languageTag })} icon={PowerIcon}>
	<div class="flex flex-col gap-2">
		{#if sleepEnabled}
			<FormButton
				label={m.power_btn_sleep({ locale: i18n.languageTag })}
				icon={Sleep}
				onclick={confirmSleep}
			/>
		{/if}
		<FormButton
			label={m.power_btn_restart({ locale: i18n.languageTag })}
			icon={Power}
			onclick={confirmRestart}
		/>
		<FormButton
			label={m.power_btn_hygiene_sleep({ locale: i18n.languageTag })}
			icon={Zap}
			onclick={confirmHygieneSleep}
			class="btn-info btn-outline"
		/>
		<FormButton
			label={m.power_btn_factory_reset({ locale: i18n.languageTag })}
			icon={FactoryReset}
			onclick={confirmReset}
			class="btn-error"
		/>
	</div>
</BaseCard>
