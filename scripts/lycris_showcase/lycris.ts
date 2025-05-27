import { connectSocket, createPayload } from "./socket"
import { match, P } from "ts-pattern"
import { filter, interval, map, merge, Observable, of, switchMap } from "rxjs"
import figlet from "figlet"
import type { JsonValue } from "type-fest"
import path from "node:path"
import { readdir } from "node:fs/promises"
import { exec, execSync, type ChildProcess } from "node:child_process"
import chalk, { type ColorName } from "chalk"

const SOCKET_PATH = "/tmp/mpv-lycris.sock" as const
const colors: ColorName[] = ["redBright", "yellowBright", "whiteBright", "gray"]

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
// the 1 is a required id, which will be send back by the socket
client.write(createPayload("observe_property", [1, "sub-text"]).payload)

const fontsToUse: figlet.Fonts[] = (
	[
		"Big Money-ne",
		"Graffiti",
		"Fire Font-s",
		"Epic",
		"Mini",
		"ANSI Shadow",
		"The Edge",
		"Cosmike",
		// "Stronger than all",
		"THIS",
		"Calvin S",
		"Bloody",
		"Delta Corps Priest 1",
		"Elite",
	] as figlet.Fonts[]
).sort(
	(a, b) => figlet.textSync("XOA", b).length - figlet.textSync("XOA", a).length
)

const gifsDirectory = path.join(import.meta.dirname, "../../assets/gifs/")
const creeps = (await readdir(gifsDirectory)).map((fileName) =>
	path.join(gifsDirectory, fileName)
)

type ToRender =
	| {
			type: "text"
			text: string
	  }
	| { type: "image"; path: string }

const toRender$: Observable<ToRender> = events$.pipe(
	map((event) => {
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
	switchMap((text) => {
		if (Math.random() < 0.1) {
			return of({ type: "image" as const, path: pickRandom(creeps)! })
		}

		const word = pickWord(text)!
		const font = pickFont(word)

		if (!font) {
			return of({ type: "image" as const, path: pickRandom(creeps)! })
		}

		const ascii = figlet.textSync(word, font)

		return merge(
			of(ascii),
			interval(Math.random() * 400 + 280).pipe(
				map((index) => distortText(ascii, index * 0.3 + 1))
			)
		).pipe(
			map((text) => ({
				type: "text" as const,
				text,
			}))
		)
	})
)

toRender$.subscribe((toRender) => {
	console.clear()

	if (toRender.type === "image") {
		const width = process.stdout.columns
		const height = process.stdout.rows

		execSync(
			`kitty icat --place=${width}x${height}@0x0 --scale-up ${toRender.path}`,
			{
				stdio: "inherit",
			}
		)
	} else {
		const color = pickRandom(colors)!
		console.log(chalk[color](toRender.text))
	}
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
	return pickRandom(glitchSymbols)!
}

function pickFont(text: string): figlet.Fonts | undefined {
	const fitting = fontsToUse
		.slice(Math.ceil(3 * Math.random()))
		.find(
			(font) =>
				figlet.textSync(text, font).indexOf("\n") < process.stdout.columns
		)

	return fitting
}

function pickWord(text: string) {
	const words = text
		.replace(/the/i, "")
		.trim()
		.replace(/[,\.-]/, "")
		.replace(/ {2,}/, " ")
		.split(" ")

	return pickRandom(words)
}

function pickRandom<T>(array: T[]): T | undefined {
	const amount = array.length
	const indexToPick = Math.floor(Math.random() * (amount - 1))

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
