/* gcl_skybox_shader.h — GLSL for the RaylibSKYBOX module.

   THE SKY ITSELF LIVES ELSEWHERE. The colour function, the uniforms it reads,
   the noise, the star layers and the cloud layers are all in
   `gcl_sky_shared.h`, because the water reflection needs the SAME sky. This
   file now only declares the dome's own stages and pulls the shared body in.

   That split is the point: a second copy of the sky would be a second sky.
   The water used to reflect just the two-colour gradient, so every cloud and
   the sun disc itself were missing from the reflection — the one difference a
   viewer notices immediately. With one implementation they cannot drift. */

#ifndef GCL_SKYBOX_SHADER_H
#define GCL_SKYBOX_SHADER_H

#include "gcl_sky_shared.h"

/* Desktop default; a script may override through Raylib.GLSL_VERSION. */
#define SKY_DEFAULT_GLSL 330

/* The dome carries no texture: the gradient must stay attached to the camera,
   and the sun's position arrives at run time. Both fall out of a function of
   the view DIRECTION, which is what `vDir` is.

   GEOMETRY CONTRACT: the dome's model matrix contains translation ONLY, so
   `vertexPosition` is also the world direction from the centre. The whole sky
   is a function of that direction; no inverse matrix is needed. */
#define SKY_VERTEX_SRC \
"#if __VERSION__ >= 300\n" \
"    #define GCL_ATTR in\n" \
"    #define GCL_VOUT out\n" \
"#else\n" \
"    #define GCL_ATTR attribute\n" \
"    #define GCL_VOUT varying\n" \
"#endif\n" \
"#ifdef GL_ES\n" \
"precision mediump float;\n" \
"#endif\n" \
"GCL_ATTR vec3 vertexPosition;\n" \
"uniform mat4 mvp;\n" \
"GCL_VOUT vec3 vDir;\n" \
"void main(void)\n" \
"{\n" \
"    vDir = vertexPosition;\n" \
"    gl_Position = mvp*vec4(vertexPosition, 1.0);\n" \
"}\n"

/* Fragment stage: the dome is opaque, so the shared sky colour is written
   straight out. Everything that shapes the sky (gradient, horizon glow,
   milky way, stars, sun disc, both cloud layers) happens inside
   gclSkyColor — see gcl_sky_shared.h for the reasoning behind each term. */
#define SKY_FRAGMENT_SRC \
"#if __VERSION__ >= 300\n" \
"    #define GCL_VIN in\n" \
"    out vec4 gclFragColor;\n" \
"    #define GCL_FRAG gclFragColor\n" \
"#else\n" \
"    #define GCL_VIN varying\n" \
"    #define GCL_FRAG gl_FragColor\n" \
"#endif\n" \
"#ifdef GL_ES\n" \
"precision mediump float;\n" \
"#endif\n" \
"GCL_VIN vec3 vDir;\n" \
GCL_SKY_FULL \
"void main(void)\n" \
"{\n" \
"    GCL_FRAG = vec4(gclSkyColor(vDir), 1.0);\n" \
"}\n"

#endif /* GCL_SKYBOX_SHADER_H */
