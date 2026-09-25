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

The four-register stack is shown on a simulated LCD screen. A dark grey plastic bezel surrounds a tinted glass panel in which the register values are rendered with a custom seven-segment font: lit segments use the bright foreground color, while unlit ghost segments stay faintly visible through the glass, just like a real LCD. The second color scheme renders the same values with a 5x7 dot matrix instead, where every dot is drawn slightly smaller than its slot so the pixel grid stays visible and the unlit dots remain faintly lit. Long values shrink the glyphs automatically so they always fit. Results too large for fixed-point display are shown in scientific notation (for example `1E20`), and magnitudes below `0.0001` switch to scientific notation with four decimal places in the mantissa (for example `1.2345E-07`) instead of rounding away to zero. Errors read `ERROR`; on the seven-segment schemes the `R` falls back to the lowercase-`r` form real LCDs use, since a segment cell cannot draw a capital `R`.

While the second function is held, a `2nd` badge appears at the right of the title strip. It is drawn from the same 5x7 bitmap font as the dot-matrix values, in solid pixels knocked out of a filled rounded rectangle.

Only the part of the screen that changed is repainted: a keypress redraws the register bands alone, the angle mode key redraws the soft keys alone, and the `M` key redraws the title strip and soft keys. Each redraw is also wrapped in a single display write, which keeps the panel from showing a half-drawn screen.

## Display Settings

Button B opens the color settings screen, and pressing it again returns to the calculator. Hardware button A cycles to the next color scheme, while button C cycles to the previous scheme, and either works while the settings screen is open. Select one of the six numbered color boxes by tapping it, which also returns to the calculator:

1. Green electroluminescence, seven-segment
2. Green electroluminescence, 5x7 dot matrix
3. Red LED
4. Purple LED
5. Blue LED
6. Liquid crystal display

Each scheme recolors the LCD foreground, the off-segment ghosts, and the glass tint. The calculator uses a black background with a dark grey display bezel and light grey soft keys for every scheme, and the soft key labels stay in inverse text.
