# m5faces-rpn-calculator

An RPN scientific calculator for the M5Stack Faces Calculator panel. It uses the panel's physical keypad for number entry and arithmetic, while the display touchscreen provides the scientific functions and settings.

## Requirements

- M5Stack device with an M5Stack Faces base
- Faces Calculator panel
- Arduino IDE or Arduino CLI
- `M5Faces` and `M5Unified` libraries

Select the board that matches the M5Stack device being used, install the required libraries, and upload `m5faces-rpn-calculator.ino`.

## Calculator Usage

The calculator uses reverse Polish notation (RPN). Values are shown in the four-register stack from top to bottom: `T`, `Z`, `Y`, and `X`.

1. Enter a number with the physical keypad.
2. Press `=` or Enter to push the number into the stack.
3. Enter the next number and press an arithmetic operator such as `+`, `-`, `*`, or `/`.
4. The result is left in the `X` register.

Pressing Enter when no number is being entered duplicates the current `X` register. The clear key resets the stack and current entry. The sign key changes the sign of the current entry or `X` register. The `M` key enables the second-function labels on the touchscreen keys.

## Touch Functions

The six touch keys below the display provide:

- `1/x`
- `sqrt`
- `sin`, `cos`, and `tan`
- `DEG`/`RAD` angle mode

With `M` active, the touch keys provide `y^x`, `x^2`, and the inverse trigonometric functions.

## Display

The four-register stack is shown on a simulated LCD screen. A plastic bezel surrounds a tinted glass panel in which the register values are rendered with a custom seven-segment font: lit segments use the bright foreground color, while unlit ghost segments stay faintly visible through the glass, just like a real LCD. Long values shrink the glyphs automatically so they always fit, and results too large for fixed-point display are shown in seven-segment scientific notation (for example `1E20`).

## Display Settings

Hardware button A cycles to the next color scheme, while button C cycles to the previous scheme. Alternatively, tap the calculator display to open the color settings screen and select one of the six numbered color boxes:

1. Green electroluminescence
2. Amber LED
3. Red LED
4. Purple LED
5. Blue LED
6. Liquid crystal display

Each scheme recolors the LCD foreground, the off-segment ghosts, and the glass tint. The calculator uses a black background with a dark grey display bezel and light grey soft keys for every scheme, and the soft key labels stay in inverse text.
