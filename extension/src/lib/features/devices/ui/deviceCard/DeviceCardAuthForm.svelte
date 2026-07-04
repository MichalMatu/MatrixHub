<script lang="ts">
  import Button from "$lib/components/ui/Button.svelte";
  import { i18n } from "$lib/i18n/store";

  export let isSigningIn = false;
  export let username = "";
  export let password = "";
  export let usernameError: string | undefined = undefined;
  export let passwordError: string | undefined = undefined;
  export let onSignIn: () => void | Promise<void> = () => undefined;
  export let onOpenDevice: () => void | Promise<void> = () => undefined;
  export let onRemove: () => void | Promise<void> = () => undefined;
</script>

<form class="stack compact-login" on:submit|preventDefault={onSignIn}>
  <label class="field">
    <span>{$i18n.t("forms.auth.username")}</span>
    <input bind:value={username} autocomplete="username" required />
    {#if usernameError}
      <small class="field-help error">{usernameError}</small>
    {/if}
  </label>

  <label class="field">
    <span>{$i18n.t("forms.auth.password")}</span>
    <input bind:value={password} type="password" autocomplete="current-password" required />
    {#if passwordError}
      <small class="field-help error">{passwordError}</small>
    {/if}
  </label>

  <div class="button-row">
    <Button variant="primary" type="submit" disabled={isSigningIn}>
      {isSigningIn
        ? $i18n.t("forms.auth.signingIn")
        : $i18n.t("forms.auth.signIn")}
    </Button>
    <Button variant="ghost" onClick={onOpenDevice}>
      {$i18n.t("common.open")}
    </Button>
    <Button variant="ghost" tone="danger" onClick={onRemove}>
      {$i18n.t("common.remove")}
    </Button>
  </div>
</form>
