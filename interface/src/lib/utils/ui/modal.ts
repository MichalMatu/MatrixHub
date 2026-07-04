import { modals, type ModalComponent, type ModalProps } from 'svelte-modals';

export type ModalOpenOptions = {
	id?: string;
	replace?: boolean;
};

export type ModalOpenService = Pick<typeof modals, 'open'>;
export type ModalCloseService = Pick<typeof modals, 'close'>;
export type ModalCloseByIdService = Pick<typeof modals, 'closeById'>;
export type ModalStackService = Pick<typeof modals, 'open' | 'close' | 'closeById'>;
export const defaultModalStack: ModalStackService = modals;

function toModalComponent<TProps extends ModalProps<unknown> = ModalProps<unknown>>(
	component: unknown
): ModalComponent<TProps> {
	return component as unknown as ModalComponent<TProps>;
}

export function openModal<TProps extends Record<string, unknown> = Record<string, unknown>>(
	component: unknown,
	props?: TProps,
	modalService: ModalOpenService | undefined = modals,
	options?: ModalOpenOptions
) {
	const service = modalService ?? modals;
	const modalComponent = toModalComponent<ModalProps<unknown> & TProps>(component);
	if (options) {
		return service.open(modalComponent, props as never, options);
	}
	return service.open(modalComponent, props as never);
}

export function closeModal(modalService: ModalCloseService | undefined = modals) {
	return (modalService ?? modals).close();
}

export function closeModalLayers(
	levels: number,
	modalService: ModalCloseService | undefined = modals
) {
	return (modalService ?? modals).close(levels);
}

export function closeModalById(
	id: string,
	modalService: ModalCloseByIdService | undefined = modals
) {
	return (modalService ?? modals).closeById(id);
}
