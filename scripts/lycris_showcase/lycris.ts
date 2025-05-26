import path from "node:path"
import { connectSocket, createPayload } from "./socket"
import fs from "node:fs/promises"
import { match, P } from "ts-pattern"
import { filter, interval, map, merge, Observable, of, switchMap } from "rxjs"
import figlet from "figlet"
import type { JsonValue } from "type-fest"

const SOCKET_PATH = "/tmp/mpv-lycris.sock" as const

const { events$, client } = await connectSocket(SOCKET_PATH, (data) =>
	data
		.toString()
		.split("\n")
		.filter(Boolean)
		.map((string) => {
			try {
				return JSON.parse(string) as JsonValue
			} catch (error) {
				console.warn("Failed to parse mpv json", { error, string })
				return undefined
			}
		})
		.filter(Boolean)
)
// the 1 is a required id, which will be send back
client.write(createPayload("observe_property", [1, "sub-text"]).payload)

const fontsToUse: figlet.Fonts[] = ["Big Money-ne"]

// the "o" letter gets used to determine the width
const fontsWidths: readonly { name: figlet.Fonts; width: number }[] = fontsToUse
	.map((name_) => ({
		name: name_,
		width: figlet.textSync("o", name_).length,
	}))
	.sort(({ width: a }, { width: b }) => b - a)

// const creeps = await fs.readdir(
// 	path.join(import.meta.dirname, "../../assets/gifs/")
// )

const toRender$: Observable<string> = events$.pipe(
	map((event) => {
		event.type === "data" && console.log(event.data)
		return match(event)
			.with({ type: "data" }, ({ data }) => data)
			.with({ type: P.union("error", "connectError") }, ({ error }) => {
				throw error
			})
			.otherwise(() => undefined)
	}),
	filter(Array.isArray),
	map((events) => events.findLast(isMpvSubtextUpdate)?.data),
	filter(Boolean),
	map((text) => figlet.textSync(pickWord(text), pickFont(text))),
	switchMap((ascii) =>
		merge(
			of(ascii),
			interval(Math.random() * 300 + 150).pipe(
				map((index) => distortText(ascii, index + 1))
			)
		)
	)
)

toRender$.subscribe((toRender) => {
	console.clear()
	console.log(toRender)
})

// prettier-ignore
const glitchSymbols = [
  '꙰', '҉', '҂', '⛧', '𖤐', '☠', '⛓', '†', '༒', '⚝', '𐂃', '𐰴', '𖣘', '꧁', '꧂', 'ᓭ', '⚚', '♆', '∷', 'ʬ', '⟁', '⛤', '₪', '₷', '₥', '⌬', '☢', '⛩', 'ᛉ', 'ᚲ', '𖤓', '⩿', '𝌆', '᙮', '✠', '𓆩', '𓆪', '𓂀', '𓄿', '𓇋', 'ꓷ', '𒀭', '�', 'Ͳ', 'ͳ', 'Ͷ', 'ͷ', 'ͺ', 'ͻ', '̷', '̸', '̶', '͏', '̀', '́', '͜', '͡', '͠', '͢', '𝖑', '𝖘', '𝖃', '𝕯', '𝕸', '𝖅', '█', '▓', '▒', '░', '␀', '␃', '␇', '␉', 'Ƀ', 'Ł', 'Ƨ', 'Ж', 'Ѯ', '⍰', '⌇', '≋', '≣', '⧖', ];

function distortText(text: string, strength: number): string {
	const columns = text.indexOf("\n")
	const rows = text.split("\n").length
	const centerX = columns / 2
	const centerY = rows / 2
	const maxDistance = Math.sqrt(centerX ** 2 + centerY ** 2)

	return text
		.split("")
		.map((letter, index) => {
			const column = index % columns
			const row = Math.floor(index / columns)
			const offsetX = centerX - column
			const offsetY = centerY - row
			const offsetFromCenter =
				Math.sqrt(offsetX * offsetX + offsetY * offsetY) / maxDistance

			const shouldGlitch =
				!/\s/.test(letter) &&
				Math.random() < 0.15 * strength * (1 - offsetFromCenter + 0.2)

			return shouldGlitch ? glitchSymbol() : letter
		})
		.join("")
}

function glitchSymbol(): string {
	return pickRandom(glitchSymbols)
}

function pickFont(text: string) {
	const fitting = fontsWidths.filter(
		({ width }) => width < process.stdout.columns
	)

	return pickRandom(fitting).name
}

function pickWord(text: string) {
	const words = text
		.trim()
		.replace(/[,\.-]/, "")
		.replace(/ {2,}/, " ")
		.split(" ")

	return pickRandom(words)
}

function pickRandom<T>(array: T[]): T {
	const amount = array.length
	const indexToPick = Math.ceil(Math.random() * (amount - 1))

	return array[indexToPick]
}

type MpvSubtextUpdate = {
	event: "property-change"
	id: number
	name: "sub-text"
	data?: string
}

function isMpvSubtextUpdate(data: unknown): data is MpvSubtextUpdate {
	return (
		!!data &&
		typeof data === "object" &&
		"event" in data &&
		"name" in data &&
		data.name === "sub-text"
	)
}
