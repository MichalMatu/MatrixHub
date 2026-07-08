import { fireEvent, render, screen, waitFor } from '@testing-library/svelte';
import { describe, expect, it, vi } from 'vitest';
import DateSelector from './DateSelector.svelte';

vi.mock('$lib/i18n.svelte', () => ({
	i18n: {
		languageTag: 'en'
	}
}));

vi.mock('$lib/paraglide/messages.js', () => ({
	charts_range_from: () => 'From',
	charts_range_to: () => 'To',
	charts_range_days_with_data: ({ count }: { count: number }) => `Days with data: ${count}`,
	charts_range_no_days: () => 'No days with data'
}));

describe('DateSelector', () => {
	it('repairs missing bounds to the latest available day', async () => {
		render(DateSelector, {
			props: {
				startDate: '',
				endDate: '',
				availableDates: ['2026-04-05', '2026-04-11']
			}
		});

		await waitFor(() => {
			const inputs = screen.getAllByDisplayValue('2026-04-11');
			expect(inputs).toHaveLength(2);
		});
	});

	it('snaps a start date without data to the next available day', async () => {
		render(DateSelector, {
			props: {
				startDate: '2026-04-05',
				endDate: '2026-04-11',
				availableDates: ['2026-04-05', '2026-04-11', '2026-04-20']
			}
		});

		const startInput = screen.getByLabelText('From');
		await fireEvent.change(startInput, { target: { value: '2026-04-10' } });

		await waitFor(() => {
			expect((screen.getByLabelText('From') as HTMLInputElement).value).toBe('2026-04-11');
			expect((screen.getByLabelText('To') as HTMLInputElement).value).toBe('2026-04-11');
			expect(screen.getByText('Days with data: 1')).toBeTruthy();
		});
	});

	it('snaps an end date without data to the previous available day', async () => {
		render(DateSelector, {
			props: {
				startDate: '2026-04-05',
				endDate: '2026-04-20',
				availableDates: ['2026-04-05', '2026-04-11', '2026-04-20']
			}
		});

		const endInput = screen.getByLabelText('To');
		await fireEvent.change(endInput, { target: { value: '2026-04-18' } });

		await waitFor(() => {
			expect(screen.getByDisplayValue('2026-04-11')).toBeTruthy();
			expect(screen.getByText('Days with data: 2')).toBeTruthy();
		});
	});
});
