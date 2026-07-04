<script lang="ts">
	import { FormInput, FormButton } from '$lib/components/shared/forms';
	import Login from '~icons/tabler/login';
	import { i18n } from '$lib/i18n.svelte';
	import * as m from '$lib/paraglide/messages.js';

	let { username = $bindable(), password = $bindable(), onSubmit, loading = false } = $props();
</script>

<form
	class="fieldset w-full max-w-xs"
	aria-busy={loading}
	onsubmit={(e) => {
		e.preventDefault();
		if (loading) return;
		onSubmit();
	}}
>
	<FormInput
		label={m.login_username({ locale: i18n.languageTag })}
		id="user"
		data-testid="login-username"
		autocomplete="username"
		bind:value={username}
		disabled={loading}
	/>

	<div class="mt-2">
		<FormInput
			label={m.login_password({ locale: i18n.languageTag })}
			id="pwd"
			type="password"
			data-testid="login-password"
			autocomplete="current-password"
			bind:value={password}
			disabled={loading}
		/>
	</div>

	<div class="card-actions mt-4 justify-end">
		<FormButton
			label={loading
				? m.login_pending({ locale: i18n.languageTag })
				: m.login_btn({ locale: i18n.languageTag })}
			icon={Login}
			type="submit"
			data-testid="login-submit"
			{loading}
		/>
	</div>
</form>
