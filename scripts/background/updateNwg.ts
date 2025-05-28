import {
	debounce,
	debounceTime,
	filter,
	map,
	pairwise,
	throttleTime,
} from "rxjs"
import { connectSocket } from "../socket"
import { match, P } from "ts-pattern"
import { exec } from "child_process"

const socketPath = "/tmp/audio_monitor.sock"
const updateSignal = 45

type RawAudioData = [
	number, // loudness
	number, // subwoofer
	number, // subtone, always zero
	number, // kickdrum
	number, // ignore, always zero, lowBass
	number, // bassBody
	number, // midBass
	number, // warmth
	number, // lowMids
	number, // midsMoody
	number, // upperMids
	number, // attack
	number, // highs
]

export async function startNwgUpdates() {
	const { events$ } = await connectSocket(socketPath, (buffer) => {
		const raw = buffer.toString().split(",").map(Number) as RawAudioData

		return raw
	})

	const shouldUpdatePanel$ = events$.pipe(
		map((event) => {
			return match(event)
				.with({ type: "data" }, ({ data }) => data)
				.with({ type: P.union("error", "connectError") }, ({ error }) => {
					throw error
				})
				.otherwise(() => undefined)
		}),
		filter(Boolean),
		filter((data) => !!data[0]),
		map((raw) => {
			const {
				1: subwoofer,
				3: kickdrum,
				5: bassBody,
				6: midBass,

				0: loudness,
			} = raw

			console.log({ bassBody, raw })
			return kickdrum == 1 && subwoofer == 1 && loudness >= 0.3 && midBass > 0.9
		}),
		throttleTime(8),
		filter(Boolean)
	)

	shouldUpdatePanel$.subscribe(() => {
		exec(`pkill -${updateSignal} nwg-panel`)
	})
}
