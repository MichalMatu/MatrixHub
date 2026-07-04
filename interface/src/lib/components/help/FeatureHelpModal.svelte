<script lang="ts">
	import { Modal } from '$lib/components';
	import ContentBox from '$lib/components/layout/ContentBox.svelte';
	import { FormButton } from '$lib/components/shared/forms';
	import { i18n } from '$lib/i18n.svelte';
	import * as m from '$lib/paraglide/messages.js';
	import X from '~icons/tabler/x';

	type HelpSection = {
		title: string;
		body: string;
	};

	type HelpLink = {
		href: string;
		label: string;
	};

	interface Props {
		isOpen: boolean;
		onClose: () => void;
		title: string;
		intro: string;
		sections: HelpSection[];
		links?: HelpLink[];
		widthClass?: string;
	}

	let {
		isOpen,
		onClose,
		title,
		intro,
		sections,
		links = [],
		widthClass = 'w-11/12 max-w-2xl'
	}: Props = $props();
</script>

<Modal {isOpen} {onClose} {title} {widthClass}>
	<div class="flex flex-col gap-4">
		<p class="text-sm leading-relaxed text-base-content/75 whitespace-pre-line">
			{intro}
		</p>

		<div class="grid grid-cols-1 gap-3">
			{#each sections as section (section.title)}
				<ContentBox title={section.title} class="border-base-300/70 bg-base-200/40">
					<p class="text-sm leading-relaxed text-base-content/70 whitespace-pre-line">
						{section.body}
					</p>
				</ContentBox>
			{/each}
		</div>

		{#if links.length > 0}
			<div class="space-y-2">
				<div class="text-xs font-semibold uppercase tracking-wide text-base-content/50">
					{m.help_links_label({ locale: i18n.languageTag })}
				</div>
				<div class="flex flex-wrap gap-2">
					{#each links as link (link.href)}
						<a href={link.href} class="btn btn-sm btn-outline">
							{link.label}
						</a>
					{/each}
				</div>
			</div>
		{/if}
	</div>

	{#snippet actions()}
		<div class="flex justify-end w-full">
			<FormButton
				label={m.action_close({ locale: i18n.languageTag })}
				icon={X}
				type="button"
				class="btn-neutral"
				onclick={onClose}
			/>
		</div>
	{/snippet}
</Modal>
