import type { MessageCatalogShape, MessageLeaf, PluralMessage } from "./types";
import { enMessages } from "./messages/en";
import { plMessages } from "./messages/pl";
import type { SupportedLocale } from "./locales";
import { DEFAULT_LOCALE } from "./locales";
import { isPluralMessage } from "./types";

type SourceCatalog = typeof enMessages;

export type MessageCatalog = MessageCatalogShape<SourceCatalog>;

type LeafPaths<T, Prefix extends string = ""> = T extends string | PluralMessage
  ? Prefix
  : T extends Record<string, unknown>
    ? {
        [K in keyof T & string]: LeafPaths<
          T[K],
          Prefix extends "" ? K : `${Prefix}.${K}`
        >;
      }[keyof T & string]
    : never;

export type TranslationKey = LeafPaths<SourceCatalog>;

export const messageCatalogs = {
  en: enMessages,
  pl: plMessages,
} satisfies Record<SupportedLocale, MessageCatalog>;

export function getMessageCatalog(locale: SupportedLocale): MessageCatalog {
  return messageCatalogs[locale] ?? messageCatalogs[DEFAULT_LOCALE];
}

export function resolveMessageValue(
  locale: SupportedLocale,
  key: TranslationKey,
): MessageLeaf | null {
  const source = getMessageCatalog(locale);
  const segments = key.split(".");
  let cursor: unknown = source;

  for (const segment of segments) {
    if (
      typeof cursor !== "object" ||
      cursor === null ||
      !(segment in cursor) ||
      isPluralMessage(cursor)
    ) {
      return null;
    }

    cursor = (cursor as Record<string, unknown>)[segment];
  }

  return typeof cursor === "string" || isPluralMessage(cursor) ? cursor : null;
}
