<script lang="ts">
	import { slide } from 'svelte/transition';
	import { cubicOut } from 'svelte/easing';
	import { Spinner } from '$lib/components';
	import BaseCard from '$lib/components/layout/BaseCard.svelte';
	import SettingsCard from '$lib/components/layout/SettingsCard.svelte';
	import { FormInput, FormButton } from '$lib/components/shared/forms';
	import UserListItem from './components/UserListItem.svelte';
	import { useUserManagement } from './useUserManagement.svelte';
	import EditUser from './EditUser.svelte';
	import AddUser from '~icons/tabler/user-plus';
	import Users from '~icons/tabler/users';
	import Lock from '~icons/tabler/lock';
	import Warning from '~icons/tabler/alert-triangle';
	import Magic from '~icons/tabler/wand';
	import { i18n } from '$lib/i18n.svelte';
	import * as m from '$lib/paraglide/messages.js';
	import { PageWrapper, GridLayout } from '$lib/components/layout';
	import ContentBox from '$lib/components/layout/ContentBox.svelte';

	const userManagement = useUserManagement(EditUser, {
		shouldLoad: () => true
	});
	const managementState = $derived(userManagement.state);
</script>

<PageWrapper>
	{#if managementState.error}
		<div class="alert alert-error mb-4">
			<Warning class="h-5 w-5 shrink-0" />
			<span>{m.error_prefix({ error: managementState.error }, { locale: i18n.languageTag })}</span>
		</div>
	{/if}
	<GridLayout cols={2}>
		<BaseCard title={m.user_manage_title({ locale: i18n.languageTag })} icon={Users}>
			{#snippet actions()}
				<FormButton
					label=""
					icon={AddUser}
					class="btn-primary btn-sm btn-circle"
					onclick={userManagement.openNewUserModal}
					ariaLabel={m.user_dialog_add_title({ locale: i18n.languageTag })}
					title={m.user_dialog_add_title({ locale: i18n.languageTag })}
				/>
			{/snippet}

			{#if managementState.isLoading}
				<div class="flex justify-center items-center py-8">
					<Spinner />
				</div>
			{:else}
				<div class="flex flex-col gap-1" transition:slide={{ duration: 300, easing: cubicOut }}>
					{#each managementState.securitySettings.users as userItem, index (userItem.username)}
						<UserListItem
							{userItem}
							onEdit={() => userManagement.openEditModal(index)}
							onDelete={() => userManagement.openDeleteConfirmation(index)}
						/>
					{/each}
				</div>
			{/if}
		</BaseCard>

		<SettingsCard
			title={m.user_security_title({ locale: i18n.languageTag })}
			icon={Lock}
			hasChanges={userManagement.isDirty}
			loading={managementState.isLoading}
			saving={managementState.isSaving}
			onSave={() => userManagement.saveSettings(managementState.securitySettings)}
			onReset={userManagement.resetSettings}
			dirtySourceId="user-security-settings"
		>
			{#if managementState.isLoading}
				<div class="flex justify-center items-center py-8">
					<Spinner />
				</div>
			{:else}
				<div class="flex flex-col gap-1">
					<ContentBox class="flex gap-3 items-center text-warning">
						<Warning class="h-5 w-5 shrink-0" />
						<span class="text-sm">{m.user_jwt_warning({ locale: i18n.languageTag })}</span>
					</ContentBox>
					<ContentBox>
						<ContentBox>
							<FormInput
								label={m.user_jwt_label({ locale: i18n.languageTag })}
								id="secret"
								type="password"
								bind:value={managementState.securitySettings.jwt_secret}
							>
								{#snippet suffix()}
									<button
										type="button"
										class="flex items-center text-base-content/50 hover:text-base-content focus:outline-none"
										onclick={userManagement.generateJwtSecret}
										aria-label={m.user_btn_generate_secret({ locale: i18n.languageTag })}
										title={m.user_btn_generate_secret({ locale: i18n.languageTag })}
									>
										<Magic class="h-4 w-4" />
									</button>
								{/snippet}
							</FormInput>
							<p class="mt-1 text-xs text-base-content/60 px-1">
								{m.user_jwt_hint({ locale: i18n.languageTag })}
							</p>
						</ContentBox>
					</ContentBox>
				</div>
			{/if}
		</SettingsCard>
	</GridLayout>
</PageWrapper>
