let counter = 0;

/**
 * Generates a deterministic-but-unique identifier for form controls on the client.
 * Prefix is optional and defaults to `field`.
 */
export function createHtmlId(prefix = 'field'): string {
	counter += 1;
	return `${prefix}-${counter}`;
}
