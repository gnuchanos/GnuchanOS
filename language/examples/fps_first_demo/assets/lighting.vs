#version 330

// ---------------------------------------------------------------------------
// lighting.vs — arazi/kutu vertex asamasi
//
// Bu dosyanin tek isi fragment shader'in ihtiyac duydugu DUNYA UZAYI verisini
// hazirlamaktir. Fragment shader'daki isik ve sis hesaplarinin ikisi de dunya
// uzayinda calisir:
//
//   * isik: normal ile isik yonu ayni uzayda olmali, yoksa golgeleme yanlis
//     yuzleri aydinlatir;
//   * sis:  `fragPosition` ile kameranin goz noktasi arasindaki GERCEK mesafe
//     olculur. Model uzayinda olculseydi, nesne donduruldugunde/otelandiginde
//     sis de onunla birlikte hareket ederdi.
//
// Bu yuzden konum ve normal, `matModel` ile dunya uzayina cevrilir; `mvp` ise
// yalnizca ekran konumu icin kullanilir.
// ---------------------------------------------------------------------------

// Giris nitelikleri (raylib'in varsayilan yerlesimi)
in vec3 vertexPosition;
in vec2 vertexTexCoord;
in vec3 vertexNormal;
in vec4 vertexColor;

// Giris uniform'lari
uniform mat4 mvp;         // model-view-projection: ekran konumu
uniform mat4 matModel;    // model -> dunya (konum ve normal icin)
uniform mat4 matNormal;   // normal matrisi (olceksiz donusum)

// Cikis nitelikleri (fragment shader'a)
out vec3 fragPosition;    // dunya uzayi konumu — sis mesafesi bundan olculur
out vec2 fragTexCoord;
out vec4 fragColor;       // vertex rengi (malzeme tonu ile carpilir)
out vec3 fragNormal;      // dunya uzayi normali — golgeleme bundan hesaplanir

void main()
{
    fragPosition = vec3(matModel*vec4(vertexPosition, 1.0));
    fragTexCoord = vertexTexCoord;
    fragColor    = vertexColor;
    fragNormal   = normalize(vec3(matNormal*vec4(vertexNormal, 1.0)));

    gl_Position = mvp*vec4(vertexPosition, 1.0);
}
