<script lang="ts">
	import ContentBox from '$lib/components/layout/ContentBox.svelte';
	import { FormInput } from '$lib/components/shared/forms';
	import * as m from '$lib/paraglide/messages.js';
	import { i18n } from '$lib/i18n.svelte';

	let { settings, updateSetting, errors } = $props<{
		settings: {
			pushover_enabled: boolean;
			pushover_user: string;
			pushover_token: string;
		};
		updateSetting: (key: 'pushover_user' | 'pushover_token', value: string) => void;
		errors: {
			pushover_user: boolean;
			pushover_token: boolean;
		};
	}>();
</script>

<div class="flex flex-col gap-1">
	<!-- User Key -->
	<ContentBox>
		<FormInput
			label={m.pushover_user_key({ locale: i18n.languageTag })}
			placeholder={m.pushover_user_placeholder({ locale: i18n.languageTag })}
			type="password"
			autocomplete="new-password"
			value={settings.pushover_user}
			oninput={(e) => updateSetting('pushover_user', (e.target as HTMLInputElement).value)}
			error={errors.pushover_user ? m.pushover_user_error({ locale: i18n.languageTag }) : undefined}
			maxlength={30}
		/>
	</ContentBox>

	<!-- API Token -->
	<ContentBox>
		<FormInput
			label={m.pushover_api_token({ locale: i18n.languageTag })}
			placeholder={m.pushover_token_placeholder({ locale: i18n.languageTag })}
			type="password"
			autocomplete="new-password"
			value={settings.pushover_token}
			oninput={(e) => updateSetting('pushover_token', (e.target as HTMLInputElement).value)}
			error={errors.pushover_token
				? m.pushover_token_error({ locale: i18n.languageTag })
				: undefined}
			maxlength={30}
		/>
	</ContentBox>

	<!-- Info & Link -->
	<ContentBox>
		<p class="text-base-content/70 text-sm mb-3">
			{m.pushover_desc_box({ locale: i18n.languageTag })}
		</p>

		<div class="mb-4">
			<details class="text-sm">
				<summary class="cursor-pointer text-sm font-medium opacity-80"
					>{m.pushover_platforms_title({ locale: i18n.languageTag })}</summary
				>
				<div class="mt-2 text-xs opacity-70 space-y-1 ml-2">
					<p class="mb-2">
						{m.pushover_platforms_list({ locale: i18n.languageTag })}
					</p>
				</div>
			</details>
		</div>

		<div class="alert shadow-sm bg-base-100/50">
			<div class="text-xs opacity-70">
				{m.pushover_register_link({ locale: i18n.languageTag })}
				<a href="https://pushover.net/" target="_blank" class="link link-primary font-bold ml-1"
					>pushover.net</a
				>
			</div>
		</div>
	</ContentBox>
</div>
