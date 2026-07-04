import { browser } from "wxt/browser";

const COOKIE_NAME = "access_token";

function cookieUrl(origin: string, path: string) {
  return `${origin}${path}`;
}

export async function syncWsAccessTokenCookie(
  origin: string,
  accessToken: string,
) {
  const secure = origin.startsWith("https://");

  // HTTP requests use a Bearer token via the shared SDK, but the ESP32 /ws
  // endpoint currently authenticates from a cookie. We mirror the token here so
  // refreshOverview and the realtime socket stay in sync.
  //
  // The sidepanel runs on chrome-extension://, so websocket handshakes to the
  // device are cross-site from the browser's perspective. SameSite=Strict was
  // preventing the access_token cookie from being sent on /ws/system even
  // though REST requests succeeded with Bearer auth. For secure device origins
  // we must allow a cross-site cookie so WS auth can actually work.
  await browser.cookies.set({
    url: cookieUrl(origin, "/ws"),
    name: COOKIE_NAME,
    value: accessToken,
    path: "/ws",
    secure,
    ...(secure ? { sameSite: "no_restriction" as const } : {}),
  });
}

export async function clearWsAccessTokenCookie(origin: string) {
  // Clearing both /ws and / helps when debugging older device builds or
  // previous experiments that may have written the cookie with a broader path.
  await browser.cookies.remove({
    url: cookieUrl(origin, "/ws"),
    name: COOKIE_NAME,
  });

  await browser.cookies.remove({
    url: cookieUrl(origin, "/"),
    name: COOKIE_NAME,
  });
}
