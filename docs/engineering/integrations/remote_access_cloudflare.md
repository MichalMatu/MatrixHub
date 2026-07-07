# Remote Access via Cloudflare Relay

This document captures the planned customer-facing remote access model for
MatrixHub. It is a product design and implementation guide, not an implemented
feature.

## Product Decision

Use a Cloudflare-hosted relay as the target production architecture:

- Cloudflare Pages for the customer portal UI.
- Cloudflare Worker for HTTP API routing, authentication, pairing, and session
  creation.
- Cloudflare Durable Objects for per-device or per-session WebSocket
  coordination.
- MatrixHub firmware opens an outbound `wss://` connection to the relay.

Do not expose customer devices by IP address. Most customer networks are behind
NAT, carrier-grade NAT, firewalls, or changing public IPs. The relay should route
traffic through the already-established device WebSocket.

Use Raspberry Pi 5 plus Cloudflare Tunnel only as an MVP or lab deployment. It is
useful for early validation, but it is not the recommended production product
surface because uptime, backup, scaling, monitoring, and incident response would
depend on a home-hosted server.

## User Experience

1. The user opens MatrixHub locally or through AP setup.
2. The user enables `Remote Access`.
3. MatrixHub shows a short pairing code and optionally a QR code.
4. The user opens the hosted customer portal, signs in, and enters the pairing
   code.
5. The relay pairs the device with the user account and provisions long-lived
   device credentials.
6. Later, the user opens the portal, selects the device, and starts a remote
   admin session.
7. The portal proxies the admin UI/API through the relay and the active ESP32
   WebSocket connection.

Remote access should default to disabled. The device UI should make it obvious
when remote access is enabled, connected, and actively in use.

## Security Model

Pairing codes are not permanent passwords. They should be short-lived,
single-use, and rate-limited.

After pairing, the device should store:

- `device_id`
- `device_secret`
- relay URL
- remote access enabled flag
- last successful relay connection timestamp

The cloud side should store:

- user account
- device ownership and display name
- salted hash or wrapped form of device secret
- pairing code state and expiration
- active connection metadata
- audit events for pairing, session start, session end, and device disconnects

The relay should require:

- HTTPS/WSS only
- authenticated user sessions
- optional MFA for the customer portal
- rate limiting for pairing and login attempts
- per-device authorization checks on every remote session
- short-lived browser-to-device session tokens
- revocation support from both portal and local device UI

The relay should never trust a device-supplied user id. The user/device
relationship must come from server-side pairing records.

## Firmware Preparation

Add a remote access domain module, for example under `src/remote_access/`, once
implementation starts. Keep transport and product state separated:

- settings model and RTC/config JSON serialization
- device identity and secret storage
- pairing code request/confirm flow
- WSS client lifecycle
- heartbeat and reconnect backoff
- request/response tunnel framing
- admin session state and visible UI status

Recommended firmware settings:

- `enabled`
- `relay_url`
- `device_id`
- `device_secret`
- `last_pairing_status`
- `last_connection_status`
- `last_connected_at`

Recommended firmware API surface:

- `GET /api/remote-access/status`
- `POST /api/remote-access/pairing/start`
- `POST /api/remote-access/pairing/cancel`
- `POST /api/remote-access/disconnect`
- `POST /api/remote-access/revoke`

The device should connect outbound to:

```text
wss://relay.example.com/device
```

Initial device handshake should include only the minimum needed fields:

```json
{
  "type": "device_hello",
  "device_id": "mh_...",
  "nonce": "...",
  "signature": "..."
}
```

Use a challenge/response or signed nonce based on `device_secret`. Do not send
the raw device secret over the WebSocket.

## Relay Preparation

Recommended Cloudflare components:

- Pages: static customer portal frontend.
- Worker: API endpoints and routing.
- Durable Object: connection coordinator for each device or session.
- D1 or external database: user, device, pairing, and audit records.
- KV only for non-critical cache data. Do not use it as the source of truth for
  ownership or secrets.

Suggested public routes:

- `GET /` customer portal
- `POST /api/auth/*` login/session flow
- `POST /api/pairing/claim` claim a pairing code
- `GET /api/devices` list owned devices
- `POST /api/devices/:id/sessions` start a remote session
- `GET /device` WebSocket upgrade endpoint for MatrixHub devices
- `GET /client/:session_id` WebSocket upgrade endpoint for browser sessions

Suggested Durable Object responsibilities:

- keep or recover active device WebSocket state
- route browser session frames to the correct device connection
- route device responses back to the correct browser session
- enforce one or more active session policy per device
- close sessions on timeout, device disconnect, or user revocation
- emit audit events

## Tunnel Framing

Keep the relay protocol explicit instead of forwarding raw TCP. A simple JSON
envelope is easier to audit and test:

```json
{
  "type": "http_request",
  "request_id": "req_...",
  "method": "GET",
  "path": "/api/system/status",
  "headers": {
    "accept": "application/json"
  },
  "body_b64": ""
}
```

Device response:

```json
{
  "type": "http_response",
  "request_id": "req_...",
  "status": 200,
  "headers": {
    "content-type": "application/json"
  },
  "body_b64": "..."
}
```

For firmware memory safety, cap request and response sizes. Large streaming
features, file downloads, uploads, and logs should be added later with a chunked
binary frame format.

## Implementation Phases

### Phase 1: MVP on Raspberry Pi 5

- Build a local relay service on Raspberry Pi 5.
- Publish it through Cloudflare Tunnel.
- Implement a minimal ESP32 outbound WSS connection.
- Route a limited set of read-only API calls first.
- Validate disconnects, reconnects, and pairing revocation.

### Phase 2: Cloudflare Product Relay

- Move relay API to Worker.
- Move connection coordination to Durable Objects.
- Add durable pairing and ownership storage.
- Add portal login, device list, session start, and audit log.
- Keep the tunnel limited to selected admin/API paths until security review.

### Phase 3: Full Remote Admin

- Proxy the admin UI through the relay.
- Support WebSocket-backed live status.
- Add explicit active-session indicators in local and remote UI.
- Add customer-facing documentation and support recovery flows.

## Validation Checklist

- Pairing code expires and cannot be reused.
- Device secret is never sent in plaintext.
- Disabled remote access closes any active relay connection.
- Revoked device cannot reconnect without pairing again.
- Browser session cannot access a device owned by another user.
- Rate limiting blocks brute-force pairing attempts.
- Device reconnect backoff does not overload Wi-Fi or the relay.
- Local admin UI clearly shows remote access state.
- Firmware handles relay outage without blocking local dashboard features.
- Remote session logs are available for support without exposing credentials.

## Open Product Questions

- Should each customer require an account, or can a device owner use magic-link
  email access for the first release?
- Should remote access be free, paid, or limited to a Pro SKU?
- Should customers be able to invite a temporary support user?
- Which local admin routes are safe enough for the first remote release?
- How much remote access audit history should be retained?

## References

- Cloudflare Tunnel:
  https://developers.cloudflare.com/cloudflare-one/networks/connectors/cloudflare-tunnel/
- Cloudflare Durable Objects WebSockets:
  https://developers.cloudflare.com/durable-objects/best-practices/websockets/
- Cloudflare Durable Objects WebSocket hibernation example:
  https://developers.cloudflare.com/durable-objects/examples/websocket-hibernation-server/
