/**
 * Sparkline hover interaction logic (Svelte 5 runes)
 */

export function useSparklineHover(
	onHover?: () => ((_value: number | null, _index: number | null) => void) | undefined,
	dataFn?: () => number[] | undefined
) {
	let hoverIndex = $state<number | null>(null);

	function handleMouseMove(
		e: MouseEvent,
		svgElement: SVGSVGElement | undefined,
		dataLength: number
	) {
		const data = dataFn?.();
		if (!svgElement || !data || dataLength < 2) return;

		const rect = svgElement.getBoundingClientRect();
		const x = e.clientX - rect.left;
		const width = rect.width;

		const index = Math.min(Math.max(0, Math.floor((x / width) * dataLength)), dataLength - 1);

		if (hoverIndex !== index) {
			hoverIndex = index;
			const callback = onHover?.();
			if (callback && data) {
				callback(data[index], index);
			}
		}
	}

	function handleMouseLeave() {
		if (hoverIndex !== null) {
			hoverIndex = null;
			const callback = onHover?.();
			if (callback) {
				callback(null, null);
			}
		}
	}
	return {
		get hoveredIndex() {
			return hoverIndex;
		},
		handleMouseMove,
		handleMouseLeave
	};
}
