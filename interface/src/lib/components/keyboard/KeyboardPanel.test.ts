// @vitest-environment jsdom
import { render, screen, fireEvent, waitFor, cleanup } from '@testing-library/svelte';
import { describe, it, expect, vi, beforeEach, afterEach } from 'vitest';
import KeyboardPanel from './KeyboardPanel.svelte';

const { mockPage, mockUser } = vi.hoisted(() => ({
	mockPage: {
		data: {
			features: { security: false }
		}
	},
	mockUser: {
		bearer_token: '',
		invalidate: vi.fn()
	}
}));

vi.mock('$app/state', () => ({
	page: mockPage
}));

vi.mock('$lib/stores/user', () => ({
	user: mockUser
}));

// Mock fetch
const fetchMock = vi.fn();
global.fetch = fetchMock;

// Mock innerWidth
Object.defineProperty(window, 'innerWidth', {
	writable: true,
	configurable: true,
	value: 1024
});

describe('KeyboardPanel Detailed Tests', () => {
	beforeEach(() => {
		vi.resetAllMocks();
		fetchMock.mockResolvedValue({ ok: true });
		window.innerWidth = 1024;
		document.body.innerHTML = ''; // Start clean
	});

	afterEach(() => {
		cleanup();
	});

	// Helper to trigger simple-keyboard events reliably in JSDOM
	async function pressKey(element: Element) {
		// simple-keyboard binds to pointer events primarily
		await fireEvent.pointerDown(element);
		await fireEvent.pointerUp(element);
		await fireEvent.click(element);
	}

	async function waitForKeyboardReady() {
		await waitFor(() => {
			expect(document.querySelector('.simple-keyboard .hg-button')).toBeTruthy();
		});
	}

	function getKeyboardInstance() {
		return (
			window as Window & {
				SimpleKeyboardInstances?: Record<string, { handleButtonClicked: (button: string) => void }>;
			}
		).SimpleKeyboardInstances?.simpleKeyboard;
	}

	it('renders the keyboard container', () => {
		render(KeyboardPanel);
		expect(document.querySelector('.simple-keyboard')).toBeTruthy();
	});

	it('types standard characters (a-z) into the queue', async () => {
		render(KeyboardPanel);
		await waitForKeyboardReady();
		const textarea = screen.getByPlaceholderText('Type text here...') as HTMLTextAreaElement;
		const keyboard = getKeyboardInstance();
		expect(keyboard).toBeTruthy();
		keyboard?.handleButtonClicked('q');

		// Check textarea queue (bound value)
		// Note: keyboard change updates textQueue state which binds to textarea value.
		// It might take a tick.
		await waitFor(() => {
			expect(textarea.value).toContain('q');
		});
	});

	it('sends direct keys (F1, Esc) immediately', async () => {
		render(KeyboardPanel);
		await waitForKeyboardReady();
		const escBtn = await screen.findByText('Esc');
		await pressKey(escBtn);

		await waitFor(() => {
			expect(fetchMock).toHaveBeenCalledWith(
				'/api/keyboard/press',
				expect.objectContaining({
					method: 'POST',
					body: JSON.stringify({ key: 177 })
				})
			);
		});
	});

	it('handles Modifiers (Sticky Keys)', async () => {
		render(KeyboardPanel);
		await waitForKeyboardReady();
		const ctrlBtn = await screen.findByText('Ctrl');

		// 1. Toggle Ctrl ON
		await pressKey(ctrlBtn);

		await waitFor(() => {
			const activeCtrl = document.querySelector('.key-active-dot');
			expect(activeCtrl).toBeTruthy();
			expect(activeCtrl?.textContent).toContain('Ctrl');
		});

		// 2. Press F1 with Ctrl
		const f1Btn = screen.getByText('F1');
		await pressKey(f1Btn);

		await waitFor(() => {
			expect(fetchMock).toHaveBeenCalledWith(
				'/api/keyboard/press',
				expect.objectContaining({
					method: 'POST',
					body: JSON.stringify({ keys: [128, 194] }) // [Ctrl, F1]
				})
			);
		});

		// Modifiers should allow chaining or auto-clear?
		// Code: clearModifiers() after pressDirect.
		// So active dot should be gone.
		await waitFor(
			() => {
				const activeCtrl = document.querySelector('.key-active-dot');
				expect(activeCtrl).toBeNull();
			},
			{ timeout: 1500 }
		); // Wait for visual update limit
	});

	it('Live mode sends characters immediately', async () => {
		render(KeyboardPanel);
		await waitForKeyboardReady();
		// Toggle Live
		const liveToggle = screen.getByText('Live');
		await fireEvent.click(liveToggle);

		// Type 'a'
		const aBtn = await screen.findByText('a');
		await pressKey(aBtn);

		await waitFor(() => {
			expect(fetchMock).toHaveBeenCalledWith(
				'/api/keyboard/type',
				expect.objectContaining({
					method: 'POST',
					body: JSON.stringify({ text: 'a', enter: false })
				})
			);
		});
	});

	it('captures real keyboard events locally without sending API requests', async () => {
		render(KeyboardPanel);
		await waitForKeyboardReady();

		const captureToggle = screen.getByText('Capture');
		await fireEvent.click(captureToggle);

		const textarea = screen.getByPlaceholderText('Type text here...') as HTMLTextAreaElement;
		expect(textarea.disabled).toBe(true);

		await fireEvent.keyDown(window, { key: 'a', code: 'KeyA' });

		await waitFor(() => {
			expect(document.querySelector('.key-capture-pressed')?.textContent).toContain('a');
		});

		expect(fetchMock).not.toHaveBeenCalled();

		await fireEvent.keyUp(window, { key: 'a', code: 'KeyA' });

		await waitFor(() => {
			expect(document.querySelector('.key-capture-pressed')).toBeNull();
		});
	});

	it('clears tracked physical keys when capture loses focus', async () => {
		render(KeyboardPanel);
		await waitForKeyboardReady();

		const captureToggle = screen.getByText('Capture');
		await fireEvent.click(captureToggle);

		await fireEvent.keyDown(window, { key: 'Control', code: 'ControlLeft' });

		await waitFor(() => {
			const activeCtrl = document.querySelector('.key-active-dot');
			expect(activeCtrl).toBeTruthy();
			expect(activeCtrl?.textContent).toContain('Ctrl');
		});

		window.dispatchEvent(new Event('blur'));

		await waitFor(() => {
			expect(document.querySelector('.key-active-dot')).toBeNull();
			expect(document.querySelector('.key-capture-pressed')).toBeNull();
		});

		expect(fetchMock).not.toHaveBeenCalled();
	});

	it('Shift toggles case (Layout Change)', async () => {
		render(KeyboardPanel);
		await waitForKeyboardReady();
		const shiftBtn = (await screen.findAllByText('⇧'))[0];

		await pressKey(shiftBtn);

		// Layout should change 'q' to 'Q'
		await waitFor(() => {
			expect(screen.queryByText('q')).toBeNull();
			expect(screen.getByText('Q')).toBeTruthy();
		});

		// Click 'Q'
		await pressKey(screen.getByText('Q'));

		// Should revert shift after one char
		await waitFor(() => {
			expect(screen.getByText('q')).toBeTruthy();
		});
	});
	it('AltGr (Right Alt) types Polish characters only in PL mode', async () => {
		render(KeyboardPanel);
		await waitForKeyboardReady();

		// Default mode is PL
		const altGrBtn = await screen.findByText('AltGr');

		// 1. Toggle AltGr ON
		await pressKey(altGrBtn);

		// Verify visual update (a -> ą)
		await waitFor(() => {
			expect(screen.queryByText('a')).toBeNull();
			expect(screen.getByText('ą')).toBeTruthy();
		});

		const polA = screen.getByText('ą');

		// Click ą
		await pressKey(polA);

		// Check textarea
		const textarea = screen.getByPlaceholderText('Type text here...') as HTMLTextAreaElement;
		await waitFor(() => {
			expect(textarea.value).toContain('ą');
		});

		// Switch to EN
		const langBtn = screen.getByText('PL');
		await fireEvent.click(langBtn);

		// Lang button should now say EN
		await waitFor(() => {
			expect(screen.getByText('EN')).toBeTruthy();
		});

		// Wait for layout to reset to default (modifiers cleared)
		await waitFor(() => {
			expect(screen.queryByText('a')).toBeTruthy();
		});

		// Clear textarea
		await fireEvent.input(textarea, { target: { value: '' } });

		// Press AltGr in EN mode
		await pressKey(altGrBtn);

		// Layout should NOT change to Polish
		// 'a' should still be visible
		expect(screen.getByText('a')).toBeTruthy();
		expect(screen.queryByText('ą')).toBeNull();
	});

	it('Switches to Russian layout', async () => {
		render(KeyboardPanel);
		await waitForKeyboardReady();
		const langBtn = screen.getByText('PL');

		// PL -> EN
		await fireEvent.click(langBtn);
		await waitFor(() => expect(screen.getByText('EN')).toBeTruthy());

		// EN -> RU
		const langBtnEn = screen.getByText('EN');
		await fireEvent.click(langBtnEn);
		await waitFor(() => expect(screen.getByText('RU')).toBeTruthy());

		// Check for Cyrillic keys (e.g. 'й' instead of 'q')
		await waitFor(() => {
			expect(screen.queryByText('q')).toBeNull();
			expect(screen.getByText('й')).toBeTruthy();
		});

		// Type a character
		const charBtn = screen.getByText('й');
		await pressKey(charBtn);

		const textarea = screen.getByPlaceholderText('Type text here...') as HTMLTextAreaElement;
		await waitFor(() => {
			expect(textarea.value).toContain('й');
		});
	});

	it('AltGr releases after typing a non-mapped character (Sticky behavior)', async () => {
		render(KeyboardPanel);
		await waitForKeyboardReady();
		const altGrBtn = await screen.findByText('AltGr');

		// 1. Toggle AltGr ON
		await pressKey(altGrBtn);
		await waitFor(() => expect(document.querySelector('.key-active-dot')).toBeTruthy());

		// 2. Press 'b' (not in Polish map)
		const bBtn = screen.getByText('b');
		await pressKey(bBtn);

		// 3. Should output 'b'
		const textarea = screen.getByPlaceholderText('Type text here...') as HTMLTextAreaElement;
		await waitFor(() => {
			expect(textarea.value).toContain('b');
		});

		// 4. AltGr should be OFF (Auto-released)
		await waitFor(() => {
			const activeAltGr = document.querySelector('.key-active-dot');
			expect(activeAltGr).toBeNull();
		});
	});
});
