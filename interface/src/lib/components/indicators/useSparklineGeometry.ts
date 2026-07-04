/**
 * Sparkline geometry calculations (paths, points, scaling)
 */

export interface SparklinePoint {
	x: number;
	y: number;
}

export interface SparklineGeometry {
	points: string; // SVG polyline format
	fillPoints: string; // SVG polygon format
	smoothPath: string; // SVG path (Catmull-Rom)
	smoothFillPath: string; // SVG closed path
	lastPoint: SparklinePoint | null;
	pointsArray: SparklinePoint[];
}

export interface SparklineParams {
	data: number[];
	width: number;
	height: number;
	strokeWidth: number;
	domainMin?: number;
	domainMax?: number;
	padding?: number;
	smoothingTension: number;
}

/**
 * Create smooth Catmull-Rom spline path using Bezier conversion
 */
function createSmoothPath(points: SparklinePoint[], tension: number): string {
	if (points.length < 2) return '';
	if (points.length === 2) {
		return `M ${points[0].x},${points[0].y} L ${points[1].x},${points[1].y}`;
	}

	let path = `M ${points[0].x},${points[0].y}`;

	for (let i = 0; i < points.length - 1; i++) {
		const p0 = points[Math.max(0, i - 1)];
		const p1 = points[i];
		const p2 = points[i + 1];
		const p3 = points[Math.min(points.length - 1, i + 2)];

		// Catmull-Rom to Bezier conversion
		const cp1x = p1.x + (p2.x - p0.x) / tension;
		const cp1y = p1.y + (p2.y - p0.y) / tension;
		const cp2x = p2.x - (p3.x - p1.x) / tension;
		const cp2y = p2.y - (p3.y - p1.y) / tension;

		path += ` C ${cp1x},${cp1y} ${cp2x},${cp2y} ${p2.x},${p2.y}`;
	}

	return path;
}

/**
 * Create smooth fill path (closed area under curve)
 */
function createSmoothFillPath(
	points: SparklinePoint[],
	baseY: number,
	padX: number,
	tension: number
): string {
	if (points.length < 2) return '';

	const smoothLine = createSmoothPath(points, tension);
	const lastPoint = points[points.length - 1];

	// Close path: go down to baseline, back to start, up to first point
	return `${smoothLine} L ${lastPoint.x},${baseY} L ${padX},${baseY} Z`;
}

/**
 * Calculate sparkline geometry from data points
 */
export function calculateSparklineGeometry(params: SparklineParams): SparklineGeometry {
	const { data, width, height, strokeWidth, domainMin, domainMax, padding, smoothingTension } =
		params;

	const emptyResult: SparklineGeometry = {
		points: '',
		fillPoints: '',
		smoothPath: '',
		smoothFillPath: '',
		lastPoint: null,
		pointsArray: []
	};

	if (data.length < 2) return emptyResult;
	if (width <= 0 || height <= 0) return emptyResult;

	const pad = padding ?? Math.max(1, strokeWidth / 2 + 0.5);
	const innerWidth = Math.max(1, width - 2 * pad);
	const innerHeight = Math.max(1, height - 2 * pad);

	let min = Math.min(...data);
	let max = Math.max(...data);

	if (Number.isFinite(domainMin) && Number.isFinite(domainMax) && domainMax! > domainMin!) {
		min = domainMin!;
		max = domainMax!;
	}

	if (max === min) {
		const delta = Math.max(1, Math.abs(max) * 0.01);
		min = min - delta;
		max = max + delta;
	}

	const range = max - min;
	if (range <= 0 || !Number.isFinite(range)) return emptyResult;

	const xStep = innerWidth / (data.length - 1);
	const yScale = innerHeight / range;

	const pointsArray: SparklinePoint[] = [];

	data.forEach((value, i) => {
		const x = pad + i * xStep;
		let y = pad + (max - value) * yScale;
		y = Math.min(height - pad, Math.max(pad, y));
		pointsArray.push({ x, y });
	});

	const points = pointsArray.map((p) => `${p.x},${p.y}`).join(' ');

	// Create fill path (closed polygon for gradient fill)
	const fillPoints =
		pointsArray.length > 0
			? `${pad},${height - pad} ` +
				pointsArray.map((p) => `${p.x},${p.y}`).join(' ') +
				` ${pointsArray[pointsArray.length - 1].x},${height - pad}`
			: '';

	// Create smooth paths
	const smoothPath = createSmoothPath(pointsArray, smoothingTension);
	const smoothFillPath = createSmoothFillPath(pointsArray, height - pad, pad, smoothingTension);

	const lastPoint = pointsArray.length > 0 ? pointsArray[pointsArray.length - 1] : null;

	return { points, fillPoints, smoothPath, smoothFillPath, lastPoint, pointsArray };
}
