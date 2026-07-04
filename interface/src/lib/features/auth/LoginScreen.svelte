<script lang="ts">
	import { useLogin } from './useLogin.svelte';
	import LoginForm from './LoginForm.svelte';
	import BaseCard from '$lib/components/layout/BaseCard.svelte';
	import type { UserSessionNotice } from '$lib/stores/user';
	import './login.css';

	import { i18n } from '$lib/i18n.svelte';
	import * as m from '$lib/paraglide/messages.js';

	let { signIn: signInCallback = undefined } = $props();

	const loginState = useLogin(() => signInCallback);

	function getAuthNoticeMessage(notice: UserSessionNotice | null): string | null {
		if (notice === 'unauthorized') {
			return m.auth_error_unauthorized({ locale: i18n.languageTag });
		}
		return null;
	}

	const authNoticeMessage = $derived(getAuthNoticeMessage(loginState.authNotice));
</script>

<div class="hero login-hero min-h-screen bg-base-100">
	<BaseCard
		class="login-card {loginState.loginFailed ? 'border-error border-2' : ''}"
		title={m.login_title({ locale: i18n.languageTag })}
	>
		<div class="w-[min(20rem,100vw)]">
			{#if authNoticeMessage}
				<div class="alert alert-warning mb-4 text-sm">
					<span>{authNoticeMessage}</span>
				</div>
			{/if}

			<LoginForm
				bind:username={loginState.username}
				bind:password={loginState.password}
				loading={loginState.isSubmitting}
				onSubmit={() => {
					void loginState.signInUser({
						username: loginState.username,
						password: loginState.password
					});
				}}
			/>
		</div>
	</BaseCard>
</div>
