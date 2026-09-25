
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
constexpr int kDisplayHeight = 126;

constexpr int kPanelX = 14;
constexpr int kPanelY = 31;
constexpr int kPanelW = 292;
constexpr int kPanelH = 118;

constexpr int kValueRight = 304;
constexpr int kValueMax = kValueRight - 46;
constexpr int kRowTCenter = 47;
constexpr int kRowZCenter = 73;
constexpr int kRowYCenter = 99;
constexpr int kRowXCenter = 129;
constexpr int kSmallRowHeight = 22;
constexpr int kBigRowHeight = 30;
constexpr int kSchemeBoxWidth = 80;
constexpr int kSchemeBoxHeight = 64;
constexpr int kSchemeBoxGap = 12;
constexpr int kSchemeBoxLeft = 16;
constexpr int kSchemeBoxTop = 48;

constexpr uint16_t rgb565(uint8_t red, uint8_t green, uint8_t blue)
{
    return ((red & 0xf8) << 8) | ((green & 0xfc) << 3) | (blue >> 3);
}

constexpr uint16_t kFrameOuter = rgb565(180, 180, 180);
constexpr uint16_t kFrameInner = rgb565(220, 220, 220);

struct ColorScheme {
    const char* name;
    uint16_t background;
    uint16_t display;
    uint16_t foreground;
    uint16_t accent;
    uint16_t ui_text;
};

const ColorScheme color_schemes[] = {
    {"Green LCD", TFT_BLACK, rgb565(8, 26, 10), rgb565(0, 255, 90), rgb565(0, 255, 90), rgb565(0, 255, 90)},
    {"Purple LCD", TFT_BLACK, rgb565(26, 10, 38), rgb565(180, 80, 255), rgb565(180, 80, 255), rgb565(180, 80, 255)},
    {"Blue LCD", TFT_BLACK, rgb565(4, 12, 26), rgb565(70, 175, 255), rgb565(70, 175, 255), rgb565(70, 175, 255)},
    {"Red LCD", TFT_BLACK, rgb565(26, 6, 6), rgb565(255, 55, 55), rgb565(255, 55, 55), rgb565(255, 55, 55)},
    {"Yellow LCD", TFT_BLACK, rgb565(24, 24, 4), rgb565(255, 255, 0), rgb565(255, 255, 0), rgb565(255, 255, 0)},
    {"White LCD", TFT_BLACK, rgb565(18, 20, 24), TFT_WHITE, TFT_WHITE, TFT_WHITE},
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

    result.replace("e+", "E");
    result.replace("e-", "E-");
    result.replace('e', 'E');
    result.replace("+", "");
    return result;
}

constexpr uint8_t kSegA = 1 << 0;
constexpr uint8_t kSegB = 1 << 1;
constexpr uint8_t kSegC = 1 << 2;
constexpr uint8_t kSegD = 1 << 3;
constexpr uint8_t kSegE = 1 << 4;
constexpr uint8_t kSegF = 1 << 5;
constexpr uint8_t kSegG = 1 << 6;

constexpr uint8_t kSevenMasks[10] = {
    kSegA|kSegB|kSegC|kSegD|kSegE|kSegF,        // 0
    kSegB|kSegC,                                // 1
    kSegA|kSegB|kSegD|kSegE|kSegG,              // 2
    kSegA|kSegB|kSegC|kSegD|kSegG,              // 3
    kSegB|kSegC|kSegF|kSegG,                    // 4
    kSegA|kSegC|kSegD|kSegF|kSegG,              // 5
    kSegA|kSegC|kSegD|kSegE|kSegF|kSegG,        // 6
    kSegA|kSegB|kSegC,                          // 7
    kSegA|kSegB|kSegC|kSegD|kSegE|kSegF|kSegG,  // 8
    kSegA|kSegB|kSegC|kSegD|kSegF|kSegG,        // 9
};

uint16_t dimmed(uint16_t color, unsigned int shift)
{
    const uint16_t red = (color >> 11) & 0x1F;
    const uint16_t green = (color >> 5) & 0x3F;
    const uint16_t blue = color & 0x1F;
    const unsigned int s = shift > 5 ? 5 : shift;
    return (uint16_t)(((red >> s) << 11) | ((green >> s) << 5) | (blue >> s));
}

uint16_t with_brightness(uint16_t color, uint16_t percent)
{
    const uint16_t red = ((color >> 11) & 0x1F) * percent;
    const uint16_t green = ((color >> 5) & 0x3F) * percent;
    const uint16_t blue = (color & 0x1F) * percent;
    return (uint16_t)((((red + 127) / 255) << 11) |
                      (((green + 127) / 255) << 5) |
                      ((blue + 127) / 255));
}

// Draws a single seven-segment glyph into a cell of width/height at (x0, y0).
// Lit segments use on_color; unlit ones use off_color so the ghost segments
// show through the LCD glass, as on a real display.
void draw_seven_char(int x0, int y0, int w, int h, int t, char c,
                     uint16_t on_color, uint16_t off_color)
{
    uint8_t mask = 0;
    bool separator = false;
    if (c >= '0' && c <= '9') {
        mask = kSevenMasks[c - '0'];
    } else {
        switch (c) {
            case '.': separator = true; break;
            case '-': mask = kSegG; break;
            case 'E': mask = kSegA|kSegD|kSegE|kSegF|kSegG; break;
            case 'r': mask = kSegE|kSegG; break;
            case 'O': mask = kSegA|kSegB|kSegC|kSegD|kSegE|kSegF; break;
            case 'H': mask = kSegB|kSegC|kSegE|kSegF|kSegG; break;
            case 'L': mask = kSegD|kSegE|kSegF; break;
            case 'P': mask = kSegA|kSegB|kSegE|kSegF|kSegG; break;
            case 't': mask = kSegD|kSegE|kSegG; break;
            default: mask = 0; break;
        }
    }

    const int vlen = max(2, (h - 3 * t) / 2);
    const int lower_y = y0 + h - t - vlen;
    const int right_x = x0 + w - 2 * t;

    if (separator) {
        M5.Display.fillRect(x0, y0 + h - t, w, t, on_color);
        return;
    }

    if (mask) {
        if (mask & kSegA) M5.Display.fillRect(x0 + t, y0, w - 3 * t, t, on_color);
        else M5.Display.fillRect(x0 + t, y0, w - 3 * t, t, off_color);

        if (mask & kSegG) M5.Display.fillRect(x0 + t, y0 + h / 2 - t / 2, w - 3 * t, t, on_color);
        else M5.Display.fillRect(x0 + t, y0 + h / 2 - t / 2, w - 3 * t, t, off_color);

        if (mask & kSegD) M5.Display.fillRect(x0 + t, y0 + h - t, w - 3 * t, t, on_color);
        else M5.Display.fillRect(x0 + t, y0 + h - t, w - 3 * t, t, off_color);

        if (mask & kSegF) M5.Display.fillRect(x0, y0 + t, t, vlen, on_color);
        else M5.Display.fillRect(x0, y0 + t, t, vlen, off_color);

        if (mask & kSegE) M5.Display.fillRect(x0, lower_y, t, vlen, on_color);
        else M5.Display.fillRect(x0, lower_y, t, vlen, off_color);

        if (mask & kSegB) M5.Display.fillRect(right_x, y0 + t, t, vlen, on_color);
        else M5.Display.fillRect(right_x, y0 + t, t, vlen, off_color);

        if (mask & kSegC) M5.Display.fillRect(right_x, lower_y, t, vlen, on_color);
        else M5.Display.fillRect(right_x, lower_y, t, vlen, off_color);
    }

    M5.Display.fillRect(right_x, y0 + h - t, t, t, off_color);
}

int seven_width(int height)
{
    const int t = max(2, height / 9);
    const int vlen = max(2, (height - 3 * t) / 2);
    return vlen + 2 * t + 1;
}

// Total width of a value at a given glyph height; '.' separators get a narrow
// cell so the dot nests tightly between digit cells.
int seven_string_width(const String& text, int height)
{
    constexpr int kGap = 3;
    const int t = max(2, height / 9);
    const int w = seven_width(height);
    int total = 0;
    for (int i = 0; i < text.length(); ++i) {
        total += (text[i] == '.' ? t : w) + kGap;
    }
    return total - kGap;
}

// Right-aligns a seven-segment value to right_x, growing the glyph size up to
// base_height while shrinking it to fit an available width of max_w.
void draw_seven_string(const String& text, int right_x, int center_y, int max_w,
                       int base_height, uint16_t on_color, uint16_t off_color)
{
    const int count = text.length();
    if (count == 0) return;
    constexpr int kGap = 3;

    int height = base_height;
    for (;;) {
        if (seven_string_width(text, height) <= max_w || height <= 8) break;
        height -= 2;
    }
    const int w = seven_width(height);
    const int t = max(2, height / 9);

    int x = right_x - seven_string_width(text, height);
    if (x < kPanelX + 28) x = kPanelX + 28;

    const int y0 = center_y - height / 2;
    for (int i = 0; i < count; ++i) {
        const bool dot = text[i] == '.';
        const int cw = dot ? t : w;
        draw_seven_char(x, y0, cw, height, t, text[i], on_color, off_color);
        x += cw + kGap;
    }
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
    const uint16_t soft_key_background = with_brightness(colors().accent, 50);
    M5.Display.fillRoundRect(x, kSoftKeyY, kSoftKeyWidth, kSoftKeyHeight, 6, soft_key_background);
    M5.Display.setFont(&fonts::FreeMonoBold9pt7b);
    M5.Display.setTextDatum(middle_center);
    M5.Display.setTextColor(colors().background, soft_key_background);
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
    M5.Display.drawString("RPN", 8, 15);

    M5.Display.setTextDatum(middle_right);
    M5.Display.setTextColor(colors().ui_text, colors().background);
    M5.Display.drawString(modifier_active ? "2nd" : "", 312, 15);

    // LCD bezel and glass panel.
    M5.Display.fillRoundRect(kDisplayX, kDisplayY, kDisplayWidth, kDisplayHeight, 6, kFrameOuter);
    M5.Display.fillRoundRect(kDisplayX + 3, kDisplayY + 3, kDisplayWidth - 6, kDisplayHeight - 6, 4, kFrameInner);
    M5.Display.fillRoundRect(kPanelX, kPanelY, kPanelW, kPanelH, 3, colors().display);
    M5.Display.fillRect(kPanelX, kPanelY, kPanelW, 2, dimmed(colors().display, 1));
    M5.Display.fillRect(kPanelX, kPanelY + kPanelH - 2, kPanelW, 2, dimmed(colors().display, 1));

    const uint16_t ghost = dimmed(colors().foreground, 2);
    const uint16_t grid = dimmed(colors().foreground, 5);

    // Printed-glass register labels.
    M5.Display.setFont(&fonts::FreeMonoBold9pt7b);
    M5.Display.setTextDatum(middle_left);
    M5.Display.setTextColor(ghost, colors().display);
    M5.Display.drawString("T", 18, kRowTCenter);
    M5.Display.drawString("Z", 18, kRowZCenter);
    M5.Display.drawString("Y", 18, kRowYCenter);
    M5.Display.drawString("X", 18, kRowXCenter);

    // Subtle register separators on the glass.
    M5.Display.drawFastHLine(kPanelX + 4, kRowZCenter - kSmallRowHeight / 2 - 1, kPanelW - 8, grid);
    M5.Display.drawFastHLine(kPanelX + 4, kRowZCenter + kSmallRowHeight / 2 + 1, kPanelW - 8, grid);
    M5.Display.drawFastHLine(kPanelX + 4, kRowYCenter + kSmallRowHeight / 2 + 1, kPanelW - 8, grid);

    if (stack_depth > 3) {
        draw_seven_string(format_number(stack[3]), kValueRight, kRowTCenter, kValueMax,
                          kSmallRowHeight, colors().foreground, ghost);
    }
    if (stack_depth > 2) {
        draw_seven_string(format_number(stack[2]), kValueRight, kRowZCenter, kValueMax,
                          kSmallRowHeight, colors().foreground, ghost);
    }
    if (stack_depth > 1) {
        draw_seven_string(format_number(stack[1]), kValueRight, kRowYCenter, kValueMax,
                          kSmallRowHeight, colors().foreground, ghost);
    }

    String x_display = calc_error ? "ERROR" : (entering ? entry : format_number(stack[0]));
    draw_seven_string(x_display, kValueRight, kRowXCenter, kValueMax,
                      kBigRowHeight, colors().foreground, ghost);

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

    if (M5.BtnA.wasClicked()) {
        selected_color_scheme = (selected_color_scheme + 1) % kColorSchemeCount;
        if (setup_screen) redraw_setup();
        else redraw_calculator();
    } else if (M5.BtnC.wasClicked()) {
        selected_color_scheme = (selected_color_scheme + kColorSchemeCount - 1) % kColorSchemeCount;
        if (setup_screen) redraw_setup();
        else redraw_calculator();
    }

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
