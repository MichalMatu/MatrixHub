export interface SparklinePoint {
  x: number;
  y: number;
}

export interface SparklineGeometry {
  points: string;
  fillPoints: string;
  smoothPath: string;
  smoothFillPath: string;
  lastPoint: SparklinePoint | null;
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

function createSmoothPath(points: SparklinePoint[], tension: number) {
  if (points.length < 2) {
    return "";
  }

  if (points.length === 2) {
    return `M ${points[0].x},${points[0].y} L ${points[1].x},${points[1].y}`;
  }

  let path = `M ${points[0].x},${points[0].y}`;

  for (let index = 0; index < points.length - 1; index += 1) {
    const p0 = points[Math.max(0, index - 1)];
    const p1 = points[index];
    const p2 = points[index + 1];
    const p3 = points[Math.min(points.length - 1, index + 2)];

    const cp1x = p1.x + (p2.x - p0.x) / tension;
    const cp1y = p1.y + (p2.y - p0.y) / tension;
    const cp2x = p2.x - (p3.x - p1.x) / tension;
    const cp2y = p2.y - (p3.y - p1.y) / tension;

    path += ` C ${cp1x},${cp1y} ${cp2x},${cp2y} ${p2.x},${p2.y}`;
  }

  return path;
}

function createSmoothFillPath(
  points: SparklinePoint[],
  baseY: number,
  padX: number,
  tension: number,
) {
  if (points.length < 2) {
    return "";
  }

  const smoothLine = createSmoothPath(points, tension);
  const lastPoint = points[points.length - 1];

  return `${smoothLine} L ${lastPoint.x},${baseY} L ${padX},${baseY} Z`;
}

export function calculateSparklineGeometry(
  params: SparklineParams,
): SparklineGeometry {
  const {
    data,
    width,
    height,
    strokeWidth,
    domainMin,
    domainMax,
    padding,
    smoothingTension,
  } = params;

  const emptyResult: SparklineGeometry = {
    points: "",
    fillPoints: "",
    smoothPath: "",
    smoothFillPath: "",
    lastPoint: null,
  };

  if (data.length < 2 || width <= 0 || height <= 0) {
    return emptyResult;
  }

  const pad = padding ?? Math.max(1, strokeWidth / 2 + 0.5);
  const innerWidth = Math.max(1, width - 2 * pad);
  const innerHeight = Math.max(1, height - 2 * pad);

  let min = Math.min(...data);
  let max = Math.max(...data);
  const boundedMin =
    typeof domainMin === "number" && Number.isFinite(domainMin)
      ? domainMin
      : undefined;
  const boundedMax =
    typeof domainMax === "number" && Number.isFinite(domainMax)
      ? domainMax
      : undefined;

  if (
    boundedMin !== undefined &&
    boundedMax !== undefined &&
    boundedMax > boundedMin
  ) {
    min = boundedMin;
    max = boundedMax;
  }

  if (max === min) {
    const delta = Math.max(1, Math.abs(max) * 0.01);
    min -= delta;
    max += delta;
  }

  const range = max - min;
  if (range <= 0 || !Number.isFinite(range)) {
    return emptyResult;
  }

  const xStep = innerWidth / (data.length - 1);
  const yScale = innerHeight / range;
  const pointsArray: SparklinePoint[] = [];

  data.forEach((value, index) => {
    const x = pad + index * xStep;
    let y = pad + (max - value) * yScale;
    y = Math.min(height - pad, Math.max(pad, y));
    pointsArray.push({ x, y });
  });

  const points = pointsArray.map((point) => `${point.x},${point.y}`).join(" ");
  const fillPoints =
    pointsArray.length > 0
      ? `${pad},${height - pad} ${points} ${pointsArray[pointsArray.length - 1].x},${height - pad}`
      : "";

  return {
    points,
    fillPoints,
    smoothPath: createSmoothPath(pointsArray, smoothingTension),
    smoothFillPath: createSmoothFillPath(
      pointsArray,
      height - pad,
      pad,
      smoothingTension,
    ),
    lastPoint: pointsArray[pointsArray.length - 1] ?? null,
  };
}
