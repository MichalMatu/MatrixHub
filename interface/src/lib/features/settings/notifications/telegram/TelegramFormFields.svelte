<script lang="ts">
	import { FormInput, FormToggle } from '$lib/components/shared/forms';
	import ContentBox from '$lib/components/layout/ContentBox.svelte';
	import { i18n } from '$lib/i18n.svelte';
	import * as m from '$lib/paraglide/messages.js';

	interface TelegramFormSettings {
		enabled: boolean;
		bot_token: string;
		chat_id: string;
		commands_enabled: boolean;
	}

	let {
		settings,
		updateSetting,
		formErrors
	}: {
		settings: TelegramFormSettings;
		updateSetting: (key: keyof TelegramFormSettings, value: string | boolean) => void;
		formErrors: { bot_token: boolean };
	} = $props();

	// Toggle synchronization handlers
	function handleEnabledChange(checked: boolean) {
		updateSetting('enabled', checked);
		// When disabling Telegram, also disable commands
		if (!checked) {
			updateSetting('commands_enabled', false);
		}
	}

	function handleCommandsEnabledChange(checked: boolean) {
		updateSetting('commands_enabled', checked);
		// When enabling commands, also enable Telegram
		if (checked && !settings.enabled) {
			updateSetting('enabled', true);
		}
	}

	type CommandEntry = {
		command: string;
		description: string;
		nested?: boolean;
	};

	type CommandGroup = {
		icon: string;
		title: string;
		items: CommandEntry[];
	};

	function getTopLevelCommands(): CommandEntry[] {
		const locale = i18n.languageTag;
		return [
			{
				command: '/help',
				description: m.telegram_cmd_help({ locale })
			}
		];
	}

	function getCommandGroups(): CommandGroup[] {
		const locale = i18n.languageTag;

		return [
			{
				icon: '📡',
				title: m.telegram_cmd_group_status({ locale }),
				items: [
					{ command: '/status', description: m.telegram_cmd_status({ locale }) },
					{ command: '/sensors', description: m.telegram_cmd_sensors({ locale }) },
					{ command: '/health', description: m.telegram_cmd_health({ locale }) },
					{ command: '/ip', description: m.telegram_cmd_ip({ locale }) }
				]
			},
			{
				icon: '🔌',
				title: m.telegram_cmd_group_features({ locale }),
				items: [
					{ command: '/alarms', description: m.telegram_cmd_alarms({ locale }) },
					{ command: '/triggered', description: m.telegram_cmd_triggered({ locale }) },
					{ command: '/shelly', description: m.telegram_cmd_shelly({ locale }) },
					{ command: '/ble', description: m.telegram_cmd_ble({ locale }) },
					{ command: '/matrix', description: m.telegram_cmd_matrix({ locale }) }
				]
			},
			{
				icon: '🤖',
				title: m.telegram_cmd_group_automation({ locale }),
				items: [
					{ command: '/scripts', description: m.telegram_cmd_scripts({ locale }) },
					{ command: '/run', description: m.telegram_cmd_run({ locale }) },
					{ command: '/macro_stop', description: m.telegram_cmd_macro_stop({ locale }) }
				]
			},
			{
				icon: '🛠️',
				title: m.telegram_cmd_group_system({ locale }),
				items: [
					{ command: '/exec', description: m.telegram_cmd_exec({ locale }) },
					{
						command: '/exec cancel',
						description: m.telegram_cmd_exec_cancel({ locale }),
						nested: true
					},
					{
						command: '/exec status',
						description: m.telegram_cmd_exec_status({ locale }),
						nested: true
					},
					{ command: '/reboot', description: m.telegram_cmd_reboot({ locale }) },
					{ command: '/users', description: m.telegram_cmd_users({ locale }) }
				]
			}
		];
	}
</script>

<div class="flex flex-col gap-1">
	<!-- Enable -->
	<FormToggle
		label={m.telegram_enable({ locale: i18n.languageTag })}
		description={settings.enabled
			? m.telegram_enabled_desc({ locale: i18n.languageTag })
			: m.telegram_disabled_desc({ locale: i18n.languageTag })}
		checked={settings.enabled}
		onchange={(e) => handleEnabledChange((e.target as HTMLInputElement).checked)}
	/>

	<!-- Commands Enable -->
	<ContentBox>
		<div class="border-b border-base-content/10 pb-4 mb-4">
			<FormToggle
				label={m.telegram_commands_enable({ locale: i18n.languageTag })}
				description={m.telegram_commands_desc({ locale: i18n.languageTag })}
				checked={settings.commands_enabled}
				onchange={(e) => handleCommandsEnabledChange((e.target as HTMLInputElement).checked)}
				plain
				class="flex items-center justify-between"
			/>
		</div>
		<div>
			<details class="text-sm">
				<summary class="cursor-pointer text-sm font-medium opacity-80"
					>{m.telegram_commands_available({ locale: i18n.languageTag })}</summary
				>
				<div class="mt-3 space-y-3 text-xs">
					<div class="rounded-xl border border-base-content/10 bg-base-200/40 p-3">
						<div
							class="mb-2 flex items-center gap-2 text-[0.65rem] font-semibold uppercase tracking-[0.14em] opacity-60"
						>
							<span>📋</span>
							<span>{m.telegram_commands_available({ locale: i18n.languageTag })}</span>
						</div>
						<div class="space-y-2 opacity-80">
							{#each getTopLevelCommands() as item}
								<div class="flex flex-wrap items-start gap-2">
									<code
										class="min-w-[9rem] rounded bg-base-300 px-1.5 py-0.5 font-mono text-[0.72rem]"
									>
										{item.command}
									</code>
									<span class="leading-5">{item.description}</span>
								</div>
							{/each}
						</div>
					</div>

					{#each getCommandGroups() as group}
						<div class="rounded-xl border border-base-content/10 bg-base-200/40 p-3">
							<div
								class="mb-2 flex items-center gap-2 text-[0.65rem] font-semibold uppercase tracking-[0.14em] opacity-60"
							>
								<span>{group.icon}</span>
								<span>{group.title}</span>
							</div>
							<div class="space-y-2 opacity-80">
								{#each group.items as item}
									<div
										class={`flex flex-wrap items-start gap-2 ${item.nested ? 'pl-4 opacity-90' : ''}`}
									>
										<code
											class="min-w-[9rem] rounded bg-base-300 px-1.5 py-0.5 font-mono text-[0.72rem]"
										>
											{item.command}
										</code>
										<span class="leading-5">{item.description}</span>
									</div>
								{/each}
							</div>
						</div>
					{/each}
				</div>
			</details>
		</div>
	</ContentBox>

	<!-- Bot Token -->
	<ContentBox>
		<FormInput
			label={m.telegram_bot_token({ locale: i18n.languageTag })}
			id="bot_token"
			type="password"
			autocomplete="new-password"
			value={settings.bot_token}
			oninput={(e) => updateSetting('bot_token', (e.target as HTMLInputElement).value)}
			placeholder={m.telegram_bot_token_placeholder({ locale: i18n.languageTag })}
			error={formErrors.bot_token
				? m.telegram_bot_token_error({ locale: i18n.languageTag })
				: undefined}
			help={m.telegram_bot_token_hint({ locale: i18n.languageTag })}
			required={settings.enabled}
			maxlength={63}
		/>
	</ContentBox>

	<!-- Chat ID (auto-discovered) -->
	<ContentBox>
		<FormInput
			label={m.telegram_chat_id({ locale: i18n.languageTag })}
			id="chat_id"
			value={settings.chat_id || ''}
			placeholder={m.telegram_chat_id_placeholder({ locale: i18n.languageTag })}
			readonly
			help={settings.chat_id
				? m.telegram_chat_id_success({ locale: i18n.languageTag })
				: m.telegram_chat_id_hint({ locale: i18n.languageTag })}
			disabled
		/>
	</ContentBox>
</div>
