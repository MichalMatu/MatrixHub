<script lang="ts">
	import { i18n } from '$lib/i18n.svelte';
	import * as m from '$lib/paraglide/messages.js';
	import ContentBox from '$lib/components/layout/ContentBox.svelte';

	const locale = $derived(i18n.languageTag);
</script>

<ContentBox>
	<div class="text-sm opacity-70 space-y-2 leading-relaxed">
		<details>
			<summary class="cursor-pointer font-bold">{m.telegram_setup_title({ locale })}</summary>
			<ol class="list-decimal ml-5 space-y-1 mt-2">
				<li>
					{@html m.telegram_setup_1(
						{ botfather: '<code class="px-1 bg-base-300 rounded">@BotFather</code>' },
						{ locale }
					)}
				</li>
				<li>
					{@html m.telegram_setup_2(
						{ newbot: '<code class="px-1 bg-base-300 rounded">/newbot</code>' },
						{ locale }
					)}
				</li>
				<li>
					{@html m.telegram_setup_3(
						{
							token_example: '<code class="px-1 bg-base-300 rounded">123456789:ABC...</code>'
						},
						{ locale }
					)}
				</li>
				<li>{m.telegram_setup_4({ locale })}</li>
				<li>
					{@html m.telegram_setup_5(
						{
							hello: `<code class="px-1 bg-base-300 rounded">${m.telegram_setup_example_message({
								locale
							})}</code>`
						},
						{ locale }
					)}
				</li>
				<li>{m.telegram_setup_6({ locale })}</li>
			</ol>
		</details>
	</div>
</ContentBox>
