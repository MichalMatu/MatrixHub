export function normalizeMac(mac: string | null | undefined): string {
	if (!mac) return '';
	return mac.trim().toLowerCase();
}
