import { filter, map, pairwise } from "rxjs"
import { connectSocket } from "../socket"
import { match, P } from "ts-pattern"
import { exec } from "child_process"

const socketPath = "/tmp/audio_monitor.sock"
const updateSignal = 45

type RawAudioData = [
	number, // subwoofer
	number, // subtone
	number, // kickdrum
	number, // lowBass
	number, // bassBody
	number, // midBass
	number, // warmth
	number, // lowMids
	number, // midsMoody
	number, // upperMids
	number, // attack
	number, // highs
]

type ParsedAudio = {
	midsMoody: number
}

const { events$ } = await connectSocket(socketPath, (buffer) => {
	const raw = JSON.parse(buffer.toString()) as RawAudioData

	return { midsMoody: raw[8] } as ParsedAudio
})

const shouldUpdatePanel$ = events$.pipe(
	map((event) => {
		return match(event)
			.with({ type: "data" }, ({ data }) => data.midsMoody)
			.with({ type: P.union("error", "connectError") }, ({ error }) => {
				throw error
			})
			.otherwise(() => undefined)
	}),
	filter(Boolean),
	pairwise(),
	map(([before, now]) => {
		const threshold = 0.8
		return Math.abs(before - now) > threshold ? true : false
	}),
	filter(Boolean)
)

shouldUpdatePanel$.subscribe(() => {
	exec(`pkill ${updateSignal} nwg-panel`)
})
