import { cleanup, fireEvent, render, screen } from '@testing-library/svelte';
import { afterEach, describe, expect, it, vi } from 'vitest';
import FeatureHelpModal from './FeatureHelpModal.svelte';

vi.mock('$lib/actions/focusTrap', () => ({
	focusTrap: () => ({
		destroy() {}
	})
}));

vi.mock('$lib/i18n.svelte', () => ({
	i18n: {
		languageTag: 'en'
	}
}));

vi.mock('$lib/paraglide/messages.js', () => ({
	help_links_label: () => 'Quick links',
	action_close: () => 'Close'
}));

describe('FeatureHelpModal', () => {
	afterEach(() => {
		cleanup();
	});

	it('renders intro, sections, links, and closes from the action button', async () => {
		const onClose = vi.fn();

		render(FeatureHelpModal, {
			props: {
				isOpen: true,
				onClose,
				title: 'Wi-Fi Help',
				intro: 'Intro text',
				sections: [
					{ title: 'How it works', body: 'Section body one' },
					{ title: 'What to watch', body: 'Section body two' }
				],
				links: [
					{ href: '/wifi/sta', label: 'Wi-Fi STA' },
					{ href: '/system/help', label: 'Help' }
				]
			}
		});

		expect(screen.getByText('Wi-Fi Help')).toBeTruthy();
		expect(screen.getByText('Intro text')).toBeTruthy();
		expect(screen.getByText('How it works')).toBeTruthy();
		expect(screen.getByText('Section body one')).toBeTruthy();
		expect(screen.getByText('Quick links')).toBeTruthy();
		expect(screen.getByRole('link', { name: 'Wi-Fi STA' }).getAttribute('href')).toBe('/wifi/sta');

		const closeButton = screen.getByText('Close').closest('button');
		expect(closeButton).toBeTruthy();
		await fireEvent.click(closeButton!);

		expect(onClose).toHaveBeenCalledTimes(1);
	});
});
