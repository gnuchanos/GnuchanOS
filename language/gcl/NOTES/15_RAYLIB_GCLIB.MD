# raylib.gclib — TYPES AND CONSTANTS
# One-to-one gcLang equivalent of raylib.h enums, structs, typedefs
# Used with #extern <raylib.web.a> / <raylib.dll>

═══════════════════════════════════════════════════════════════════════
RULE: Everything in raylib.gclib is the exact same as in raylib.h.
═══════════════════════════════════════════════════════════════════════

  In gcLang, struct, enum, typedef are no different from C.
  raylib.gclib only takes #extern declarations and
  constants from raylib.h. C_CODEGEN converts it to C header,
  no conflict occurs.

═══════════════════════════════════════════════════════════════════════
1. #extern — LIBRARY LINKING
═══════════════════════════════════════════════════════════════════════

  #extern <raylib.web.a>    # wasm build
  # or: #extern <raylib.dll>   # windows run
  # or: #extern <raylib.so>    # linux run

═══════════════════════════════════════════════════════════════════════
2. COLOR CONSTANTS (one-to-one from raylib.h)
═══════════════════════════════════════════════════════════════════════

  In raylib.h:
      #define LIGHTGRAY  (Color){ 200, 200, 200, 255 }

  In raylib.gclib (exact same):
      #define LIGHTGRAY  (Color){ 200, 200, 200, 255 }
      #define GRAY       (Color){ 130, 130, 130, 255 }
      #define DARKGRAY   (Color){ 80, 80, 80, 255 }
      #define YELLOW     (Color){ 253, 249, 0, 255 }
      #define GOLD       (Color){ 255, 203, 0, 255 }
      #define ORANGE     (Color){ 255, 161, 0, 255 }
      #define PINK       (Color){ 255, 109, 194, 255 }
      #define RED        (Color){ 230, 41, 55, 255 }
      #define MAROON     (Color){ 190, 33, 55, 255 }
      #define GREEN      (Color){ 0, 228, 48, 255 }
      #define LIME       (Color){ 0, 158, 47, 255 }
      #define DARKGREEN  (Color){ 0, 117, 44, 255 }
      #define SKYBLUE    (Color){ 102, 191, 255, 255 }
      #define BLUE       (Color){ 0, 121, 241, 255 }
      #define DARKBLUE   (Color){ 0, 82, 172, 255 }
      #define PURPLE     (Color){ 200, 122, 255, 255 }
      #define VIOLET     (Color){ 135, 60, 190, 255 }
      #define DARKPURPLE (Color){ 112, 31, 126, 255 }
      #define BEIGE      (Color){ 211, 176, 131, 255 }
      #define BROWN      (Color){ 127, 106, 79, 255 }
      #define DARKBROWN  (Color){ 76, 63, 47, 255 }

      #define WHITE      (Color){ 255, 255, 255, 255 }
      #define BLACK      (Color){ 0, 0, 0, 255 }
      #define BLANK      (Color){ 0, 0, 0, 0 }
      #define MAGENTA    (Color){ 255, 0, 255, 255 }
      #define RAYWHITE   (Color){ 245, 245, 245, 255 }

═══════════════════════════════════════════════════════════════════════
3. BASIC TYPES (one-to-one typedef from raylib.h)
═══════════════════════════════════════════════════════════════════════

  In raylib.h:
      typedef unsigned char uchar;

  In raylib.gclib (exact same):
      typedef unsigned char uchar;
      typedef unsigned int uint;

═══════════════════════════════════════════════════════════════════════
4. struct (one-to-one from raylib.h)
═══════════════════════════════════════════════════════════════════════

  In raylib.h:
      typedef struct Color {
          unsigned char r, g, b, a;
      } Color;

  In raylib.gclib (exact same):
      typedef struct Color {
          unsigned char r;
          unsigned char g;
          unsigned char b;
          unsigned char a;
      } Color;

  In raylib.h:
      typedef struct Vector2 { float x, y; } Vector2;

  In raylib.gclib:
      typedef struct Vector2 {
          float x;
          float y;
      } Vector2;

  In raylib.h:
      typedef struct Vector3 { float x, y, z; } Vector3;

  In raylib.gclib:
      typedef struct Vector3 {
          float x;
          float y;
          float z;
      } Vector3;

  In raylib.h:
      typedef struct Vector4 { float x, y, z, w; } Vector4;

  In raylib.gclib:
      typedef struct Vector4 {
          float x;
          float y;
          float z;
          float w;
      } Vector4;
      # Vector4 = Quaternion (same type in raylib)

  In raylib.h:
      typedef struct Matrix {
          float m0, m4, m8, m12;
          float m1, m5, m9, m13;
          float m2, m6, m10, m14;
          float m3, m7, m11, m15;
      } Matrix;

  In raylib.gclib:
      typedef struct Matrix {
          float m0, m4, m8, m12;
          float m1, m5, m9, m13;
          float m2, m6, m10, m14;
          float m3, m7, m11, m15;
      } Matrix;

  In raylib.h:
      typedef struct Rectangle { float x, y, width, height; } Rectangle;

  In raylib.gclib:
      typedef struct Rectangle {
          float x;
          float y;
          float width;
          float height;
      } Rectangle;

  In raylib.h:
      typedef struct Image {
          void* data;
          int width;
          int height;
          int mipmaps;
          int format;
      } Image;

  In raylib.gclib:
      typedef struct Image {
          void* data;
          int width;
          int height;
          int mipmaps;
          int format;
      } Image;

  In raylib.h:
      typedef struct Texture {
          unsigned int id;
          int width;
          int height;
          int mipmaps;
          int format;
      } Texture;
      typedef Texture Texture2D;

  In raylib.gclib:
      typedef struct Texture {
          unsigned int id;
          int width;
          int height;
          int mipmaps;
          int format;
      } Texture;
      typedef Texture Texture2D;

  In raylib.h:
      typedef struct RenderTexture {
          unsigned int id;
          Texture texture;
          Texture depth;
      } RenderTexture;
      typedef RenderTexture RenderTexture2D;

  In raylib.gclib:
      typedef struct RenderTexture {
          unsigned int id;
          Texture texture;
          Texture depth;
      } RenderTexture;
      typedef RenderTexture RenderTexture2D;

═══════════════════════════════════════════════════════════════════════
5. enum — ORDERED CONSTANTS (one-to-one from raylib.h)
═══════════════════════════════════════════════════════════════════════

  In raylib.h:
      enum { KEY_SPACE = 32, KEY_ENTER = 257, KEY_ESCAPE = 256,
             KEY_RIGHT = 262, KEY_LEFT = 263, KEY_DOWN = 264, KEY_UP = 265 };

  In raylib.gclib (exact same):
      enum {
          KEY_SPACE = 32,
          KEY_ENTER = 257,
          KEY_ESCAPE = 256,
          KEY_RIGHT = 262,
          KEY_LEFT = 263,
          KEY_DOWN = 264,
          KEY_UP = 265,
          KEY_W = 87,
          KEY_A = 65,
          KEY_S = 83,
          KEY_D = 68,
          KEY_F = 70,
          KEY_LEFT_SHIFT = 340,
          KEY_LEFT_CONTROL = 341
          # ... continue as needed
      };

  In raylib.h:
      typedef enum {
          MOUSE_BUTTON_LEFT = 0,
          MOUSE_BUTTON_RIGHT = 1,
          MOUSE_BUTTON_MIDDLE = 2
      } MouseButton;

  In raylib.gclib:
      typedef enum {
          MOUSE_BUTTON_LEFT = 0,
          MOUSE_BUTTON_RIGHT = 1,
          MOUSE_BUTTON_MIDDLE = 2
      } MouseButton;

  In raylib.h:
      typedef enum {
          FLAG_VSYNC_HINT = 64,
          FLAG_FULLSCREEN_MODE = 2,
          FLAG_WINDOW_RESIZABLE = 4,
          FLAG_MSAA_4X_HINT = 32
      } ConfigFlags;

  In raylib.gclib:
      typedef enum {
          FLAG_VSYNC_HINT = 64,
          FLAG_FULLSCREEN_MODE = 2,
          FLAG_WINDOW_RESIZABLE = 4,
          FLAG_MSAA_4X_HINT = 32
      } ConfigFlags;

═══════════════════════════════════════════════════════════════════════
6. FUNCTION PROTOTYPES (inside extern "c")
═══════════════════════════════════════════════════════════════════════

  #extern <raylib.web.a>
      # Window
      extern void InitWindow(int width, int height, const char* title);
      extern void CloseWindow(void);
      extern bool WindowShouldClose(void);
      extern void SetConfigFlags(uint flags);

      # Drawing
      extern void BeginDrawing(void);
      extern void EndDrawing(void);
      extern void ClearBackground(Color color);

      extern void DrawRectangle(int x, int y, int w, int h, Color color);
      extern void DrawText(const char* text, int x, int y, int size, Color color);

      # Input
      extern bool IsKeyDown(int key);
      extern bool IsKeyPressed(int key);
      extern bool IsMouseButtonDown(int button);

      # Time
      extern double GetTime(void);
      extern void SetTargetFPS(int fps);
      extern int GetFPS(void);

      # ... as many functions from raylib.h as needed


═══════════════════════════════════════════════════════════════════════
7. EXAMPLE USAGE
═══════════════════════════════════════════════════════════════════════

  #lib <raylib>

  int main() {
      SetConfigFlags(FLAG_VSYNC_HINT);
      InitWindow(800, 600, "gcLang + raylib");

      while (!WindowShouldClose()) {
          BeginDrawing();
          ClearBackground(RAYWHITE);
          DrawRectangle(100, 100, 200, 150, BLUE);
          DrawText("hello gcLang", 120, 120, 20, BLACK);
          EndDrawing();
      }

      CloseWindow();
      return 0;
  }

  NOTE: No need to memorize hex color codes.
  RAYWHITE, BLUE, BLACK — all defined in raylib.gclib.
  You can also use (Color){r,g,b,a} if you prefer.