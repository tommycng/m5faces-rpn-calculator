
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
// Below this magnitude a value is shown in scientific notation.
constexpr double kSmallValueThreshold = 0.0001;
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
constexpr int kScreenWidth = 320;
constexpr int kHeaderTop = 2;
constexpr int kHeaderHeight = 24;
constexpr int kHeaderCenter = 15;

// Registers are painted one band at a time so that a value change only
// repaints the glass it sits on instead of rebuilding the whole screen.
struct RegisterRow {
    const char* label;
    int center_y;
    int height;
    int index;   // stack slot; index 0 is X and is always shown
};

const RegisterRow kRegisterRows[] = {
    {"T", kRowTCenter, kSmallRowHeight, 3},
    {"Z", kRowZCenter, kSmallRowHeight, 2},
    {"Y", kRowYCenter, kSmallRowHeight, 1},
    {"X", kRowXCenter, kBigRowHeight, 0},
};
constexpr int kRegisterRowCount = sizeof(kRegisterRows) / sizeof(kRegisterRows[0]);

constexpr uint16_t rgb565(uint8_t red, uint8_t green, uint8_t blue)
{
    return ((red & 0xf8) << 8) | ((green & 0xfc) << 3) | (blue >> 3);
}

constexpr uint16_t kFrameOuter = rgb565(56, 56, 58);
constexpr uint16_t kFrameInner = rgb565(90, 90, 92);
constexpr uint16_t kSoftKeyFace = rgb565(202, 202, 204);
constexpr uint16_t kSoftKeyText = rgb565(20, 20, 22);

struct ColorScheme {
    const char* name;
    bool dot_matrix;
    uint16_t background;
    uint16_t display;
    uint16_t foreground;
    uint16_t accent;
    uint16_t ui_text;
};

const ColorScheme color_schemes[] = {
    {"EL Green",   false, rgb565(4, 10, 8),    rgb565(8, 30, 20),     rgb565(96, 255, 176), rgb565(64, 224, 152),  rgb565(200, 240, 216)},
    {"EL Dot",     true,  rgb565(4, 10, 8),    rgb565(8, 30, 20),     rgb565(96, 255, 176), rgb565(64, 224, 152),  rgb565(200, 240, 216)},
    {"Red LED",    false, rgb565(12, 4, 4),    rgb565(30, 7, 6),      rgb565(255, 56, 40),  rgb565(255, 72, 56),   rgb565(255, 200, 192)},
    {"Purple LED", false, rgb565(9, 5, 15),    rgb565(24, 11, 36),    rgb565(186, 92, 255), rgb565(170, 100, 255), rgb565(226, 206, 255)},
    {"Blue LED",   false, rgb565(3, 7, 15),    rgb565(6, 15, 32),     rgb565(64, 160, 255), rgb565(72, 168, 255),  rgb565(198, 226, 255)},
    {"LCD",        false, rgb565(8, 9, 8),     rgb565(163, 189, 122), rgb565(24, 30, 20),   rgb565(120, 200, 96),  rgb565(214, 226, 190)},
};
constexpr int kColorSchemeCount = sizeof(color_schemes) / sizeof(color_schemes[0]);
int selected_color_scheme = 0;
bool setup_screen = false;

// Screen regions to repaint on the next incremental redraw. A full redraw
// clears them all.
bool values_dirty = true;
bool header_dirty = true;
bool soft_keys_dirty = true;

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

    // Anything this small would round away to zero in fixed notation, so show
    // it as a mantissa with four decimal places instead.
    if (value != 0.0 && fabs(value) < kSmallValueThreshold) {
        char buffer[16];
        snprintf(buffer, sizeof(buffer), "%.4E", value);
        return String(buffer);
    }

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

// Blends two RGB565 colors; percent is the weight of b in the result.
uint16_t mix_color(uint16_t a, uint16_t b, uint16_t percent)
{
    const uint16_t inverse = 255 - percent;
    const uint16_t red = (((a >> 11) & 0x1F) * inverse + ((b >> 11) & 0x1F) * percent + 127) / 255;
    const uint16_t green = (((a >> 5) & 0x3F) * inverse + ((b >> 5) & 0x3F) * percent + 127) / 255;
    const uint16_t blue = ((a & 0x1F) * inverse + (b & 0x1F) * percent + 127) / 255;
    return (uint16_t)((red << 11) | (green << 5) | blue);
}

// Unlit segment and dot color: a hint of the foreground seen through the glass.
uint16_t ghost_color()
{
    return mix_color(colors().display, colors().foreground, 18);
}

uint16_t grid_color()
{
    return mix_color(colors().display, colors().foreground, 10);
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
            // A seven-segment cell cannot spell a capital R, so it borrows the
            // conventional lowercase-r form that real LCDs use for "ErrOr".
            case 'R': mask = kSegE|kSegG; break;
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

// Classic 5x7 dot matrix font: five column bytes per glyph, bit 0 is the top
// dot. The leading space doubles as the fallback for unsupported characters.
// Lowercase glyphs sit on the bottom five rows, with ascenders reaching row 0.
const char kDotFontChars[] = " 0123456789.:+-EROnd";
const uint8_t kDotFont[][5] = {
    {0x00, 0x00, 0x00, 0x00, 0x00},  // ' '
    {0x3E, 0x51, 0x49, 0x45, 0x3E},  // '0'
    {0x00, 0x42, 0x7F, 0x40, 0x00},  // '1'
    {0x42, 0x61, 0x51, 0x49, 0x46},  // '2'
    {0x21, 0x41, 0x45, 0x4B, 0x31},  // '3'
    {0x18, 0x14, 0x12, 0x7F, 0x10},  // '4'
    {0x27, 0x45, 0x45, 0x45, 0x39},  // '5'
    {0x3C, 0x4A, 0x49, 0x49, 0x30},  // '6'
    {0x01, 0x71, 0x09, 0x05, 0x03},  // '7'
    {0x36, 0x49, 0x49, 0x49, 0x36},  // '8'
    {0x06, 0x49, 0x49, 0x29, 0x1E},  // '9'
    {0x60, 0x60, 0x00, 0x00, 0x00},  // '.'
    {0x00, 0x36, 0x36, 0x00, 0x00},  // ':'
    {0x08, 0x08, 0x08, 0x08, 0x08},  // '-'
    {0x08, 0x08, 0x3E, 0x08, 0x08},  // '+'
    {0x7F, 0x49, 0x49, 0x49, 0x41},  // 'E'
    {0x7F, 0x09, 0x19, 0x29, 0x46},  // 'R'
    {0x3E, 0x41, 0x41, 0x41, 0x3E},  // 'O'
    {0x78, 0x04, 0x04, 0x04, 0x7C},  // 'n'
    {0x78, 0x44, 0x44, 0x04, 0x7F},  // 'd'
};

const uint8_t* dot_glyph(char c)
{
    if (c == '\0') return kDotFont[0];
    const char* found = strchr(kDotFontChars, c);
    return kDotFont[found == nullptr ? 0 : found - kDotFontChars];
}

// One character cell: five dot columns plus a one pixel gap.
int dot_cell(int scale)
{
    return 5 * scale + 1;
}

int dot_string_width(int count, int scale)
{
    return count * dot_cell(scale) - 1;
}

// Right-aligns a dot matrix value to right_x, shrinking the dot scale from
// base_scale down to 1 until the text fits an available width of max_w. Each
// dot is drawn one pixel smaller than its slot so the gaps between dots keep
// the pixel grid visible.
void draw_dot_string(const String& text, int right_x, int center_y, int max_w,
                     int base_scale, uint16_t on_color, uint16_t off_color)
{
    const int count = text.length();
    if (count == 0) return;

    int scale = base_scale;
    while (scale > 1 && dot_string_width(count, scale) > max_w) --scale;

    int x = right_x - dot_string_width(count, scale);
    if (x < kPanelX + 8) x = kPanelX + 8;
    const int y0 = center_y - 7 * scale / 2;
    const int dot = max(1, scale - 1);

    for (int i = 0; i < count; ++i) {
        const uint8_t* glyph = dot_glyph(text[i]);
        for (int col = 0; col < 5; ++col) {
            for (int row = 0; row < 7; ++row) {
                const uint16_t color = glyph[col] & (1 << row) ? on_color : off_color;
                M5.Display.fillRect(x + col * scale, y0 + row * scale, dot, dot, color);
            }
        }
        x += dot_cell(scale);
    }
}

// Draws a register value with the renderer the active scheme asks for: the
// seven-segment face, or a 5x7 dot matrix sized from the same row height.
void draw_value(const String& text, int center_y, int height,
                uint16_t on_color, uint16_t off_color)
{
    if (colors().dot_matrix) {
        draw_dot_string(text, kValueRight, center_y, kValueMax, height / 7, on_color, off_color);
    } else {
        draw_seven_string(text, kValueRight, center_y, kValueMax, height, on_color, off_color);
    }
}

// Draws a short label straight from the 5x7 bitmap font inside a rounded
// frame, so status text reads as a badge instead of as a title. The glyphs are
// solid pixels rather than a dot grid, which keeps the small text legible.
void draw_bitmap_badge(const char* text, int right_x, int center_y, int scale,
                       int pad_x, int pad_y, int radius,
                       uint16_t frame_color, uint16_t fill_color, uint16_t text_color)
{
    const int count = (int)strlen(text);
    if (count == 0) return;

    const int text_w = dot_string_width(count, scale);
    const int text_h = 7 * scale;
    const int width = text_w + 2 * pad_x;
    const int height = text_h + 2 * pad_y;
    const int x0 = right_x - width;
    const int y0 = center_y - height / 2;

    // The fill always covers the shape; the outline only shows when the two
    // colors differ, which keeps a solid badge free of a doubled edge.
    M5.Display.fillRoundRect(x0, y0, width, height, radius, fill_color);
    if (frame_color != fill_color) {
        M5.Display.drawRoundRect(x0, y0, width, height, radius, frame_color);
    }

    int x = x0 + pad_x;
    const int y = center_y - text_h / 2;
    for (int i = 0; i < count; ++i) {
        const uint8_t* glyph = dot_glyph(text[i]);
        for (int col = 0; col < 5; ++col) {
            for (int row = 0; row < 7; ++row) {
                if (glyph[col] & (1 << row)) {
                    M5.Display.fillRect(x + col * scale, y + row * scale, scale, scale, text_color);
                }
            }
        }
        x += dot_cell(scale);
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
    values_dirty = true;
    if (modifier_active) {
        modifier_active = false;
        header_dirty = true;
        soft_keys_dirty = true;
    }
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
    M5.Display.fillRoundRect(x, kSoftKeyY, kSoftKeyWidth, kSoftKeyHeight, 6, kSoftKeyFace);
    M5.Display.setFont(&fonts::FreeMonoBold9pt7b);
    M5.Display.setTextDatum(middle_center);
    M5.Display.setTextColor(kSoftKeyText, kSoftKeyFace);
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

void draw_soft_keys()
{
    for (int index = 0; index < 6; ++index) {
        draw_soft_key(index);
    }
}

// The title strip is cleared first so the "2nd" badge can be removed again.
void draw_header()
{
    M5.Display.fillRect(0, kHeaderTop, kScreenWidth, kHeaderHeight, colors().background);
    M5.Display.setFont(&fonts::FreeMonoBold12pt7b);
    M5.Display.setTextColor(colors().ui_text, colors().background);
    M5.Display.setTextDatum(middle_left);
    M5.Display.drawString("RPN", 8, kHeaderCenter);
    if (modifier_active) {
        draw_bitmap_badge("2nd", kScreenWidth - 8, kHeaderCenter, 2, 4, 3, 6,
                          colors().ui_text, colors().ui_text, colors().background);
    }
}

void draw_glass()
{
    // Dark grey bezel around the tinted glass panel.
    M5.Display.fillRoundRect(kDisplayX, kDisplayY, kDisplayWidth, kDisplayHeight, 6, kFrameOuter);
    M5.Display.fillRoundRect(kDisplayX + 3, kDisplayY + 3, kDisplayWidth - 6, kDisplayHeight - 6, 4, kFrameInner);
    M5.Display.fillRoundRect(kPanelX, kPanelY, kPanelW, kPanelH, 3, colors().display);
    const uint16_t glass_edge = mix_color(colors().display, colors().background, 35);
    M5.Display.fillRect(kPanelX, kPanelY, kPanelW, 2, glass_edge);
    M5.Display.fillRect(kPanelX, kPanelY + kPanelH - 2, kPanelW, 2, glass_edge);

    // Subtle register separators printed on the glass.
    const uint16_t grid = grid_color();
    M5.Display.drawFastHLine(kPanelX + 4, kRowZCenter - kSmallRowHeight / 2 - 1, kPanelW - 8, grid);
    M5.Display.drawFastHLine(kPanelX + 4, kRowZCenter + kSmallRowHeight / 2 + 1, kPanelW - 8, grid);
    M5.Display.drawFastHLine(kPanelX + 4, kRowYCenter + kSmallRowHeight / 2 + 1, kPanelW - 8, grid);
}

String register_text(const RegisterRow& row)
{
    if (row.index != 0) return format_number(stack[row.index]);
    if (calc_error) return "ERROR";
    if (entering) return entry;
    return format_number(stack[0]);
}

// Repaints one register: its glass band, the printed label, and the value.
void draw_register(const RegisterRow& row)
{
    M5.Display.fillRect(kPanelX, row.center_y - row.height / 2, kPanelW, row.height,
                        colors().display);

    const uint16_t ghost = ghost_color();
    M5.Display.setFont(&fonts::FreeMonoBold9pt7b);
    M5.Display.setTextDatum(middle_left);
    M5.Display.setTextColor(ghost, colors().display);
    M5.Display.drawString(row.label, 18, row.center_y);

    if (row.index != 0 && row.index >= stack_depth) return;
    draw_value(register_text(row), row.center_y, row.height, colors().foreground, ghost);
}

void redraw_calculator()
{
    M5.Display.startWrite();
    M5.Display.fillScreen(colors().background);
    draw_header();
    draw_glass();
    for (int row = 0; row < kRegisterRowCount; ++row) {
        draw_register(kRegisterRows[row]);
    }
    draw_soft_keys();
    M5.Display.endWrite();

    header_dirty = false;
    values_dirty = false;
    soft_keys_dirty = false;
}

// Repaints only the regions whose state changed, so a keystroke no longer
// blanks and rebuilds the whole screen.
void redraw_dirty()
{
    if (!header_dirty && !values_dirty && !soft_keys_dirty) return;

    M5.Display.startWrite();
    if (header_dirty) {
        draw_header();
        header_dirty = false;
    }
    if (values_dirty) {
        for (int row = 0; row < kRegisterRowCount; ++row) {
            draw_register(kRegisterRows[row]);
        }
        values_dirty = false;
    }
    if (soft_keys_dirty) {
        draw_soft_keys();
        soft_keys_dirty = false;
    }
    M5.Display.endWrite();
}

void redraw_setup()
{
    M5.Display.startWrite();
    M5.Display.fillScreen(colors().background);
    for (int index = 0; index < kColorSchemeCount; ++index) {
        const int column = index % 3;
        const int row = index / 3;
        const int x = kSchemeBoxLeft + column * (kSchemeBoxWidth + kSchemeBoxGap);
        const int y = kSchemeBoxTop + row * (kSchemeBoxHeight + kSchemeBoxGap);
        const bool selected = index == selected_color_scheme;
        M5.Display.fillRoundRect(x, y, kSchemeBoxWidth, kSchemeBoxHeight, 8,
                                color_schemes[index].display);
        M5.Display.setFont(&fonts::FreeMonoBold12pt7b);
        M5.Display.setTextDatum(middle_center);
        M5.Display.setTextColor(color_schemes[index].foreground, color_schemes[index].display);
        M5.Display.drawString(String(index + 1), x + kSchemeBoxWidth / 2,
                              y + kSchemeBoxHeight / 2);
        if (selected) {
            M5.Display.drawRoundRect(x - 3, y - 3, kSchemeBoxWidth + 6,
                                     kSchemeBoxHeight + 6, 10, color_schemes[index].accent);
        }
    }
    M5.Display.endWrite();
}

void redraw_current_screen()
{
    if (setup_screen) redraw_setup();
    else redraw_calculator();
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
        soft_keys_dirty = true;
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
    values_dirty = true;
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
        values_dirty = true;
        return;
    }

    if (value == '.') {
        if (!entering) { entry = "0"; entering = true; }
        if (entry.indexOf('.') < 0) entry += ".";
        values_dirty = true;
        return;
    }

    if (value == 'A' || value == '\b') {
        clear_calculator();
        return;
    }

    if (value == 'M') {
        // M acts as the second-function modifier for the touch soft keys.
        modifier_active = !modifier_active;
        header_dirty = true;
        soft_keys_dirty = true;
        return;
    }

    if (value == '%') {
        commit_entry();
        push(kPi);
        entering = false;
        values_dirty = true;
        return;
    }

    if (value == '`') {
        if (entering) {
            if (entry.startsWith("-")) entry.remove(0,1);
            else if (entry != "0") entry = "-" + entry;
        } else {
            stack[0] = -stack[0];
        }
        values_dirty = true;
        return;
    }

    if (value == '=' || value == '\n') {
        if (entering) {
            commit_entry();   // Commit a newly typed value.
        } else {
            push(stack[0]);    // ENTER duplicates the current X register.
        }
        values_dirty = true;
        return;
    }

    if (value == '+' || value == '-' || value == '*' || value == '/') {
        commit_entry();
        if (!do_op(value)) calc_error = true;
        values_dirty = true;
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
        redraw_current_screen();
    } else if (M5.BtnC.wasClicked()) {
        selected_color_scheme = (selected_color_scheme + kColorSchemeCount - 1) % kColorSchemeCount;
        redraw_current_screen();
    } else if (M5.BtnB.wasClicked()) {
        // Button B opens and closes the color scheme screen.
        setup_screen = !setup_screen;
        redraw_current_screen();
    }

    if (!setup_screen && calculator.update()) {
        char value = calculator.getChar();
        if (value != '\0') {
            handle_key(value);
            redraw_dirty();
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
            } else {
                const int index = soft_key_at(touch.x, touch.y);
                if (index >= 0) {
                    handle_soft_key(index);
                    redraw_dirty();
                }
            }
        }
    }
    delay(5);
}
