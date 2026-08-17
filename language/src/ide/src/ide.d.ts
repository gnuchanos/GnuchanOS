import type { IdeApi } from "./types";

declare global {
  interface Window {
    ide: IdeApi;
  }
}

export {};
