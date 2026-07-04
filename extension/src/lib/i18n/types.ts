export type TranslationParamValue =
  | string
  | number
  | boolean
  | null
  | undefined;

export type TranslationParams = Readonly<Record<string, TranslationParamValue>>;

export interface PluralMessage {
  readonly _type: "plural";
  readonly zero?: string;
  readonly one?: string;
  readonly two?: string;
  readonly few?: string;
  readonly many?: string;
  readonly other: string;
}

export type MessageLeaf = string | PluralMessage;

export type MessageTree = {
  readonly [key: string]: MessageLeaf | MessageTree;
};

export type MessageCatalogShape<T> = T extends string
  ? string
  : T extends PluralMessage
    ? PluralMessage
    : T extends Record<string, unknown>
      ? {
          readonly [K in keyof T]: MessageCatalogShape<T[K]>;
        }
      : never;

export function plural<T extends Omit<PluralMessage, "_type">>(forms: T) {
  return {
    _type: "plural" as const,
    ...forms,
  };
}

export function isPluralMessage(value: unknown): value is PluralMessage {
  return (
    typeof value === "object" &&
    value !== null &&
    "_type" in value &&
    (value as { _type?: unknown })._type === "plural"
  );
}
