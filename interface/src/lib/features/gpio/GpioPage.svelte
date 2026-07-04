<script lang="ts">
	import Refresh from '~icons/tabler/refresh';
	import Cpu from '~icons/tabler/cpu-2';
	import Input from '~icons/tabler/login-2';
	import Output from '~icons/tabler/logout-2';
	import { PageWrapper } from '$lib/components/layout';
	import LoadingCard from '$lib/components/layout/LoadingCard.svelte';
	import SettingsCard from '$lib/components/layout/SettingsCard.svelte';
	import { FormButton } from '$lib/components/shared/forms';
	import { i18n } from '$lib/i18n.svelte';
	import * as m from '$lib/paraglide/messages.js';
	import type { GpioChannelConfig, GpioMode } from '$lib/types/domain/gpio';
	import { useGpioManagement } from './useGpioManagement.svelte';
	import GpioStatusSummary from './components/GpioStatusSummary.svelte';
	import GpioChannelRow from './components/GpioChannelRow.svelte';

	const gpio = useGpioManagement({ shouldLoad: () => true });
	const config = $derived(gpio.config);
	const channels = $derived(config?.channels ?? []);
	const inputChannels = $derived(channels.filter((channel) => channel.mode === 'input'));
	const outputChannels = $derived(channels.filter((channel) => channel.mode === 'output'));
	const availableChannels = $derived(channels.filter((channel) => channel.mode === 'disabled'));
	const safePinsCount = $derived(gpio.pins.length || channels.length);
	const controlsDisabled = $derived(!gpio.canManage || gpio.saving);
	const pinsById = $derived.by(() => {
		const map = new Map(gpio.pins.map((pin) => [pin.id, pin]));
		return map;
	});

	function setChannelMode(channel: GpioChannelConfig, mode: GpioMode) {
		gpio.updateChannel(channel.id, { mode });
	}
</script>

<PageWrapper>
	{#if gpio.error}
		<div class="alert alert-warning mb-3">
			<span>{m.error_prefix({ error: gpio.error }, { locale: i18n.languageTag })}</span>
		</div>
	{/if}

	<div class="flex flex-col gap-4">
		{#if gpio.loading && !config}
			<LoadingCard
				title={m.gpio_status_title({ locale: i18n.languageTag })}
				icon={Cpu}
				loading={true}
			/>
			<LoadingCard
				title={m.gpio_config_title({ locale: i18n.languageTag })}
				icon={Cpu}
				loading={true}
			/>
		{:else if config}
			<GpioStatusSummary status={gpio.status} pinsCount={safePinsCount} />

			<SettingsCard
				title={m.gpio_config_title({ locale: i18n.languageTag })}
				icon={Cpu}
				hasChanges={gpio.hasChanges}
				saving={gpio.saving}
				disabled={!gpio.canManage}
				onSave={gpio.save}
				onReset={gpio.reset}
				dirtySourceId="gpio-config"
			>
				{#snippet actions()}
					<FormButton
						label=""
						icon={Refresh}
						variant="ghost"
						size="sm"
						class="btn-circle"
						onclick={() => void gpio.refreshStatus()}
						disabled={gpio.refreshing}
						aria-label={m.action_refresh_status({ locale: i18n.languageTag })}
						title={m.action_refresh_status({ locale: i18n.languageTag })}
					/>
				{/snippet}

				<div class="flex flex-col gap-4">
					<section class="flex flex-col gap-2">
						<div class="flex items-center justify-between gap-3">
							<h3 class="flex min-w-0 items-center gap-2 text-sm font-bold">
								<Input class="h-4 w-4 opacity-70" />
								<span class="truncate"
									>{m.gpio_alarm_inputs_title({ locale: i18n.languageTag })}</span
								>
								<span class="badge badge-sm">{inputChannels.length}</span>
							</h3>
						</div>

						{#if inputChannels.length === 0}
							<div
								class="rounded-md border border-dashed border-base-300/70 px-3 py-2 text-sm opacity-70"
							>
								{m.gpio_no_alarm_inputs({ locale: i18n.languageTag })}
							</div>
						{:else}
							<div class="flex flex-col gap-2">
								{#each inputChannels as channel (channel.id)}
									<GpioChannelRow
										{channel}
										status={gpio.statusById.get(channel.id)}
										pin={pinsById.get(channel.id)}
										canManage={gpio.canManage}
										hasChanges={gpio.hasChanges}
										saving={gpio.saving}
										outputBusy={gpio.outputBusyId === channel.id}
										onChange={gpio.updateChannel}
										onOutputChange={gpio.setOutput}
									/>
								{/each}
							</div>
						{/if}
					</section>

					<section class="flex flex-col gap-2">
						<div class="flex items-center justify-between gap-3">
							<h3 class="flex min-w-0 items-center gap-2 text-sm font-bold">
								<Output class="h-4 w-4 opacity-70" />
								<span class="truncate">{m.gpio_outputs_title({ locale: i18n.languageTag })}</span>
								<span class="badge badge-sm">{outputChannels.length}</span>
							</h3>
						</div>

						{#if outputChannels.length === 0}
							<div
								class="rounded-md border border-dashed border-base-300/70 px-3 py-2 text-sm opacity-70"
							>
								{m.gpio_no_outputs({ locale: i18n.languageTag })}
							</div>
						{:else}
							<div class="flex flex-col gap-2">
								{#each outputChannels as channel (channel.id)}
									<GpioChannelRow
										{channel}
										status={gpio.statusById.get(channel.id)}
										pin={pinsById.get(channel.id)}
										canManage={gpio.canManage}
										hasChanges={gpio.hasChanges}
										saving={gpio.saving}
										outputBusy={gpio.outputBusyId === channel.id}
										onChange={gpio.updateChannel}
										onOutputChange={gpio.setOutput}
									/>
								{/each}
							</div>
						{/if}
					</section>

					{#if availableChannels.length > 0}
						<section class="flex flex-col gap-2">
							<h3 class="flex min-w-0 items-center gap-2 text-sm font-bold">
								<Cpu class="h-4 w-4 opacity-70" />
								<span class="truncate"
									>{m.gpio_available_pins_title({ locale: i18n.languageTag })}</span
								>
								<span class="badge badge-sm">{availableChannels.length}</span>
							</h3>

							<div class="grid grid-cols-1 gap-2 md:grid-cols-2 xl:grid-cols-3">
								{#each availableChannels as channel (channel.id)}
									<div
										class="flex min-w-0 items-center justify-between gap-3 rounded-md border border-base-300/50 bg-base-100/20 px-3 py-2"
									>
										<div class="min-w-0">
											<div class="truncate text-sm font-bold">{channel.name}</div>
											<div class="truncate text-xs opacity-60">
												{m.gpio_pin({ pin: channel.pin }, { locale: i18n.languageTag })}
											</div>
										</div>
										<div class="flex shrink-0 items-center gap-1">
											<FormButton
												label={m.gpio_enable_input_short({ locale: i18n.languageTag })}
												icon={Input}
												variant="ghost"
												size="xs"
												disabled={controlsDisabled}
												onclick={() => setChannelMode(channel, 'input')}
												title={m.gpio_enable_input({ locale: i18n.languageTag })}
											/>
											<FormButton
												label={m.gpio_enable_output_short({ locale: i18n.languageTag })}
												icon={Output}
												variant="ghost"
												size="xs"
												disabled={controlsDisabled}
												onclick={() => setChannelMode(channel, 'output')}
												title={m.gpio_enable_output({ locale: i18n.languageTag })}
											/>
										</div>
									</div>
								{/each}
							</div>
						</section>
					{/if}
				</div>
			</SettingsCard>
		{/if}
	</div>
</PageWrapper>
