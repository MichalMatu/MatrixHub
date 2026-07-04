<script lang="ts">
	import { onMount } from 'svelte';
	import { browser } from '$app/environment';
	import { beforeNavigate, goto } from '$app/navigation';
	import { page } from '$app/state';
	import { Modals } from 'svelte-modals';
	import { appFeatures } from '$lib/stores/appFeatures.svelte';
	import { installGlobalErrorHandling } from '$lib/bootstrap/globalErrorHandling';
	import Toast from '$lib/components/toasts/Toast.svelte';
	import { fade } from 'svelte/transition';
	import { useSessionAccess } from '$lib/features/auth/useSessionAccess.svelte';
	import { themeStore } from '$lib/stores/theme.svelte';
	import { unsavedChanges } from '$lib/stores/unsavedChanges.svelte';
	import { confirm } from '$lib/utils/ui/dialogs';
	import { markAppPerformance, measureAppPerformance } from '$lib/utils/performanceMarks';
	import '../app.css';
	import AppShell from './AppShell.svelte';
	import Login from './login.svelte';
	import { i18n } from '$lib/i18n.svelte';
	import * as m from '$lib/paraglide/messages.js';
	import ArrowRight from '~icons/tabler/arrow-right';
	import X from '~icons/tabler/x';

	// Eagerly initialize the theme store for the whole app.
	void themeStore;
	void installGlobalErrorHandling();

	interface Props {
		children?: import('svelte').Snippet;
	}

	let { children }: Props = $props();
	const session = useSessionAccess();
	let allowUnsavedNavigation = false;

	if (browser) {
		beforeNavigate((navigation) => {
			if (allowUnsavedNavigation) {
				allowUnsavedNavigation = false;
				return;
			}
			if (!unsavedChanges.hasChanges) return;
			const target = navigation.to;
			if (!target || navigation.willUnload || target.route.id === null) return;

			navigation.cancel();
			confirm({
				title: m.unsaved_changes_title({ locale: i18n.languageTag }),
				message: m.unsaved_changes_confirm({ locale: i18n.languageTag }),
				labels: {
					cancel: {
						label: m.unsaved_changes_stay({ locale: i18n.languageTag }),
						icon: X
					},
					confirm: {
						label: m.unsaved_changes_leave({ locale: i18n.languageTag }),
						icon: ArrowRight
					}
				},
				onConfirm: () => {
					allowUnsavedNavigation = true;
					if (navigation.type === 'popstate') {
						history.go(navigation.delta);
						return;
					}
					void goto(target.url);
				}
			});
		});
	}

	onMount(() => {
		document.getElementById('matrixhub-boot-fallback')?.remove();
		markAppPerformance('matrixhub:boot:layout-mounted', { once: true });
		appFeatures.ensureBoot();

		const handleBeforeUnload = (event: BeforeUnloadEvent) => {
			if (!unsavedChanges.hasChanges) return;
			event.preventDefault();
			event.returnValue = m.unsaved_changes_confirm({ locale: i18n.languageTag });
		};

		window.addEventListener('beforeunload', handleBeforeUnload);
		return () => {
			window.removeEventListener('beforeunload', handleBeforeUnload);
		};
	});

	$effect(() => {
		if (!session.isAuthenticated) return;
		if (markAppPerformance('matrixhub:auth:ready', { once: true })) {
			measureAppPerformance(
				'matrixhub:boot-to-auth-ready',
				'matrixhub:boot:start',
				'matrixhub:auth:ready'
			);
		}
	});
</script>

<svelte:head>
	<title>{page.data?.title || m.app_title({ locale: i18n.languageTag })}</title>
</svelte:head>

{#if !session.isAuthenticated}
	<Login />
{:else}
	<AppShell {session}>
		{@render children?.()}
	</AppShell>
{/if}

<Modals>
	{#snippet backdrop({ close })}
		<div
			class="fixed inset-0 z-40 max-h-full max-w-full bg-black/20 backdrop-blur-sm"
			transition:fade|global
			onclick={() => close()}
			onkeydown={(e) => {
				if (e.key === 'Enter' || e.key === ' ') close();
			}}
			role="button"
			tabindex="0"
			aria-label={m.aria_close_modal({ locale: i18n.languageTag })}
		></div>
	{/snippet}
</Modals>

<Toast />
