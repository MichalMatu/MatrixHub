<script lang="ts">
	import { untrack } from 'svelte';
	import Avatar from '~icons/tabler/user-circle';
	import Logout from '~icons/tabler/logout';
	import { page } from '$app/state';
	import { useSessionAccess } from '$lib/features/auth/useSessionAccess.svelte';
	import { createMenuStructure } from '$lib/features/navigation/menuConfig';
	import { useSystemStatusReadModel } from '$lib/features/system/status/useSystemStatusReadModel.svelte';
	import { appFeatures } from '$lib/stores/appFeatures.svelte';
	import MenuItem from './menu/MenuItem.svelte';
	import { FormButton } from '$lib/components/shared/forms';
	import { i18n } from '$lib/i18n.svelte';
	import * as m from '$lib/paraglide/messages.js';

	let { closeMenu } = $props();
	const session = useSessionAccess();
	const statusModel = useSystemStatusReadModel();
	const features = $derived(appFeatures.features);
	const featuresResolved = $derived(appFeatures.resolved);

	let menuItems = $derived(
		createMenuStructure(
			{
				ntpEnabled: featuresResolved ? features.ntp : false,
				canManage: session.canManage,
				isStaConnected: statusModel.isStaConnected
			},
			i18n.languageTag,
			page.url.pathname
		)
	);

	let openGroup = $state<string | null>(null);

	$effect(() => {
		page.url.pathname;

		untrack(() => {
			const activeParent = menuItems.find((item) => item.submenu?.some((sub) => sub.active));
			openGroup = activeParent?.title ?? null;
		});
	});
</script>

<div class="bg-base-200 text-base-content flex h-full w-[min(20rem,100vw)] flex-col p-4">
	<!-- Sidebar content here -->
	<a href="/" class="rounded-box mb-4 flex items-center" onclick={closeMenu}>
		<h1 class="px-4 text-2xl font-bold">{m.app_title({ locale: i18n.languageTag })}</h1>
	</a>
	<ul class="menu w-full rounded-box menu-vertical flex-nowrap overflow-y-auto">
		{#each menuItems as menuItem (menuItem.title)}
			<MenuItem
				item={menuItem}
				{openGroup}
				onItemClick={closeMenu}
				onToggleGroup={(title) => {
					openGroup = title;
				}}
			/>
		{/each}
	</ul>

	<div class="flex-col"></div>
	<div class="grow"></div>

	<div class="flex flex-row justify-center gap-2 p-2">
		<FormButton
			label="EN"
			class="btn-xs {i18n.languageTag === 'en' ? 'btn-primary' : 'btn-ghost'}"
			onclick={() => i18n.setLocale('en')}
		/>
		<FormButton
			label="PL"
			class="btn-xs {i18n.languageTag === 'pl' ? 'btn-primary' : 'btn-ghost'}"
			onclick={() => i18n.setLocale('pl')}
		/>
	</div>

	{#if session.isAuthenticated}
		<div class="flex items-center">
			<Avatar class="h-8 w-8" />
			<span class="grow px-4 text-xl font-bold">{session.username}</span>
			<FormButton
				label=""
				icon={Logout}
				class="btn-ghost"
				onclick={() => {
					session.invalidate();
				}}
				title={m.action_sign_out({ locale: i18n.languageTag })}
				ariaLabel={m.action_sign_out({ locale: i18n.languageTag })}
			/>
		</div>
	{/if}
</div>
