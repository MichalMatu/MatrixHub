<script lang="ts">
	import LoginScreen from '$lib/features/auth/LoginScreen.svelte';

	let { signIn: signInCallback = undefined } = $props();
</script>

<LoginScreen signIn={signInCallback} />
