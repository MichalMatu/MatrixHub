import { readable } from "svelte/store";
import { getI18nRuntime, subscribeI18n } from "./runtime";

export const i18n = readable(getI18nRuntime(), (set) => subscribeI18n(set));
