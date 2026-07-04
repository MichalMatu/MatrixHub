import type { ShellyDeviceDraft } from './shellyModel';
import { isValidIPv4 } from '$lib/utils/validation/validators';

interface ShellyDraftErrors {
	ip: boolean;
}

export interface ShellyDraftValidationResult {
	valid: boolean;
	errors: ShellyDraftErrors;
	normalizedDraft: ShellyDeviceDraft;
}

export function sanitizeShellyIpv4Input(value: string): string {
	let sanitized = value.trim();
	sanitized = sanitized.replace(/^https?:\/\//i, '');
	sanitized = sanitized.replace(/:\d+/, '');
	sanitized = sanitized.split('/')[0];
	return sanitized.trim();
}

export function validateShellyDraft(draft: ShellyDeviceDraft): ShellyDraftValidationResult {
	const normalizedDraft: ShellyDeviceDraft = {
		...draft,
		ip: sanitizeShellyIpv4Input(draft.ip)
	};
	const errors = {
		ip: !isValidIPv4(normalizedDraft.ip)
	};

	return {
		valid: !errors.ip,
		errors,
		normalizedDraft
	};
}
