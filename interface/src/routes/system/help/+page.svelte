<script lang="ts">
	import { GridLayout, PageWrapper } from '$lib/components/layout';
	import BaseCard from '$lib/components/layout/BaseCard.svelte';
	import ContentBox from '$lib/components/layout/ContentBox.svelte';
	import { useSessionAccess } from '$lib/features/auth/useSessionAccess.svelte';
	import { i18n } from '$lib/i18n.svelte';
	import * as m from '$lib/paraglide/messages.js';
	import {
		createFeatureLinks,
		createHelpLinkGroups,
		type FeatureLink,
		type FeatureLinkGroup
	} from '$lib/features/navigation/featureRegistry';
	import HelpCircle from '~icons/tabler/help-circle';
	import Bell from '~icons/tabler/bell';
	import AlertTriangle from '~icons/tabler/alert-triangle';
	import Settings from '~icons/tabler/settings';

	type HelpItem = {
		title: string;
		description: string;
	};

	const session = useSessionAccess();
	const locale = $derived(i18n.languageTag);

	const alertLinks = $derived.by((): FeatureLink[] => {
		return createHelpLinkGroups(['alerts'], i18n.languageTag)[0]?.links ?? [];
	});

	const troubleshootingItems = $derived.by((): HelpItem[] => {
		const locale = i18n.languageTag;

		return [
			{
				title: m.help_troubleshooting_issue_1_title({ locale }),
				description: m.help_troubleshooting_issue_1_desc({ locale })
			},
			{
				title: m.help_troubleshooting_issue_2_title({ locale }),
				description: m.help_troubleshooting_issue_2_desc({ locale })
			},
			{
				title: m.help_troubleshooting_issue_3_title({ locale }),
				description: m.help_troubleshooting_issue_3_desc({ locale })
			},
			{
				title: m.help_troubleshooting_issue_4_title({ locale }),
				description: m.help_troubleshooting_issue_4_desc({ locale })
			}
		];
	});

	const troubleshootingLinks = $derived.by((): FeatureLink[] => {
		return createFeatureLinks(['wifi_sta', 'wifi_ap', 'status', 'logs'], i18n.languageTag);
	});

	const quickLinkGroups = $derived.by((): FeatureLinkGroup[] => {
		return createHelpLinkGroups(
			['setup', 'notify', 'diagnostics', 'integrations', 'system'],
			i18n.languageTag
		);
	});

	function isLinkAvailable(link: FeatureLink) {
		return !link.adminOnly || session.canManage;
	}
</script>

<svelte:head>
	<title>{m.menu_help({ locale })}</title>
</svelte:head>

<PageWrapper>
	<BaseCard title={m.help_intro_title({ locale })} icon={HelpCircle}>
		<div class="space-y-4">
			<p class="text-sm leading-relaxed text-base-content/75">
				{m.help_intro_body({ locale })}
			</p>

			<ol class="list-decimal space-y-2 pl-5 text-sm leading-relaxed text-base-content/75">
				<li>{m.help_intro_step_1({ locale })}</li>
				<li>{m.help_intro_step_2({ locale })}</li>
				<li>{m.help_intro_step_3({ locale })}</li>
			</ol>

			<ContentBox class="border-info/30 bg-info/10 text-sm leading-relaxed text-base-content/80">
				{@html m.help_intro_note(
					{
						hostname:
							'<code class="rounded bg-base-300 px-1 font-mono text-xs sm:text-sm">matrixhub.local</code>',
						ip: '<code class="rounded bg-base-300 px-1 font-mono text-xs sm:text-sm">192.168.4.1</code>'
					},
					{ locale }
				)}
			</ContentBox>

			<ContentBox
				title={m.help_contextual_modals_title({ locale })}
				class="border-base-300/70 bg-base-200/40"
			>
				<p class="text-sm leading-relaxed text-base-content/75">
					{m.help_contextual_modals_body({ locale })}
				</p>
			</ContentBox>
		</div>
	</BaseCard>

	<BaseCard title={m.help_alarm_flow_title({ locale })} icon={Bell} class="mt-6">
		<div class="space-y-4">
			<p class="text-sm leading-relaxed text-base-content/75">
				{m.help_alarm_flow_body({ locale })}
			</p>

			<ul class="space-y-2 text-sm leading-relaxed text-base-content/75">
				<li>{m.help_alarm_flow_point_1({ locale })}</li>
				<li>{m.help_alarm_flow_point_2({ locale })}</li>
				<li>{m.help_alarm_flow_point_3({ locale })}</li>
			</ul>

			<ContentBox
				class="border-warning/30 bg-warning/10 text-sm leading-relaxed text-base-content/80"
			>
				{m.help_alarm_flow_note({ locale })}
			</ContentBox>

			<div class="space-y-2">
				<div class="text-xs font-semibold uppercase tracking-wide text-base-content/50">
					{m.help_links_label({ locale })}
				</div>

				<div class="flex flex-wrap gap-2">
					{#each alertLinks as link (link.href)}
						{#if isLinkAvailable(link)}
							<a href={link.href} class="btn btn-sm btn-outline">
								{link.label}
							</a>
						{:else}
							<span
								class="btn btn-sm btn-disabled pointer-events-none"
								title={m.menu_locked_admin({ locale })}
							>
								{link.label}
							</span>
						{/if}
					{/each}
				</div>
			</div>
		</div>
	</BaseCard>

	<GridLayout cols={2} marginTop>
		<BaseCard title={m.help_troubleshooting_title({ locale })} icon={AlertTriangle} class="h-full">
			<div class="flex h-full flex-col gap-4">
				<p class="text-sm leading-relaxed text-base-content/75">
					{m.help_troubleshooting_body({ locale })}
				</p>

				<div class="space-y-3">
					{#each troubleshootingItems as item (item.title)}
						<ContentBox title={item.title} class="border-base-300/70 bg-base-200/40">
							<p class="text-sm leading-relaxed text-base-content/70">
								{item.description}
							</p>
						</ContentBox>
					{/each}
				</div>

				<div class="mt-auto space-y-2">
					<div class="text-xs font-semibold uppercase tracking-wide text-base-content/50">
						{m.help_links_label({ locale })}
					</div>

					<div class="flex flex-wrap gap-2">
						{#each troubleshootingLinks as link (link.href)}
							<a href={link.href} class="btn btn-sm btn-outline">
								{link.label}
							</a>
						{/each}
					</div>
				</div>
			</div>
		</BaseCard>

		<BaseCard title={m.help_quick_links_title({ locale })} icon={Settings} class="h-full">
			<div class="flex h-full flex-col gap-4">
				<p class="text-sm leading-relaxed text-base-content/75">
					{m.help_quick_links_body({ locale })}
				</p>

				<div class="space-y-3">
					{#each quickLinkGroups as group (group.title)}
						<ContentBox title={group.title} class="border-base-300/70 bg-base-200/40">
							<div class="flex flex-wrap gap-2">
								{#each group.links as link (link.href)}
									{#if isLinkAvailable(link)}
										<a href={link.href} class="btn btn-sm btn-outline">
											{link.label}
										</a>
									{:else}
										<span
											class="btn btn-sm btn-disabled pointer-events-none"
											title={m.menu_locked_admin({ locale })}
										>
											{link.label}
										</span>
									{/if}
								{/each}
							</div>
						</ContentBox>
					{/each}
				</div>
			</div>
		</BaseCard>
	</GridLayout>
</PageWrapper>
