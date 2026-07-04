<!-- @runes -->
<script module lang="ts">
	export type Breadcrumb = {
		label: string;
		path: string;
	};
</script>

<script lang="ts">
	import { FormButton } from '$lib/components/shared/forms';
	import ContentBox from '$lib/components/layout/ContentBox.svelte';
	import { i18n } from '$lib/i18n.svelte';
	import * as m from '$lib/paraglide/messages.js';
	import ArrowUpIcon from '~icons/tabler/arrow-up';
	import HomeIcon from '~icons/tabler/home';
	import RefreshIcon from '~icons/tabler/reload';

	interface Props {
		breadcrumbs?: Breadcrumb[];
		isLoading?: boolean;
		canGoParent?: boolean;
		backendLabel?: string;
		onNavigateRoot?: () => void;
		onSelectBreadcrumb?: (path: string) => void;
		onGoParent?: () => void;
		onRefresh?: () => void;
		children?: import('svelte').Snippet;
	}

	let {
		breadcrumbs = [],
		isLoading = false,
		canGoParent = false,
		backendLabel = 'LittleFS',
		onNavigateRoot = () => {},
		onSelectBreadcrumb = (path: string): void => {
			void path;
		},
		onGoParent = () => {},
		onRefresh = () => {},
		children
	}: Props = $props();
</script>

<ContentBox paddingClass="px-4 py-3">
	<div class="flex flex-col gap-4 lg:flex-row lg:items-center lg:justify-between">
		<div class="flex flex-wrap items-center gap-2">
			<FormButton
				variant="icon"
				size="xs"
				class="h-8 w-8"
				ariaLabel={m.file_manager_go_root({ locale: i18n.languageTag })}
				title={m.file_manager_go_root({ locale: i18n.languageTag })}
				onclick={onNavigateRoot}
				disabled={isLoading}
			>
				<HomeIcon class="h-4 w-4" />
			</FormButton>
			<span class="badge badge-sm badge-ghost text-[10px] font-medium uppercase tracking-wide">
				{backendLabel}
			</span>
			{#each breadcrumbs as crumb (crumb.path)}
				<FormButton
					variant="ghost"
					size="xs"
					class="min-h-0 h-8 px-3"
					onclick={() => onSelectBreadcrumb(crumb.path)}
					disabled={isLoading}
				>
					{crumb.label}
				</FormButton>
			{/each}
		</div>
		<div class="flex flex-wrap items-center gap-2">
			{@render children?.()}
			<FormButton
				variant="icon"
				size="sm"
				class="h-9 w-9"
				ariaLabel={m.file_manager_go_parent({ locale: i18n.languageTag })}
				title={m.file_manager_go_parent({ locale: i18n.languageTag })}
				onclick={onGoParent}
				disabled={!canGoParent || isLoading}
			>
				<ArrowUpIcon class="h-4 w-4" />
			</FormButton>
			<FormButton
				variant="icon"
				size="sm"
				class="h-9 w-9"
				ariaLabel={m.file_manager_refresh({ locale: i18n.languageTag })}
				title={m.file_manager_refresh({ locale: i18n.languageTag })}
				onclick={onRefresh}
				disabled={isLoading}
			>
				<RefreshIcon class="h-4 w-4" />
			</FormButton>
		</div>
	</div>
</ContentBox>
