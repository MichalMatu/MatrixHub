<script lang="ts">
  import Button from "$lib/components/ui/Button.svelte";
  import { i18n } from "$lib/i18n/store";

  export let onOpenAddSheet: () => void | Promise<void> = () => undefined;
</script>

<section class="panel settings-shell">
  <div class="section-head compact-head">
    <h2>{$i18n.t("app.emptyState.title")}</h2>
  </div>
  <p class="panel-copy">{$i18n.t("app.emptyState.copy")}</p>
  <Button variant="primary" onClick={onOpenAddSheet}>
    {$i18n.t("app.emptyState.action")}
  </Button>
</section>
