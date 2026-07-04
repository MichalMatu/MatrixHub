const FOCUSABLE_SELECTOR = [
	'a[href]',
	'button:not([disabled])',
	'input:not([disabled]):not([type="hidden"])',
	'select:not([disabled])',
	'textarea:not([disabled])',
	'[tabindex]:not([tabindex="-1"])',
	'[contenteditable="true"]'
].join(',');

function isVisible(element: HTMLElement) {
	return element.getClientRects().length > 0;
}

function getFocusableElements(node: HTMLElement) {
	return Array.from(node.querySelectorAll<HTMLElement>(FOCUSABLE_SELECTOR)).filter(
		(element) =>
			!element.hasAttribute('hidden') &&
			element.getAttribute('aria-hidden') !== 'true' &&
			isVisible(element)
	);
}

function focusFirstElement(node: HTMLElement) {
	const [firstFocusable] = getFocusableElements(node);
	(firstFocusable ?? node).focus({ preventScroll: true });
}

export function focusTrap(node: HTMLElement, enabled: boolean = true) {
	const previousActiveElement =
		document.activeElement instanceof HTMLElement ? document.activeElement : null;

	function handleKeydown(event: KeyboardEvent) {
		if (!enabled || event.key !== 'Tab') return;

		const focusableElements = getFocusableElements(node);
		if (focusableElements.length === 0) {
			event.preventDefault();
			node.focus({ preventScroll: true });
			return;
		}

		const firstFocusable = focusableElements[0];
		const lastFocusable = focusableElements[focusableElements.length - 1];
		const activeElement =
			document.activeElement instanceof HTMLElement ? document.activeElement : null;

		if (event.shiftKey) {
			if (!activeElement || activeElement === firstFocusable || !node.contains(activeElement)) {
				event.preventDefault();
				lastFocusable.focus({ preventScroll: true });
			}
			return;
		}

		if (!activeElement || activeElement === lastFocusable || !node.contains(activeElement)) {
			event.preventDefault();
			firstFocusable.focus({ preventScroll: true });
		}
	}

	function handleFocusIn(event: FocusEvent) {
		if (!enabled || !(event.target instanceof HTMLElement)) return;
		if (node.contains(event.target)) return;
		focusFirstElement(node);
	}

	queueMicrotask(() => {
		if (enabled) {
			focusFirstElement(node);
		}
	});

	document.addEventListener('focusin', handleFocusIn);
	node.addEventListener('keydown', handleKeydown);

	return {
		update(nextEnabled: boolean) {
			enabled = nextEnabled;
			if (enabled) {
				queueMicrotask(() => focusFirstElement(node));
			}
		},
		destroy() {
			document.removeEventListener('focusin', handleFocusIn);
			node.removeEventListener('keydown', handleKeydown);
			previousActiveElement?.focus({ preventScroll: true });
		}
	};
}
