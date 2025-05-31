import { spawn } from "node:child_process"
import path from "node:path"

const amountTiles = 10
const toExecute = path.join(import.meta.dirname, "lycris.ts")

for (let i = 0; i < amountTiles; i++) {
	spawn(
		"kitty",
		[
			"-o",
			"window_padding_width=0",
			"-o",
			"background_opacity=0.0",
			"sh",
			"-c",
			`bun ${toExecute} || read`,
		],
		{
			env: process.env,
			stdio: "ignore",
		}
	)
}
