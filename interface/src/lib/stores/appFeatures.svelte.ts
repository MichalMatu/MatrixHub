import { browser } from '$app/environment';
import {
	DEFAULT_APP_FEATURES,
	fetchAppFeatures,
	type AppFeatures
} from '$lib/bootstrap/appFeatures';

class AppFeaturesStore {
	features = $state<AppFeatures>({ ...DEFAULT_APP_FEATURES });
	resolved = $state(false);
	private bootstrapped = false;
	private inflight: Promise<AppFeatures> | null = null;

	ensureBoot(fetchImpl: typeof fetch = fetch) {
		if (!browser || this.bootstrapped) return;
		this.bootstrapped = true;
		void this.refresh(fetchImpl);
	}

	async refresh(fetchImpl: typeof fetch = fetch) {
		if (this.inflight) return this.inflight;

		const request = (async () => {
			const next = await fetchAppFeatures(fetchImpl);
			this.features = next;
			this.resolved = true;
			return next;
		})();

		this.inflight = request;

		try {
			return await request;
		} finally {
			this.inflight = null;
		}
	}
}

export const appFeatures = new AppFeaturesStore();
