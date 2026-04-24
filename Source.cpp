#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include <iostream>
#include <vector>
#include <cmath>
#include <windows.h> 
#include <fstream>   
#include <sstream>   
#include <chrono> 

#define GLEW_STATIC
#include <GL/glew.h>
#include <GLFW/glfw3.h>

// --- حالات اللعبة ---
enum GameState { START, PLAYING, WIN };
GameState currentState = GameState::START;

int screenWidth = 800, screenHeight = 600;
const float PI = 3.1415926535f;

// --- متغيرات الكاميرا والتحكم ---
float camX = 0.0f, camY = 0.8f, camZ = 5.0f;
float yaw = -90.0f;
float deltaTime = 0.0f, lastFrame = 0.0f;

// --- التايمر والسكور ---
float gameTimer = 0.0f;
float lastPrint = 0.0f;
int totalItems = 4;
int score = 0;

// --- بناء المتاهة (الجدران والمكعبات) ---
struct Wall { float x, z; float sx, sz; };
std::vector<Wall> walls = {
    {0, -8, 16, 0.5}, {8, 0, 0.5, 16}, {-8, 0, 0.5, 16}, {0, 8, 16, 0.5},
    {3, 0, 0.5, 6}, {-3, 2, 6, 0.5}, {0, -4, 4, 0.5}
};

struct Item { float x, z; bool collected; };
std::vector<Item> items = { {5, 5, false}, {-5, 5, false}, {5, -5, false}, {-5, -5, false} };

// --- دوال المساعدة للصور والشيدرز ---
std::string getFilePath(const std::string& fileName) {
    std::ifstream f1(fileName);
    if (f1.good()) return fileName;
    std::string path2 = "x64/Debug/" + fileName;
    std::ifstream f2(path2);
    if (f2.good()) return path2;
    return fileName;
}

std::string loadShaderFile(const char* filePath) {
    std::string path = getFilePath(filePath);
    std::ifstream file(path);
    if (!file.is_open()) return "";
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

unsigned int loadTexture(char const* path) {
    std::string finalPath = getFilePath(path);
    unsigned int textureID;
    glGenTextures(1, &textureID);
    int width, height, nrComponents;
    stbi_set_flip_vertically_on_load(true);
    unsigned char* data = stbi_load(finalPath.c_str(), &width, &height, &nrComponents, 0);
    if (data) {
        GLenum format = (nrComponents == 4) ? GL_RGBA : GL_RGB;
        glBindTexture(GL_TEXTURE_2D, textureID);
        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        stbi_image_free(data);
    }
    return textureID;
}

// --- مصفوفات التحويل ---
void identity(float* m) { for (int i = 0; i < 16; i++) m[i] = 0; m[0] = m[5] = m[10] = m[15] = 1; }
void perspective(float* m, float fov, float asp, float n, float f) {
    float t = tan(fov / 2.0f); for (int i = 0; i < 16; i++) m[i] = 0;
    m[0] = 1.0f / (asp * t); m[5] = 1.0f / t; m[10] = -(f + n) / (f - n); m[11] = -1.0f; m[14] = -(2.0f * f * n) / (f - n);
}
void ortho(float* m, float l, float r, float b, float t, float n, float f) {
    for (int i = 0; i < 16; i++) m[i] = 0;
    m[0] = 2.0f / (r - l); m[5] = 2.0f / (t - b); m[10] = -2.0f / (f - n);
    m[12] = -(r + l) / (r - l); m[13] = -(t + b) / (t - b); m[14] = -(f + n) / (f - n); m[15] = 1.0f;
}

bool canMove(float x, float z) {
    for (auto& w : walls) {
        if (x > w.x - w.sx / 2 - 0.3f && x < w.x + w.sx / 2 + 0.3f &&
            z > w.z - w.sz / 2 - 0.3f && z < w.z + w.sz / 2 + 0.3f) return false;
    }
    return true;
}

// --- معالجة المدخلات وحل مشكلة السكور ---
void processInput(GLFWwindow* window) {
    if (currentState == START) {
        if (glfwGetKey(window, GLFW_KEY_ENTER) == GLFW_PRESS) {
            currentState = PLAYING;
            system("cls");
        }
    }
    else if (currentState == PLAYING) {
        float speed = 4.5f * deltaTime;
        float rotSpeed = 120.0f * deltaTime;

        if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS) yaw -= rotSpeed;
        if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS) yaw += rotSpeed;

        float rad = yaw * PI / 180.0f;
        float dirX = cos(rad), dirZ = sin(rad);
        float rightX = -sin(rad), rightZ = cos(rad);
        float nX = camX, nZ = camZ;

        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) { nX += dirX * speed; nZ += dirZ * speed; }
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) { nX -= dirX * speed; nZ -= dirZ * speed; }
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) { nX -= rightX * speed; nZ -= rightZ * speed; }
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) { nX += rightX * speed; nZ += rightZ * speed; }

        if (canMove(nX, nZ)) { camX = nX; camZ = nZ; }

        for (auto& it : items) {
            if (!it.collected && sqrt(pow(camX - it.x, 2) + pow(camZ - it.z, 2)) < 0.7f) {
                it.collected = true;
                score++;
                // تحديث فوري للكونسول لضمان ظهور 4/4
                std::cout << " [ SCORE: " << score << "/" << totalItems << " ] | [ TIMER: " << (int)gameTimer << "s ] \r" << std::flush;
            }
        }
        if (score >= totalItems) currentState = WIN;
    }
    else if (currentState == WIN) {
        if (glfwGetKey(window, GLFW_KEY_ENTER) == GLFW_PRESS || glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
            glfwSetWindowShouldClose(window, true);
        }
    }
}

void drawFullScreenQuad(unsigned int shaderProgram, unsigned int textureID) {
    glDisable(GL_DEPTH_TEST);
    glUseProgram(shaderProgram);
    float pr[16]; ortho(pr, 0.0f, (float)screenWidth, 0.0f, (float)screenHeight, -1.0f, 1.0f);
    float vM[16], mM[16]; identity(vM); identity(mM);
    mM[0] = (float)screenWidth; mM[5] = (float)screenHeight;
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "pr"), 1, 0, pr);
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "v"), 1, 0, vM);
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "m"), 1, 0, mM);
    glUniform3f(glGetUniformLocation(shaderProgram, "colorMix"), 1.0f, 1.0f, 1.0f);
    glUniform1f(glGetUniformLocation(shaderProgram, "uvScale"), 1.0f);
    glBindTexture(GL_TEXTURE_2D, textureID);
    glDrawArrays(GL_TRIANGLES, 36, 6);
    glEnable(GL_DEPTH_TEST);
}

int main() {
    ShowWindow(GetConsoleWindow(), SW_MAXIMIZE);
    if (!glfwInit()) return -1;
    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    const GLFWvidmode* mode = glfwGetVideoMode(monitor);
    screenWidth = mode->width; screenHeight = mode->height;
    GLFWwindow* window = glfwCreateWindow(screenWidth, screenHeight, "IT Project - 3D Maze", monitor, NULL);
    glfwMakeContextCurrent(window);
    glewInit();

    // الشيدرز
    std::string vertexSource = loadShaderFile("Shaders/vertex_shader.glsl");
    std::string fragmentSource = loadShaderFile("Shaders/fragment_shader.glsl");
    const char* vCode = vertexSource.c_str(); const char* fCode = fragmentSource.c_str();
    unsigned int s = glCreateProgram();
    unsigned int vS = glCreateShader(GL_VERTEX_SHADER), fS = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(vS, 1, &vCode, 0); glCompileShader(vS);
    glShaderSource(fS, 1, &fCode, 0); glCompileShader(fS);
    glAttachShader(s, vS); glAttachShader(s, fS); glLinkProgram(s);

    float vertices[] = {
        -0.5,-0.5,-0.5, 0,0,  0.5,-0.5,-0.5, 1,0,  0.5, 0.5,-0.5, 1,1,  0.5, 0.5,-0.5, 1,1, -0.5, 0.5,-0.5, 0,1, -0.5,-0.5,-0.5, 0,0,
        -0.5,-0.5, 0.5, 0,0,  0.5,-0.5, 0.5, 1,0,  0.5, 0.5, 0.5, 1,1,  0.5, 0.5, 0.5, 1,1, -0.5, 0.5, 0.5, 0,1, -0.5,-0.5, 0.5, 0,0,
        -0.5, 0.5, 0.5, 1,0, -0.5, 0.5,-0.5, 1,1, -0.5,-0.5,-0.5, 0,1, -0.5,-0.5,-0.5, 0,1, -0.5,-0.5, 0.5, 0,0, -0.5, 0.5, 0.5, 1,0,
         0.5, 0.5, 0.5, 1,0,  0.5, 0.5,-0.5, 1,1,  0.5,-0.5,-0.5, 0,1,  0.5,-0.5,-0.5, 0,1,  0.5,-0.5, 0.5, 0,0,  0.5, 0.5, 0.5, 1,0,
        -0.5,-0.5,-0.5, 0,1,  0.5,-0.5,-0.5, 1,1,  0.5,-0.5, 0.5, 1,0,  0.5,-0.5, 0.5, 1,0, -0.5,-0.5, 0.5, 0,0, -0.5,-0.5,-0.5, 0,1,
        -0.5, 0.5,-0.5, 0,1,  0.5, 0.5,-0.5, 1,1,  0.5, 0.5, 0.5, 1,0,  0.5, 0.5, 0.5, 1,0, -0.5, 0.5, 0.5, 0,0, -0.5, 0.5,-0.5, 0,1,
        0.0f, 0.0f, 0.0f, 0,0,  1.0f, 0.0f, 0.0f, 1,0,  1.0f, 1.0f, 0.0f, 1,1,
        1.0f, 1.0f, 0.0f, 1,1,  0.0f, 1.0f, 0.0f, 0,1,  0.0f, 0.0f, 0.0f, 0,0
    };

    unsigned int VAO, VBO;
    glGenVertexArrays(1, &VAO); glGenBuffers(1, &VBO);
    glBindVertexArray(VAO); glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0); glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float))); glEnableVertexAttribArray(1);

    // الصور
    unsigned int wallTex = loadTexture("Assets/wall.jpg.png");
    unsigned int floorTex = loadTexture("Assets/floor.jpg.png");
    unsigned int ceilingTex = loadTexture("Assets/floor.jpg.png");
    unsigned int startTex = loadTexture("Assets/welcome.jpg.png");
    unsigned int winTex = loadTexture("Assets/congrats.jpg.png");

    while (!glfwWindowShouldClose(window)) {
        float currentFrame = (float)glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        processInput(window);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        if (currentState == START) drawFullScreenQuad(s, startTex);
        else if (currentState == WIN) {
            drawFullScreenQuad(s, winTex);
            static bool printedOnce = false;
            if (!printedOnce) {
                std::cout << "\n\n--- WINNER ---" << "\nFinal Time: " << (int)gameTimer << "s" << std::endl;
                printedOnce = true;
            }
        }
        else {
            gameTimer += deltaTime;
            if (gameTimer - lastPrint > 0.5f) {
                std::cout << " [ SCORE: " << score << "/" << totalItems << " ] | [ TIMER: " << (int)gameTimer << "s ] \r" << std::flush;
                lastPrint = gameTimer;
            }

            glEnable(GL_DEPTH_TEST);
            glUseProgram(s);
            float pr[16], vM[16], mM[16];
            perspective(pr, 45.0f * PI / 180.0f, (float)screenWidth / screenHeight, 0.1f, 100.0f);
            float cosY = cos(-yaw * PI / 180.0f), sinY = sin(-yaw * PI / 180.0f);
            identity(vM); vM[0] = cosY; vM[2] = -sinY; vM[8] = sinY; vM[10] = cosY;
            vM[12] = -(camX * vM[0] + camZ * vM[8]); vM[13] = -camY; vM[14] = -(camX * vM[2] + camZ * vM[10]);
            glUniformMatrix4fv(glGetUniformLocation(s, "pr"), 1, 0, pr);
            glUniformMatrix4fv(glGetUniformLocation(s, "v"), 1, 0, vM);
            glUniform3f(glGetUniformLocation(s, "colorMix"), 1.0f, 1.0f, 1.0f);

            // الأرضية
            glBindTexture(GL_TEXTURE_2D, floorTex);
            glUniform1f(glGetUniformLocation(s, "uvScale"), 5.0f);
            identity(mM); mM[0] = 50.0f; mM[5] = 0.1f; mM[10] = 50.0f;
            glUniformMatrix4fv(glGetUniformLocation(s, "m"), 1, 0, mM);
            glDrawArrays(GL_TRIANGLES, 0, 36);

            // السطح (الجديد)
            glBindTexture(GL_TEXTURE_2D, ceilingTex);
            glUniform1f(glGetUniformLocation(s, "uvScale"), 5.0f);
            identity(mM); mM[0] = 50.0f; mM[5] = 0.1f; mM[10] = 50.0f; mM[13] = 3.0f;
            glUniformMatrix4fv(glGetUniformLocation(s, "m"), 1, 0, mM);
            glDrawArrays(GL_TRIANGLES, 0, 36);

            // الجدران
            glBindTexture(GL_TEXTURE_2D, wallTex);
            glUniform1f(glGetUniformLocation(s, "uvScale"), 1.0f);
            for (auto& w : walls) {
                identity(mM); mM[0] = w.sx; mM[5] = 3.0f; mM[10] = w.sz;
                mM[12] = w.x; mM[13] = 1.5f; mM[14] = w.z;
                glUniformMatrix4fv(glGetUniformLocation(s, "m"), 1, 0, mM);
                glDrawArrays(GL_TRIANGLES, 0, 36);
            }

            // المكعبات
            glUniform3f(glGetUniformLocation(s, "colorMix"), 1.0f, 1.0f, 0.0f);
            for (auto& it : items) {
                if (!it.collected) {
                    identity(mM); mM[0] = 0.5f; mM[5] = 0.5f; mM[10] = 0.5f;
                    mM[12] = it.x; mM[13] = 0.5f; mM[14] = it.z;
                    glUniformMatrix4fv(glGetUniformLocation(s, "m"), 1, 0, mM);
                    glDrawArrays(GL_TRIANGLES, 0, 36);
                }
            }
        }
        glfwSwapBuffers(window);
        glfwPollEvents();
    }
    glfwTerminate();
    return 0;
}