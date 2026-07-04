/**
 * Chart cursor synchronization manager
 * Allows multiple charts to sync their cursor positions
 */
import type uPlot from 'uplot';

interface CursorConfig {
	drag: { x: boolean; y: boolean; setScale: boolean };
	focus: { prox: number };
	points: { size: number; width: number; fill: string };
}

/**
 * Common cursor configuration with drag-to-zoom enabled
 */
export const commonCursorConfig: CursorConfig = {
	drag: { x: true, y: false, setScale: true },
	focus: { prox: 30 },
	points: { size: 10, width: 2, fill: '#fff' }
};

/**
 * Chart cursor synchronization manager
 */
class ChartSyncManager {
	private charts: Map<string, uPlot> = new Map();
	private syncing = false;

	register(id: string, chart: uPlot) {
		this.charts.set(id, chart);
	}

	unregister(id: string) {
		this.charts.delete(id);
	}

	syncCursor(sourceId: string, left: number, _top: number) {
		if (this.syncing) return;
		this.syncing = true;

		this.charts.forEach((chart, id) => {
			if (id !== sourceId) {
				chart.setCursor({ left, top: -10 }); // -10 hides Y crosshair on synced charts
			}
		});

		this.syncing = false;
	}

	clearCursors(sourceId: string) {
		if (this.syncing) return;
		this.syncing = true;

		this.charts.forEach((chart, id) => {
			if (id !== sourceId) {
				chart.setCursor({ left: -10, top: -10 });
			}
		});

		this.syncing = false;
	}
}

export const chartSyncManager = new ChartSyncManager();
