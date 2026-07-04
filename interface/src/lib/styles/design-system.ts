/**
 * MatrixHub Design System
 * Centralized design tokens and component variants for consistent styling
 *
 * This is a barrel export that re-exports all design system modules.
 * Individual modules are organized by category for better maintainability.
 */

// Tokens
export { icon } from './design-system/tokens/icons';
export { semantic } from './design-system/tokens/semantic';

// Variants
export { getInputClasses } from './design-system/variants/inputs';
export { getButtonClasses } from './design-system/variants/buttons';
export { cardVariants, dialogVariants, formVariants } from './design-system/variants/layouts';

// Presets
export { fileManagerStyles } from './design-system/presets/fileManager';
export { powerManagementStyles } from './design-system/presets/powerManagement';

// Utilities
export { surfaceCard, surfacePanel, detailTile, miniCard } from './design-system/utils/classNames';
