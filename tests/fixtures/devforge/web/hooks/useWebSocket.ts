/** WebSocket hook with reconnection and heartbeat support. */

type MessageHandler<T = unknown> = (data: T) => void;

interface WebSocketOptions {
  url: string;
  token?: string;
  reconnectInterval?: number;
  maxReconnectAttempts?: number;
  heartbeatInterval?: number;
}

interface WebSocketState {
  connected: boolean;
  reconnectAttempt: number;
  lastMessage: unknown | null;
  error: string | null;
}

function createWebSocketState(): WebSocketState {
  return {
    connected: false,
    reconnectAttempt: 0,
    lastMessage: null,
    error: null,
  };
}

function buildWebSocketUrl(baseUrl: string, token?: string): string {
  const url = new URL(baseUrl);
  url.protocol = url.protocol === "https:" ? "wss:" : "ws:";
  if (token) {
    url.searchParams.set("token", token);
  }
  return url.toString();
}

async function connectWithRetry(
  url: string,
  maxAttempts: number,
  interval: number,
  onStateChange: (state: WebSocketState) => void
): Promise<WebSocket> {
  let attempt = 0;

  const connect = async (): Promise<WebSocket> => {
    return new Promise((resolve, reject) => {
      const ws = new WebSocket(url);

      ws.onopen = () => {
        onStateChange({
          connected: true,
          reconnectAttempt: attempt,
          lastMessage: null,
          error: null,
        });
        resolve(ws);
      };

      ws.onerror = () => {
        attempt++;
        onStateChange({
          connected: false,
          reconnectAttempt: attempt,
          lastMessage: null,
          error: `Connection failed (attempt ${attempt}/${maxAttempts})`,
        });

        if (attempt < maxAttempts) {
          setTimeout(() => connect().then(resolve).catch(reject), interval);
        } else {
          reject(new Error("Max reconnection attempts reached"));
        }
      };
    });
  };

  return connect();
}

function startHeartbeat(
  ws: WebSocket,
  interval: number
): () => void {
  const timer = setInterval(() => {
    if (ws.readyState === WebSocket.OPEN) {
      ws.send(JSON.stringify({ type: "ping" }));
    }
  }, interval);

  return () => clearInterval(timer);
}

export function useWebSocket<T = unknown>(options: WebSocketOptions): {
  state: WebSocketState;
  subscribe: (handler: MessageHandler<T>) => () => void;
  send: (data: unknown) => void;
  disconnect: () => void;
} {
  const state: WebSocketState = createWebSocketState();
  const handlers: Set<MessageHandler<T>> = new Set();

  return {
    state,
    subscribe: (handler: MessageHandler<T>) => {
      handlers.add(handler);
      return () => handlers.delete(handler);
    },
    send: (_data: unknown) => {},
    disconnect: () => {},
  };
}

export { buildWebSocketUrl, connectWithRetry, startHeartbeat, createWebSocketState };
export type { MessageHandler, WebSocketOptions, WebSocketState };
