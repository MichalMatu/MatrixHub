import { cleanup, fireEvent, render, screen } from '@testing-library/svelte';
import { afterEach, describe, expect, it, vi } from 'vitest';
import HelpTriggerButton from './HelpTriggerButton.svelte';

describe('HelpTriggerButton', () => {
	afterEach(() => {
		cleanup();
	});

	it('renders icon-only button with an accessible label', async () => {
		const onclick = vi.fn();

		render(HelpTriggerButton, {
			props: {
				label: 'Open help',
				onclick
			}
		});

		await fireEvent.click(screen.getByRole('button', { name: 'Open help' }));

		expect(onclick).toHaveBeenCalledTimes(1);
	});

	it('renders labeled button when iconOnly is disabled', () => {
		render(HelpTriggerButton, {
			props: {
				label: 'Help',
				iconOnly: false
			}
		});

		expect(screen.getByRole('button', { name: 'Help' })).toBeTruthy();
	});
});
