<script lang="ts">
	import { PageWrapper, GridLayout } from '$lib/components/layout';
	import { useApiClient } from '$lib/utils/api/useApiClient.svelte';
	import { WifiSensingApiService } from '$lib/services/api/connectivity/WifiSensingApiService';
	import CsiAlarmControls from '$lib/features/wifisensing/csi/CsiAlarmControls.svelte';
	import CsiAmplitudeChart from '$lib/features/wifisensing/csi/CsiAmplitudeChart.svelte';
	import CsiWaterfallChart from '$lib/features/wifisensing/csi/CsiWaterfallChart.svelte';
	import { useCsiAlarmConfig } from '$lib/features/wifisensing/csi/useCsiAlarmConfig.svelte';
	import { useCsiConnection } from '$lib/features/wifisensing/csi/useCsiConnection.svelte';

	const { createService } = useApiClient();
	const api = createService(WifiSensingApiService);
	const csi = useCsiConnection();
	const csiAlarm = useCsiAlarmConfig(() => api);
	let selectionMode = $state(false);

	$effect(() => {
		void csiAlarm.loadSettings();
		csiAlarm.startStatusPolling();

		return () => {
			csiAlarm.destroy();
		};
	});
</script>

<PageWrapper>
	<GridLayout cols={1}>
		<div class="flex flex-col gap-4">
			<CsiAlarmControls
				settings={csiAlarm.settings}
				status={csiAlarm.motionStatus}
				isAdmin={csiAlarm.isAdmin}
				hasChanges={csiAlarm.hasChanges}
				saving={csiAlarm.saving}
				calibrating={csiAlarm.calibrating}
				subcarriers={csi.subcarriers}
				bind:selectionMode
				onSave={csiAlarm.save}
				onReset={csiAlarm.reset}
				onCalibrate={csiAlarm.calibrate}
				onAddBand={() => csiAlarm.addBand()}
				onRemoveBand={csiAlarm.removeBand}
				onSensitivity={csiAlarm.setSensitivity}
			/>

			<CsiAmplitudeChart
				amplitudes={csi.amplitudes}
				subcarriers={csi.subcarriers}
				rssi={csi.rssi}
				gain={csi.gain}
				isConnected={csi.isConnected}
				userEnabled={csi.userEnabled}
				bands={csiAlarm.settings.bands}
				{selectionMode}
				onToggleConnection={() => csi.toggle()}
				onBandSelected={(band) => csiAlarm.setBandFromSelection(band)}
			/>

			<CsiWaterfallChart
				amplitudes={csi.amplitudes}
				subcarriers={csi.subcarriers}
				timestamp={csi.timestamp}
				fps={csi.fps}
				bands={csiAlarm.settings.bands}
			/>
		</div>
	</GridLayout>
</PageWrapper>
