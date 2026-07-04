<script lang="ts">
	import Modal from '$lib/components/Modal.svelte';
	import IconEditor from '$lib/components/matrix/IconEditor.svelte';
	import { DEFAULT_ICONS } from '$lib/components/matrix/defaultIcons';
	import { FormButton } from '$lib/components/shared/forms';
	import IconDeviceFloppy from '~icons/tabler/device-floppy';
	import IconX from '~icons/tabler/x';
	import * as m from '$lib/paraglide/messages.js';

	interface Props {
		isOpen: boolean;
		onClose: () => void;
		customIcons: number[][];
		onSave: (icons: number[][]) => Promise<boolean> | boolean;
	}

	let { isOpen, onClose, customIcons = [[], [], []], onSave }: Props = $props();

	let activeTab = $state(0);
	let localIcons = $state<number[][]>([[], [], []]);
	let saving = $state(false);

	let wasOpen = $state(false);

	// Reset tab only when modal transitions from closed to open.
	$effect(() => {
		if (isOpen && !wasOpen) {
			activeTab = 0;
		}
		wasOpen = isOpen;
	});

	// Clone current state when modal is open and customIcons change.
	$effect(() => {
		if (isOpen) {
			const source = customIcons && customIcons.length > 0 ? customIcons : [[], [], []];
			try {
				localIcons = structuredClone($state.snapshot(source));
			} catch (e) {
				console.error('Clone failed', e);
				localIcons = [[], [], []];
			}
		}
	});

	async function handleSave() {
		saving = true;
		try {
			const saved = await onSave(structuredClone($state.snapshot(localIcons)));
			if (saved === false) {
				return;
			}
			onClose();
		} finally {
			saving = false;
		}
	}

	function updateIcon(idx: number, pixels: number[]) {
		localIcons[idx] = [...pixels];
	}

	// Ensure the editor always receives a valid 64-pixel icon.
	function getEditorValue(index: number): number[] {
		const current = localIcons[index];
		if (current && current.length === 64) {
			return current;
		}
		return DEFAULT_ICONS[index] || new Array(64).fill(0);
	}
</script>

<Modal {isOpen} {onClose} title={m.matrix_icon_editor_title()}>
	<div class="flex flex-col gap-4">
		<div class="grid grid-cols-3 gap-2">
			<button
				type="button"
				class="btn btn-sm {activeTab === 0 ? 'btn-info' : 'btn-ghost'}"
				onclick={() => (activeTab = 0)}
				aria-pressed={activeTab === 0}
			>
				{m.matrix_icon_tab_info()}
			</button>

			<button
				type="button"
				class="btn btn-sm {activeTab === 1 ? 'btn-warning' : 'btn-ghost'}"
				onclick={() => (activeTab = 1)}
				aria-pressed={activeTab === 1}
			>
				{m.matrix_icon_tab_warning()}
			</button>

			<button
				type="button"
				class="btn btn-sm {activeTab === 2 ? 'btn-error' : 'btn-ghost'}"
				onclick={() => (activeTab = 2)}
				aria-pressed={activeTab === 2}
			>
				{m.matrix_icon_tab_critical()}
			</button>
		</div>

		<div class="flex justify-center p-2 bg-base-200 rounded-box min-h-[340px]">
			<IconEditor
				value={getEditorValue(activeTab)}
				onChange={(val) => updateIcon(activeTab, val)}
			/>
		</div>
	</div>

	{#snippet actions()}
		<div class="flex justify-end gap-2 w-full">
			<FormButton label={m.action_cancel()} icon={IconX} onclick={onClose} class="btn-neutral" />
			<FormButton
				label={m.action_save()}
				icon={IconDeviceFloppy}
				onclick={handleSave}
				loading={saving}
				disabled={saving}
			/>
		</div>
	{/snippet}
</Modal>
