#include <SDL.h>
#include <cstdio>
#include <glad/glad.h>
#include <vector>
#include <stb/stb_image.h>
#include <stb/stb_truetype.h>
// #define STB_VORBIS_HEADER_ONLY
#include <stb/stb_vorbis.c>
#include <miniz.h>

static int winW = 640;
static int winH = 480;

namespace {
    class Zip {
        mz_zip_archive zip{};
    public:
        Zip(const char* name) {
            memset(&zip, 0, sizeof(zip));
            mz_bool ret = mz_zip_reader_init_file(&zip, name, 0);
            if (ret == MZ_FALSE) {
                printf("file not found: %s\n", name);
                exit(-1);
            }
        }
        ~Zip() {
            mz_zip_reader_end(&zip);
        }

        void* open(const char* name, size_t & size) {
            return mz_zip_reader_extract_file_to_heap(&zip, name, &size, 0);
        }

        void close(void* p) {
            mz_free(p);
        }

    };
    Zip* gZip;
    class Shader {
        GLuint vsID;
        GLuint fsID;
        GLuint programID;

    public:
        Shader(const char *vsSrc, const char *fsSrc) {
            auto checkShaderInfo = [](const GLuint id) {
                GLint len;
                glGetShaderiv(id, GL_INFO_LOG_LENGTH, &len);
                if (len != 0) {
                    std::vector<char> info(len + 1);
                    glGetShaderInfoLog(id, len, nullptr, info.data());
                    info[len] = *"\0";
                    printf("%s\n", info.data());
                }
            };
            auto checkProgramInfo = [](const GLuint id) {
                GLint len;
                glGetProgramiv(id, GL_INFO_LOG_LENGTH, &len);
                if (len != 0) {
                    std::vector<char> info(len + 1);
                    glGetProgramInfoLog(id, len, nullptr, info.data());
                    info[len] = *"\0";
                    printf("%s\n", info.data());
                }
            };
            vsID = glCreateShader(GL_VERTEX_SHADER);
            glShaderSource(vsID, 1, &vsSrc, nullptr);
            glCompileShader(vsID);
            checkShaderInfo(vsID);

            fsID = glCreateShader(GL_FRAGMENT_SHADER);
            glShaderSource(fsID, 1, &fsSrc, nullptr);
            glCompileShader(fsID);
            checkShaderInfo(fsID);

            programID = glCreateProgram();
            glAttachShader(programID, vsID);
            glAttachShader(programID, fsID);
            glLinkProgram(programID);
            checkProgramInfo(programID);
        }

        [[nodiscard]] GLuint getID() const { return programID; }

        void attrib(const char *name, const GLint size, const GLenum type, const GLsizei stride = 0,
                    const void *pointer = nullptr) const {
            const GLint location = glGetAttribLocation(programID, name);
            glEnableVertexAttribArray(location);
            glVertexAttribPointer(location, size, type, GL_FALSE, stride, pointer);
        }

        void setVec4(const char *name, const float v0, const float v1, const float v2, const float v3) const {
            // x, y, z, w
            const GLint location = glGetUniformLocation(programID, name);
            glUniform4f(location, v0, v1, v2, v3);
        }

        void setTexture(const char *name, const GLint texture) const {
            const GLint location = glGetUniformLocation(programID, name);
            glUniform1i(location, texture);
        }

        void use() const {
            glUseProgram(programID);
        }

        ~Shader() {
            glDeleteShader(vsID);
            glDeleteShader(fsID);
            glDeleteProgram(programID);
        }
    };

    class Buffer {
        GLuint bufferID{};

        void makeBuffer(const std::vector<float> &point) {
            glGenBuffers(1, &bufferID);
            glBindBuffer(GL_ARRAY_BUFFER, bufferID);
            glBufferData(GL_ARRAY_BUFFER, point.size() * sizeof(float), point.data(), GL_STATIC_DRAW);
            glBindBuffer(GL_ARRAY_BUFFER, 0);
        };

    public:
        Buffer(float x, float y, float w, float h, bool uv = true) {
            // uv: use texture
            const float l = x;
            const float r = x + w;
            const float t = y;
            const float b = y + h;
            const std::vector<float> rect = {
                l, t,
                l, b,
                r, t,
                r, b,
            };
            const std::vector<float> rectUV = {
                l, t, 0, 0,
                l, b, 0, 1,
                r, t, 1, 0,
                r, b, 1, 1,
            };
            makeBuffer(uv ? rectUV : rect);
        }

        explicit Buffer(const std::vector<float> &point) {
            makeBuffer(point);
        }

        [[nodiscard]] GLuint getID() const { return bufferID; }

        void bind() const {
            glBindBuffer(GL_ARRAY_BUFFER, bufferID);
        }

        static void unbind() {
            glBindBuffer(GL_ARRAY_BUFFER, 0);
        }

        ~Buffer() {
            glDeleteBuffers(1, &bufferID);
        }
    };

    class Texture {
        GLuint textureID;

    public:
        Texture(const char *name) {
            int w, h, c;
            stbi_uc *p = stbi_load(name, &w, &h, &c, 4); // r g b a 四个通道
            if (p == nullptr) {
                size_t size;
                void* data = gZip->open(name, size);
                if (data == nullptr) {
                    printf("[ERROR] failed to load %s\n", name);
                    exit(-1);
                }
                else {
                    p = stbi_load_from_memory(reinterpret_cast<stbi_uc*>(data), static_cast<int>(size), &w, &h, &c, 4);
                    gZip->close(data);
                }
            }
            glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
            glGenTextures(1, &textureID);
            glBindTexture(GL_TEXTURE_2D, textureID);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, p);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            stbi_image_free(p);
        }

        Texture(const std::vector<unsigned char> &bitmap, int w, int h) {
            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
            glGenTextures(1, &textureID);
            glBindTexture(GL_TEXTURE_2D, textureID);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, w, h, 0, GL_ALPHA, GL_UNSIGNED_BYTE, bitmap.data());
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        }

        [[nodiscard]] GLuint getID() const { return textureID; }

        void bind(GLint texture) const {
            glActiveTexture(GL_TEXTURE0 + texture);
            glBindTexture(GL_TEXTURE_2D, textureID);
        }

        void unbind() const {
            glBindTexture(GL_TEXTURE_2D, 0);
        }

        ~Texture() {
            glDeleteTextures(1, &textureID);
        }
    };

    class Font {
        std::vector<unsigned char> font;
        stbtt_fontinfo info{};

    public:
        explicit Font(const char *name) {
            FILE *f = fopen(name, "rb");
            if (f == nullptr) {
                size_t size;
                void* data = gZip->open(name, size);
                if (data == nullptr) {
                    printf("failed to open font file: %s\n", name);
                    exit(-1);
                }
                font.resize(size);
                memcpy(font.data(), data, size);
                gZip->close(data);
            }
            else {

            fseek(f, 0, SEEK_END);
            long size = ftell(f);
            fseek(f, 0, SEEK_SET);
            font.resize(size);
            fread(&font[0], 1, size, f);
            fclose(f);
            }

            stbtt_InitFont(&info, font.data(), 0);
        }

        void makeBitmap(wchar_t code, float size, std::vector<unsigned char> &bitmap, int &x0, int &y0, int &w,
                        int &h) const {
            float scale = stbtt_ScaleForPixelHeight(&info, size);
            int x1, y1;
            stbtt_GetCodepointBitmapBox(&info, code, scale, scale, &x0, &y0, &x1, &y1);
            w = x1 - x0;
            h = y1 - y0;
            bitmap.resize(w * h);
            stbtt_MakeCodepointBitmap(&info, bitmap.data(), w, h, w, scale, scale, code);
        }
    };

    class Audio {
        struct Vorbis {
            stb_vorbis *vorbis = nullptr;
            int loop = 0;
            int pause = 0;
            void* data = nullptr;
        };

        static constexpr int MAX_AUDIO = 5;
        SDL_AudioDeviceID audioDeviceID;
        Vorbis vorbis[MAX_AUDIO];
        std::vector<short> samples[MAX_AUDIO];

        int findVorbis() const {
            for (int i = 0; i < MAX_AUDIO; ++i) {
                if (vorbis[i].pause == 0) {
                    return i;
                }
            }
            return -1;
        }

    public:
        Audio() {
            SDL_AudioSpec spec;
            spec.freq = 44100;
            spec.format = AUDIO_S16;
            spec.channels = 2;
            spec.samples = 1024;
            spec.callback = nullptr;
            spec.userdata = nullptr;
            audioDeviceID = SDL_OpenAudioDevice(nullptr, 0, &spec, nullptr, 0);
        }

        ~Audio() {
            SDL_PauseAudioDevice(audioDeviceID, 1);
            SDL_CloseAudioDevice(audioDeviceID);
            for (const auto &item : vorbis) {
                stb_vorbis_close(item.vorbis);
                gZip->close(item.data);
            }
        }

        int open(const char *name, const int loop = 1) {
            int idx = findVorbis();
            if (idx == -1) {
                return -1;
            }
            int error = 0;
            vorbis[idx].vorbis = stb_vorbis_open_filename(name, &error, nullptr);
            if (vorbis[idx].vorbis == nullptr) {
                size_t size;
                void* data = gZip->open(name, size);
                if (data == nullptr) {
                    printf("failed to open file: %s\n", name);
                    exit(-1);
                }
                vorbis[idx].data = data;
                vorbis[idx].vorbis = stb_vorbis_open_memory(reinterpret_cast<const unsigned char*>(data), static_cast<int>(size), &error, nullptr);
            }
            vorbis[idx].loop = loop;
            vorbis[idx].pause = 0;
            return idx;
            // SDL_PauseAudioDevice(audioDeviceID, 0);
        }
        void close(const int idx) {
            stb_vorbis_close(vorbis[idx].vorbis);
            gZip->close(vorbis[idx].data);
            vorbis[idx] = Vorbis();
        }
        void play() {
            Uint32 queuedAudioSize = SDL_GetQueuedAudioSize(audioDeviceID);
            if (queuedAudioSize == 0) {
                for (int i = 0; i < MAX_AUDIO; ++i) {
                    if (!vorbis[i].vorbis || vorbis[i].pause) {
                        continue;
                    }
                    samples[i].resize(2048);
                    int ret = stb_vorbis_get_samples_short_interleaved(vorbis[i].vorbis, 2, samples[i].data(),
                                                                       static_cast<int>(samples[i].size()));
                    if (ret == 0) {
                        stb_vorbis_seek_start(vorbis[i].vorbis);
                        if (!vorbis[i].loop) {
                            vorbis[i].pause = 1;
                        }
                    }
                }
                std::vector<short> samples_mix(2048);
                for (size_t k = 0; k < samples_mix.size(); ++k) {
                    int sample = 0;
                    for (int i = 0; i < MAX_AUDIO; ++i) {
                        if (!vorbis[i].vorbis || vorbis[i].pause) {
                            continue;
                        }
                        sample += samples[i][k];
                    }
                    constexpr int s16max = static_cast<short>(0x7FFF);
                    constexpr int s16min = static_cast<short>(0x8000);
                    if (sample > s16max) {
                        sample = s16max;
                    }
                    if (sample < s16min) {
                        sample = s16min;
                    }
                    samples_mix[k] = static_cast<short>(sample);
                }
                SDL_QueueAudio(audioDeviceID, samples_mix.data(), static_cast<Uint32>(samples_mix.size()) * sizeof(short));
            }
        }

        void pause(int pause) const {
            SDL_PauseAudioDevice(audioDeviceID, pause);
        }
    };
}

int main(int argc, char **argv) {
    printf("mini2d\n");
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO);
    SDL_Window *window = SDL_CreateWindow("mini2d", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, winW, winH,
                                          SDL_WINDOW_OPENGL);
    const SDL_GLContext context = SDL_GL_CreateContext(window);
    SDL_GL_MakeCurrent(window, context);
    if (!gladLoadGLLoader(SDL_GL_GetProcAddress)) {
        printf("Failed to initialize GLAD\n");
        return -1;
    }

    printf("GL_VERSION:%s\n", reinterpret_cast<const char *>(glGetString(GL_VERSION)));

    gZip = new Zip("../data/data.zip");

    auto _checkGLError = [](const char *file, const int line) {
        for (GLint error = glGetError(); error != GL_NO_ERROR; error = glGetError()) {
            printf("[%s][%d]: 0x%04x\n", file, line, error);
        }
    };

#define checkGLError() _checkGLError(__FILE__, __LINE__)

    const auto vsSrc = R"(
        #version 330 core
        attribute vec2 position;
        void main() {
            float x = 2.0 * position.x / (640.0 - 1.0) - 1.0;
            float y = 1.0 - 2.0 * position.y / (480.0 - 1.0);
            gl_Position = vec4(x, y, 0.0, 1.0);
        }
    )";

    const auto fsSrc = R"(
        #version 330 core
        uniform vec4 color;
        void main() {
            // gl_FragColor = vec4(1.0, 0.0, 0.0, 1.0);
            gl_FragColor = color;
        }
    )";

    Shader shader(vsSrc, fsSrc);
    const GLuint programID = shader.getID();


    const auto vsSrcUV = R"(
        #version 330 core
        attribute vec2 position;
        attribute vec2 texcoord;
        varying vec2 uv;
        void main() {
            float x = 2.0 * position.x / (640.0 - 1.0) - 1.0;
            float y = 1.0 - 2.0 * position.y / (480.0 - 1.0);
            gl_Position = vec4(x, y, 0.0, 1.0);
            uv = texcoord;
        }
    )";

    const auto fsSrcUV = R"(
        #version 330 core
        uniform vec4 color;
        uniform sampler2D texture0;
        varying vec2 uv;
        void main() {
            vec4 texcolor = texture2D(texture0, uv);
            gl_FragColor = color * texcolor;
        }
    )";

    const Shader shaderUV(vsSrcUV, fsSrcUV);
    const GLuint programIDUV = shaderUV.getID();

    const Buffer buffer(150, 50, 300, 300, true);
    const Buffer bufferRect(150, 50, 350, 350, false);
    const Buffer bufferPoint(std::vector<float>{50, 100,});
    const Buffer bufferLine(std::vector<float>{0, 0, static_cast<float>(winW) - 1, static_cast<float>(winH) - 1});

    Texture texture("../data/uvchecker.png");

    Font font("../data/AlibabaPuHuiTi-3-55-Regular.ttf");
    int x0, y0, w, h;
    std::vector<unsigned char> bitmap(w * h);
    font.makeBitmap(*L"啊", 50, bitmap, x0, y0, w, h);

    Texture textureFont(bitmap, w, h);

    Buffer bufferFont(150, 50, static_cast<float>(w), static_cast<float>(h));
    const auto fsSrcFont = R"(
        #version 330 core
        uniform vec4 color;
        uniform sampler2D texture0;
        varying vec2 uv;
        void main() {
            vec4 texcolor = texture2D(texture0, uv);
            gl_FragColor = vec4(color.rgb, color.a * texcolor.a);
        }
    )";

    Shader shaderfont(vsSrcUV, fsSrcFont);

    auto draw = [&]() {
        glClearColor(0.5, 0.5f, 0.5f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        glViewport(0, 0, winW, winH);

        auto drawPoint = [&](const Buffer &buffer, const Shader &shader) {
            buffer.bind();
            shader.attrib("position", 2, GL_FLOAT);
            shader.use();
            shader.setVec4("color", 1.0f, 0.0f, 0.0f, 1.0f);
            glDrawArrays(GL_POINTS, 0, 2);
            buffer.unbind();
        };
        auto drawLine = [&](const Buffer &buffer, const Shader &shader) {
            buffer.bind();
            shader.attrib("position", 2, GL_FLOAT);
            shader.use();
            shader.setVec4("color", 0.0f, 1.0f, 0.0f, 1.0f);
            glDrawArrays(GL_LINES, 0, 4);
            buffer.unbind();
        };
        auto drawRect = [&](const Buffer &buffer, const Shader &shader) {
            buffer.bind();
            shader.attrib("position", 2, GL_FLOAT);
            shader.use();
            shader.setVec4("color", 0.0f, 0.0f, 1.0f, 1.0f);
            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
            buffer.unbind();
        };
        auto drawRectUV = [&](const Buffer &buffer, const Shader &shader, const Texture &texture) {
            buffer.bind();
            shader.attrib("position", 2, GL_FLOAT, 4 * sizeof(float), nullptr);
            shader.attrib("texcoord", 2, GL_FLOAT, 4 * sizeof(float),
                          reinterpret_cast<const void *>(2 * sizeof(float)));
            shader.use();
            shader.setVec4("color", 1.0f, 1.0f, 1.0f, 1.0f);
            shader.setTexture("texture0", 0);
            texture.bind(0);
            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
            texture.unbind();
            buffer.unbind();
        };
        drawPoint(bufferPoint, shader);
        drawLine(bufferLine, shader);
        drawRect(bufferRect, shader);
        drawRectUV(buffer, shaderUV, texture);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        drawRectUV(bufferFont, shaderfont, textureFont);
        glDisable(GL_BLEND);
        checkGLError();
        SDL_GL_SwapWindow(window);
    };
    Audio audio;
    audio.open("../data/MeetingTheStars.ogg");
    audio.open("../data/SadSoul.ogg");
    audio.pause(0);

    int done = 0;
    while (!done) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                done = 1;
            }
        }
        draw();
        audio.play();
    };
    SDL_DestroyWindow(window);
    delete gZip;
    return 0;
}
