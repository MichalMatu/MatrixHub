import { fireEvent, render, screen, waitFor } from '@testing-library/svelte';
import { describe, expect, it, vi } from 'vitest';
import DateSelector from './DateSelector.svelte';

vi.mock('$lib/i18n.svelte', () => ({
	i18n: {
		languageTag: 'en'
	}
}));

vi.mock('$lib/paraglide/messages.js', () => ({
	charts_nav_prev_month: () => 'Previous month',
	charts_nav_prev_day: () => 'Previous day',
	charts_nav_next_day: () => 'Next day',
	charts_nav_next_month: () => 'Next month'
}));

describe('DateSelector', () => {
	it('selects the latest available day when navigating to another month', async () => {
		render(DateSelector, {
			props: {
				selectedDate: '2026-04-10',
				currentMonth: '2026-04',
				availableDates: ['2026-03-01', '2026-03-15', '2026-04-10'],
				showCalendar: false
			}
		});

		await fireEvent.click(screen.getByRole('button', { name: '◀◀' }));

		await waitFor(() => {
			expect(screen.getByRole('button', { name: /2026-03-15/ })).toBeTruthy();
		});
	});

	it('repairs an invalid selected date to the latest day in the current month', async () => {
		render(DateSelector, {
			props: {
				selectedDate: '2026-04-20',
				currentMonth: '2026-04',
				availableDates: ['2026-04-05', '2026-04-11'],
				showCalendar: false
			}
		});

		await waitFor(() => {
			expect(screen.getByRole('button', { name: /2026-04-11/ })).toBeTruthy();
		});
	});
});
