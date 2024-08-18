# Sergiovan's own QMK toys

These are my forays into QMK, hacking away at these keyboards for fun.

I'm not a keyboard enthusiast so please forgive my transgressions (or don't, that's ok too)

## Installation

Symlink `users/sergiovan` and `keyboards/**/sergiovan` to their appropriate locations in a working [qmk_firmware](https://github.com/qmk/qmk_firmware) clone, set your keyboard settings and compile.

## Running Tests

Unit tests exist for some of my common functionality, since it's hard to test them from inside the keyboard.

Go to [users/sergiovan](users/sergiovan).

Run [build/setup.sh](users/sergiovan/build/setup.sh) first.

Then run [build/run_tests.sh](users/sergiovan/build/run_tests.sh) to run all tests, or [build/run_coverage.sh](users/sergiovan/build/run_coverage.sh) to generate a code coverage html and open it in Firefox.

You can also run the tests manually from `build/<debug or coverage>/<test binary name>` if you'd like to pass gtest your own parameters.

## VS Code

Helpful settings and launch targets.

### Settings

Automated formatting and clangd intellisense. Requires [LLVM's clangd](https://marketplace.visualstudio.com/items?itemName=llvm-vs-code-extensions.vscode-clangd) extension and [Xaver Hellauer's clang-format](https://marketplace.visualstudio.com/items?itemName=xaver.clang-format) extension.

```json
{
	"[c]": {
		"editor.formatOnSave": true,
		"editor.defaultFormatter": "xaver.clang-format"
	},
	"[cpp]": {
		"editor.formatOnSave": true,
		"editor.defaultFormatter": "xaver.clang-format"
	},
	"clangd.arguments": [
		"-log=verbose",
		"--compile-commands-dir=${workspaceRoot}/build/debug"
	]
}
```

### Launch

Launching a gtest unit test binary with breaks on failure. Requires [Microsoft's C/C++](https://marketplace.visualstudio.com/items?itemName=ms-vscode.cpptools) extension

```json
{
	"configurations": [
		{
			"name": "Launch <binary name>",
			"type": "cppdbg",
			"cwd": "${workspaceFolder}",
			"request": "launch",
			"program": "${workspaceFolder}/build/debug/<binary name>",
			"args": ["--gtest_break_on_failure"]
		}
	]
}
```
