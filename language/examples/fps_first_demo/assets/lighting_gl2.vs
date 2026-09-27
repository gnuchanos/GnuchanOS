#version 120

// ---------------------------------------------------------------------------
// lighting_gl2.vs — lighting.vs'in OpenGL 2.1 / GLSL 1.20 varyanti.
//
// NEDEN AYRI BIR DOSYA: raylib bir shader dosyasi kendi `#version` satirini
// tasiyorsa onu OLDUGU GIBI derler (bkz. gcl_raylib_simple_shader). GLSL 3.30
// ile 1.20 ARASINDAKI fark yalnizca surum numarasi degildir: 1.20'de `in/out`
// yerine `attribute/varying`, `texture()` yerine `texture2D()` ve cikis
// degiskeni yerine `gl_FragColor` kullanilir. Ayni dosya ikisini birden
// konusamaz, bu yuzden her surum kendi dosyasinda durur ve script
// `Raylib.GetGLSLVersion()` ile hangisini yukleyecegini secer.
//
// Bu dosya Intel GM965/GL960 (GMA X3100) gibi OpenGL 2.1 tavanli bir GPU
// icindir; orada 330'luk dosya HIC DERLENMEZ ve sahne cizimsiz kalir.
//
// Islev 330'luk dosyayla BIREBIR AYNIDIR; degisen yalnizca sozdizimidir:
//   in/out  -> attribute/varying
//   texture -> texture2D        (fragment tarafinda)
//   out vec4 finalColor -> gl_FragColor
// ---------------------------------------------------------------------------

attribute vec3 vertexPosition;
attribute vec2 vertexTexCoord;
attribute vec3 vertexNormal;
attribute vec4 vertexColor;

uniform mat4 mvp;         // model-view-projection: ekran konumu
uniform mat4 matModel;    // model -> dunya (konum ve normal icin)
uniform mat4 matNormal;   // normal matrisi (olceksiz donusum)

varying vec3 fragPosition;    // dunya uzayi konumu — sis mesafesi
varying vec2 fragTexCoord;
varying vec4 fragColor;
varying vec3 fragNormal;      // dunya uzayi normali — golgeleme

void main()
{
    fragPosition = vec3(matModel*vec4(vertexPosition, 1.0));
    fragTexCoord = vertexTexCoord;
    fragColor    = vertexColor;
    fragNormal   = normalize(vec3(matNormal*vec4(vertexNormal, 1.0)));

    gl_Position = mvp*vec4(vertexPosition, 1.0);
}
