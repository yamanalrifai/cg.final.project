#version 330 core
out vec4 f; // اللون النهائي للبكسل

in vec2 TexCoord; 

uniform sampler2D tex;   // الصورة (Texture)
uniform vec3 colorMix;   // لون إضافي (لتمويل الأهداف باللون الأصفر مثلاً)

void main() { 
    // جلب اللون من الصورة بناءً على الإحداثيات
    vec4 texColor = texture(tex, TexCoord); 
    
    // حذف الأجزاء الشفافة (عشان لو في صور بصيغة PNG)
    if(texColor.a < 0.1) discard; 
    
    // ضرب لون الصورة باللون الممرر (colorMix)
    f = texColor * vec4(colorMix, 1.0); 
}