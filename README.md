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

## Display Settings

Tap the calculator display to open the color settings screen. Select one of the six color boxes to immediately apply the corresponding outline color and return to the calculator:

- Green
- Amber
- Blue
- Red
- Yellow
- White

The calculator uses a black background with colored lines and text for the selected scheme.
