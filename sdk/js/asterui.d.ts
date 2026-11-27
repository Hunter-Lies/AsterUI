export {};

export type AsterUIEventName = string;
export type AsterUIEventPayload = Record<string, unknown> | unknown[] | string | number | boolean | null;
export type AsterUIEventHandler<T = AsterUIEventPayload> = (payload: T) => void;

export interface AsterUI {
  on<T = AsterUIEventPayload>(
    name: AsterUIEventName,
    callback: AsterUIEventHandler<T>
  ): () => void;
  off<T = AsterUIEventPayload>(
    name: AsterUIEventName,
    callback?: AsterUIEventHandler<T>
  ): boolean;
  emit(name: AsterUIEventName, payload?: AsterUIEventPayload): void;
}

declare global {
  interface Window {
    asterui: AsterUI;
    /** @deprecated Use window.asterui. */
    gtanui: AsterUI;
  }

  interface WindowEventMap {
    "asterui:ready": Event;
    /** @deprecated Use asterui:ready. */
    "gtanui:ready": Event;
  }
}
