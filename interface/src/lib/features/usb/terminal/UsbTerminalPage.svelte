<script lang="ts">
	import { tick } from 'svelte';
	import BaseCard from '$lib/components/layout/BaseCard.svelte';
	import { FormButton, FormInput } from '$lib/components/shared/forms';
	import { AdminAccessGate } from '$lib/components/layout';
	import LoadingCard from '$lib/components/layout/LoadingCard.svelte';
	import Terminal from '~icons/tabler/terminal-2';
	import Send from '~icons/tabler/send';
	import Stop from '~icons/tabler/player-stop';
	import Trash from '~icons/tabler/trash';
	import Settings from '~icons/tabler/settings';
	import { i18n } from '$lib/i18n.svelte';
	import * as m from '$lib/paraglide/messages.js';
	import UsbTerminalSettingsModal from './UsbTerminalSettingsModal.svelte';
	import { useUsbTerminalSettings } from './useUsbTerminalSettings.svelte';
	import { useUsbTerminalConsole } from './useUsbTerminalConsole.svelte';
	import { useUsbTerminalQuickScripts } from './useUsbTerminalQuickScripts.svelte';
	import { useSessionAccess } from '$lib/features/auth/useSessionAccess.svelte';
	const session = useSessionAccess();
	const canManage = $derived(session.canManage);
	const canUseConsole = $derived(session.canManage);
	const terminalState = useUsbTerminalSettings({
		shouldLoad: () => canManage
	});
	const consoleState = useUsbTerminalConsole({
		shouldInit: () => canUseConsole
	});
	const quickScriptsState = useUsbTerminalQuickScripts({
		shouldInit: () => canUseConsole
	});

	let consoleContainer: HTMLDivElement | undefined = $state();
	let autoscroll = $state(true);
	let settingsModalOpen = $state(false);

	function handleConsoleSubmit(event: Event) {
		event.preventDefault();
		consoleState.sendCommand();
	}

	function handleConsoleScroll() {
		if (!consoleContainer) return;
		const { scrollTop, scrollHeight, clientHeight } = consoleContainer;
		autoscroll = scrollHeight - scrollTop - clientHeight < 20;
	}

	function openSettingsModal() {
		terminalState.resetAdvancedDraft();
		settingsModalOpen = true;
	}

	function closeSettingsModal() {
		settingsModalOpen = false;
		terminalState.resetAdvancedDraft();
	}

	async function saveAdvancedSettings() {
		const saved = await terminalState.saveAdvancedSettings();
		if (saved) {
			settingsModalOpen = false;
		}
	}

	function getConnectionBadgeClass() {
		return consoleState.isConnected ? 'badge-success' : 'badge-ghost';
	}

	function getConnectionLabel() {
		return consoleState.isConnected
			? m.status_online({ locale: i18n.languageTag })
			: m.status_offline({ locale: i18n.languageTag });
	}

	function getTerminalBadgeClass() {
		if (!terminalState.enabled) return 'badge-warning';
		if (!consoleState.busy) return 'badge-success';
		if (consoleState.transport === 'telegram') return 'badge-warning';
		return consoleState.owner ? 'badge-info' : 'badge-warning';
	}

	function getTerminalLabel() {
		if (!terminalState.enabled) {
			return m.usb_terminal_status_disabled({ locale: i18n.languageTag });
		}
		if (!consoleState.busy) {
			return m.usb_terminal_status_idle({ locale: i18n.languageTag });
		}
		if (consoleState.transport === 'telegram') {
			return m.usb_terminal_status_busy_telegram({ locale: i18n.languageTag });
		}
		return m.usb_terminal_status_busy_web({ locale: i18n.languageTag });
	}

	function isDisableBlockedByActiveSession() {
		return consoleState.busy && terminalState.hasEnabledChanges && !terminalState.enabled;
	}

	$effect(() => {
		const _ = consoleState.entries.length;
		if (autoscroll && consoleContainer) {
			tick().then(() => {
				consoleContainer?.scrollTo({ top: consoleContainer.scrollHeight, behavior: 'smooth' });
			});
		}
	});
</script>

<AdminAccessGate allow={canManage}>
	{#if terminalState.loading}
		<LoadingCard
			title={m.usb_terminal_title({ locale: i18n.languageTag })}
			icon={Terminal}
			loading
		/>
	{:else if terminalState.hasConfig}
		<BaseCard
			title={m.usb_terminal_title({ locale: i18n.languageTag })}
			icon={Terminal}
			iconClass="h-6 w-6 text-primary flex-none"
		>
			{#snippet actions()}
				<div class="flex flex-wrap items-center justify-end gap-2">
					<span class={`badge badge-sm ${getConnectionBadgeClass()}`}>
						{getConnectionLabel()}
					</span>
					<span class={`badge badge-sm ${getTerminalBadgeClass()}`}>
						{getTerminalLabel()}
					</span>

					<FormButton
						type="button"
						variant="ghost"
						size="xs"
						icon={Settings}
						class="btn-circle"
						ariaLabel={m.usb_terminal_open_settings({ locale: i18n.languageTag })}
						title={m.usb_terminal_open_settings({ locale: i18n.languageTag })}
						onclick={openSettingsModal}
					/>
					{#if canUseConsole}
						<FormButton
							type="button"
							variant="ghost"
							size="xs"
							icon={Trash}
							class="btn-circle text-error"
							ariaLabel={m.action_clear({ locale: i18n.languageTag })}
							title={m.action_clear({ locale: i18n.languageTag })}
							onclick={() => consoleState.clearEntries()}
						/>
					{/if}

					<label class="flex items-center justify-end">
						<span class="sr-only">{m.usb_terminal_enable({ locale: i18n.languageTag })}</span>
						<input
							type="checkbox"
							class="toggle toggle-primary toggle-sm"
							checked={terminalState.enabled}
							disabled={terminalState.saving}
							onchange={(event) =>
								terminalState.confirmEnabledChange((event.target as HTMLInputElement).checked)}
						/>
					</label>
				</div>
			{/snippet}

			<div class="flex flex-col gap-4">
				{#if terminalState.error}
					<div class="alert alert-error">
						<span>{terminalState.error}</span>
					</div>
				{/if}

				{#if canUseConsole}
					{#if isDisableBlockedByActiveSession()}
						<div class="alert alert-warning text-sm">
							<span>{m.usb_terminal_disable_requires_cancel({ locale: i18n.languageTag })}</span>
						</div>
					{/if}

					<div
						bind:this={consoleContainer}
						onscroll={handleConsoleScroll}
						class="h-80 overflow-y-auto overflow-x-hidden rounded-xl border border-base-300/70 bg-base-300 p-2.5 font-mono text-[11px] sm:p-3 sm:text-xs"
					>
						{#if consoleState.entries.length === 0}
							<div class="opacity-60">
								{m.usb_terminal_empty({ locale: i18n.languageTag })}
							</div>
						{:else}
							<div class="flex flex-col gap-2">
								{#each consoleState.entries as entry (entry.id)}
									<div class="overflow-x-auto" data-testid="usb-terminal-entry">
										<pre
											class={`whitespace-pre-wrap leading-5 ${
												entry.phase === 'command'
													? 'text-primary'
													: entry.phase === 'interrupted'
														? 'text-warning'
														: entry.phase === 'status'
															? 'text-info'
															: ''
											}`}
											style="overflow-wrap: normal; word-break: normal; tab-size: 4;">
{entry.text}</pre>
									</div>
								{/each}
							</div>
						{/if}
					</div>

					<form class="flex flex-col gap-3 lg:flex-row lg:items-end" onsubmit={handleConsoleSubmit}>
						<div class="flex-1">
							<div
								class="mb-2 overflow-x-auto rounded-lg border border-base-300/70 bg-base-200/60 px-3 py-2 font-mono text-[11px] text-base-content/80 sm:text-xs"
								data-testid="usb-terminal-prompt"
							>
								<span class="whitespace-nowrap">{consoleState.currentPrompt}</span>
							</div>
							<FormInput
								id="usb-terminal-command"
								type="text"
								label={m.usb_terminal_command_label({ locale: i18n.languageTag })}
								placeholder={m.usb_terminal_command_placeholder({ locale: i18n.languageTag })}
								value={consoleState.command}
								oninput={(event) =>
									consoleState.updateCommand((event.target as HTMLInputElement).value)}
								disabled={!terminalState.enabled ||
									!consoleState.canExecute ||
									quickScriptsState.isTerminalCommandDisabled}
							/>
						</div>

						<div class="flex items-center justify-end gap-2 lg:shrink-0">
							<FormButton
								type="button"
								label={m.usb_terminal_stop({ locale: i18n.languageTag })}
								icon={Stop}
								variant="neutral"
								disabled={!consoleState.canCancel}
								onclick={() => consoleState.sendCancel()}
							/>
							<FormButton
								type="submit"
								label={m.keyboard_send({ locale: i18n.languageTag })}
								icon={Send}
								disabled={!terminalState.enabled ||
									!consoleState.canExecute ||
									quickScriptsState.isTerminalCommandDisabled}
							/>
						</div>
					</form>

					{#if quickScriptsState.shouldShowSection}
						<div class="rounded-xl border border-base-300/70 bg-base-100 p-4">
							<div class="flex flex-col gap-3">
								<div class="text-xs font-semibold uppercase tracking-wide opacity-60">
									{m.usb_terminal_quick_scripts_title({ locale: i18n.languageTag })}
								</div>

								{#if quickScriptsState.error}
									<div class="alert alert-error text-sm">
										<span>{quickScriptsState.error}</span>
									</div>
								{/if}

								{#if quickScriptsState.macrosEnabled === false}
									<div class="text-xs text-warning">
										{m.usb_terminal_quick_scripts_requires_macros({
											locale: i18n.languageTag
										})}
										<a href="/usb-features/macros" class="link link-primary ml-1 font-medium">
											{m.usb_terminal_quick_scripts_open_macros({
												locale: i18n.languageTag
											})}
										</a>
									</div>
								{/if}

								{#if quickScriptsState.loading}
									<div class="text-sm opacity-60">
										{m.usb_terminal_quick_scripts_loading({ locale: i18n.languageTag })}
									</div>
								{:else if quickScriptsState.scripts.length === 0}
									<div class="text-sm opacity-60">
										{m.usb_terminal_quick_scripts_empty({ locale: i18n.languageTag })}
									</div>
								{:else}
									<div class="flex flex-wrap gap-2">
										{#each quickScriptsState.scripts as script (script.name)}
											<FormButton
												type="button"
												size="sm"
												variant={quickScriptsState.isRunningScript(script.name)
													? 'danger'
													: 'ghost'}
												label={quickScriptsState.isRunningScript(script.name)
													? m.usb_terminal_stop({ locale: i18n.languageTag })
													: script.name}
												loading={quickScriptsState.pendingScriptName === script.name}
												disabled={quickScriptsState.isScriptDisabled(
													script.name,
													consoleState.busy
												)}
												onclick={() =>
													quickScriptsState.isRunningScript(script.name)
														? void quickScriptsState.stopScript()
														: void quickScriptsState.runScript(script.name)}
											/>
										{/each}
									</div>
								{/if}
							</div>
						</div>
					{/if}
				{/if}
			</div>
		</BaseCard>

		<UsbTerminalSettingsModal
			isOpen={settingsModalOpen}
			saving={terminalState.saving}
			targetPort={terminalState.advancedSettings.target_port}
			idleTimeout={terminalState.advancedSettings.idle_timeout_ms}
			errors={terminalState.errors}
			onClose={closeSettingsModal}
			onSave={() => void saveAdvancedSettings()}
			onTargetPortChange={terminalState.updateTargetPort}
			onIdleTimeoutChange={terminalState.updateIdleTimeout}
			onIdleTimeoutBlur={terminalState.normalizeIdleTimeout}
		/>
	{:else}
		<BaseCard
			title={m.usb_terminal_title({ locale: i18n.languageTag })}
			icon={Terminal}
			iconClass="h-6 w-6 text-primary flex-none"
		>
			<div class="alert alert-error">
				<span>{terminalState.error ?? m.settings_load_error({ locale: i18n.languageTag })}</span>
			</div>
		</BaseCard>
	{/if}
</AdminAccessGate>
