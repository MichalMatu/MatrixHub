export type Breadcrumb = {
	label: string;
	path: string;
};

function joinPathSegments(base: string, segment: string): string {
	const merged = `${base}/${segment}`;
	const parts = merged.split('/').filter(Boolean);
	return `/${parts.join('/')}`;
}

function trimTrailingSlashes(path: string): string {
	if (path === '/') {
		return '/';
	}

	let endIndex = path.length;
	while (endIndex > 1 && path[endIndex - 1] === '/') {
		endIndex -= 1;
	}

	return path.slice(0, endIndex);
}

export function basename(path: string): string {
	if (path === '/') {
		return '/';
	}

	const trimmed = trimTrailingSlashes(path);
	const parts = trimmed.split('/');
	return parts[parts.length - 1] || '/';
}

export function parentPath(path: string): string {
	if (path === '/' || path.length === 0) {
		return '/';
	}

	const segments = path.split('/').filter(Boolean);
	segments.pop();

	if (segments.length === 0) {
		return '/';
	}

	return `/${segments.join('/')}`;
}

export function getBreadcrumbs(path: string): Breadcrumb[] {
	if (path === '/' || path.length === 0) {
		return [];
	}

	const segments = path.split('/').filter(Boolean);
	const breadcrumbs: Breadcrumb[] = [];

	segments.reduce((acc, segment) => {
		const nextPath = joinPathSegments(acc, segment);
		breadcrumbs.push({
			label: segment,
			path: nextPath.length ? nextPath : `/${segment}`
		});
		return nextPath;
	}, '');

	return breadcrumbs;
}
