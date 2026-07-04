<script lang="ts">
	import { untrack } from 'svelte';
	import { Modal } from '$lib/components';
	import { FormButton, FormInput, FormToggle } from '$lib/components/shared/forms';
	import { closeModal } from '$lib/utils/ui/modal';
	import Cancel from '~icons/tabler/x';
	import Save from '~icons/tabler/device-floppy';
	import { i18n } from '$lib/i18n.svelte';
	import * as m from '$lib/paraglide/messages.js';
	import type { UserSetting } from '$lib/services/api/core/SecurityApiService';

	// provided by <Modals />

	interface Props {
		isOpen: boolean;
		title: string;
		onSaveUser: (_user: UserSetting) => void;
		user?: UserSetting;
	}

	let {
		isOpen,
		title,
		onSaveUser,
		user: _user = {
			username: '',
			password: '',
			admin: false
		}
	}: Props = $props();

	// Make passed object reactive to prevent Svelte warning 'binding_property_non_reactive'
	// https://github.com/sveltejs/svelte/issues/12320
	let user = $state(
		untrack(() => ({
			username: _user?.username ?? '',
			password: _user?.password ?? '',
			admin: _user?.admin ?? false
		}))
	);

	let errorUsername = $state(false);

	let usernameEditable = $state(user.username === '');
	let formElement: HTMLFormElement | undefined = $state();

	function handleSave() {
		// Validate if username is within range
		if (user.username.length < 3 || user.username.length > 32) {
			errorUsername = true;
		} else {
			errorUsername = false;
			// Callback on saving
			onSaveUser(user);
			closeModal();
		}
	}

	function preventDefault(fn: (_event: Event) => void) {
		return function (_event: Event) {
			_event.preventDefault();
			fn(_event);
		};
	}
</script>

<Modal {isOpen} onClose={() => closeModal()} {title} widthClass="w-full md:w-md min-w-fit max-w-md">
	<form
		class="fieldset text-base-content mb-1 w-full"
		onsubmit={preventDefault(handleSave)}
		novalidate
		bind:this={formElement}
	>
		<FormInput
			label={m.user_field_username({ locale: i18n.languageTag })}
			id="username"
			bind:value={user.username}
			minlength={3}
			maxlength={32}
			disabled={!usernameEditable}
			error={errorUsername ? m.user_error_username_len({ locale: i18n.languageTag }) : undefined}
		/>

		<div class="mt-3">
			<FormInput
				label={m.user_field_password({ locale: i18n.languageTag })}
				id="pwd"
				type="password"
				bind:value={user.password}
			/>
		</div>

		<div class="mt-4">
			<FormToggle
				label={m.user_form_admin({ locale: i18n.languageTag })}
				description={user.admin
					? m.user_form_admin_full({ locale: i18n.languageTag })
					: m.user_form_admin_limited({ locale: i18n.languageTag })}
				bind:checked={user.admin}
			/>
		</div>
	</form>

	<!-- Actions -->
	{#snippet actions()}
		<div class="flex justify-end gap-2 w-full">
			<FormButton
				label={m.action_cancel({ locale: i18n.languageTag })}
				icon={Cancel}
				type="button"
				class="btn-neutral"
				onclick={() => closeModal()}
			/>
			<FormButton
				label={m.action_save({ locale: i18n.languageTag })}
				icon={Save}
				type="button"
				onclick={() => formElement?.requestSubmit()}
			/>
		</div>
	{/snippet}
</Modal>
