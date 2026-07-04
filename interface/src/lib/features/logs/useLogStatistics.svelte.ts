import { estimateRecordCount } from '$lib/utils/logs/binaryLogParser';
import type { LogListResponse } from '$lib/services/api/monitoring/LogsApiService';

export function useLogStatistics(getLogs: () => LogListResponse) {
	let totalFiles = $derived(getLogs().months.reduce((sum, month) => sum + month.files.length, 0));

	let totalSizeKB = $derived(
		getLogs().months.reduce((sum, month) => sum + month.files.reduce((s, f) => s + f.size, 0), 0) /
			1024
	);

	let averageSizeKB = $derived(totalFiles > 0 ? totalSizeKB / totalFiles : 0);

	let oldestMonth = $derived(getLogs().months.length > 0 ? getLogs().months[0].name : '-');

	let newestMonth = $derived(
		getLogs().months.length > 0 ? getLogs().months[getLogs().months.length - 1].name : '-'
	);

	let estimatedEntries = $derived(
		getLogs().months.reduce((sum, month) => {
			return (
				sum +
				month.files.reduce((s, f) => {
					if (f.name.endsWith('.bin')) {
						return s + estimateRecordCount(f.size);
					}
					return s;
				}, 0)
			);
		}, 0)
	);

	return {
		get totalFiles() {
			return totalFiles;
		},
		get totalSizeKB() {
			return totalSizeKB;
		},
		get averageSizeKB() {
			return averageSizeKB;
		},
		get oldestMonth() {
			return oldestMonth;
		},
		get newestMonth() {
			return newestMonth;
		},
		get estimatedEntries() {
			return estimatedEntries;
		}
	};
}
