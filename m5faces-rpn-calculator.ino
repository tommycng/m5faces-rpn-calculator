
#include <Arduino.h>
#include <M5Faces.h>
#include <M5Unified.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

namespace {

M5Faces_Calculator3 calculator;

String entry = "0";
double stack[4] = {0,0,0,0};   // X,Y,Z,T
int stack_depth = 0;
bool entering = false;
bool calc_error = false;
bool modifier_active = false;

constexpr double kPi = 3.14159265358979323846;
constexpr double kDegreesToRadians = kPi / 180.0;
constexpr int kSoftKeyY = 160;
constexpr int kSoftKeyHeight = 72;
constexpr int kSoftKeyWidth = 48;
constexpr int kSoftKeyGap = 4;
constexpr int kSoftKeyLeft = 4;
constexpr int kDisplayX = 8;
constexpr int kDisplayY = 27;
constexpr int kDisplayWidth = 304;
constexpr int kDisplayHeight = 121;
constexpr int kSchemeBoxWidth = 80;
constexpr int kSchemeBoxHeight = 64;
constexpr int kSchemeBoxGap = 12;
constexpr int kSchemeBoxLeft = 16;
constexpr int kSchemeBoxTop = 48;

constexpr uint16_t rgb565(uint8_t red, uint8_t green, uint8_t blue)
{
    return ((red & 0xf8) << 8) | ((green & 0xfc) << 3) | (blue >> 3);
}

struct ColorScheme {
    const char* name;
    uint16_t background;
    uint16_t display;
    uint16_t foreground;
    uint16_t accent;
    uint16_t ui_text;
};

const ColorScheme color_schemes[] = {
    {"Green Lines", TFT_BLACK, TFT_BLACK, rgb565(0, 255, 90), rgb565(0, 255, 90), rgb565(0, 255, 90)},
    {"Amber Lines", TFT_BLACK, TFT_BLACK, rgb565(255, 180, 0), rgb565(255, 180, 0), rgb565(255, 180, 0)},
    {"Blue Lines", TFT_BLACK, TFT_BLACK, rgb565(70, 175, 255), rgb565(70, 175, 255), rgb565(70, 175, 255)},
    {"Red Lines", TFT_BLACK, TFT_BLACK, rgb565(255, 55, 55), rgb565(255, 55, 55), rgb565(255, 55, 55)},
    {"Yellow Lines", TFT_BLACK, TFT_BLACK, rgb565(255, 255, 0), rgb565(255, 255, 0), rgb565(255, 255, 0)},
    {"White Lines", TFT_BLACK, TFT_BLACK, TFT_WHITE, TFT_WHITE, TFT_WHITE},
};
constexpr int kColorSchemeCount = sizeof(color_schemes) / sizeof(color_schemes[0]);
int selected_color_scheme = 0;
bool setup_screen = false;

const ColorScheme& colors()
{
    return color_schemes[selected_color_scheme];
}

const char* const normal_soft_key_labels[] = {"1/x", "sqrt", "sin", "cos", "tan"};
const char* const modified_soft_key_labels[] = {"y^x", "x^2", "sin", "cos", "tan"};
bool angle_degrees = true;

String format_number(double value)
{
    if (!isfinite(value)) return "ERROR";
    String result(value, 6);
    while (result.indexOf('.') >= 0 && result.endsWith("0")) result.remove(result.length() - 1);
    if (result.endsWith(".")) result.remove(result.length() - 1);
    if (result == "-0") result = "0";

    if (result.length() > 12) {
        char buffer[24];
        result = "";
        for (int precision = 9; precision >= 1; --precision) {
            snprintf(buffer, sizeof(buffer), "%.*g", precision, value);
            if (strlen(buffer) <= 12) {
                result = buffer;
                break;
            }
        }
    }
    return result;
}

void push(double v) {
    stack[3] = stack[2];
    stack[2] = stack[1];
    stack[1] = stack[0];
    stack[0] = v;
    if (stack_depth < 4) ++stack_depth;
}

void drop_stack() {
    stack[0] = stack[1];
    stack[1] = stack[2];
    stack[2] = stack[3];
    stack[3] = 0.0;
    if (stack_depth > 0) --stack_depth;
}

void clear_calculator()
{
    entry = "0";
    entering = false;
    calc_error = false;
    for (int i=0;i<4;i++) stack[i]=0;
    stack_depth = 0;
    modifier_active = false;
}

void commit_entry();

bool do_op(char op)
{
    double x = stack[0];
    double y = stack[1];

    switch(op) {
        case '+': stack[1] = y + x; break;
        case '-': stack[1] = y - x; break;
        case '*': stack[1] = y * x; break;
        case '/':
            if (x == 0.0) return false;
            stack[1] = y / x;
            break;
        default: return true;
    }
    drop_stack();
    return isfinite(stack[0]);
}

double current_x()
{
    return entering ? entry.toDouble() : stack[0];
}

bool apply_unary(double (*function)(double))
{
    const double result = function(current_x());
    if (!isfinite(result)) {
        calc_error = true;
        entering = false;
        return false;
    }
    stack[0] = result;
    if (stack_depth == 0) stack_depth = 1;
    entering = false;
    return true;
}

double reciprocal(double value)
{
    return value == 0.0 ? NAN : 1.0 / value;
}

double square_root(double value)
{
    return value < 0.0 ? NAN : sqrt(value);
}

double sine_degrees(double value)
{
    return sin(angle_degrees ? value * kDegreesToRadians : value);
}

double cosine_degrees(double value)
{
    return cos(angle_degrees ? value * kDegreesToRadians : value);
}

double tangent_degrees(double value)
{
    return tan(angle_degrees ? value * kDegreesToRadians : value);
}

double square(double value)
{
    return value * value;
}

double arcsine_degrees(double value)
{
    const double result = asin(value);
    return angle_degrees ? result / kDegreesToRadians : result;
}

double arccosine_degrees(double value)
{
    const double result = acos(value);
    return angle_degrees ? result / kDegreesToRadians : result;
}

double arctangent_degrees(double value)
{
    const double result = atan(value);
    return angle_degrees ? result / kDegreesToRadians : result;
}

bool power_y_to_x()
{
    commit_entry();
    if (stack_depth < 2) {
        calc_error = true;
        return false;
    }

    const double result = pow(stack[1], stack[0]);
    if (!isfinite(result)) {
        calc_error = true;
        return false;
    }

    stack[1] = result;
    drop_stack();
    return true;
}

void draw_soft_key(int index)
{
    const int x = kSoftKeyLeft + index * (kSoftKeyWidth + kSoftKeyGap);
    M5.Display.drawRoundRect(x, kSoftKeyY, kSoftKeyWidth, kSoftKeyHeight, 6, colors().accent);
    M5.Display.setFont(&fonts::FreeMonoBold9pt7b);
    M5.Display.setTextDatum(middle_center);
    M5.Display.setTextColor(colors().ui_text, colors().background);
    if (index == 5) {
        M5.Display.drawString(angle_degrees ? "DEG" : "RAD", x + kSoftKeyWidth / 2,
                              kSoftKeyY + kSoftKeyHeight / 2);
        return;
    }
    const char* const* labels = modifier_active ? modified_soft_key_labels : normal_soft_key_labels;
    if (modifier_active && index >= 2) {
        M5.Display.drawString("arc", x + kSoftKeyWidth / 2, kSoftKeyY + kSoftKeyHeight / 2 - 10);
        M5.Display.drawString(labels[index], x + kSoftKeyWidth / 2, kSoftKeyY + kSoftKeyHeight / 2 + 10);
    } else {
        M5.Display.drawString(labels[index], x + kSoftKeyWidth / 2,
                              kSoftKeyY + kSoftKeyHeight / 2);
    }
}

void redraw_calculator()
{
    M5.Display.fillScreen(colors().background);
    M5.Display.setFont(&fonts::FreeMonoBold12pt7b);
    M5.Display.setTextDatum(middle_left);
    M5.Display.setTextColor(colors().ui_text, colors().background);
    M5.Display.drawString("RPN", 8, 13);

    M5.Display.setTextDatum(middle_right);
    M5.Display.setTextColor(colors().ui_text, colors().background);
    M5.Display.drawString(modifier_active ? "2nd" : "", 312, 13);

    // Classical calculator-style LCD area.
    M5.Display.drawRoundRect(kDisplayX, kDisplayY, kDisplayWidth, kDisplayHeight, 5, colors().accent);
    M5.Display.setFont(&fonts::FreeMonoBold9pt7b);
    M5.Display.setTextDatum(middle_left);
    M5.Display.setTextColor(colors().foreground, colors().display);
    M5.Display.drawString("T", 17, 47);
    M5.Display.drawString("Z", 17, 70);
    M5.Display.drawString("Y", 17, 93);
    M5.Display.drawString("X", 17, 126);

    M5.Display.setTextColor(colors().foreground, colors().display);
    M5.Display.setTextDatum(middle_right);
    M5.Display.drawString(stack_depth > 3 ? format_number(stack[3]) : "", 300, 47);
    M5.Display.drawString(stack_depth > 2 ? format_number(stack[2]) : "", 300, 70);
    M5.Display.drawString(stack_depth > 1 ? format_number(stack[1]) : "", 300, 93);

    M5.Display.setFont(&fonts::FreeMonoBold18pt7b);
    M5.Display.setTextDatum(middle_right);
    M5.Display.setTextColor(colors().foreground, colors().display);
    String x_display = calc_error ? "ERROR" : (entering ? entry : format_number(stack[0]));
    if (x_display.length() > 12) x_display.remove(12);
    M5.Display.drawString(x_display, 300, 126);

    for (int index = 0; index < 6; ++index) {
        draw_soft_key(index);
    }
}

void redraw_setup()
{
    M5.Display.fillScreen(colors().background);
    for (int index = 0; index < kColorSchemeCount; ++index) {
        const int column = index % 3;
        const int row = index / 3;
        const int x = kSchemeBoxLeft + column * (kSchemeBoxWidth + kSchemeBoxGap);
        const int y = kSchemeBoxTop + row * (kSchemeBoxHeight + kSchemeBoxGap);
        const bool selected = index == selected_color_scheme;
        M5.Display.fillRoundRect(x, y, kSchemeBoxWidth, kSchemeBoxHeight, 8,
                                color_schemes[index].foreground);
        if (selected) {
            M5.Display.drawRoundRect(x - 3, y - 3, kSchemeBoxWidth + 6,
                                     kSchemeBoxHeight + 6, 10, TFT_WHITE);
        }
    }
}

int soft_key_at(int x, int y)
{
    if (y < kSoftKeyY || y >= kSoftKeyY + kSoftKeyHeight) return -1;
    for (int index = 0; index < 6; ++index) {
        const int left = kSoftKeyLeft + index * (kSoftKeyWidth + kSoftKeyGap);
        if (x >= left && x < left + kSoftKeyWidth) return index;
    }
    return -1;
}

bool display_at(int x, int y)
{
    return x >= kDisplayX && x < kDisplayX + kDisplayWidth &&
           y >= kDisplayY && y < kDisplayY + kDisplayHeight;
}

int color_scheme_at(int x, int y)
{
    if (x < kSchemeBoxLeft || x >= kSchemeBoxLeft + 3 * kSchemeBoxWidth + 2 * kSchemeBoxGap) return -1;
    for (int index = 0; index < kColorSchemeCount; ++index) {
        const int column = index % 3;
        const int row = index / 3;
        const int box_x = kSchemeBoxLeft + column * (kSchemeBoxWidth + kSchemeBoxGap);
        const int box_y = kSchemeBoxTop + row * (kSchemeBoxHeight + kSchemeBoxGap);
        if (x >= box_x && x < box_x + kSchemeBoxWidth &&
            y >= box_y && y < box_y + kSchemeBoxHeight) return index;
    }
    return -1;
}

void handle_soft_key(int index)
{
    if (calc_error) clear_calculator();
    if (index == 5) {
        angle_degrees = !angle_degrees;
        return;
    }

    if (!modifier_active) {
        switch (index) {
            case 0: apply_unary(reciprocal); break;
            case 1: apply_unary(square_root); break;
            case 2: apply_unary(sine_degrees); break;
            case 3: apply_unary(cosine_degrees); break;
            case 4: apply_unary(tangent_degrees); break;
        }
    } else {
        switch (index) {
            case 1: apply_unary(square); break;
            case 0: power_y_to_x(); break;
            case 2: apply_unary(arcsine_degrees); break;
            case 3: apply_unary(arccosine_degrees); break;
            case 4: apply_unary(arctangent_degrees); break;
        }
    }
}

void commit_entry()
{
    if (entering) {
        push(entry.toDouble());
        entering = false;
    }
}

void handle_key(char value)
{
    if (calc_error) clear_calculator();

    if (value >= '0' && value <= '9') {
        if (!entering || entry == "0") entry = "";
        entry += value;
        entering = true;
        return;
    }

    if (value == '.') {
        if (!entering) { entry = "0"; entering = true; }
        if (entry.indexOf('.') < 0) entry += ".";
        return;
    }

    if (value == 'A' || value == '\b') {
        clear_calculator();
        return;
    }

    if (value == 'M') {
        // M acts as the second-function modifier for the touch soft keys.
        modifier_active = !modifier_active;
        return;
    }

    if (value == '%') {
        commit_entry();
        push(kPi);
        entering = false;
        return;
    }

    if (value == '`') {
        if (entering) {
            if (entry.startsWith("-")) entry.remove(0,1);
            else if (entry != "0") entry = "-" + entry;
        } else {
            stack[0] = -stack[0];
        }
        return;
    }

    if (value == '=' || value == '\n') {
        if (entering) {
            commit_entry();   // Commit a newly typed value.
        } else {
            push(stack[0]);    // ENTER duplicates the current X register.
        }
        return;
    }

    if (value == '+' || value == '-' || value == '*' || value == '/') {
        commit_entry();
        if (!do_op(value)) calc_error = true;
    }
}

} // namespace

void setup()
{
    Serial.begin(115200);
    auto config = M5.config();
    config.fallback_board = m5::board_t::board_M5StackCoreS3;
    M5.begin(config);

    if (!M5.In_I2C.isEnabled()) {
        int sda = M5.getPin(m5::pin_name_t::in_i2c_sda);
        int scl = M5.getPin(m5::pin_name_t::in_i2c_scl);
        M5.In_I2C.begin(I2C_NUM_1, sda, scl);
    }

    M5.Display.setRotation(1);
    M5.Display.setFont(&fonts::FreeMonoBold12pt7b);
    redraw_calculator();

    calculator.begin(&M5.In_I2C);
    redraw_calculator();
}

void loop()
{
    M5.update();

    if (!setup_screen && calculator.update()) {
        char value = calculator.getChar();
        if (value != '\0') {
            handle_key(value);
            redraw_calculator();
        }
    }

    if (M5.Touch.getCount()) {
        const auto& touch = M5.Touch.getDetail();
        if (touch.wasClicked()) {
            if (setup_screen) {
                const int scheme = color_scheme_at(touch.x, touch.y);
                if (scheme >= 0) {
                    selected_color_scheme = scheme;
                    setup_screen = false;
                    redraw_calculator();
                }
            } else if (display_at(touch.x, touch.y)) {
                setup_screen = true;
                redraw_setup();
            } else {
                const int index = soft_key_at(touch.x, touch.y);
                if (index >= 0) {
                    handle_soft_key(index);
                    redraw_calculator();
                }
            }
        }
    }
    delay(5);
}
