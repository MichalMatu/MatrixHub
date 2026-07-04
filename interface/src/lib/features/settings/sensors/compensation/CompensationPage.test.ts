// @vitest-environment jsdom
import { cleanup, fireEvent, render, screen, waitFor, within } from '@testing-library/svelte';
import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest';
import CompensationPage from './CompensationPage.svelte';
import CompensationPreview from './CompensationPreview.svelte';
import CompensationSettings from './CompensationSettings.svelte';

type CompensationState = ReturnType<
	typeof import('./useCompensationSettings.svelte').useCompensationSettings
>;

type MockCompState = {
	settings: {
		enabled: boolean;
		base_temp_offset: number;
		reference_cpu_temp: number;
		temp_offset_per_cpu_degree: number;
		min_temp_offset: number;
		max_temp_offset: number;
	};
	loading: boolean;
	saving: boolean;
	hasChanges: boolean;
	canPreview: boolean;
	savedSettings: {
		enabled: boolean;
		base_temp_offset: number;
		reference_cpu_temp: number;
		temp_offset_per_cpu_degree: number;
		min_temp_offset: number;
		max_temp_offset: number;
	} | null;
	errors: {
		base_temp_offset: boolean;
		reference_cpu_temp: boolean;
		temp_offset_per_cpu_degree: boolean;
		min_temp_offset: boolean;
		max_temp_offset: boolean;
	};
	errorMessage: string | null;
	loadSettings: ReturnType<typeof vi.fn>;
	saveSettings: ReturnType<typeof vi.fn>;
	updateSetting: ReturnType<typeof vi.fn>;
	compensateHumidity: ReturnType<typeof vi.fn>;
	applyPreset: ReturnType<typeof vi.fn>;
	PRESETS: Record<string, unknown>;
};

function buildCompState(overrides: Partial<MockCompState> = {}): MockCompState {
	return {
		settings: {
			enabled: false,
			base_temp_offset: 2,
			reference_cpu_temp: 45,
			temp_offset_per_cpu_degree: 0.2,
			min_temp_offset: 0,
			max_temp_offset: 8
		},
		loading: false,
		saving: false,
		hasChanges: false,
		canPreview: false,
		savedSettings: null,
		errors: {
			base_temp_offset: false,
			reference_cpu_temp: false,
			temp_offset_per_cpu_degree: false,
			min_temp_offset: false,
			max_temp_offset: false
		},
		errorMessage: null,
		loadSettings: vi.fn(),
		saveSettings: vi.fn(),
		updateSetting: vi.fn(),
		compensateHumidity: vi.fn(),
		applyPreset: vi.fn(),
		PRESETS: {},
		...overrides
	};
}

function setViewportWidth(width: number) {
	window.innerWidth = width;
	Object.defineProperty(window, 'matchMedia', {
		writable: true,
		value: vi.fn().mockImplementation((query: string) => {
			const normalizedQuery = query.replace(/\s+/g, '');
			const minWidth = /min-width:(\d+)px/.exec(normalizedQuery);
			const maxWidth = /max-width:(\d+)px/.exec(normalizedQuery);
			let matches = true;

			if (minWidth) {
				matches &&= width >= Number(minWidth[1]);
			}

			if (maxWidth) {
				matches &&= width <= Number(maxWidth[1]);
			}

			return {
				matches,
				media: query,
				onchange: null,
				addListener: vi.fn(),
				removeListener: vi.fn(),
				addEventListener: vi.fn(),
				removeEventListener: vi.fn(),
				dispatchEvent: vi.fn()
			};
		})
	});
}

function getCardByTitle(title: string) {
	const titleElement = screen.getByText(title);
	const cardElement = titleElement.closest('.card');

	expect(cardElement).toBeTruthy();

	return cardElement as HTMLElement;
}

const { mockSession, mockPageCompState } = vi.hoisted(() => ({
	mockSession: { canManage: true },
	mockPageCompState: {
		settings: {
			enabled: false,
			base_temp_offset: 2,
			reference_cpu_temp: 45,
			temp_offset_per_cpu_degree: 0.2,
			min_temp_offset: 0,
			max_temp_offset: 8
		},
		loading: true,
		saving: false,
		hasChanges: false,
		canPreview: false,
		savedSettings: null,
		errors: {
			base_temp_offset: false,
			reference_cpu_temp: false,
			temp_offset_per_cpu_degree: false,
			min_temp_offset: false,
			max_temp_offset: false
		},
		errorMessage: null,
		loadSettings: vi.fn(),
		saveSettings: vi.fn(),
		updateSetting: vi.fn(),
		compensateHumidity: vi.fn(),
		applyPreset: vi.fn(),
		PRESETS: {}
	}
}));

vi.mock('$lib/utils/api/useApiClient.svelte', () => ({
	useApiClient: () => ({
		session: mockSession
	})
}));

vi.mock('$lib/features/auth/useSessionAccess.svelte', () => ({
	useSessionAccess: () => ({
		canManage: mockSession.canManage
	})
}));

vi.mock('./useCompensationSettings.svelte', () => ({
	useCompensationSettings: (deps?: { shouldLoad?: () => boolean }) => {
		if (deps?.shouldLoad?.()) {
			mockPageCompState.loadSettings();
		}
		return mockPageCompState;
	}
}));

vi.mock('$lib/features/system/status/useSystemStatusReadModel.svelte', () => ({
	useSystemStatusReadModel: () => ({
		coreTemp: 42.5
	})
}));

vi.mock('$lib/i18n.svelte', () => ({
	i18n: {
		languageTag: 'en'
	}
}));

vi.mock('$lib/paraglide/messages.js', () => ({
	comp_title: () => 'Compensation Settings',
	comp_enable: () => 'Enable compensation',
	comp_enabled_desc: () => 'Enabled',
	comp_disabled_desc: () => 'Disabled',
	comp_base_offset: () => 'Base offset',
	comp_base_offset_desc: () => 'Base offset description',
	comp_ref_cpu: () => 'Reference CPU',
	comp_ref_cpu_desc: () => 'Reference CPU description',
	comp_slope: () => 'Slope',
	comp_slope_desc: () => 'Slope description',
	comp_min_offset: () => 'Min offset',
	comp_min_offset_desc: () => 'Min offset description',
	comp_max_offset: () => 'Max offset',
	comp_max_offset_desc: () => 'Max offset description',
	comp_preview_title: () => 'Live Preview',
	comp_preview_cpu: () => 'Current CPU Temp:',
	comp_preview_correction: () => 'Calculated Correction:',
	comp_preview_humid_factor: () => 'Humidity Increase',
	comp_preview_formula: () => 'Formula:',
	comp_preview_clamped: ({ min, max }: { min: string; max: string }) =>
		`clamped to range ${min}-${max}`,
	comp_preset_label: () => 'Quick Presets',
	comp_preset_none: () => 'None',
	comp_preset_factory: () => 'Factory',
	comp_preset_small: () => 'Small',
	comp_preset_large: () => 'Large',
	comp_magnus_temp: () => 'Compensated temperature:',
	comp_magnus_humid: () => 'Humidity correction:',
	comp_magnus_gamma: () => 'Where:',
	access_admin_required_title: () => 'Admin required',
	access_admin_required_desc: () => 'Admin access is required.',
	action_save: () => 'Save',
	common_loading: () => 'Loading',
	settings_title: () => 'Settings'
}));

describe('CompensationPage', () => {
	beforeEach(() => {
		vi.spyOn(console, 'warn').mockImplementation(() => {});
		setViewportWidth(1024);
		mockSession.canManage = true;
		mockPageCompState.settings.enabled = false;
		mockPageCompState.loading = true;
		mockPageCompState.hasChanges = true;
		mockPageCompState.canPreview = false;
		mockPageCompState.loadSettings.mockClear();
		mockPageCompState.saveSettings.mockClear();
		mockPageCompState.updateSetting.mockClear();
		mockPageCompState.applyPreset.mockClear();
	});

	afterEach(() => {
		vi.restoreAllMocks();
		cleanup();
	});

	it('keeps preview visible while disabling is only a local draft', async () => {
		mockPageCompState.loading = false;
		mockPageCompState.canPreview = true;
		mockPageCompState.settings.enabled = false;

		render(CompensationPage);

		await waitFor(() => {
			expect(mockPageCompState.loadSettings).toHaveBeenCalledTimes(1);
		});
		expect(screen.getByText('Compensation Settings')).toBeTruthy();
		expect(screen.getByText('Live Preview')).toBeTruthy();
	});

	it('hides preview when saved state no longer allows it', async () => {
		mockPageCompState.loading = false;
		mockPageCompState.canPreview = false;

		render(CompensationPage);

		expect(screen.getByText('Compensation Settings')).toBeTruthy();
		expect(screen.queryByText('Live Preview')).toBeNull();
	});

	it('shows save in both cards when both cards are visible', async () => {
		mockPageCompState.loading = false;
		mockPageCompState.canPreview = true;

		render(CompensationPage);

		expect(
			within(getCardByTitle('Compensation Settings')).getByRole('button', { name: 'Save' })
		).toBeTruthy();
		expect(
			within(getCardByTitle('Live Preview')).getByRole('button', { name: 'Save' })
		).toBeTruthy();
		expect(screen.getAllByRole('button', { name: 'Save' })).toHaveLength(2);
	});

	it('keeps save placement independent from viewport width', async () => {
		mockPageCompState.loading = false;
		mockPageCompState.canPreview = true;
		setViewportWidth(390);

		render(CompensationPage);

		expect(
			within(getCardByTitle('Compensation Settings')).getByRole('button', { name: 'Save' })
		).toBeTruthy();
		expect(
			within(getCardByTitle('Live Preview')).getByRole('button', { name: 'Save' })
		).toBeTruthy();
		expect(screen.getAllByRole('button', { name: 'Save' })).toHaveLength(2);
	});

	it('keeps save in the settings card when preview is hidden', async () => {
		mockPageCompState.loading = false;
		mockPageCompState.canPreview = false;
		setViewportWidth(1024);

		render(CompensationPage);

		expect(
			within(getCardByTitle('Compensation Settings')).getByRole('button', { name: 'Save' })
		).toBeTruthy();
		expect(screen.queryByText('Live Preview')).toBeNull();
		expect(screen.getAllByRole('button', { name: 'Save' })).toHaveLength(1);
	});

	it('keeps range inputs editable when compensation is disabled', async () => {
		const compState = buildCompState();

		render(CompensationSettings, {
			props: {
				compState: compState as unknown as CompensationState
			}
		});

		expect(screen.getByLabelText('Base offset').hasAttribute('disabled')).toBe(false);
		expect(screen.getByLabelText('Reference CPU (42.5 °C)').hasAttribute('disabled')).toBe(false);
		expect(screen.getByLabelText('Slope (CPU: 42.5 °C)').hasAttribute('disabled')).toBe(false);
		expect(screen.getByLabelText('Min offset').hasAttribute('disabled')).toBe(false);
		expect(screen.getByLabelText('Max offset').hasAttribute('disabled')).toBe(false);
	});

	it('keeps presets usable when compensation is disabled', async () => {
		const compState = buildCompState();

		render(CompensationPreview, {
			props: {
				compState: compState as unknown as CompensationState
			}
		});

		const factoryButton = screen.getByRole('button', { name: /Factory/ });
		expect(factoryButton.hasAttribute('disabled')).toBe(false);

		await fireEvent.click(factoryButton);

		expect(compState.applyPreset).toHaveBeenCalledWith('factory');
	});

	it('saves from the preview card action button', async () => {
		const compState = buildCompState({ hasChanges: true });

		render(CompensationPreview, {
			props: {
				compState: compState as unknown as CompensationState
			}
		});

		await fireEvent.click(screen.getByRole('button', { name: 'Save' }));

		expect(compState.saveSettings).toHaveBeenCalledTimes(1);
	});
});
