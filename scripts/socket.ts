import type { Socket } from "bun"
import type { JsonValue } from "type-fest"
import { Subject, type Observable } from "rxjs"

type SocketEvents<T> =
	| { type: "data"; data: T }
	| { type: "error"; error: Error }
	| { type: "connectError"; error: Error }
	| { type: "close" }
	| { type: "open" }

type SocketWrapper<T> = {
	events$: Observable<SocketEvents<T>>
	client: Socket<undefined>
}

export async function connectSocket<Data>(
	unixSocket: string,
	onData: (data: Buffer<ArrayBufferLike>) => Data
): Promise<SocketWrapper<Data>> {
	const events$ = new Subject<SocketEvents<Data>>()

	const client = await Bun.connect({
		unix: unixSocket,

		socket: {
			data(socket, data) {
				events$.next({ type: "data", data: onData(data) })
			},
			connectError(socket, error) {
				events$.next({ type: "connectError", error })
			},
			close(socket) {
				events$.next({ type: "close" })
				events$.complete()
			},
			open(socket) {
				events$.next({ type: "open" })
			},
			error(socket, error) {
				events$.next({ type: "error", error })
			},
		},
	})

	return { events$, client }
}

/** Creates a command payload to be send to the mpv unix socket */
export function createPayload(
	command: string,
	args: JsonValue[]
): { payload: string; id: number } {
	const id = Math.random() * 1000
	const payload = { command: [command, ...args], request_id: id }

	// mpv needs the trailing new line
	return { payload: JSON.stringify(payload) + "\n", id }
}
