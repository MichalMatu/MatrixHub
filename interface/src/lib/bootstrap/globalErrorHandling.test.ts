import { describe, expect, it, vi } from 'vitest';
import {
	extractChunkAssetId,
	isAbortLikeError,
	isRecoverableChunkError,
	renderFatalErrorOverlay
} from './globalErrorHandling';

describe('globalErrorHandling', () => {
	it('recognizes abort-like errors', () => {
		expect(isAbortLikeError(new DOMException('The operation was aborted.', 'AbortError'))).toBe(
			true
		);
		expect(isAbortLikeError(new Error('Request was aborted by navigation'))).toBe(true);
		expect(isAbortLikeError(new Error('Regular error'))).toBe(false);
	});

	it('recognizes recoverable chunk errors', () => {
		expect(
			isRecoverableChunkError('Failed to fetch dynamically imported module: /_app/immutable/foo.js')
		).toBe(true);
		expect(isRecoverableChunkError('Something else failed')).toBe(false);
		expect(
			extractChunkAssetId('Failed to fetch dynamically imported module: /_app/immutable/foo.js')
		).toBe('/_app/immutable/foo.js');
	});

	it('renders the fatal overlay without interpreting message html', () => {
		const reload = vi.fn();

		renderFatalErrorOverlay(document, 'Application Error', '<img src=x onerror=alert(1)>', reload);

		const overlay = document.getElementById('sys-crash-overlay');
		expect(overlay).not.toBeNull();
		expect(overlay?.querySelector('img')).toBeNull();
		expect(overlay?.textContent).toContain('<img src=x onerror=alert(1)>');
	});
});
