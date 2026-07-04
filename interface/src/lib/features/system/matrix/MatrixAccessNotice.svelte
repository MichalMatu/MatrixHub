<script lang="ts">
	import Lock from '~icons/tabler/lock';
	import * as m from '$lib/paraglide/messages.js';
</script>

<div class="rounded-md bg-base-200 p-8 flex flex-col items-center justify-center text-center gap-3">
	<div class="p-3 bg-base-100 rounded-full">
		<Lock class="h-6 w-6 text-base-content/50" />
	</div>
	<div class="flex flex-col gap-1">
		<h4 class="font-medium">{m.matrix_admin_title()}</h4>
		<p class="text-sm text-base-content/70 max-w-sm">
			{m.matrix_admin_desc()}
		</p>
	</div>
</div>
