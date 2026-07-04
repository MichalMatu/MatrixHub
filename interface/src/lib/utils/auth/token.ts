import { decodeAccessTokenPayload } from '@matrixhub/device-sdk';

export function getTokenPayload(token: string) {
	// Frontend decoding is intentionally minimal: session lifetime is enforced by
	// the device, not by local exp-based heuristics.
	return decodeAccessTokenPayload(token);
}
