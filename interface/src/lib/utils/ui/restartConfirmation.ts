/**
 * @file restartConfirmation.ts
 * @brief Reusable restart confirmation utility
 *
 * Provides a unified way to confirm device restarts across all modules.
 * This ensures consistent UX and proper memory cleanup by restarting ESP32.
 */

import * as m from '$lib/paraglide/messages.js';
import { confirm } from './dialogs';
import { openModal } from './modal';
import { RestartProgressDialog } from '$lib/components';
import Power from '~icons/tabler/reload';
import Cancel from '~icons/tabler/x';

// Constants
const MODAL_TRANSITION_DELAY_MS = 50;

export interface RestartConfirmOptions {
	/** Dialog title (default: 'Confirm Restart') */
	title?: string;
	/** Dialog message explaining the restart reason */
	message: string;
	/** Confirm button label (default: 'Save & Restart') */
	confirmLabel?: string;
	/** Cancel button label (default: 'Cancel') */
	cancelLabel?: string;
	/** Success toast message shown after confirmation (default: 'Settings saved. Restarting...') */
	successMessage?: string;
	/** Duration to show success toast in ms (default: 4000) */
	successDuration?: number;
	/** Delay before page reload in ms (default: 4000) */
	reloadDelay?: number;
	/** Whether to auto-reload the page after save (default: true) */
	autoReload?: boolean;
	/** Function to trigger device restart via API (optional - if not provided, assumes backend triggers restart) */
	triggerRestart?: () => Promise<void>;
	/** Use hygiene sleep instead of full restart - faster (~100ms vs ~5s) for DRAM defragmentation */
	/** Use hygiene sleep instead of full restart - faster (~100ms vs ~5s) for DRAM defragmentation */
	useSleepInsteadOfRestart?: boolean;
	/** Callback function to execute when the dialog is cancelled */
	onCancel?: () => void;
}

export function confirmRestartAndSave(
	onSave: () => Promise<unknown>,
	options: RestartConfirmOptions
): void {
	const {
		title = m.restart_dialog_title_confirm(),
		message,
		confirmLabel = m.restart_btn_save_restart(),
		cancelLabel = m.action_cancel(),
		triggerRestart,
		useSleepInsteadOfRestart,
		onCancel
	} = options;

	confirm({
		title,
		message,
		onCancel,
		labels: {
			cancel: { label: cancelLabel, icon: Cancel },
			confirm: { label: confirmLabel, icon: Power }
		},
		onConfirm: async () => {
			// Small delay to ensure the confirm dialog transition completes.
			await new Promise((resolve) => setTimeout(resolve, MODAL_TRANSITION_DELAY_MS));

			// Open progress dialog that handles save, restart, and reload
			openModal(RestartProgressDialog, {
				title: useSleepInsteadOfRestart
					? m.restart_dialog_title_applying()
					: m.restart_dialog_title_restarting(),
				onSave,
				triggerRestart,
				useSleepInsteadOfRestart,
				savingMessage: m.restart_stage_saving(),
				restartingMessage: useSleepInsteadOfRestart
					? m.restart_stage_resetting()
					: m.restart_stage_restarting(),
				waitingMessage: m.restart_stage_waiting(),
				successMessage: m.restart_stage_success()
			});
		}
	});
}
