export function portal(node: HTMLElement, target: HTMLElement | string = document.body) {
	let targetEl: HTMLElement;

	function update(newTarget: HTMLElement | string) {
		if (typeof newTarget === 'string') {
			targetEl = document.querySelector(newTarget) as HTMLElement;
			if (targetEl === null) {
				targetEl = document.body;
			}
		} else if (newTarget instanceof HTMLElement) {
			targetEl = newTarget;
		} else {
			targetEl = document.body;
		}

		targetEl.appendChild(node);
		node.hidden = false;
	}

	function destroy() {
		if (node.parentNode) {
			node.parentNode.removeChild(node);
		}
	}

	update(target);

	return {
		update,
		destroy
	};
}
