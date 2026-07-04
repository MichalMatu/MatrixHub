import { modals } from 'svelte-modals';
import { ConfirmDialog, InfoDialog, PromptDialog } from '$lib/components';
import { openModal, type ModalOpenService } from './modal';
import type { Component } from 'svelte';

type ConfirmLabels = {
	cancel?: { label: string; icon: Component };
	confirm?: { label: string; icon: Component };
};

export interface ConfirmOptions {
	title: string;
	message?: string;
	messageHtml?: string;
	labels?: ConfirmLabels;
	onConfirm: () => void;
	onCancel?: () => void;
	component?: unknown;
	modalService?: ModalOpenService;
}

export interface InfoOptions {
	title: string;
	message?: string;
	messageHtml?: string;
	onDismiss?: () => void;
	dismiss?: { label: string; icon: Component } | (() => void);
	component?: unknown;
	modalService?: ModalOpenService;
}

type PromptLabels = ConfirmLabels;

export interface PromptOptions {
	title: string;
	message?: string;
	defaultValue?: string;
	placeholder?: string;
	maxlength?: number;
	asciiOnly?: boolean;
	onConfirm: (value: string) => void;
	onCancel?: () => void;
	labels?: PromptLabels;
	component?: unknown;
	modalService?: ModalOpenService;
}

export function confirm(options: ConfirmOptions) {
	return openModal(
		options.component ?? ConfirmDialog,
		{
			title: options.title,
			message: options.message,
			messageHtml: options.messageHtml,
			labels: options.labels,
			onConfirm: options.onConfirm,
			onCancel: options.onCancel
		},
		options.modalService ?? modals
	);
}

export function info(options: InfoOptions) {
	return openModal(
		options.component ?? InfoDialog,
		{
			title: options.title,
			message: options.message,
			messageHtml: options.messageHtml,
			onDismiss: options.onDismiss,
			dismiss: options.dismiss
		},
		options.modalService ?? modals
	);
}

export function prompt(options: PromptOptions) {
	return openModal(
		options.component ?? PromptDialog,
		{
			title: options.title,
			message: options.message,
			defaultValue: options.defaultValue,
			placeholder: options.placeholder,
			maxlength: options.maxlength,
			asciiOnly: options.asciiOnly,
			onConfirm: options.onConfirm,
			onCancel: options.onCancel,
			labels: options.labels
		},
		options.modalService ?? modals
	);
}
