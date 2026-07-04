<script lang="ts">
  import { tick } from "svelte";
  import { fly } from "svelte/transition";
  import type { DeviceRecord, DeviceSession } from "@matrixhub/device-sdk";
  import type { RealtimeConnectionState } from "$lib/features/realtime/socket/deviceOverviewSocket";
  import IconButton from "$lib/components/ui/IconButton.svelte";
  import { i18n } from "$lib/i18n/store";
  import { realtimeStatusLabel } from "./model";
  import DotsVertical from "~icons/tabler/dots-vertical";
  import Logout from "~icons/tabler/logout";
  import Open from "~icons/tabler/world";
  import Reload from "~icons/tabler/reload";
  import Settings from "~icons/tabler/settings";
  import Edit from "~icons/tabler/edit";
  import Check from "~icons/tabler/check";
  import X from "~icons/tabler/x";

  export let device: DeviceRecord;
  export let session: DeviceSession | null = null;
  export let isSelected = false;
  export let realtimeState: RealtimeConnectionState = "idle";
  export let onSelect: () => void | Promise<void> = () => undefined;
  export let onOpenDevice: () => void | Promise<void> = () => undefined;
  export let onLogout: () => void | Promise<void> = () => undefined;
  export let onOpenSettings: () => void | Promise<void> = () => undefined;
  export let onReconnect: () => void | Promise<void> = () => undefined;
  export let onRename: (name: string) => boolean | Promise<boolean> = async () =>
    true;

  let isEditingName = false;
  let isActionMenuOpen = false;
  let draftDeviceName = device.name;
  let nameInput: HTMLInputElement | null = null;

  function beginNameEdit() {
    closeActionMenu();
    isEditingName = true;
    draftDeviceName = device.name;
    tick().then(() => nameInput?.focus());
  }

  function cancelNameEdit() {
    isEditingName = false;
    draftDeviceName = device.name;
  }

  function getTrimmedDeviceName() {
    return draftDeviceName.trim();
  }

  async function submitNameEdit() {
    const nextName = getTrimmedDeviceName();
    if (!nextName) {
      return;
    }

    if (nextName === device.name) {
      cancelNameEdit();
      return;
    }

    const renamed = await onRename(nextName);
    if (renamed !== false) {
      isEditingName = false;
    }
  }

  function handleNameInputKeydown(event: KeyboardEvent) {
    if (event.key === "Escape") {
      event.preventDefault();
      cancelNameEdit();
    }
  }

  function closeActionMenu() {
    isActionMenuOpen = false;
  }

  function toggleActionMenu() {
    isActionMenuOpen = !isActionMenuOpen;
  }

  async function runAction(action: () => void | Promise<void>) {
    closeActionMenu();
    await action();
  }

  function handleWindowClick() {
    if (!isActionMenuOpen) {
      return;
    }

    closeActionMenu();
  }

  function handleWindowKeydown(event: KeyboardEvent) {
    if (event.key !== "Escape" || !isActionMenuOpen) {
      return;
    }

    event.preventDefault();
    closeActionMenu();
  }

  $: if (!isEditingName) {
    draftDeviceName = device.name;
  }
  $: if (!isSelected && isEditingName) {
    cancelNameEdit();
  }
  $: if ((!isSelected || isEditingName || !session) && isActionMenuOpen) {
    closeActionMenu();
  }
</script>

<svelte:window on:click={handleWindowClick} on:keydown={handleWindowKeydown} />

<div class="device-card-head">
  {#if isEditingName}
    <form class="device-card-name-editor" on:submit|preventDefault={submitNameEdit}>
      <div class="device-card-title">
        {#if isSelected && session}
          <span
            class="status-dot device-status-dot"
            data-state={realtimeState}
            role="img"
            aria-label={realtimeStatusLabel($i18n, realtimeState)}
            title={realtimeStatusLabel($i18n, realtimeState)}
          ></span>
        {/if}
        <label class="sr-only" for={`device-name-${device.id}`}>
          {$i18n.t("deviceCard.rename.titleLabel")}
        </label>
        <input
          bind:this={nameInput}
          bind:value={draftDeviceName}
          aria-label={$i18n.t("deviceCard.rename.titleLabel")}
          class="device-card-name-input"
          id={`device-name-${device.id}`}
          maxlength="64"
          on:keydown={handleNameInputKeydown}
        />
      </div>

      <div
        aria-label={$i18n.t("deviceCard.rename.actionsLabel")}
        class="device-card-actions"
      >
        <IconButton
          aria-label={$i18n.t("deviceCard.rename.saveTitle")}
          title={$i18n.t("deviceCard.rename.saveTitle")}
          disabled={getTrimmedDeviceName().length === 0}
        >
          <Check aria-hidden="true" class="card-icon" />
        </IconButton>
        <IconButton
          aria-label={$i18n.t("deviceCard.rename.cancelEdit")}
          title={$i18n.t("deviceCard.rename.cancel")}
          onClick={cancelNameEdit}
        >
          <X aria-hidden="true" class="card-icon" />
        </IconButton>
      </div>
    </form>
  {:else}
    <button class="device-card-select" type="button" on:click={onSelect}>
      <div class="device-card-title">
        {#if isSelected && session}
          <span
            class="status-dot device-status-dot"
            data-state={realtimeState}
            role="img"
            aria-label={realtimeStatusLabel($i18n, realtimeState)}
            title={realtimeStatusLabel($i18n, realtimeState)}
          ></span>
        {/if}
        <h2>{device.name}</h2>
      </div>
    </button>
  {/if}

  {#if isSelected && !isEditingName}
    <div aria-label={$i18n.t("deviceCard.actions.label")} class="device-card-actions">
      {#if session}
        <div class="device-card-menu">
          <IconButton
            aria-expanded={isActionMenuOpen}
            aria-haspopup="menu"
            aria-label={isActionMenuOpen
              ? $i18n.t("deviceCard.actions.closeMenu")
              : $i18n.t("deviceCard.actions.openMenu")}
            active={isActionMenuOpen}
            class="device-card-menu-toggle"
            stopPropagation={true}
            title={$i18n.t("deviceCard.actions.label")}
            onClick={toggleActionMenu}
          >
            <DotsVertical aria-hidden="true" class="card-icon" />
          </IconButton>

          {#if isActionMenuOpen}
            <div
              class="device-card-menu-popover"
              in:fly={{ y: -8, duration: 180 }}
              out:fly={{ y: -6, duration: 140 }}
            >
              <div class="device-card-menu-list panel" role="menu">
                <IconButton
                  aria-label={$i18n.t("deviceCard.actions.settings")}
                  class="device-card-menu-item"
                  role="menuitem"
                  stopPropagation={true}
                  title={$i18n.t("deviceCard.actions.settings")}
                  onClick={() => runAction(onOpenSettings)}
                >
                  <Settings aria-hidden="true" class="card-icon" />
                </IconButton>
                <IconButton
                  aria-label={$i18n.t("deviceCard.actions.openDevice")}
                  class="device-card-menu-item"
                  role="menuitem"
                  stopPropagation={true}
                  title={$i18n.t("deviceCard.actions.openDevice")}
                  onClick={() => runAction(onOpenDevice)}
                >
                  <Open aria-hidden="true" class="card-icon" />
                </IconButton>
                <IconButton
                  aria-label={$i18n.t("deviceCard.actions.reconnect")}
                  class="device-card-menu-item"
                  role="menuitem"
                  stopPropagation={true}
                  title={$i18n.t("deviceCard.actions.reconnect")}
                  onClick={() => runAction(onReconnect)}
                >
                  <Reload aria-hidden="true" class="card-icon" />
                </IconButton>
                <IconButton
                  aria-label={$i18n.t("deviceCard.rename.editTitle")}
                  class="device-card-menu-item"
                  role="menuitem"
                  stopPropagation={true}
                  title={$i18n.t("deviceCard.rename.editTitle")}
                  onClick={beginNameEdit}
                >
                  <Edit aria-hidden="true" class="card-icon" />
                </IconButton>
                <IconButton
                  aria-label={$i18n.t("deviceCard.actions.signOut")}
                  class="device-card-menu-item"
                  role="menuitem"
                  stopPropagation={true}
                  tone="danger"
                  title={$i18n.t("deviceCard.actions.signOut")}
                  onClick={() => runAction(onLogout)}
                >
                  <Logout aria-hidden="true" class="card-icon" />
                </IconButton>
              </div>
            </div>
          {/if}
        </div>
      {:else}
        <IconButton
          aria-label={$i18n.t("deviceCard.rename.editTitle")}
          stopPropagation={true}
          title={$i18n.t("deviceCard.rename.editTitle")}
          onClick={beginNameEdit}
        >
          <Edit aria-hidden="true" class="card-icon" />
        </IconButton>
      {/if}
    </div>
  {/if}
</div>
