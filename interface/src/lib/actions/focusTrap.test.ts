import { afterEach, describe, expect, it } from 'vitest';
import { focusTrap } from './focusTrap';

function flushMicrotasks() {
	return new Promise<void>((resolve) => queueMicrotask(resolve));
}

function markVisible(element: HTMLElement) {
	Object.defineProperty(element, 'getClientRects', {
		configurable: true,
		value: () => [{ width: 10, height: 10 }]
	});
}

function createButton(label: string) {
	const button = document.createElement('button');
	button.textContent = label;
	markVisible(button);
	return button;
}

function createTrap(...children: HTMLElement[]) {
	const trap = document.createElement('div');
	trap.tabIndex = -1;
	children.forEach((child) => trap.appendChild(child));
	document.body.appendChild(trap);
	return trap;
}

describe('focusTrap', () => {
	afterEach(() => {
		document.body.innerHTML = '';
	});

	it('focuses the first focusable element on init and restores previous focus on destroy', async () => {
		const previous = document.createElement('button');
		previous.textContent = 'previous';
		document.body.appendChild(previous);
		previous.focus();

		const first = createButton('first');
		const second = createButton('second');
		const trap = createTrap(first, second);

		const action = focusTrap(trap);
		await flushMicrotasks();

		expect(document.activeElement).toBe(first);

		action.destroy();
		expect(document.activeElement).toBe(previous);
	});

	it('wraps Tab from the last element back to the first element', async () => {
		const first = createButton('first');
		const second = createButton('second');
		const trap = createTrap(first, second);

		focusTrap(trap);
		await flushMicrotasks();
		second.focus();

		trap.dispatchEvent(new KeyboardEvent('keydown', { key: 'Tab', bubbles: true }));

		expect(document.activeElement).toBe(first);
	});

	it('wraps Shift+Tab from the first element back to the last element', async () => {
		const first = createButton('first');
		const second = createButton('second');
		const trap = createTrap(first, second);

		focusTrap(trap);
		await flushMicrotasks();
		first.focus();

		trap.dispatchEvent(new KeyboardEvent('keydown', { key: 'Tab', shiftKey: true, bubbles: true }));

		expect(document.activeElement).toBe(second);
	});

	it('focuses the container when no focusable elements are present', async () => {
		const trap = createTrap();
		const action = focusTrap(trap);
		await flushMicrotasks();

		expect(document.activeElement).toBe(trap);

		trap.dispatchEvent(new KeyboardEvent('keydown', { key: 'Tab', bubbles: true }));
		expect(document.activeElement).toBe(trap);

		action.destroy();
	});

	it('redirects outside focus back inside while enabled and ignores it when disabled', async () => {
		const first = createButton('first');
		const trap = createTrap(first);
		const outside = document.createElement('button');
		outside.textContent = 'outside';
		document.body.appendChild(outside);

		const action = focusTrap(trap);
		await flushMicrotasks();

		const focusInEvent = new FocusEvent('focusin', { bubbles: true });
		Object.defineProperty(focusInEvent, 'target', { configurable: true, value: outside });
		document.dispatchEvent(focusInEvent);

		expect(document.activeElement).toBe(first);

		action.update(false);
		outside.focus();

		const disabledFocusInEvent = new FocusEvent('focusin', { bubbles: true });
		Object.defineProperty(disabledFocusInEvent, 'target', { configurable: true, value: outside });
		document.dispatchEvent(disabledFocusInEvent);

		expect(document.activeElement).toBe(outside);

		action.destroy();
	});

	it('re-focuses the first focusable element when re-enabled', async () => {
		const first = createButton('first');
		const second = createButton('second');
		const trap = createTrap(first, second);

		const action = focusTrap(trap, false);
		second.focus();

		action.update(true);
		await flushMicrotasks();

		expect(document.activeElement).toBe(first);

		action.destroy();
	});
});
