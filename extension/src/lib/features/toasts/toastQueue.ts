export type ToastTone = "info" | "error";

export interface ToastItem {
  id: number;
  tone: ToastTone;
  message: string;
  key?: string;
}

interface ToastQueueOptions {
  maxToasts?: number;
  lifetimeMs?: number;
  onChange?: (items: ToastItem[]) => void;
}

export function createToastQueue(options: ToastQueueOptions = {}) {
  let items: ToastItem[] = [];
  let nextToastId = 1;
  const timers = new Map<number, ReturnType<typeof setTimeout>>();
  const maxToasts = options.maxToasts ?? 3;
  const lifetimeMs = options.lifetimeMs ?? 3000;

  function notify() {
    options.onChange?.(items);
  }

  function clearTimer(id: number) {
    const timer = timers.get(id);
    if (!timer) {
      return;
    }

    clearTimeout(timer);
    timers.delete(id);
  }

  function dismiss(id: number) {
    clearTimer(id);
    items = items.filter((toast) => toast.id !== id);
    notify();
    return items;
  }

  function armTimer(id: number) {
    clearTimer(id);

    const timer = setTimeout(() => {
      dismiss(id);
    }, lifetimeMs);

    timers.set(id, timer);
  }

  return {
    show(tone: ToastTone, message: string, options?: { key?: string }) {
      if (!message) {
        return items;
      }

      if (options?.key) {
        const keyedToast = items.find((toast) => toast.key === options.key);
        if (keyedToast) {
          keyedToast.tone = tone;
          keyedToast.message = message;
          armTimer(keyedToast.id);
          notify();
          return items;
        }
      }

      const existing = items.find(
        (toast) =>
          !toast.key && toast.tone === tone && toast.message === message,
      );
      if (existing) {
        armTimer(existing.id);
        notify();
        return items;
      }

      const toast: ToastItem = {
        id: nextToastId++,
        tone,
        message,
        key: options?.key,
      };

      items = [...items, toast];
      if (items.length > maxToasts) {
        const [oldest, ...rest] = items;
        clearTimer(oldest.id);
        items = rest;
      }

      armTimer(toast.id);
      notify();
      return items;
    },
    dismissByKey(key: string) {
      const toast = items.find((item) => item.key === key);
      if (!toast) {
        return items;
      }

      return dismiss(toast.id);
    },
    dismiss,
    clear() {
      timers.forEach((timer) => clearTimeout(timer));
      timers.clear();
      items = [];
      notify();
      return items;
    },
  };
}
