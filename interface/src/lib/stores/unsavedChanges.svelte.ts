export class UnsavedChangesRegistry {
	private sources: Record<string, boolean> = {};

	setSourceDirty(sourceId: string, dirty: boolean) {
		if (!sourceId) return;
		if (this.sources[sourceId] === dirty) return;
		this.sources = { ...this.sources, [sourceId]: dirty };
	}

	clearSource(sourceId: string) {
		if (!sourceId || !(sourceId in this.sources)) return;
		const { [sourceId]: _removed, ...rest } = this.sources;
		this.sources = rest;
	}

	clearAllForTests() {
		this.sources = {};
	}

	get hasChanges() {
		return Object.values(this.sources).some(Boolean);
	}

	get dirtySourceCount() {
		return Object.values(this.sources).filter(Boolean).length;
	}
}

export const unsavedChanges = new UnsavedChangesRegistry();
