<script lang="ts">
	import * as m from '$lib/paraglide/messages.js';
	import { i18n } from '$lib/i18n.svelte';
	import { FormButton } from '$lib/components/shared/forms';
	import { PALETTE, RED } from './defaultIcons';

	interface Props {
		value?: number[];
		onChange?: (value: number[]) => void;
	}

	let { value = new Array(64).fill(0), onChange = () => {} }: Props = $props();

	let selectedColor = $state(RED);
	let isDrawing = $state(false);
	// In Svelte 5, arrays in $state are proxies. Mutation is fine and performant.
	let localValue: number[] = $state(new Array(64).fill(0));

	let gridElement: HTMLElement;

	$effect(() => {
		if (value && value.length === 64) {
			// One-time sync when prop changes significantly (e.g. parent reset)
			// We check if content is different to avoid loops if needed,
			// but here we just trust the prop flow.
			localValue = [...value];
		}
	});

	function toHex(num: number): string {
		return '#' + num.toString(16).padStart(6, '0');
	}

	function fromHex(hex: string): number {
		return parseInt(hex.replace('#', ''), 16);
	}

	function emitChange() {
		onChange([...$state.snapshot(localValue)]);
	}

	function setPixel(index: number) {
		if (localValue[index] !== selectedColor) {
			localValue[index] = selectedColor;
			emitChange();
		}
	}

	function handleMouseEnter(index: number) {
		if (isDrawing) setPixel(index);
	}

	function handleTouchMove(e: TouchEvent) {
		if (!isDrawing) return;
		// e.preventDefault() is crucial here to prevent scrolling while drawing
		if (e.cancelable) {
			e.preventDefault();
		}

		const touch = e.touches[0];
		const target = document.elementFromPoint(touch.clientX, touch.clientY);

		if (target instanceof HTMLButtonElement && target.dataset.index) {
			const index = parseInt(target.dataset.index);
			if (!isNaN(index)) {
				setPixel(index);
			}
		}
	}

	function clear() {
		localValue.fill(0);
		emitChange();
	}

	function fill() {
		localValue.fill(selectedColor);
		emitChange();
	}

	$effect(() => {
		if (gridElement) {
			// We need { passive: false } to allow e.preventDefault() in touchmove
			gridElement.addEventListener('touchmove', handleTouchMove, { passive: false });
			return () => {
				gridElement.removeEventListener('touchmove', handleTouchMove);
			};
		}
	});
</script>

<div class="flex flex-col gap-4 items-center">
	<!-- 8x8 Grid -->
	<div
		bind:this={gridElement}
		class="grid grid-cols-8 gap-1 bg-base-300 p-2 rounded-lg touch-none select-none w-full max-w-[320px] aspect-square"
		onmousedown={() => (isDrawing = true)}
		onmouseup={() => (isDrawing = false)}
		onmouseleave={() => (isDrawing = false)}
		ontouchstart={() => (isDrawing = true)}
		ontouchend={() => (isDrawing = false)}
		role="grid"
		tabindex="0"
	>
		{#each localValue as color, i}
			<button
				type="button"
				class="aspect-square w-full rounded-sm border border-base-content/10 transition-transform active:scale-95"
				style="background-color: {toHex(color)}"
				data-index={i}
				onmousedown={(e) => {
					e.preventDefault();
					setPixel(i);
				}}
				onmouseenter={() => handleMouseEnter(i)}
				aria-label={m.matrix_icon_editor_pixel(
					{ index: String(i + 1) },
					{ locale: i18n.languageTag }
				)}
			></button>
		{/each}
	</div>

	<!-- Tools -->
	<div class="flex flex-col gap-2 w-full max-w-xs">
		<!-- Palette -->
		<div class="flex flex-wrap gap-2 justify-center">
			{#each PALETTE as color}
				<button
					type="button"
					class="w-8 h-8 rounded-full border-2 transition-all {selectedColor === color
						? 'border-primary scale-110'
						: 'border-transparent hover:scale-105'}"
					style="background-color: {toHex(color)}"
					onclick={() => (selectedColor = color)}
					aria-pressed={selectedColor === color}
					aria-label={m.matrix_icon_editor_select_color(
						{ color: toHex(color) },
						{ locale: i18n.languageTag }
					)}
				></button>
			{/each}

			<!-- Custom Color Picker (rainbow circle) -->
			<div
				class="relative w-8 h-8 rounded-full overflow-hidden border-2 {PALETTE.includes(
					selectedColor
				)
					? 'border-transparent'
					: 'border-primary'}"
				style="background: linear-gradient(to right, red, yellow, lime, aqua, blue, magenta)"
			>
				<input
					type="color"
					class="absolute top-1/2 left-1/2 -translate-x-1/2 -translate-y-1/2 w-[150%] h-[150%] p-0 cursor-pointer opacity-0"
					value={toHex(selectedColor)}
					aria-label={m.matrix_icon_editor_custom_color({ locale: i18n.languageTag })}
					oninput={(e) => (selectedColor = fromHex(e.currentTarget.value))}
				/>
			</div>
		</div>

		<!-- Actions -->
		<div class="flex gap-2 justify-center mt-2">
			<FormButton label={m.action_clear()} class="btn-sm btn-outline btn-error" onclick={clear} />
			<FormButton
				label={m.action_default()}
				class="btn-sm btn-outline"
				onclick={() => onChange([])}
			/>
			<FormButton label={m.action_fill()} class="btn-sm btn-outline" onclick={fill} />
		</div>
	</div>
</div>
