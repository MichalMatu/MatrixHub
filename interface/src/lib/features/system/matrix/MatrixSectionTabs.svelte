<script lang="ts">
	import { page } from '$app/state';
	import GridDots from '~icons/tabler/grid-dots';
	import Bell from '~icons/tabler/bell';
	import Wand from '~icons/tabler/wand';
	import ChartBar from '~icons/tabler/chart-bar';
	import * as m from '$lib/paraglide/messages.js';
	import { MATRIX_DISPLAY_TAB_PATH, isMatrixTabPath, rememberMatrixTabPath } from './matrixNavigation';

	const sections = $derived([
		{
			href: '/system/matrix',
			label: m.matrix_tab_display(),
			icon: GridDots
		},
		{
			href: '/system/matrix/alarms',
			label: m.matrix_tab_alarms(),
			icon: Bell
		},
		{
			href: '/system/matrix/effects',
			label: m.matrix_tab_effects(),
			icon: Wand
		},
		{
			href: '/system/matrix/data',
			label: m.matrix_tab_data_visualization(),
			icon: ChartBar
		}
	]);

	$effect(() => {
		const path = page.url.pathname;
		if (path !== MATRIX_DISPLAY_TAB_PATH && isMatrixTabPath(path)) {
			rememberMatrixTabPath(path);
		}
	});
</script>

<nav class="tabs tabs-box mb-3 w-full sm:mb-4 sm:w-auto" aria-label={m.matrix_nav_label()}>
	{#each sections as section}
		<a
			href={section.href}
			class="tab gap-2 text-sm"
			class:tab-active={page.url.pathname === section.href}
			aria-current={page.url.pathname === section.href ? 'page' : undefined}
			onclick={() => rememberMatrixTabPath(section.href)}
		>
			<section.icon class="h-4 w-4" />
			<span>{section.label}</span>
		</a>
	{/each}
</nav>
