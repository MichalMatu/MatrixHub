# Shared UI Components

This library provides a set of standardized UI components to ensure consistency across the application.

## Layout Components

### PageWrapper

Standard container for pages. Handles max-width and padding.

```svelte
<script>
	import { PageWrapper } from '$lib/components/layout';
</script>

<PageWrapper>
	<!-- Page Content -->
</PageWrapper>
```

### GridLayout

Responsive grid layout wrapper.

```svelte
<script>
	import { GridLayout } from '$lib/components/layout';
</script>

<GridLayout cols={2}>
	<div class="card">...</div>
	<div class="card">...</div>
</GridLayout>
```

### BaseCard

Standard card component with title and icon.

```svelte
<script>
  import BaseCard from '$lib/components/layout/BaseCard.svelte';
  import Icon from '~icons/tabler/icon-name';
</script>

<BaseCard title="Card Title" icon={Icon}>
  Content

  {#snippet actions}
    <button class="btn">Action</button>
  {/snippet}
</BaseCard>
```

Other leaf layout components like `ContentBox`, `StatusRow`, and `LoadingCard`
should also be imported directly from their `.svelte` files in
`$lib/components/layout/`.

## Form Components

### FormInput

Standard input field (text, number, password, email, url).

```svelte
<FormInput
	label="Username"
	bind:value={username}
	error={errorMessage}
	placeholder="Enter username"
/>
```

Import from `$lib/components/shared/forms`.

### FormToggle

Toggle switch with label and description. Supports `onchange` event.

```svelte
<FormToggle
	label="Enable Feature"
	description="This turns on the feature"
	bind:checked={isEnabled}
	onchange={handleToggle}
/>
```

Import from `$lib/components/shared/forms`.

### FormSelect

Select dropdown.

```svelte
<FormSelect
	label="Choose Option"
	options={[
		{ value: 'opt1', label: 'Option 1' },
		{ value: 'opt2', label: 'Option 2' }
	]}
	bind:value={selectedOption}
/>
```

Import from `$lib/components/shared/forms`.

### FormButton

Button with loading state and icon support.

```svelte
<FormButton
	label="Save Changes"
	icon={SaveIcon}
	loading={isSaving}
	disabled={!hasChanges}
	onclick={handleSave}
/>
```

Import from `$lib/components/shared/forms`.

## Dialogs

### ConfirmDialog

Modal for confirming destructive actions.

```svelte
<ConfirmDialog
	bind:open={isOpen}
	title="Delete User?"
	message="Are you sure you want to delete this user?"
	confirmLabel="Delete"
	onConfirm={handleDelete}
/>
```
