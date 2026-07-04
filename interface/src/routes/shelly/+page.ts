import type { PageLoad } from './$types';

export const load: PageLoad = async () => {
	return {
		devices: [],
		title: 'Shelly Devices',
		loadError: null
	};
};
